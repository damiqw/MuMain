#include "stdafx.h"
#include "MuEmu/MuEmuMonster.h"

#include "Core/Text/Utf8.h"
#include "Data/DataHandler/LoadData.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Render/Models/ZzzBMD.h"
#include "World/MapInfra/MapManager.h"

#include <cwchar>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
constexpr int TypeNpc = 0;

// Los modelos de monstruo que el cliente no usa (MONSTER_MODEL_COUNT a MAX_MODEL_MONSTER).
constexpr int FirstFreeModel = static_cast<int>(MODEL_MONSTER01) + MONSTER_MODEL_COUNT;
constexpr int LastFreeModel = static_cast<int>(MODEL_MONSTER_END) - 1;

struct MonsterInfo
{
    int type = TypeNpc;
    std::wstring name;
    std::wstring dir;
    std::wstring folder;
    std::wstring file;
    float size = 1.f;
    int model = -1;
};

struct NpcName
{
    int map = 0;
    int x = 0;
    int y = 0;
    std::wstring name;
};

std::map<int, MonsterInfo> g_monsters;
std::multimap<int, NpcName> g_npcNames;
bool g_loaded = false;

// Saca los textos entre comillas de la linea; lo que no esta entre comillas queda en `rest`.
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

std::string StripComment(const std::string& line)
{
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

// GetMainInfo escribe las rutas con "\\" (como en C); el cliente usa una sola barra.
std::wstring ToPath(const std::string& text)
{
    std::string path = text;
    for (std::string::size_type at = path.find("\\\\"); at != std::string::npos; at = path.find("\\\\", at + 1))
        path.erase(at, 1);
    return Core::Text::FromUtf8(path);
}

void LoadMonsters()
{
    std::ifstream file("Data/MuEmu/CustomMonster.txt");
    int nextModel = FirstFreeModel;
    bool inSection = false;
    for (std::string line; std::getline(file, line);)
    {
        line = StripComment(line);
        std::string rest;
        const std::vector<std::string> texts = ReadQuoted(line, rest);
        std::istringstream tokens(rest);

        std::string first;
        if (!(tokens >> first))
            continue;
        if (!inSection)
        {
            inSection = true; // numero de seccion
            continue;
        }
        if (first == "end")
            break;
        if (texts.size() < 4)
            continue;

        MonsterInfo info;
        const int monsterId = std::atoi(first.c_str());
        tokens >> info.type >> info.size;
        info.name = Core::Text::FromUtf8(texts[0]);
        info.dir = ToPath(texts[1]);
        info.folder = ToPath(texts[2]);
        info.file = ToPath(texts[3]);
        if (info.size <= 0.f)
            info.size = 1.f;

        if (g_monsters.count(monsterId) != 0 || nextModel > LastFreeModel)
            continue;

        info.model = nextModel++;
        g_monsters.emplace(monsterId, std::move(info));
    }
}

void LoadNpcNames()
{
    std::ifstream file("Data/MuEmu/CustomNPCName.txt");
    for (std::string line; std::getline(file, line);)
    {
        line = StripComment(line);
        std::string rest;
        const std::vector<std::string> texts = ReadQuoted(line, rest);
        std::istringstream tokens(rest);

        int npc = 0;
        NpcName entry;
        if (texts.empty() || !(tokens >> npc >> entry.map >> entry.x >> entry.y))
            continue;

        entry.name = Core::Text::FromUtf8(texts[0]);
        g_npcNames.emplace(npc, std::move(entry));
    }
}

void Load()
{
    g_loaded = true;
    LoadMonsters();
    LoadNpcNames();
}

const MonsterInfo* FindInfo(int type)
{
    if (!g_loaded)
        Load();

    const auto found = g_monsters.find(type);
    return found == g_monsters.end() ? nullptr : &found->second;
}

bool OpenModel(const MonsterInfo& info)
{
    BMD* model = &Models[info.model];
    if (model->NumMeshs > 0 || model->NumActions > 0)
        return true;

    if (!gLoadData.AccessModel(info.model, info.dir.c_str(), info.file.c_str(), -1, true) || model->NumMeshs == 0)
        return false;

    gLoadData.OpenTexture(info.model, info.folder.c_str());

    // Como Main.dll: todas las animaciones del modelo custom a la misma velocidad.
    for (int n = 0; n < model->NumActions; ++n)
        model->Actions[n].PlaySpeed = 0.25f;

    return true;
}

void SetName(CHARACTER* c, const std::wstring& name)
{
    constexpr std::size_t size = sizeof(c->ID) / sizeof(c->ID[0]);
    std::wcsncpy(c->ID, name.c_str(), size - 1);
    c->ID[size - 1] = L'\0';
}
} // namespace

CHARACTER* MuEmu::Monster::Create(int type, int positionX, int positionY, int key)
{
    const MonsterInfo* info = FindInfo(type);
    if (info == nullptr || !OpenModel(*info))
        return nullptr;

    CHARACTER* c = CreateCharacter(key, info->model, static_cast<unsigned char>(positionX),
                                   static_cast<unsigned char>(positionY));
    if (c != nullptr)
        c->Object.Scale = info->size;

    return c;
}

void MuEmu::Monster::Apply(CHARACTER* c, int type, int positionX, int positionY)
{
    if (c == nullptr)
        return;

    if (const MonsterInfo* info = FindInfo(type))
    {
        SetName(c, info->name);
        if (info->type == TypeNpc)
            c->Object.Kind = KIND_NPC;
    }

    const auto names = g_npcNames.equal_range(type);
    for (auto entry = names.first; entry != names.second; ++entry)
    {
        const NpcName& npc = entry->second;
        if (npc.map == gMapManager.WorldActive && npc.x == positionX && npc.y == positionY)
        {
            SetName(c, npc.name);
            break;
        }
    }
}
