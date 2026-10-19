#include "stdafx.h"
#include "MuEmu/MuEmuTools.h"

#include "Core/Utilities/Log/MuLogger.h"

#include "Data/DataHandler/ItemData/ItemBmdImport.h"
#include "Data/DataHandler/ItemData/ItemJsonStorage.h"
#include "Data/GameData/ItemData/ItemAttributeConversion.h"
#include "Data/GameData/ItemData/ItemModelJsonFormat.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
bool HasFlag(const char* commandLine, const char* flag)
{
    return commandLine != nullptr && std::strstr(commandLine, flag) != nullptr;
}

void LogIssues(const std::vector<Data::Items::ItemDataIssue>& issues)
{
    for (const Data::Items::ItemDataIssue& issue : issues)
    {
        MU_LOG_WARN(mu::log::Get("data"), "MuEmu item import: {}", issue.ToString());
    }
}

// Convierte los Item_<idioma>.bmd del servidor a los JSON por grupo que usa el cliente.
bool ImportItems()
{
    Data::Items::ItemBmdImportResult result = Data::Items::ImportItemBmdFiles();
    LogIssues(result.issues);
    if (Data::Items::HasErrors(result.issues))
    {
        MU_LOG_ERROR(mu::log::Get("data"), "MuEmu item import: the item.bmd files could not be read");
        return false;
    }

    // Los servidores MuEmu pueden usar slots de equipo propios (12, 13, 14...) que este cliente no tiene:
    // esos items quedan sin slot (no equipables) hasta que el cliente los soporte.
    int unsupportedSlots = 0;
    for (Data::Items::ItemDefinition& item : result.items)
    {
        // El item.bmd no guarda el tipo de arma: se toma el de su grupo, como hace el editor
        if (item.weaponKind == Data::Items::WeaponKind::None)
        {
            item.weaponKind = Data::Items::GetGroupWeaponKind(item);
        }

        if (item.slot != Data::Items::ItemSlot::None && item.slot > Data::Items::ItemSlot::Ring)
        {
            MU_LOG_WARN(mu::log::Get("data"), "MuEmu item import: item ({},{}) uses equipment slot {}, imported without slot",
                        item.group, item.number, static_cast<int>(item.slot));
            item.slot = Data::Items::ItemSlot::None;
            ++unsupportedSlots;
        }
    }

    std::vector<Data::Items::ItemDataIssue> saveIssues;
    const auto saved =
        Data::Items::SaveItemDataDirectory(Data::Items::GetItemDataDirectory(), result.items, saveIssues);
    LogIssues(saveIssues);
    if (saved != Data::Items::ItemDataSaveResult::Saved)
    {
        MU_LOG_ERROR(mu::log::Get("data"), "MuEmu item import: the item data could not be saved");
        return false;
    }

    std::string locales;
    for (const std::string& locale : result.importedLocales)
    {
        locales += (locales.empty() ? "" : ", ") + locale;
    }

    MU_LOG_INFO(mu::log::Get("data"), "MuEmu item import: {} items saved to {} (languages: {}, repaired fields: {}, without slot: {})",
                result.items.size(), Data::Items::GetItemDataDirectory().string(), locales, result.repairs.size(),
                unsupportedSlots);
    return true;
}

struct CustomModelEntry
{
    int itemIndex = -1;
    std::string modelName;
    std::string folder;
};

// Lee una tabla custom de GetMainInfo: una entrada por linea, el indice del item en la columna indicada y el
// modelo entre comillas. Con sections, solo se toma la seccion 0 (CustomJewel.txt).
std::vector<CustomModelEntry> ReadCustomModelTable(const std::filesystem::path& file, int indexColumn, bool lastQuoted,
                                                   bool sections, const std::string& folder)
{
    std::vector<CustomModelEntry> entries;
    std::ifstream stream(file);
    if (!stream)
    {
        return entries;
    }

    int section = sections ? -1 : 0;
    std::string line;
    while (std::getline(stream, line))
    {
        const auto comment = line.find("//");
        if (comment != std::string::npos)
        {
            line.erase(comment);
        }

        std::istringstream tokens(line);
        std::vector<std::string> words;
        for (std::string word; tokens >> word;)
        {
            words.push_back(word);
        }

        if (words.empty())
        {
            continue;
        }

        if (words[0] == "end")
        {
            section = sections ? -1 : 0;
            continue;
        }

        if (sections && section == -1)
        {
            section = std::atoi(words[0].c_str());
            continue;
        }

        if (section != 0 || static_cast<int>(words.size()) <= indexColumn)
        {
            continue;
        }

        const auto first = line.find('"');
        const auto last = line.rfind('"');
        if (first == std::string::npos || last == first)
        {
            continue;
        }

        CustomModelEntry entry;
        entry.itemIndex = std::atoi(words[indexColumn].c_str());
        if (lastQuoted)
        {
            const auto open = line.rfind('"', last - 1);
            entry.modelName = line.substr(open + 1, last - open - 1);
        }
        else
        {
            const auto close = line.find('"', first + 1);
            entry.modelName = line.substr(first + 1, close - first - 1);
        }

        entry.folder = folder;
        if (entry.itemIndex >= 0 && !entry.modelName.empty())
        {
            entries.push_back(entry);
        }
    }

    return entries;
}

