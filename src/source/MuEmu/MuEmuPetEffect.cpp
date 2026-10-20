#include "stdafx.h"
#include "MuEmu/MuEmuPetEffect.h"

#include "MuEmu/MuEmuPet.h"
#include "MuEmu/MuEmuTexture.h"
#include "Render/Effects/ZzzEffect.h"
#include "Render/Models/ZzzBMD.h"

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

std::multimap<int, StaticEffect> g_static;
std::multimap<int, MeshGlow> g_glows;
bool g_loaded = false;

// Las lineas de numeros del archivo, sin comentarios y hasta "end".
std::vector<std::vector<float>> ReadRows(const char* path, std::size_t minColumns)
{
    std::vector<std::vector<float>> rows;
    std::ifstream file(path);
    for (std::string line; std::getline(file, line);)
    {
        const auto comment = line.find("//");
        if (comment != std::string::npos)
            line.erase(comment);

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
