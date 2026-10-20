#pragma once

// Niveles de entrada a los eventos de los clientes MuEmu (GetMainInfo: CustomEventEntryLevel.txt), leidos de
// Data/MuEmu. Secciones: 0 Blood Castle, 1 Devil Square, 2 Chaos Castle, 3 Kalima, 4 Illusion Temple.
namespace MuEmu::EventLevel
{
enum Section
{
    BloodCastle = 0,
    DevilSquare = 1,
    ChaosCastle = 2,
    Kalima = 3,
    IllusionTemple = 4,
};

// Niveles minimo y maximo de la fila `row`; `special` es la columna de MG, DL y RF. Falso si no hay fila.
bool Get(int section, int row, bool special, int& minLevel, int& maxLevel);

// Reemplaza las filas de una tabla del cliente: las comunes desde 0 y las especiales desde `specialStart`.
void Apply(int section, int (*levels)[2], int count, int specialStart);
} // namespace MuEmu::EventLevel
