#pragma once

// Textos custom de la lista de servidores (GetMainInfo: CustomServerInfo.txt), leidos de Data/MuEmu.
namespace MuEmu::ServerInfo
{
// Texto del servidor `index` del grupo `groupName`, o nullptr si no tiene texto custom.
const wchar_t* GetText(const wchar_t* groupName, int index);
} // namespace MuEmu::ServerInfo
