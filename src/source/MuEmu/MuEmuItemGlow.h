#pragma once

#include <array>

// Color del brillo (+7 en adelante) de los items custom de los clientes MuEmu (GetMainInfo: CustomItem.txt y
// CustomFlag.txt, columnas R G B de 0 a 255), leidos de Data/MuEmu, como GetCustomItemColor de Main.dll.
namespace MuEmu::ItemGlow
{
// Reemplaza `color` por el del item `itemType` (grupo * 512 + numero) si tiene uno custom.
void Apply(int itemType, std::array<float, 3>& color);
} // namespace MuEmu::ItemGlow
