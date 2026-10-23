#include "stdafx.h"
#include "MuEmu/MuEmuTools.h"
#include "MuEmu/MuEmuItemPrice.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
constexpr int Any = -1;

enum CurrencyType
{
    CurrencyZen = 0,
    CurrencyWCoinC = 1,
    CurrencyWCoinP = 2,
    CurrencyGoblinPoint = 3,
    CurrencyRuud = 4,
};

struct PriceInfo
{
    int itemIndex = Any;
    int level = Any;
    int grade = Any;
    int ancient = Any;
    int optionEx = Any;
    int type = CurrencyZen;
    int64_t buyPrice = 0;
    int64_t sellPrice = Any;
};

// Items que se apilan: su precio se multiplica por la cantidad (la durabilidad), como en Main.dll.
constexpr int StackItems[] = { 6688, 6689, 6690, 6288, 6290, 7168, 7169, 7170, 7171, 7172, 7173, 7174, 7175, 7176,
    7197, 7203, 7204, 7205, 7206, 7207, 7008, 7214, 7215, 7216, 7217, 7218, 7238, 7239, 7253, 7254, 7255, 7256,
    7257, 7258, 7268, 7269, 7278, 7301, 7389 };

std::vector<PriceInfo> g_prices;
bool g_loaded = false;

// "*" es cualquier valor.
bool ReadValue(std::istringstream& tokens, int64_t& value)
{
    std::string token;
    if (!(tokens >> token))
        return false;

    if (token == "*")
    {
        value = Any;
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

    const auto fileStream = MuEmu::OpenData("Data/MuEmu/ItemValue.txt");

    std::istream& file = *fileStream;
    for (std::string line; std::getline(file, line);)
    {
        const auto comment = line.find("//");
        if (comment != std::string::npos)
            line.erase(comment);

        std::istringstream tokens(line);
        int64_t values[8];
        bool complete = true;
        for (int64_t& value : values)
            complete = complete && ReadValue(tokens, value);

        if (!complete || values[0] < 0)
            continue;

        PriceInfo info;
        info.itemIndex = static_cast<int>(values[0]);
        info.level = static_cast<int>(values[1]);
        info.grade = static_cast<int>(values[2]);
        info.ancient = static_cast<int>(values[3]);
        info.optionEx = static_cast<int>(values[4]);
        info.type = static_cast<int>(values[5]);
        info.buyPrice = values[6];
        info.sellPrice = values[7];
        g_prices.push_back(info);
    }
}

bool IsStackItem(int itemIndex)
{
    for (const int stackItem : StackItems)
    {
        if (stackItem == itemIndex)
            return true;
    }
    return false;
}

// Primera entrada que coincide, como en Main.dll.
const PriceInfo* FindInfo(const ITEM* item)
{
    if (!g_loaded)
        Load();

    if (item == nullptr || item->Type < 0)
        return nullptr;

    // La opcion ancient llega separada (tipo de set y bonus); Main.dll la compara con el byte del servidor.
    const int ancient = (item->AncientDiscriminator & 3) | ((item->AncientBonusOption & 3) << 2);
    const int grade = item->ExcellentFlags & 0x3F;

    for (const PriceInfo& info : g_prices)
    {
        if (info.itemIndex != item->Type)
            continue;
        if (info.level != Any && info.level != item->Level)
            continue;
        if (info.grade != Any && info.grade != grade)
            continue;
        if (info.ancient != Any && info.ancient != ancient)
            continue;
        if (info.optionEx != Any && info.optionEx != (item->option_380 ? 1 : 0))
            continue;
        return &info;
    }
    return nullptr;
}
} // namespace

bool MuEmu::ItemPrice::Find(const ITEM* item, int goldType, int64_t& value)
{
    if (goldType != 0 && goldType != 1)
        return false;

    const PriceInfo* info = FindInfo(item);
    if (info == nullptr)
        return false;

    const int64_t count = IsStackItem(item->Type) ? item->Durability : 1;
    if (goldType == 0)
        value = info->buyPrice * count;
    else if (info->sellPrice == Any)
        value = (info->buyPrice * count) / 3;
    else
        value = info->sellPrice * count;

    return true;
}

const wchar_t* MuEmu::ItemPrice::GetCurrencyName(const ITEM* item)
{
    const PriceInfo* info = FindInfo(item);
    if (info == nullptr)
        return nullptr;

    switch (info->type)
    {
    case CurrencyWCoinC: return L"WCoinC";
    case CurrencyWCoinP: return L"WCoinP";
    case CurrencyGoblinPoint: return L"Goblin Point";
    case CurrencyRuud: return L"Ruud";
    default: return nullptr;
    }
}
