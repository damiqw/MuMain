#include "stdafx.h"
#include "MuEmu/MuEmuTools.h"
#include "MuEmu/MuEmuPet.h"

#include "Engine/AI/GOBoid.h"
#include "Engine/AI/ZzzAI.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Render/Terrain/ZzzLodTerrain.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
// Columna Type de CustomPet.txt.
enum PetType
{
    PetFenrir = 5,
    PetDarkHorse = 6,
    PetUniria = 7,
    PetDinorant = 11,
};

struct PetInfo
{
    int type = 0;
    float viewScale = 1.f;
};

// Como Pet.cpp de Main.dll: sigue al personaje a 150 de distancia; mas lejos corre mas rapido y a 1000 salta.
constexpr float FollowRange = 150.f;
constexpr float JumpRange = 1000.f;
constexpr float CharacterSceneExtraScale = 0.3f;

// Acciones de los modelos de mascota que caminan (como en Main.dll): quieta, quieta 2 y corriendo.
constexpr int FollowerStand = 0;
constexpr int FollowerStand2 = 1;
constexpr int FollowerRun = 2;
// Main.dll usa la accion 1 durante 800 ms de cada 12 s.
constexpr float IdleCycle = 12000.f;
constexpr float IdleStand2Time = 800.f;

std::map<int, PetInfo> g_pets;
bool g_loaded = false;

// Los numeros de la linea, sin el texto entre comillas ni los comentarios; vacio en "end".
std::vector<float> ReadNumbers(std::string line, bool& end)
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
        {
            end = true;
            return {};
        }
        values.push_back(std::strtof(token.c_str(), nullptr));
    }
    return values;
}

void EnsureLoaded()
{
    if (g_loaded)
        return;

    g_loaded = true;
    // ItemIndex IndexInv Type "ModelName" Fenrir Inv Drop View
    const auto fileStream = MuEmu::OpenData("Data/MuEmu/CustomPet.txt");
    std::istream& file = *fileStream;
    bool end = false;
    for (std::string line; !end && std::getline(file, line);)
    {
        const std::vector<float> values = ReadNumbers(line, end);
        if (values.size() < 7)
            continue;

        PetInfo pet;
        pet.type = static_cast<int>(values[2]);
        pet.viewScale = values[6];
        g_pets.emplace(static_cast<int>(values[0]), pet);
    }
}

const PetInfo* Find(int itemType)
{
    EnsureLoaded();
    const auto found = g_pets.find(itemType);
    return found == g_pets.end() ? nullptr : &found->second;
}

const PetInfo* FindByModel(int model)
{
    return Find(model - MODEL_ITEM);
}

// Helper y montura originales para los tipos que se montan; falso para los que caminan detras del personaje.
bool GetRider(int type, int& helperType, int& mountType)
{
    switch (type)
    {
    case PetFenrir:
        helperType = MODEL_HORN_OF_FENRIR;
        mountType = MODEL_FENRIR_RED;
        return true;
    case PetDarkHorse:
        helperType = MODEL_DARK_HORSE_ITEM;
        mountType = MODEL_DARK_HORSE;
        return true;
    case PetUniria:
        helperType = MODEL_HORN_OF_UNIRIA;
        mountType = MODEL_UNICON;
        return true;
    case PetDinorant:
        helperType = MODEL_HORN_OF_DINORANT;
        mountType = MODEL_PEGASUS;
        return true;
    }
    return false;
}
} // namespace

