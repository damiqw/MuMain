#include "stdafx.h"
#include "MuEmu/MuEmuTools.h"
#include "MuEmu/MuEmuBuff.h"

#include "Core/Text/Utf8.h"

#include <array>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>

namespace
{
// Primer buff que el cliente no conoce: desde aca el icono sale de la segunda columna.
constexpr int FirstServerBuff = 205;
constexpr int MaxValues = 5;

std::vector<MuEmu::Buff::Entry> g_entries;
std::map<int, int> g_icons;
std::map<int, std::array<int, MaxValues>> g_values;
bool g_loaded = false;

std::vector<std::string> ReadQuoted(const std::string& line, std::string& rest)
{
    std::vector<std::string> texts;
    std::string::size_type at = 0;
    for (;;)
    {
        const auto open = line.find('"', at);
        const auto close = open == std::string::npos ? std::string::npos : line.find('"', open + 1);
        if (close == std::string::npos)
        {
            rest += line.substr(at);
            break;
        }
        rest += line.substr(at, open - at) + " ";
        texts.push_back(line.substr(open + 1, close - open - 1));
        at = close + 1;
    }
    return texts;
}

void Load()
{
    g_loaded = true;

    const auto fileStream = MuEmu::OpenData("Data/MuEmu/CustomBuffIcon.txt");

    std::istream& file = *fileStream;
    for (std::string line; std::getline(file, line);)
    {
        // Los comentarios empiezan con "//" fuera de las comillas.
        bool quoted = false;
        for (std::string::size_type i = 0; i + 1 < line.size(); ++i)
        {
            if (line[i] == '"')
                quoted = !quoted;
            else if (!quoted && line[i] == '/' && line[i + 1] == '/')
            {
                line.erase(i);
                break;
            }
        }

        std::string rest;
        const std::vector<std::string> texts = ReadQuoted(line, rest);
        std::istringstream tokens(rest);

        MuEmu::Buff::Entry entry;
        if (texts.size() < 2 || !(tokens >> entry.index >> entry.effectType >> entry.itemType >> entry.itemIndex >>
                                  entry.classType >> entry.noticeType >> entry.clearType))
            continue;
        if (entry.index <= 0 || entry.index > 255)
            continue;

        entry.name = Core::Text::FromUtf8(texts[0]);
        entry.descript = Core::Text::FromUtf8(texts[1]);

        if (entry.index >= FirstServerBuff)
        {
            // La segunda columna es el icono; el tipo de efecto propio evita que compartan tiempo con otros.
            g_icons[entry.index] = entry.effectType;
            entry.effectType = entry.index;
        }

        g_entries.push_back(std::move(entry));
    }
}

void EnsureLoaded()
{
    if (!g_loaded)
        Load();
}

// Que valor va en cada "%d" para los buffs que repiten valores (como en Main.dll); el resto va en orden.
int ValueSlot(int buff, int position)
{
    switch (buff)
    {
    case 1: // Increase Attack Power
        return 0;
    case 3: // Shadow Phantom: ataque x3 y defensa
        return position < 3 ? 0 : 1;
    case 11: // Potion of Soul
        return position < 2 ? 0 : 1;
    default:
        return position < MaxValues ? position : MaxValues - 1;
    }
}
} // namespace

void MuEmu::Buff::ForEach(const std::function<void(const Entry&)>& apply)
{
    EnsureLoaded();
    for (const Entry& entry : g_entries)
        apply(entry);
}

int MuEmu::Buff::GetIcon(int buff)
{
    EnsureLoaded();
    const auto found = g_icons.find(buff);
    return found == g_icons.end() ? buff : found->second;
}

void MuEmu::Buff::ReceiveValues(const unsigned char* buffer, int size)
{
    // C1 FB 13: indice del buff, nombre (20), tipos (5) y 5 valores (int) al final del paquete.
    constexpr int ValuesSize = MaxValues * 4;
    if (buffer == nullptr || size < 5 + ValuesSize)
        return;

    std::array<int, MaxValues> values{};
    const unsigned char* data = buffer + size - ValuesSize;
    for (int n = 0; n < MaxValues; ++n)
    {
        const unsigned char* value = data + (n * 4);
        values[n] = static_cast<int>(value[0] | (value[1] << 8) | (value[2] << 16) | (static_cast<unsigned>(value[3]) << 24));
    }
    g_values[buffer[4]] = values;
}

void MuEmu::Buff::FormatLines(int buff, std::list<std::wstring>& lines)
{
    const auto found = g_values.find(buff);
    int position = 0;
    for (std::wstring& line : lines)
    {
        std::wstring text;
        for (std::wstring::size_type i = 0; i < line.size(); ++i)
        {
            if (line[i] == L'%' && i + 1 < line.size() && line[i + 1] == L'%')
            {
                text += L'%';
                ++i;
            }
            else if (line[i] == L'%' && i + 1 < line.size() && line[i + 1] == L'd')
            {
                text += found == g_values.end() ? std::wstring(L"?")
                                                : std::to_wstring(found->second[ValueSlot(buff, position)]);
                ++position;
                ++i;
            }
            else
            {
                text += line[i];
            }
        }
        line = std::move(text);
    }
}
