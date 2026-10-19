#include "stdafx.h"
#include "MuEmu/MuEmuTools.h"

#include "Core/Utilities/Log/MuLogger.h"

#include "Data/DataHandler/ItemData/ItemBmdImport.h"
#include "Data/DataHandler/ItemData/ItemJsonStorage.h"
#include "Data/GameData/ItemData/ItemAttributeConversion.h"

#include <cstring>
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
} // namespace

bool MuEmu::RunCommandLineTool(const char* commandLine)
{
    if (HasFlag(commandLine, "--muemu-import-items"))
    {
        ImportItems();
        mu::log::Get("data")->flush();
        return true;
    }

    return false;
}
