#include "stdafx.h"
#include "MuEmu/MuEmuEventLevel.h"

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
struct LevelRow
{
    int commonMin = 0;
    int commonMax = 0;
    int specialMin = 0;
    int specialMax = 0;
};

// Seccion -> fila -> niveles.
std::map<int, std::map<int, LevelRow>> g_levels;
bool g_loaded = false;

void Load()
{
    g_loaded = true;

    std::ifstream file("Data/MuEmu/CustomEventEntryLevel.txt");
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
            section = -1;
            continue;
        }

        std::vector<int> values;
        for (int value; tokens >> value;)
            values.push_back(value);
        if (values.size() < 2)
            continue;

        // Illusion Temple trae solo minimo y maximo, para todas las clases.
        LevelRow row;
        row.commonMin = values[0];
        row.commonMax = values[1];
        row.specialMin = values.size() >= 4 ? values[2] : values[0];
        row.specialMax = values.size() >= 4 ? values[3] : values[1];
        g_levels[section][std::atoi(first.c_str())] = row;
    }
}
} // namespace

bool MuEmu::EventLevel::Get(int section, int row, bool special, int& minLevel, int& maxLevel)
{
    if (!g_loaded)
        Load();

    const auto rows = g_levels.find(section);
    if (rows == g_levels.end())
        return false;

    const auto found = rows->second.find(row);
    if (found == rows->second.end())
        return false;

    minLevel = special ? found->second.specialMin : found->second.commonMin;
    maxLevel = special ? found->second.specialMax : found->second.commonMax;
    return true;
}

void MuEmu::EventLevel::Apply(int section, int (*levels)[2], int count, int specialStart)
{
    for (int n = 0; n < count; ++n)
    {
        Get(section, n, false, levels[n][0], levels[n][1]);
        if (specialStart >= 0)
            Get(section, n, true, levels[specialStart + n][0], levels[specialStart + n][1]);
    }
}
