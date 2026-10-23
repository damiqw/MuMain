#include "stdafx.h"
#include "MuEmu/MuEmuTools.h"
#include "MuEmu/MuEmuEffect.h"

#include "MuEmu/MuEmuTexture.h"
#include "MuEmu/MuEmuUiFeatures.h"

#include "Engine/Object/ZzzCharacter.h"
#include "Render/Effects/ZzzEffect.h"
#include "Render/Models/ZzzBMD.h"
#include "Render/Terrain/ZzzLodTerrain.h"
#include "Render/Textures/ZzzOpenglUtil.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
// Las texturas de efectos empiezan en 32001 en los dos clientes (con otros numeros desde 32275, ver MuEmuTexture);
// los modelos (numeros menores) no coinciden.
constexpr int FirstEffectTexture = 32001;

struct StaticEffect
{
    int texture = 0;
    int bone = 0;
    vec3_t color = { 1.f, 1.f, 1.f };
    float size = 1.f;
    float rotation = 0.f;
    int subType = 0;
};

enum ModelEffectKind
{
    ModelEffectSprite = 0,
    ModelEffectParticle = 1,
    ModelEffectSkill = 2,
};

struct ModelEffect
{
    int itemType = 0;
    int minLevel = 0;
    int maxLevel = 15;
    int minExcellent = 0;
    vec3_t color = { 1.f, 1.f, 1.f };
    int kind = ModelEffectSprite;
    int code = 0;
    int subType = 0;
    float scale = 1.f;
};

struct RuneEffect
{
    bool active = false;
    int texture = 0;
    float speed = 0.f;
    float size = 1.f;
    vec3_t color = { 1.f, 1.f, 1.f };
    float floorDistance = 5.f;
};

struct RuneState
{
    int level = 0;
    int vip = 0;
    int gm = 0;
};

// Indices de CustomRuneEffect.txt: VIP 0 a 3 y PK nivel 6.
constexpr int RunePk = 4;
constexpr BYTE PkNormal = 3;
constexpr BYTE PkMurderer2 = 6;
constexpr int GameMasterFlag = 32;

std::multimap<int, StaticEffect> g_static;
std::vector<ModelEffect> g_models;
std::vector<RuneEffect> g_runes;
std::map<int, RuneState> g_runeStates;
bool g_loaded = false;

// Las lineas de numeros del archivo, sin comentarios y hasta "end". "13,4" se lee como dos numeros.
std::vector<std::vector<float>> ReadRows(const char* path, std::size_t minColumns)
{
    std::vector<std::vector<float>> rows;
    const auto fileStream = MuEmu::OpenData(path);
    std::istream& file = *fileStream;
    for (std::string line; std::getline(file, line);)
    {
        const auto comment = line.find("//");
        if (comment != std::string::npos)
            line.erase(comment);
        for (char& ch : line)
        {
            if (ch == ',')
                ch = ' ';
        }

        std::istringstream tokens(line);
        std::vector<float> values;
        for (std::string token; tokens >> token;)
        {
            if (token == "end")
                return rows;
            values.push_back(std::strtof(token.c_str(), nullptr));
        }

        if (values.size() >= minColumns)
            rows.push_back(std::move(values));
    }
    return rows;
}

void LoadStatic(const char* path)
{
    // ItemIndex EffectIndex EffectValue(hueso) EffectLevel R G B Size Main(rotacion) Side(subtipo)
    for (const auto& row : ReadRows(path, 10))
    {
        StaticEffect effect;
        effect.texture = MuEmu::Texture::From104E(static_cast<int>(row[1]));
        effect.bone = static_cast<int>(row[2]);
        Vector(row[4], row[5], row[6], effect.color);
        effect.size = row[7];
        effect.rotation = row[8];
        effect.subType = static_cast<int>(row[9]);
        if (effect.texture >= FirstEffectTexture)
            g_static.emplace(static_cast<int>(row[0]), effect);
    }
}

