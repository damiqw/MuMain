#include "stdafx.h"
#include "MuEmu/MuEmuTools.h"
#include "MuEmu/MuEmuJewel.h"

#include <fstream>
#include <map>
#include <sstream>
#include <string>

namespace
{
// Columnas de la seccion 0 (sin contar el nombre del modelo).
constexpr int ColumnCount = 22;
constexpr int ColumnItemIndex = 1;
constexpr int ColumnEnableSlotWeapon = 13;
constexpr int ColumnEnableSlotArmor = 14;
constexpr int ColumnEnableSlotWing = 15;
constexpr int ColumnEnableSlotAccesory = 16;
constexpr int ColumnSalePrice = 21;

struct JewelInfo
{
    bool weapon = false;
    bool armor = false;
    bool wing = false;
    bool accesory = false;
    int64_t salePrice = 0;
};

std::map<int, JewelInfo> g_jewels;
bool g_loaded = false;

constexpr int GetItem(int group, int index)
{
    return (group * MAX_ITEM_INDEX) + index;
}

bool ReadValue(const std::string& token, int64_t& value)
{
    if (token == "*")
    {
        value = -1;
        return true;
    }

    try
    {
        value = std::stoll(token);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

void Load()
{
    g_loaded = true;

    const auto fileStream = MuEmu::OpenData("Data/MuEmu/CustomJewel.txt");

    std::istream& file = *fileStream;
    int section = -1;
    for (std::string line; std::getline(file, line);)
    {
        const auto comment = line.find("//");
        if (comment != std::string::npos)
            line.erase(comment);

        std::istringstream tokens(line);
        std::string first;
        if (!(tokens >> first))
            continue;

        if (section == -1)
        {
            section = std::atoi(first.c_str());
            continue;
        }

        if (first == "end")
        {
            // Solo interesa la seccion 0 (definicion de las joyas).
            if (section == 0)
                break;
            section = -1;
            continue;
        }

        if (section != 0)
            continue;

        int64_t values[ColumnCount];
        bool complete = ReadValue(first, values[0]);
        for (int n = 1; n < ColumnCount; ++n)
        {
            std::string token;
            complete = complete && (tokens >> token) && ReadValue(token, values[n]);
        }

        if (!complete || values[ColumnItemIndex] < 0)
            continue;

        JewelInfo info;
        info.weapon = values[ColumnEnableSlotWeapon] == 1;
        info.armor = values[ColumnEnableSlotArmor] == 1;
        info.wing = values[ColumnEnableSlotWing] == 1;
        info.accesory = values[ColumnEnableSlotAccesory] == 1;
        info.salePrice = values[ColumnSalePrice];
        g_jewels.emplace(static_cast<int>(values[ColumnItemIndex]), info);
    }
}

const JewelInfo* FindInfo(int itemType)
{
    if (!g_loaded)
        Load();

    const auto found = g_jewels.find(itemType);
    return found == g_jewels.end() ? nullptr : &found->second;
}

bool IsAccesory(int itemType)
{
    switch (itemType)
    {
    case GetItem(13, 8):  // Ring of Ice
    case GetItem(13, 9):  // Ring of Poison
    case GetItem(13, 21): // Ring of Fire
    case GetItem(13, 22): // Ring of Earth
    case GetItem(13, 23): // Ring of Wind
    case GetItem(13, 24): // Ring of Magic
    case GetItem(13, 12): // Pendant of Lighting
    case GetItem(13, 13): // Pendant of Fire
    case GetItem(13, 25): // Pendant of Ice
    case GetItem(13, 26): // Pendant of Wind
    case GetItem(13, 27): // Pendant of Water
    case GetItem(13, 28): // Pendant of Ability
        return true;
    default:
        return false;
    }
}

// Las alas y capas que acepta Main.dll (CustomWing.txt no tiene alas en nuestros servidores).
bool IsWing(int itemType)
{
    return (itemType >= GetItem(12, 0) && itemType <= GetItem(12, 6))
        || (itemType >= GetItem(12, 36) && itemType <= GetItem(12, 43))
        || itemType == GetItem(12, 49) || itemType == GetItem(12, 50)
        || (itemType >= GetItem(12, 262) && itemType <= GetItem(12, 265))
        || itemType == GetItem(13, 30);
}
} // namespace

bool MuEmu::Jewel::IsJewel(int itemType)
{
    return FindInfo(itemType) != nullptr;
}

// Las mismas reglas que CCustomJewel::CheckCustomJewelApplyItem de Main.dll.
bool MuEmu::Jewel::CanApply(int jewelType, int targetType)
{
    const JewelInfo* info = FindInfo(jewelType);
    if (info == nullptr || targetType < 0)
        return false;

    const int group = targetType / MAX_ITEM_INDEX;
    const int index = targetType % MAX_ITEM_INDEX;
    if (group >= 7 && group <= 11 && index >= 62 && index <= 72)
        return false;

    const bool wing = IsWing(targetType);
    const bool accesory = IsAccesory(targetType);
    if (targetType > GetItem(12, 6) && !wing && !accesory)
        return false;

    if (!info->weapon && targetType < GetItem(6, 0))
        return false;
    if (!info->armor && targetType >= GetItem(6, 0) && targetType < GetItem(12, 0))
        return false;
    if (!info->wing && wing)
        return false;
    if (!info->accesory && accesory)
        return false;

    return true;
}

bool MuEmu::Jewel::GetSalePrice(int itemType, int64_t& price)
{
    const JewelInfo* info = FindInfo(itemType);
    if (info == nullptr)
        return false;

    price = info->salePrice;
    return true;
}