// Agrega los modelos de los items custom (tablas de MAIN_INFO/Custom) a Data/Items/Models.
bool ImportModels(const std::filesystem::path& customFolder)
{
    constexpr int MaxItemIndex = 512;

    std::vector<CustomModelEntry> entries;
    const auto add = [&](std::vector<CustomModelEntry> more) { entries.insert(entries.end(), more.begin(), more.end()); };
    add(ReadCustomModelTable(customFolder / "CustomItem.txt", 0, false, false, ""));
    add(ReadCustomModelTable(customFolder / "CustomJewel.txt", 1, true, true, "Item"));
    add(ReadCustomModelTable(customFolder / "CustomPet.txt", 0, false, false, "Item"));
    add(ReadCustomModelTable(customFolder / "CustomFlag.txt", 0, false, false, "Custom/CustomFlag"));

    if (entries.empty())
    {
        MU_LOG_ERROR(mu::log::Get("data"), "MuEmu model import: no custom models found in {}", customFolder.string());
        return false;
    }

    Data::Items::ItemModelDataLoadResult loaded =
        Data::Items::LoadItemModelDataDirectory(Data::Items::GetItemModelDataDirectory());
    std::vector<Data::Items::ItemModelDefinition>& models = loaded.models;

    std::set<int> changedGroups;
    int added = 0;
    int updated = 0;
    for (const CustomModelEntry& entry : entries)
    {
        const int group = entry.itemIndex / MaxItemIndex;
        const int number = entry.itemIndex % MaxItemIndex;
        // Como Main.dll: los sets (grupos 7 a 11) estan en Data/Player, el resto en la carpeta de su tabla.
        std::string folder = entry.folder;
        if (folder.empty())
        {
            folder = (group >= 7 && group <= 11) ? "Player" : "Item";
        }

        Data::Items::ItemModelDefinition definition;
        definition.group = group;
        definition.number = number;
        definition.file = "Data/" + folder + "/" + entry.modelName + ".bmd";
        definition.textureFolders = {folder};

        auto existing = std::find_if(models.begin(), models.end(), [&](const Data::Items::ItemModelDefinition& m)
                                     { return m.group == group && m.number == number; });
        if (existing == models.end())
        {
            models.push_back(definition);
            ++added;
        }
        else
        {
            existing->model.clear();
            existing->file = definition.file;
            existing->textureFolders = definition.textureFolders;
            ++updated;
        }

        changedGroups.insert(group);
    }

    for (const int group : changedGroups)
    {
        const std::filesystem::path file =
            Data::Items::GetItemModelDataDirectory() / Data::Items::GetItemGroupFileName(group);
        std::ofstream output(file, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            MU_LOG_ERROR(mu::log::Get("data"), "MuEmu model import: could not write {}", file.string());
            return false;
        }

        output << Data::Items::WriteItemModelGroupJson(group, models);
    }

    MU_LOG_INFO(mu::log::Get("data"), "MuEmu model import: {} custom models ({} added, {} updated) in {} groups",
                entries.size(), added, updated, changedGroups.size());
    return true;
}

// Valor de "--flag=valor", o el valor por defecto.
std::filesystem::path GetFlagPath(const char* commandLine, const char* flag, const char* defaultValue)
{
    const char* found = std::strstr(commandLine, flag);
    std::string value = defaultValue;
    if (found != nullptr && found[std::strlen(flag)] == '=')
    {
        const char* start = found + std::strlen(flag) + 1;
        const bool quoted = (*start == '"');
        if (quoted)
        {
            ++start;
        }
        const char* end = start;
        while (*end != 0 && (quoted ? *end != '"' : *end != ' '))
        {
            ++end;
        }
        value.assign(start, end);
    }
    return std::filesystem::path(value);
}
} // namespace

bool MuEmu::RunCommandLineTool(const char* commandLine)
{
    if (HasFlag(commandLine, "--muemu-import-items"))
    {
        ImportItems();
        mu::log::Get("data")->flush();
        return true;
    }

    if (HasFlag(commandLine, "--muemu-import-models"))
    {
        ImportModels(GetFlagPath(commandLine, "--muemu-import-models", "MAIN_INFO/Custom"));
        mu::log::Get("data")->flush();
        return true;
    }

    return false;
}