bool MuEmu::Pet::CreateMount(CHARACTER* c, OBJECT* o, OBJECT* pHelper)
{
    if (c == nullptr || o == nullptr)
        return false;

    const int model = c->Helper.Type;
    const PetInfo* pet = FindByModel(model);
    if (pet == nullptr)
        return false;

    int helperType = 0;
    int mountType = 0;
    vec3_t position;
    VectorCopy(o->Position, position);
    if (GetRider(pet->type, helperType, mountType))
    {
        c->Helper.Type = helperType;
    }
    else
    {
        // Las que caminan aparecen al lado del personaje.
        position[0] += static_cast<float>(rand() % 128 - 64);
        position[1] += static_cast<float>(rand() % 128 - 64);
    }

    if (pHelper == nullptr)
    {
        DeleteMount(o);
        ::CreateMount(model, position, o);
    }
    else
    {
        CreateMountSub(model, position, o, pHelper);
    }
    return true;
}

int MuEmu::Pet::RiderHelperType(int helperType)
{
    const PetInfo* pet = FindByModel(helperType);
    int rider = 0;
    int mount = 0;
    if (pet != nullptr && GetRider(pet->type, rider, mount))
        return rider;
    return helperType;
}

int MuEmu::Pet::MoveType(int mountType)
{
    const PetInfo* pet = FindByModel(mountType);
    int rider = 0;
    int mount = 0;
    if (pet != nullptr && GetRider(pet->type, rider, mount))
        return mount;
    return mountType;
}

void MuEmu::Pet::MoveFollower(OBJECT* o)
{
    if (o == nullptr || o->Owner == nullptr || FindByModel(o->Type) == nullptr)
        return;

    OBJECT* owner = o->Owner;
    if (owner->Teleport == TELEPORT_BEGIN || owner->Teleport == TELEPORT)
    {
        o->Alpha -= 0.1f * FPS_ANIMATION_FACTOR;
        if (o->Alpha < 0.f)
            o->Alpha = 0.f;
    }
    else
    {
        o->Alpha = 1.f;
    }

    float dx = owner->Position[0] - o->Position[0];
    float dy = owner->Position[1] - o->Position[1];
    float distance = std::sqrt(dx * dx + dy * dy);

    if (distance >= JumpRange)
    {
        o->Position[0] = owner->Position[0] + static_cast<float>(rand() % 128 - 64);
        o->Position[1] = owner->Position[1] + static_cast<float>(rand() % 128 - 64);
        dx = owner->Position[0] - o->Position[0];
        dy = owner->Position[1] - o->Position[1];
        distance = std::sqrt(dx * dx + dy * dy);
    }

    if (distance >= FollowRange)
    {
        // Velocidades de Main.dll (por cuadro a 25 fps).
        float speed = 10.f;
        o->Velocity = 0.4f;
        if (distance >= FollowRange * 2.f)
        {
            speed = 15.f;
            o->Velocity = 0.89f;
        }
        else if (distance >= FollowRange * 1.5f)
        {
            speed = 13.f;
            o->Velocity = 0.69f;
        }

        const float step = (std::min)(speed * FPS_ANIMATION_FACTOR, distance);
        o->Position[0] += dx / distance * step;
        o->Position[1] += dy / distance * step;
        o->Angle[2] = TurnAngle2(o->Angle[2], CreateAngle(o->Position[0], o->Position[1], owner->Position[0], owner->Position[1]), 20.f);
        SetAction(o, FollowerRun);
    }
    else
    {
        o->Velocity = 0.4f;
        SetAction(o, std::fmod(WorldTime, IdleCycle) < IdleStand2Time ? FollowerStand2 : FollowerStand);
    }

    o->Position[2] = RequestTerrainHeight(o->Position[0], o->Position[1]);
}

void MuEmu::Pet::ApplyScale(OBJECT* o, bool bForceRender)
{
    if (o == nullptr || bForceRender)
        return;

    const PetInfo* pet = FindByModel(o->Type);
    if (pet == nullptr)
        return;

    o->Scale = pet->viewScale;
    if (SceneFlag == CHARACTER_SCENE)
        o->Scale += CharacterSceneExtraScale;
}

int MuEmu::Pet::GetItemType(const OBJECT* o)
{
    if (o == nullptr || FindByModel(o->Type) == nullptr)
        return -1;
    return o->Type - MODEL_ITEM;
}
