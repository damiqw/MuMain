#include "stdafx.h"
#include "MuEmu/MuEmuTools.h"

#include "Core/Utilities/Log/MuLogger.h"

#include "Data/DataHandler/ItemData/ItemBmdImport.h"
#include "Data/DataHandler/ItemData/ItemJsonStorage.h"
#include "Data/GameData/ItemData/ItemAttributeConversion.h"
#include "Data/GameData/ItemData/ItemModelJsonFormat.h"

#include <algorithm>
#include <cstdlib>
#include <cctype>
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

    // El item.bmd no tiene las etiquetas del cliente (pocion de vida, de mana...) ni la prioridad de uso: se toman
    // de los items originales del cliente. Antes de la primera importacion se guardan en Data/Items.sven.
    const std::filesystem::path itemDirectory = Data::Items::GetItemDataDirectory();
    const std::filesystem::path originalDirectory = itemDirectory.parent_path() / "Items.sven";
    std::error_code copyError;
    if (!std::filesystem::exists(originalDirectory))
    {
        std::filesystem::copy(itemDirectory, originalDirectory, std::filesystem::copy_options::recursive, copyError);
    }
    const Data::Items::ItemDataLoadResult original = Data::Items::LoadItemDataDirectory(originalDirectory);
    int keptTags = 0;
    for (Data::Items::ItemDefinition& item : result.items)
    {
        const auto found = std::find_if(original.items.begin(), original.items.end(),
                                        [&item](const Data::Items::ItemDefinition& other)
                                        { return other.group == item.group && other.number == item.number; });
        if (found != original.items.end())
        {
            item.tags = found->tags;
            item.potionPriority = found->potionPriority;
            ++keptTags;
        }
    }
    MU_LOG_INFO(mu::log::Get("data"), "MuEmu item import: tags of {} items taken from {}", keptTags,
                originalDirectory.string());

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
    int missing = 0;
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

        // Un modelo que no esta en la Data no se registra: el item conserva el modelo que ya tenia (si tenia).
        if (!std::filesystem::exists(definition.file))
        {
            MU_LOG_WARN(mu::log::Get("data"), "MuEmu model import: item ({},{}) model {} not found, skipped", group, number,
                        definition.file);
            ++missing;
            continue;
        }

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

    // CustomItemPosition.txt: ItemIndex TranslationX TranslationY RotationX RotationY RotationZ Scale ScaleDrop.
    // Como Main.dll: la traslacion se suma a la posicion, las rotaciones reemplazan las del item y Scale
    // multiplica la escala de inventario; ScaleDrop (si no es 0) es la escala en el piso.
    int positions = 0;
    std::ifstream positionStream(customFolder / "CustomItemPosition.txt");
    for (std::string line; std::getline(positionStream, line);)
    {
        const auto comment = line.find("//");
        if (comment != std::string::npos)
        {
            line.erase(comment);
        }

        std::istringstream tokens(line);
        int itemIndex = 0;
        double tx = 0, ty = 0, rx = 0, ry = 0, rz = 0, scale = 0, scaleDrop = 0;
        if (!(tokens >> itemIndex >> tx >> ty >> rx >> ry >> rz >> scale))
        {
            continue;
        }
        tokens >> scaleDrop;

        const int group = itemIndex / MaxItemIndex;
        const int number = itemIndex % MaxItemIndex;
        auto model = std::find_if(models.begin(), models.end(), [&](const Data::Items::ItemModelDefinition& m)
                                  { return m.group == group && m.number == number; });
        if (model == models.end())
        {
            MU_LOG_WARN(mu::log::Get("data"), "MuEmu model import: position of item ({},{}) without model, skipped", group,
                        number);
            continue;
        }

        model->inventory.offset = {tx, ty, 0.0};
        model->inventory.rotation = {rx, ry, rz};
        if (scale > 0)
        {
            model->inventory.scale = Data::Items::ItemInventoryDisplay::DefaultScale * scale;
        }
        if (scaleDrop > 0)
        {
            model->ground.scale = scaleDrop;
        }

        changedGroups.insert(group);
        ++positions;
    }

    // CustomPet.txt: Index IndexInv Type "Model" Fenrir ScaleInv ScaleDrop ScaleView. Como Main.dll, la mascota
    // se ve en el inventario con su modelo completo, a ScaleInv de la escala normal y un poco corrida.
    std::ifstream petStream(customFolder / "CustomPet.txt");
    for (std::string line; std::getline(petStream, line);)
    {
        const auto comment = line.find("//");
        if (comment != std::string::npos)
        {
            line.erase(comment);
        }

        const auto open = line.find('"');
        const auto close = open == std::string::npos ? std::string::npos : line.find('"', open + 1);
        if (close == std::string::npos)
        {
            continue;
        }

        std::istringstream head(line.substr(0, open));
        std::istringstream tail(line.substr(close + 1));
        int itemIndex = 0;
        int fenrir = 0;
        double scaleInventory = 0, scaleDrop = 0;
        if (!(head >> itemIndex) || !(tail >> fenrir >> scaleInventory >> scaleDrop))
        {
            continue;
        }

        const int group = itemIndex / MaxItemIndex;
        const int number = itemIndex % MaxItemIndex;
        auto model = std::find_if(models.begin(), models.end(), [&](const Data::Items::ItemModelDefinition& m)
                                  { return m.group == group && m.number == number; });
        if (model == models.end())
        {
            continue;
        }

        if (scaleInventory > 0)
        {
            model->inventory.scale = Data::Items::ItemInventoryDisplay::DefaultScale * scaleInventory;
            model->inventory.offset = {0.01, -0.05, 0.0};
        }
        if (scaleDrop > 0)
        {
            model->ground.scale = scaleDrop;
        }
        changedGroups.insert(group);
        ++positions;
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

    MU_LOG_INFO(mu::log::Get("data"),
                "MuEmu model import: {} custom models ({} added, {} updated, {} without file), {} positions, in {} groups",
                entries.size(), added, updated, missing, positions, changedGroups.size());
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

int MuEmu::GetConfigInt(const char* key, int defaultValue)
{
    if (std::string locked; GetInfoValue("MuEmu", key, locked))
        return std::atoi(locked.c_str());

    std::ifstream stream("config.ini");
    bool inSection = false;
    for (std::string line; std::getline(stream, line);)
    {
        const auto first = line.find_first_not_of(" 	");
        if (first == std::string::npos || line[first] == ';' || line[first] == '#')
        {
            continue;
        }

        if (line[first] == '[')
        {
            inSection = line.compare(first, 7, "[MuEmu]") == 0;
            continue;
        }

        const auto separator = line.find('=');
        if (!inSection || separator == std::string::npos)
        {
            continue;
        }

        std::string name = line.substr(first, separator - first);
        name.erase(name.find_last_not_of(" 	") + 1);
        const std::string wanted = key;
        if (name.size() == wanted.size() &&
            std::equal(name.begin(), name.end(), wanted.begin(),
                       [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); }))
        {
            return std::atoi(line.c_str() + separator + 1);
        }
    }

    return defaultValue;
}