void LoadModelEffects()
{
    // ItemType ItemIndex MinLevel MaxLevel MinExc MaxExc R G B EffectIndex EffectCode EffectType EffectScale
    for (const auto& row : ReadRows("Data/MuEmu/CustomModelEffect.txt", 13))
    {
        ModelEffect effect;
        effect.itemType = (static_cast<int>(row[0]) * MAX_ITEM_INDEX) + static_cast<int>(row[1]);
        effect.minLevel = static_cast<int>(row[2]);
        effect.maxLevel = static_cast<int>(row[3]);
        effect.minExcellent = static_cast<int>(row[4]);
        Vector(row[6], row[7], row[8], effect.color);
        effect.kind = static_cast<int>(row[9]);
        effect.code = static_cast<int>(row[10]);
        effect.subType = static_cast<int>(row[11]);
        effect.scale = row[12];

        // Los efectos de skill con modelos de 1.04E tienen otros numeros en este cliente: se omiten.
        if (effect.code >= FirstEffectTexture)
            effect.code = MuEmu::Texture::From104E(effect.code);
        if (effect.code >= FirstEffectTexture)
            g_models.push_back(effect);
    }
}

void LoadRunes()
{
    // RuneEffect de MainInfo.ini (SvenInfo.bmd): apagado, no se carga ninguna runa.
    if (MuEmu::GetConfigInt("RuneEffect", 1) == 0)
        return;

    // Active TextureIndex Speed Size R G B FloorDistance
    for (const auto& row : ReadRows("Data/MuEmu/CustomRuneEffect.txt", 8))
    {
        RuneEffect rune;
        rune.active = row[0] != 0.f;
        rune.texture = MuEmu::Texture::From104E(static_cast<int>(row[1]));
        if (rune.texture < 0)
            rune.active = false;
        rune.speed = row[2];
        rune.size = row[3];
        Vector(row[4], row[5], row[6], rune.color);
        rune.floorDistance = row[7];
        g_runes.push_back(rune);
    }
}

void EnsureLoaded()
{
    if (g_loaded)
        return;

    g_loaded = true;
    LoadStatic("Data/MuEmu/CustomEffectStatic.txt");
    LoadModelEffects();
    LoadRunes();
}

void CreateModelEffect(const ModelEffect& effect, OBJECT* o, float height)
{
    vec3_t position, angle, color;
    Vector(o->Position[0], o->Position[1], o->Position[2] + height, position);
    // Main.dll tambien le suma la altura al angulo.
    Vector(o->Angle[0], o->Angle[1], o->Angle[2] + height, angle);
    VectorCopy(effect.color, color);

    switch (effect.kind)
    {
    case ModelEffectParticle:
        CreateParticle(effect.code, position, angle, color, effect.subType, effect.scale);
        break;
    case ModelEffectSkill:
        CreateEffect(effect.code, position, angle, color, effect.subType);
        break;
    default:
        CreateSprite(effect.code, position, effect.scale, color, o);
        break;
    }
}

bool IsItem(const PART_t& part, int itemType)
{
    return part.Type - MODEL_ITEM == itemType;
}

bool Matches(const ModelEffect& effect, const PART_t& part)
{
    return part.Level >= effect.minLevel && part.Level <= effect.maxLevel &&
           part.ExcellentFlags >= effect.minExcellent;
}

void RenderSetEffects(CHARACTER* c, OBJECT* o)
{
    for (const ModelEffect& effect : g_models)
    {
        // Como Main.dll: casco, armadura, pantalones o botas (los guantes no tienen efecto).
        if (IsItem(c->BodyPart[BODYPART_HELM], effect.itemType))
        {
            if (Matches(effect, c->BodyPart[BODYPART_HELM]))
                CreateModelEffect(effect, o, 150.f);
        }
        else if (IsItem(c->BodyPart[BODYPART_ARMOR], effect.itemType))
        {
            if (Matches(effect, c->BodyPart[BODYPART_ARMOR]))
                CreateModelEffect(effect, o, 120.f);
        }
        else if (IsItem(c->BodyPart[BODYPART_PANTS], effect.itemType))
        {
            if (Matches(effect, c->BodyPart[BODYPART_PANTS]))
                CreateModelEffect(effect, o, 60.f);
        }
        else if (IsItem(c->BodyPart[BODYPART_BOOTS], effect.itemType))
        {
            if (Matches(effect, c->BodyPart[BODYPART_BOOTS]))
            {
                float height = 0.f;
                if (c->SafeZone)
                    height = 20.f;
                else if (c->Helper.Type == MODEL_HORN_OF_FENRIR)
                    height = 210.f;
                else if (c->Wing.Type != -1)
                    height = 180.f;
                CreateModelEffect(effect, o, height);
            }
        }
    }
}

