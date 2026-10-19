#pragma once

// Herramientas de linea de comandos para servidores MuEmu (ver docs/muemu/README.md).
namespace MuEmu
{
// Si la linea de comandos pide una herramienta, la ejecuta y devuelve true: el cliente debe cerrarse.
//   --muemu-import-items  Convierte Data/Local/<idioma>/Item_<idioma>.bmd a Data/Items/*.json
//   --muemu-import-models[=carpeta]  Agrega a Data/Items/Models los modelos de las tablas custom de
//                                     GetMainInfo (CustomItem, CustomJewel, CustomPet, CustomFlag). Por defecto MAIN_INFO/Custom.
bool RunCommandLineTool(const char* commandLine);

// Lee un entero de la seccion [MuEmu] de config.ini (junto a Main.exe); defaultValue si no esta.
int GetConfigInt(const char* key, int defaultValue);
} // namespace MuEmu