void MuEmu::SetConfigInt(const char* key, int value)
{
    std::vector<std::string> lines;
    {
        std::ifstream stream("config.ini");
        for (std::string line; std::getline(stream, line);)
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            lines.push_back(line);
        }
    }

    const std::string wanted = key;
    const auto sameName = [&wanted](const std::string& line, std::size_t first, std::size_t separator)
    {
        std::string name = line.substr(first, separator - first);
        name.erase(name.find_last_not_of(" \t") + 1);
        return name.size() == wanted.size() &&
               std::equal(name.begin(), name.end(), wanted.begin(), [](char a, char b)
                          { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); });
    };

    const std::string entry = wanted + " = " + std::to_string(value);
    bool inSection = false;
    std::size_t sectionEnd = std::string::npos;
    bool written = false;
    for (std::size_t n = 0; n < lines.size() && !written; ++n)
    {
        const std::string& line = lines[n];
        const auto first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] == ';' || line[first] == '#')
            continue;
        if (line[first] == '[')
        {
            inSection = line.compare(first, 7, "[MuEmu]") == 0;
            if (inSection)
                sectionEnd = n + 1;
            continue;
        }
        if (!inSection)
            continue;
        sectionEnd = n + 1;
        const auto separator = line.find('=');
        if (separator != std::string::npos && sameName(line, first, separator))
        {
            lines[n] = entry;
            written = true;
        }
    }

    if (!written)
    {
        if (sectionEnd == std::string::npos)
        {
            lines.push_back("[MuEmu]");
            lines.push_back(entry);
        }
        else
        {
            lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(sectionEnd), entry);
        }
    }

    std::ofstream out("config.ini", std::ios::binary | std::ios::trunc);
    for (const std::string& line : lines)
        out << line << "\r\n";
}

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
