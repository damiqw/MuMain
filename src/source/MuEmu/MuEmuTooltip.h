#pragma once

// Tooltips custom de los clientes MuEmu (GetMainInfo: CustomItemToolTip.txt + CustomItemTooltipText.txt),
// leidos de Data/MuEmu. Agrega al tooltip del item un titulo y hasta 15 lineas con su color.
namespace MuEmu::Tooltip
{
// Agrega las lineas del item `itemType` (grupo * 512 + numero) desde `textNum`; devuelve el nuevo total.
int Append(int itemType, int textNum);
} // namespace MuEmu::Tooltip
