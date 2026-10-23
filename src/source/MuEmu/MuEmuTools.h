#pragma once

#include <istream>
#include <memory>
#include <string>

// Herramientas de linea de comandos para servidores MuEmu (ver docs/muemu/README.md).
namespace MuEmu
{
// Si la linea de comandos pide una herramienta, la ejecuta y devuelve true: el cliente debe cerrarse.
//   --muemu-import-items  Convierte Data/Local/<idioma>/Item_<idioma>.bmd a Data/Items/*.json
//   --muemu-import-models[=carpeta]  Agrega a Data/Items/Models los modelos de las tablas custom de
//                                     GetMainInfo (CustomItem, CustomJewel, CustomPet, CustomFlag). Por defecto MAIN_INFO/Custom.
bool RunCommandLineTool(const char* commandLine);

// Lee un entero de la seccion [MuEmu]: el de SvenInfo.bmd si lo trae (el jugador no lo puede cambiar), si no
// el de config.ini (junto a Main.exe); defaultValue si no esta.
int GetConfigInt(const char* key, int defaultValue);
// Data/Local/SvenInfo.bmd (GetMainInfo_SVEN.exe): un valor de la seccion indicada ([Connection] o [MuEmu]).
// Falso si no hay archivo o no trae la clave. Los de [MuEmu] mandan sobre config.ini (ver GetConfigInt).
bool GetInfoValue(const char* section, const char* key, std::string& value);
// Abre un archivo custom ("Data/MuEmu/CustomItem.txt"): el de SvenInfo.bmd si lo trae, si no el del disco.
std::unique_ptr<std::istream> OpenData(const char* path);
// Guarda un entero en la seccion [MuEmu] de config.ini (reemplaza la clave o la agrega al final de la seccion).
void SetConfigInt(const char* key, int value);
} // namespace MuEmu
