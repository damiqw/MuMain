#include "stdafx.h"
#include "MuEmu/MuEmuUiFeatures.h"

#include "MuEmu/MuEmuUi.h"

#include "Engine/Object/ZzzInfomation.h"

// Stats de mas de 65.535: el formato de OpenMU los manda en 16 bits, asi que el cliente toma los valores de 32 bits
// de los paquetes propios de los servidores MuEmu (los mismos que leia Main.dll):
// - C1 F3 E0 (120 bytes): puntos libres (64) y stats base (100 fuerza, 104 agilidad, 108 vitalidad, 112 energia,
//   116 comando).
// - C1 F3 E1 (172 bytes): stats agregados por items y buffs (36 a 52).
bool MuEmu::Ui::Stats::Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size)
{
    if (head != 0xF3 || buffer[0] != 0xC1 || (sub != 0xE0 && sub != 0xE1))
        return false;

    if (CharacterAttribute == nullptr)
        return true;

    if (sub == 0xE0 && size >= 120)
    {
        CharacterAttribute->LevelUpPoint = static_cast<DWORD>(ReadInt(buffer, 64));
        CharacterAttribute->Strength = static_cast<DWORD>(ReadInt(buffer, 100));
        CharacterAttribute->Dexterity = static_cast<DWORD>(ReadInt(buffer, 104));
        CharacterAttribute->Vitality = static_cast<DWORD>(ReadInt(buffer, 108));
        CharacterAttribute->Energy = static_cast<DWORD>(ReadInt(buffer, 112));
        CharacterAttribute->Charisma = static_cast<DWORD>(ReadInt(buffer, 116));
    }
    else if (sub == 0xE1 && size >= 56)
    {
        CharacterAttribute->AddStrength = static_cast<DWORD>(ReadInt(buffer, 36));
        CharacterAttribute->AddDexterity = static_cast<DWORD>(ReadInt(buffer, 40));
        CharacterAttribute->AddVitality = static_cast<DWORD>(ReadInt(buffer, 44));
        CharacterAttribute->AddEnergy = static_cast<DWORD>(ReadInt(buffer, 48));
        CharacterAttribute->AddCharisma = static_cast<DWORD>(ReadInt(buffer, 52));
    }
    return true;
}
