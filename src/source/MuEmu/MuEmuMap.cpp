#include "stdafx.h"
#include "MuEmu/MuEmuMap.h"

#include "App/Platform/Windows/Winmain.h"
#include "Core/Text/Utf8.h"
#include "I18N/All.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>

namespace
{
struct MapInfo
{
    std::wstring name;
    std::wstring image;
    std::string music; // ruta para PlayMp3, vacia si el archivo no esta
};

std::map<int, MapInfo> g_maps;
bool g_loaded = false;
const MapInfo* g_playing = nullptr;

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

// El texto de la linea si el cliente lo tiene; si no, el nombre de la imagen ("mundioca.tga" es "Mundioca").
std::wstring MakeName(int lineText, const std::string& image)
{
    if (const wchar_t* const* slot = I18N::Game::LookupSlot(lineText); slot != nullptr && *slot != nullptr && **slot != L'\0')
        return *slot;

    std::string name = image.substr(0, image.find('.'));
    if (!name.empty())
        name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
    return Core::Text::FromUtf8(name);
}

void Load()
{
    g_loaded = true;

    std::ifstream file("Data/MuEmu/CustomMap.txt");
    for (std::string line; std::getline(file, line);)
    {
        const auto comment = line.find("//");
        if (comment != std::string::npos)
            line.erase(comment);

        std::string rest;
        const std::vector<std::string> texts = ReadQuoted(line, rest);
        std::istringstream tokens(rest);

        int world = -1;
        int lineText = -1;
        if (texts.size() < 2 || !(tokens >> world >> lineText) || world < 0)
            continue;

        MapInfo info;
        info.name = MakeName(lineText, texts[0]);
        info.image = Core::Text::FromUtf8(texts[0]);

        // Sin el archivo no se pide la musica (el reproductor avisaria en cada frame).
        const std::string music = "Data/Music/" + texts[1];
        if (!texts[1].empty() && std::filesystem::exists(music))
            info.music = "data\music\\" + texts[1];

        g_maps.emplace(world, std::move(info));
    }
}

const MapInfo* FindInfo(int world)
{
    if (!g_loaded)
        Load();

    const auto found = g_maps.find(world);
    return found == g_maps.end() ? nullptr : &found->second;
}
} // namespace

const wchar_t* MuEmu::Map::GetName(int world)
{
    const MapInfo* info = FindInfo(world);
    return info == nullptr ? nullptr : info->name.c_str();
}

std::wstring MuEmu::Map::GetImage(int world)
{
    const MapInfo* info = FindInfo(world);
    return info == nullptr ? std::wstring() : info->image;
}

void MuEmu::Map::UpdateMusic(int world)
{
    const MapInfo* info = FindInfo(world);
    if (info != nullptr && info->music.empty())
        info = nullptr;

    if (g_playing != nullptr && g_playing != info)
        StopMp3(g_playing->music.c_str());

    g_playing = info;
    if (g_playing != nullptr)
        PlayMp3(g_playing->music.c_str());
}
