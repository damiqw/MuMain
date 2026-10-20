#include "stdafx.h"
#include "MuEmu/MuEmuQuickConnect.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
// Usuario y clave de MU: hasta 10 caracteres, como en Main.dll.
constexpr DWORD MaxLength = 11;
constexpr const char* RegistryKey = "SOFTWARE\\Webzen\\Mu\\Config";

std::array<MuEmu::QuickConnect::Account, MuEmu::QuickConnect::SlotCount> g_accounts;
bool g_loaded = false;

// El primer lugar es "ID"/"PW" (el de Main.dll y el launcher), los demas "ID_2".."ID_5".
std::string ValueName(const char* name, int slot)
{
    return slot == 0 ? std::string(name) : std::string(name) + "_" + std::to_string(slot + 1);
}

#ifdef _WIN32
std::string ReadValue(HKEY key, const std::string& name)
{
    char value[MaxLength + 1] = {};
    DWORD size = MaxLength;
    DWORD type = 0;
    if (RegQueryValueExA(key, name.c_str(), nullptr, &type, reinterpret_cast<LPBYTE>(value), &size) != ERROR_SUCCESS ||
        type != REG_SZ)
        return std::string();
    value[MaxLength] = 0;
    return value;
}

void WriteValue(const std::string& name, const std::string& value)
{
    HKEY key = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, RegistryKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr,
                        &key, nullptr) != ERROR_SUCCESS)
        return;
    RegSetValueExA(key, name.c_str(), 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                   static_cast<DWORD>(value.size() + 1));
    RegCloseKey(key);
}
#endif

void Load()
{
    g_loaded = true;
    g_accounts = {};
#ifdef _WIN32
    HKEY key = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, RegistryKey, 0, KEY_READ, &key) != ERROR_SUCCESS)
        return;
    for (int slot = 0; slot < MuEmu::QuickConnect::SlotCount; ++slot)
    {
        g_accounts[slot].id = ReadValue(key, ValueName("ID", slot));
        g_accounts[slot].password = ReadValue(key, ValueName("PW", slot));
    }
    RegCloseKey(key);
#endif
}
} // namespace

const std::array<MuEmu::QuickConnect::Account, MuEmu::QuickConnect::SlotCount>& MuEmu::QuickConnect::Accounts()
{
    if (!g_loaded)
        Load();
    return g_accounts;
}

void MuEmu::QuickConnect::Save(int slot, const std::string& id, const std::string& password)
{
    if (slot < 0 || slot >= SlotCount || id.empty())
        return;
#ifdef _WIN32
    WriteValue(ValueName("ID", slot), id.substr(0, MaxLength - 1));
    WriteValue(ValueName("PW", slot), password.substr(0, MaxLength - 1));
#endif
    Load();
}

void MuEmu::QuickConnect::Remove(int slot)
{
    if (slot < 0 || slot >= SlotCount)
        return;
#ifdef _WIN32
    WriteValue(ValueName("ID", slot), "");
    WriteValue(ValueName("PW", slot), "");
#endif
    Load();
}
