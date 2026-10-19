#pragma once

// Herramientas de linea de comandos para servidores MuEmu (ver docs/muemu/README.md).
namespace MuEmu
{
// Si la linea de comandos pide una herramienta, la ejecuta y devuelve true: el cliente debe cerrarse.
//   --muemu-import-items  Convierte Data/Local/<idioma>/Item_<idioma>.bmd a Data/Items/*.json
bool RunCommandLineTool(const char* commandLine);
} // namespace MuEmu
