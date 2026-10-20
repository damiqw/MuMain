#include "stdafx.h"
#include "MuEmu/MuEmuUi.h"

#include "MuEmu/MuEmuTools.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <rpc.h>
#include <winioctl.h>
#pragma comment(lib, "rpcrt4.lib")
#endif

// HWID de los servidores MuEmu (HWID.cpp de Main.dll): despues del saludo del GameServer (C1 F1 00) el cliente
// manda C1 F3 E9 (84 bytes, CG_HWID_SEND): el identificador del equipo (36, 4), la version (20, 40) y los CRC
// de los archivos del cliente (6 int, 60). Con el HWID el servidor aplica los limites y baneos por PC.
// El identificador se calcula igual que Main.dll (serial del disco o, si no hay, volumen + MachineGuid + UUID),
// asi una PC tiene el mismo HWID con los dos clientes. La version "SVEN" le dice al servidor que no hay Main.dll
// que controlar (los CRC van en 0); el servidor lo confirma en el login con la version del cliente SVEN.
namespace
{
constexpr int PacketSize = 84;

#ifdef _WIN32
bool GetPhysicalDriveSerialNumber(int number, char* serial, int serialSize)
{
    char path[MAX_PATH];
    std::snprintf(path, sizeof(path), "\\\\.\\PhysicalDrive%d", number);

    HANDLE drive = CreateFileA(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (drive == INVALID_HANDLE_VALUE)
        return false;

    STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;

    STORAGE_DESCRIPTOR_HEADER header{};
    DWORD returned = 0;
    if (!DeviceIoControl(drive, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), &header, sizeof(header), &returned,
                         nullptr) ||
        header.Size < sizeof(STORAGE_DEVICE_DESCRIPTOR))
    {
        CloseHandle(drive);
        return false;
    }

    std::vector<BYTE> buffer(header.Size, 0);
    const bool ok = DeviceIoControl(drive, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), buffer.data(),
                                    header.Size, &returned, nullptr) != 0;
    CloseHandle(drive);
    if (!ok)
        return false;

    const auto* descriptor = reinterpret_cast<const STORAGE_DEVICE_DESCRIPTOR*>(buffer.data());
    if (descriptor->SerialNumberOffset == 0 || descriptor->SerialNumberOffset >= header.Size)
        return false;

    const char* text = reinterpret_cast<const char*>(buffer.data() + descriptor->SerialNumberOffset);
    const std::size_t length = strnlen(text, header.Size - descriptor->SerialNumberOffset);
    if (length + 1 > static_cast<std::size_t>(serialSize))
        return false;
    std::memcpy(serial, text, length);
    serial[length] = 0;
    return true;
}

DWORD ReadDword(const char* buffer, int offset)
{
    DWORD value;
    std::memcpy(&value, buffer + offset, sizeof(value));
    return value;
}

std::string GetHardwareId()
{
    char serial[256] = {};
    bool driveFound = false;
    for (int n = 0; n < 5 && !driveFound; n++)
    {
        if (!GetPhysicalDriveSerialNumber(n, serial, sizeof(serial)))
            continue;
        for (char c : serial)
        {
            if (c != 0 && c != ' ')
            {
                driveFound = true;
                break;
            }
        }
    }

    DWORD id1, id2, id3, id4;
    if (driveFound)
    {
        id1 = ReadDword(serial, 0x00) ^ ReadDword(serial, 0x10) ^ 0x3AD3B74A;
        id2 = ReadDword(serial, 0x04) ^ ReadDword(serial, 0x14) ^ 0x94FDC685;
        id3 = ReadDword(serial, 0x08) ^ ReadDword(serial, 0x18) ^ 0xF45BBF4C;
        id4 = ReadDword(serial, 0x0C) ^ ReadDword(serial, 0x1C) ^ 0x8941D8E7;
    }
    else
    {
        DWORD volumeSerial = 0;
        GetVolumeInformationA("C:\\", nullptr, 0, &volumeSerial, nullptr, nullptr, nullptr, 0);

        UUID uuid{};
        UuidCreateSequential(&uuid);

        SYSTEM_INFO systemInfo{};
        GetSystemInfo(&systemInfo);

        char machineGuid[64] = {};
        HKEY key;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ | KEY_WOW64_64KEY, &key) ==
            ERROR_SUCCESS)
        {
            DWORD size = sizeof(machineGuid);
            DWORD type = REG_SZ;
            RegQueryValueExA(key, "MachineGuid", nullptr, &type, reinterpret_cast<LPBYTE>(machineGuid), &size);
            RegCloseKey(key);
        }

        const DWORD guidHash1 = ReadDword(machineGuid, 0) ^ ReadDword(machineGuid, 8);
        const DWORD guidHash2 = ReadDword(machineGuid, 16) ^ ReadDword(machineGuid, 24);
        DWORD uuidTail;
        std::memcpy(&uuidTail, &uuid.Data4[2], sizeof(uuidTail));
        WORD uuidLast;
        std::memcpy(&uuidLast, &uuid.Data4[6], sizeof(uuidLast));

        id1 = volumeSerial ^ guidHash1 ^ 0x12B586FE;
        id2 = uuidTail ^ guidHash2 ^ 0x5D78A569;
        id3 = (static_cast<DWORD>(uuidLast) | (static_cast<DWORD>(systemInfo.wProcessorArchitecture) << 16)) ^ 0xF45BC123;
        id4 = (static_cast<DWORD>(systemInfo.wProcessorLevel) | (static_cast<DWORD>(systemInfo.wProcessorRevision) << 16)) ^
              0xB542D8E1;
    }

    char text[36];
    std::snprintf(text, sizeof(text), "%08X-%08X-%08X-%08X", static_cast<unsigned>(id1), static_cast<unsigned>(id2),
                  static_cast<unsigned>(id3), static_cast<unsigned>(id4));
    return text;
}
#endif
} // namespace

void MuEmu::Ui::SendHwid()
{
#ifdef _WIN32
    if (MuEmu::GetConfigInt("Enabled", 0) == 0 || MuEmu::GetConfigInt("SendHwid", 1) == 0)
        return;

    static const std::string hardwareId = GetHardwareId();

    std::vector<uint8_t> packet(PacketSize, 0);
    packet[0] = 0xC1;
    packet[1] = static_cast<uint8_t>(PacketSize);
    packet[2] = 0xF3;
    packet[3] = 0xE9;
    std::memcpy(packet.data() + 4, hardwareId.c_str(), std::min<std::size_t>(hardwareId.size(), 35));
    std::memcpy(packet.data() + 40, "SVEN", 4);
    SendRaw(packet);
#endif
}
