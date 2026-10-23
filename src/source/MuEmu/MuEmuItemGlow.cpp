#include "stdafx.h"
#include "MuEmu/MuEmuTools.h"
#include "MuEmu/MuEmuItemGlow.h"

#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
std::map<int, std::array<float, 3>> g_colors;
bool g_loaded = false;

// Los numeros de la linea, sin el texto entre comillas ni los comentarios; vacio en "end".
std::vector<float> ReadNumbers(std::string line)
{
    const auto comment = line.find("//");
    if (comment != std::string::npos)
        line.erase(comment);

    for (auto open = line.find('"'); open != std::string::npos; open = line.find('"'))
    {
        const auto close = line.find('"', open + 1);
        line.replace(open, close == std::string::npos ? std::string::npos : close - open + 1, " ");
    }

    std::vector<float> values;
    std::istringstream tokens(line);
    for (std::string token; tokens >> token;)
    {
        if (token == "end")
            return {};
        values.push_back(std::strtof(token.c_str(), nullptr));
    }
    return values;
}

// CustomItem y CustomFlag: indice y R G B (el nombre del modelo o de la bandera va entre comillas).
void LoadFile(const char* path)
{
    const auto fileStream = MuEmu::OpenData(path);
    std::istream& file = *fileStream;
    for (std::string line; std::getline(file, line);)
    {
        const std::vector<float> values = ReadNumbers(line);
        if (values.size() < 4)
            continue;

        const int itemType = static_cast<int>(values[0]);
        // Como Main.dll, la primera definicion es la que vale (CustomItem antes que CustomFlag).
        g_colors.emplace(itemType, std::array<float, 3>{ values[1] / 255.f, values[2] / 255.f, values[3] / 255.f });
    }
}
} // namespace

void MuEmu::ItemGlow::Apply(int itemType, std::array<float, 3>& color)
{
    if (!g_loaded)
    {
        g_loaded = true;
        LoadFile("Data/MuEmu/CustomItem.txt");
        LoadFile("Data/MuEmu/CustomFlag.txt");
    }

    const auto found = g_colors.find(itemType);
    if (found != g_colors.end())
        color = found->second;
}
