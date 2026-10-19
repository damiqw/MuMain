#include "stdafx.h"
#include "MuEmu/MuEmuTooltip.h"

#include "Core/Text/Utf8.h"
#include "Engine/Object/ZzzInventory.h"

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
constexpr int MaxTextLines = 50;

struct TooltipLine
{
    int text = -1;
    int color = 0;
};

struct TooltipInfo
{
    std::wstring title;
    int color = 0;
    std::vector<TooltipLine> lines;
};

std::map<int, TooltipInfo> g_tooltips;
std::map<int, std::wstring> g_texts;
bool g_loaded = false;

// Colores de GetMainInfo: 1 a 13 son los de Webzen (los mismos TEXT_COLOR_* del cliente); 14 es blanco y
// 15 a 24 (RGB propios) van al color del cliente mas parecido.
int ToTextColor(int color)
{
    if (color >= 1 && color <= 13)
        return color;

    switch (color)
    {
    case 15: return TEXT_COLOR_REDPURPLE; // DeepPink1
    case 16: return TEXT_COLOR_BLUE;      // RoyalBlue1
    case 17: return TEXT_COLOR_VIOLET;    // SlateBlue1
    case 19: return TEXT_COLOR_GREEN_BLUE; // DarkSlateGray1
    case 20: return TEXT_COLOR_DARKBLUE;  // SlateGray1
    case 22: return TEXT_COLOR_VIOLET;    // LightSlateBlue
    case 23: return TEXT_COLOR_BLUE;      // DeepSkyBlue1
    case 24: return TEXT_COLOR_GREEN_BLUE; // Turquoise1
    default: return TEXT_COLOR_WHITE;
    }
}

// Saca el texto entre comillas y deja en `rest` lo que sigue. GetMainInfo usa "%%" para un "%".
bool ReadQuoted(const std::string& line, std::string& text, std::string& rest)
{
    const auto open = line.find('"');
    const auto close = open == std::string::npos ? std::string::npos : line.find('"', open + 1);
    if (close == std::string::npos)
        return false;

    text = line.substr(open + 1, close - open - 1);
    for (std::string::size_type at = text.find("%%"); at != std::string::npos; at = text.find("%%", at + 1))
        text.erase(at, 1);

    rest = line.substr(close + 1);
    return true;
}

std::string StripComment(std::string line)
{
    // Los comentarios empiezan con "//" fuera de las comillas.
    bool quoted = false;
    for (std::string::size_type i = 0; i + 1 < line.size(); ++i)
    {
        if (line[i] == '"')
            quoted = !quoted;
        else if (!quoted && line[i] == '/' && line[i + 1] == '/')
            return line.substr(0, i);
    }
    return line;
}

void Load()
{
    g_loaded = true;

    std::ifstream texts("Data/MuEmu/CustomItemTooltipText.txt");
    for (std::string line; std::getline(texts, line);)
    {
        line = StripComment(line);
        std::istringstream tokens(line);
        int index = -1;
        std::string text, rest;
        if ((tokens >> index) && ReadQuoted(line, text, rest))
            g_texts[index] = Core::Text::FromUtf8(text);
    }

    std::ifstream tooltips("Data/MuEmu/CustomItemToolTip.txt");
    for (std::string line; std::getline(tooltips, line);)
    {
        line = StripComment(line);
        std::istringstream head(line);
        int itemIndex = -1;
        std::string title, rest;
        if (!(head >> itemIndex) || !ReadQuoted(line, title, rest))
            continue;

        TooltipInfo info;
        info.title = Core::Text::FromUtf8(title);

        std::istringstream values(rest);
        values >> info.color;
        info.color = ToTextColor(info.color);
        for (TooltipLine entry; values >> entry.text >> entry.color;)
        {
            if (entry.text >= 0)
            {
                entry.color = ToTextColor(entry.color);
                info.lines.push_back(entry);
            }
        }

        // Como Main.dll, la primera definicion de cada item es la que vale.
        g_tooltips.emplace(itemIndex, std::move(info));
    }
}
} // namespace

int MuEmu::Tooltip::Append(int itemType, int textNum)
{
    if (!g_loaded)
        Load();

    const auto found = g_tooltips.find(itemType);
    if (found == g_tooltips.end())
        return textNum;

    const TooltipInfo& info = found->second;
    if (textNum + 2 >= MaxTextLines)
        return textNum;

    mu_swprintf(TextList[textNum], L"\n");
    TextListColor[textNum] = TEXT_COLOR_WHITE;
    TextBold[textNum] = false;
    ++textNum;

    mu_swprintf(TextList[textNum], L"%ls", info.title.c_str());
    TextListColor[textNum] = info.color;
    TextBold[textNum] = true;
    ++textNum;

    for (const TooltipLine& line : info.lines)
    {
        const auto text = g_texts.find(line.text);
        if (text == g_texts.end() || textNum >= MaxTextLines - 1)
            continue;

        mu_swprintf(TextList[textNum], L"%ls", text->second.c_str());
        TextListColor[textNum] = line.color;
        TextBold[textNum] = false;
        ++textNum;
    }

    return textNum;
}
