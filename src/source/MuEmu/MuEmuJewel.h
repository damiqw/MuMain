#pragma once

#include <cstdint>

// Joyas custom de los clientes MuEmu (GetMainInfo: CustomJewel.txt, seccion 0), leidas de Data/MuEmu. Permiten
// soltar la joya sobre un item del inventario como las de Webzen; el servidor valida el resto y aplica la joya.
namespace MuEmu::Jewel
{
// Si el item (grupo * 512 + numero) es una joya custom.
bool IsJewel(int itemType);

// Si la joya se puede soltar sobre el item destino (armas, armaduras, alas o accesorios segun la joya).
bool CanApply(int jewelType, int targetType);

// Precio de venta de la joya custom (SalePrice); falso si el item no es una joya custom.
bool GetSalePrice(int itemType, int64_t& price);
} // namespace MuEmu::Jewel
