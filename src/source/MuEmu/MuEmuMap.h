#pragma once

#include <string>

// Mapas custom de los clientes MuEmu (GetMainInfo: CustomMap.txt), leidos de Data/MuEmu: nombre (linea de
// texto), imagen del nombre (Local/<idioma>/ImgsMapName) y musica (Data/Music).
namespace MuEmu::Map
{
// Nombre del mapa custom, o nullptr si el mapa no es custom.
const wchar_t* GetName(int world);

// Imagen del nombre del mapa (solo el archivo), o vacio si el mapa no es custom.
std::wstring GetImage(int world);

// Pone o quita la musica de los mapas custom; se llama en cada frame con el mapa actual.
void UpdateMusic(int world);
} // namespace MuEmu::Map