void RenderRune(CHARACTER* c, OBJECT* o)
{
    const auto state = g_runeStates.find(c->Key);
    if (state == g_runeStates.end() || state->second.level <= 0 || state->second.gm == GameMasterFlag)
        return;

    // Como Main.dll: sin PK se elige por VIP (0 a 3); con PK nivel 6 se usa la runa 4.
    int index = -1;
    if (c->PK == PkNormal)
        index = state->second.vip;
    else if (c->PK == PkMurderer2)
        index = RunePk;

    if (index < 0 || index > RunePk || index >= static_cast<int>(g_runes.size()) || !g_runes[index].active)
        return;

    const RuneEffect& rune = g_runes[index];
    const float lumi = sinf(WorldTime * 0.0015f) * 0.3f + 0.5f;
    vec3_t light;
    Vector(rune.color[0] * lumi, rune.color[1] * lumi, rune.color[2] * lumi, light);

    EnableAlphaBlend();
    RenderTerrainAlphaBitmap(rune.texture, o->Position[0], o->Position[1], rune.size, rune.size, light,
                             WorldTime * rune.speed, 1.f, rune.floorDistance);
    DisableAlphaBlend();
}
} // namespace

bool MuEmu::Effect::RenderItem(int itemType, BMD* model, OBJECT* object)
{
    EnsureLoaded();
    const auto effects = g_static.equal_range(itemType);
    if (effects.first == effects.second || model == nullptr || object == nullptr)
        return false;
    // AntiLag: el item custom queda sin efectos (ni los suyos ni los del cliente).
    if (MuEmu::AntiLag::IsHidden(MuEmu::AntiLag::CustomItemEffects))
        return true;

    for (auto it = effects.first; it != effects.second; ++it)
    {
        const StaticEffect& effect = it->second;
        if (effect.bone < 0 || effect.bone >= model->NumBones)
            continue;

        vec3_t position, color;
        model->TransformByObjectBone(position, object, effect.bone);
        VectorCopy(effect.color, color);
        CreateSprite(effect.texture, position, effect.size, color, object, effect.rotation, effect.subType);
    }
    return true;
}

void MuEmu::Effect::RenderCharacter(CHARACTER* c, OBJECT* o)
{
    EnsureLoaded();
    if (c == nullptr || o == nullptr || g_isCharacterBuff(o, eBuff_Cloaking))
        return;
    if (MuEmu::AntiLag::IsHidden(MuEmu::AntiLag::SetEffects))
        return;

    RenderSetEffects(c, o);
    RenderRune(c, o);
}

void MuEmu::Effect::ReceiveRune(const unsigned char* buffer, int size)
{
    // C1 F3 EF: indice, nivel, resets, master resets, VIP, GM y PK (un int cada uno, desde el byte 4).
    constexpr int Offset = 4;
    if (buffer == nullptr || size < Offset + (7 * 4))
        return;

    auto readInt = [buffer](int at)
    {
        const unsigned char* v = buffer + at;
        return static_cast<int>(v[0] | (v[1] << 8) | (v[2] << 16) | (static_cast<unsigned>(v[3]) << 24));
    };

    RuneState state;
    const int index = readInt(Offset);
    state.level = readInt(Offset + 4);
    state.vip = readInt(Offset + 16);
    state.gm = readInt(Offset + 20);
    g_runeStates[index] = state;
}
