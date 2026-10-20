#include "stdafx.h"
#include "MuEmu/MuEmuPetEffect.h"

#include "MuEmu/MuEmuPet.h"
#include "MuEmu/MuEmuTexture.h"
#include "Engine/Object/ZzzObject.h"
#include "Render/Effects/ZzzEffect.h"
#include "Render/Models/ZzzBMD.h"
#include "Render/Textures/ZzzOpenglUtil.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
struct StaticEffect
{
    int texture = 0;
    int bone = 0;
    vec3_t color = { 1.f, 1.f, 1.f };
    float size = 1.f;
    float rotation = 0.f;
    int subType = 0;
};

struct MeshGlow
{
    int mesh = 0;
    int renderType = 0;
    vec3_t color = { 1.f, 1.f, 1.f };
};

struct HorseQuake
{
    int wave = -1;
    vec3_t color = { 1.f, 1.f, 1.f };
    bool useColor = false;
    int waveSubType = 0;
    int insideSkill = -1;
    int outsideSkill = -1;
};

// Como RenderDarkHorseSkill: una onda cada 10 cuadros a 25 fps (400 ms).
constexpr float WaveInterval = 10 * (1000.f / 25.f);

std::multimap<int, StaticEffect> g_static;
std::multimap<int, MeshGlow> g_glows;
std::map<int, HorseQuake> g_quakes;
bool g_loaded = false;

// Efecto de 1.04E en este cliente: texturas desde 32001 con MuEmu::Texture; de los modelos solo los de skill que
// coinciden (del Dark Horse a las piedras del Earthquake: 226 a 332). -1 si no hay.
int Effect104E(int code)
{
    if (code >= 32001)
        return MuEmu::Texture::From104E(code);
    if (code >= MODEL_DARK_HORSE && code <= MODEL_GROUND_STONE2)
        return code;
    return -1;
}

// Las lineas de numeros del archivo, sin comentarios y hasta "end". "13,4" se lee como dos numeros.
std::vector<std::vector<float>> ReadRows(const char* path, std::size_t minColumns)
{
    std::vector<std::vector<float>> rows;
    std::ifstream file(path);
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

void EnsureLoaded()
{
    if (g_loaded)
        return;

    g_loaded = true;

    // ItemIndex EffectIndex EffectValue(hueso) EffectLevel R G B Size Main(rotacion) Side(subtipo)
    for (const auto& row : ReadRows("Data/MuEmu/CustomPetEffectStatic.txt", 10))
    {
        StaticEffect effect;
        effect.texture = MuEmu::Texture::From104E(static_cast<int>(row[1]));
        effect.bone = static_cast<int>(row[2]);
        Vector(row[4], row[5], row[6], effect.color);
        effect.size = row[7];
        effect.rotation = row[8];
        effect.subType = static_cast<int>(row[9]);
        if (effect.texture > 0)
            g_static.emplace(static_cast<int>(row[0]), effect);
    }

    // ItemIndex Malla TipoDeRender R G B (0 a 255)
    for (const auto& row : ReadRows("Data/MuEmu/CustomPetGlow.txt", 6))
    {
        MeshGlow glow;
        glow.mesh = static_cast<int>(row[1]);
        glow.renderType = static_cast<int>(row[2]);
        Vector(row[3] / 255.f, row[4] / 255.f, row[5] / 255.f, glow.color);
        g_glows.emplace(static_cast<int>(row[0]), glow);
    }

    // Grupo,Numero EffectCode UseRGB R G B EffectSize(subtipo de la onda) Skill1(adentro) Skill2(afuera)
    for (const auto& row : ReadRows("Data/MuEmu/CustomEarthQuakeSkill.txt", 10))
    {
        HorseQuake quake;
        quake.wave = Effect104E(static_cast<int>(row[2]));
        quake.useColor = row[3] == 1.f;
        // Main.dll pasa los valores de 0 a 255 tal cual; el color se satura en 1.
        Vector((std::min)(row[4], 1.f), (std::min)(row[5], 1.f), (std::min)(row[6], 1.f), quake.color);
        quake.waveSubType = static_cast<int>(row[7]);
        quake.insideSkill = Effect104E(static_cast<int>(row[8]));
        quake.outsideSkill = Effect104E(static_cast<int>(row[9]));
        g_quakes.emplace((static_cast<int>(row[0]) * MAX_ITEM_INDEX) + static_cast<int>(row[1]), quake);
    }
}
} // namespace

