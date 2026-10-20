#pragma once

#include <array>
#include <string>

// Conexion rapida del login de Main.dll: 5 cuentas guardadas debajo del cuadro de login. Una cuenta llena el
// usuario y la clave con un click; un lugar vacio guarda lo que esta escrito. Se guardan en el mismo lugar que
// Main.dll (registro de Windows, HKCU\Software\Webzen\Mu\Config: ID/PW y ID_2..ID_5/PW_2..PW_5), asi aparecen
// las cuentas que ya estaban guardadas.
namespace MuEmu::QuickConnect
{
constexpr int SlotCount = 5;

struct Account
{
    std::string id;
    std::string password;
};

// Las cuentas guardadas (se leen la primera vez y despues de cada cambio).
const std::array<Account, SlotCount>& Accounts();

void Save(int slot, const std::string& id, const std::string& password);
void Remove(int slot);
} // namespace MuEmu::QuickConnect
