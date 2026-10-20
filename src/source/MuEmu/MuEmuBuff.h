#pragma once

#include <functional>
#include <list>
#include <string>

// Buffs custom de los clientes MuEmu (GetMainInfo: CustomBuffIcon.txt), leidos de Data/MuEmu. Las lineas
// tienen el mismo formato que BuffEffect y la reemplazan o la extienden (buffs del servidor hasta el 255).
// Los "%d" de la descripcion se llenan con los valores que manda el servidor (paquete C1 FB 13).
namespace MuEmu::Buff
{
struct Entry
{
    int index = 0;
    int effectType = 0;
    int itemType = 255;
    int itemIndex = 255;
    std::wstring name;
    int classType = 0;
    int noticeType = 0;
    int clearType = 0;
    std::wstring descript;
};

// Llama a `apply` con cada buff de CustomBuffIcon.txt.
void ForEach(const std::function<void(const Entry&)>& apply);

// Icono del buff: los buffs nuevos del servidor usan el de la segunda columna, como en Main.dll.
int GetIcon(int buff);

// Guarda los valores del buff que manda el servidor (C1 FB 13).
void ReceiveValues(const unsigned char* buffer, int size);

// Llena los "%d" (y "%%") de las lineas de la descripcion con los valores recibidos.
void FormatLines(int buff, std::list<std::wstring>& lines);
} // namespace MuEmu::Buff