void MuEmu::PetEffect::Render(OBJECT* o)
{
    const int itemType = MuEmu::Pet::GetItemType(o);
    if (itemType < 0 || o->Alpha <= 0.f || g_isCharacterBuff(o->Owner, eBuff_Cloaking))
        return;

    EnsureLoaded();
    BMD* b = &Models[o->Type];

    const auto effects = g_static.equal_range(itemType);
    for (auto it = effects.first; it != effects.second; ++it)
    {
        const StaticEffect& effect = it->second;
        if (effect.bone < 0 || effect.bone >= b->NumBones)
            continue;

        vec3_t position, color;
        b->TransformByObjectBone(position, o, effect.bone);
        VectorCopy(effect.color, color);
        CreateSprite(effect.texture, position, effect.size, color, o, effect.rotation, effect.subType);
    }

    // Como Main.dll: la malla se dibuja otra vez (dos veces) con el tipo de render y el color de la linea.
    const auto glows = g_glows.equal_range(itemType);
    if (glows.first == glows.second)
        return;

    vec3_t bodyLight;
    VectorCopy(b->BodyLight, bodyLight);
    for (auto it = glows.first; it != glows.second; ++it)
    {
        const MeshGlow& glow = it->second;
        if (glow.mesh < 0 || glow.mesh >= b->NumMeshs)
            continue;

        VectorCopy(glow.color, b->BodyLight);
        for (int i = 0; i < 2; ++i)
            b->RenderMesh(glow.mesh, glow.renderType, o->Alpha, o->BlendMesh, o->BlendMeshLight, o->BlendMeshTexCoordU, o->BlendMeshTexCoordV);
    }
    VectorCopy(bodyLight, b->BodyLight);
}

bool MuEmu::PetEffect::RenderHorseSkill(OBJECT* o, BMD* b)
{
    const int itemType = MuEmu::Pet::GetItemType(o);
    if (itemType < 0 || b == nullptr)
        return false;

    EnsureLoaded();

    // El nivel del arma cuenta los cuadros de referencia del efecto, como en RenderDarkHorseSkill.
    o->WeaponLevel += REFERENCE_FRAME_STEP;

    // Como Main.dll: una mascota custom sin linea en CustomEarthQuakeSkill no dibuja efectos.
    const auto found = g_quakes.find(itemType);
    if (found == g_quakes.end())
        return true;

    const HorseQuake& quake = found->second;
    if (quake.wave > 0 && o->LastHorseWaveEffect < WorldTime - WaveInterval)
    {
        vec3_t color;
        VectorCopy(quake.useColor ? quake.color : o->Light, color);
        CreateEffect(quake.wave, o->Position, o->Angle, color, quake.waveSubType);
        o->LastHorseWaveEffect = WorldTime;
    }

    if (o->AnimationFrame >= 8.f && o->AnimationFrame <= 9.5f)
    {
        if (REFERENCE_FRAME_STEP > 0 && (o->WeaponLevel % 2) == 1)
        {
            float matrix[3][4];
            vec3_t angle, p, position;
            Vector(0.f, 150.f * (o->WeaponLevel / 2), 0.f, p);
            Vector(0.f, 0.f, static_cast<float>(rand() % 360), angle);
            for (int i = 0; i < 6 && quake.outsideSkill > 0; ++i)
            {
                angle[2] += 60.f;
                AngleMatrix(angle, matrix);
                VectorRotate(p, matrix, position);
                VectorAdd(o->Position, position, position);
                CreateEffect(quake.outsideSkill, position, o->Angle, o->Light);
            }
        }
        SetEarthQuakeAt(o->Position, (rand() % 3 - 3) * 0.7f);
    }
    else if (o->WeaponLevel == 19)
    {
        if (quake.insideSkill > 0)
            CreateEffect(quake.insideSkill, o->Position, o->Angle, o->Light, 0, o, -1, 0, 2);
        o->WeaponLevel = -3;
    }
    return true;
}
