#pragma once

#include <cstdint>

// Precios custom de los clientes MuEmu (GetMainInfo: ItemValue.txt), leidos de Data/MuEmu. Cambian el precio
// de compra y venta del NPC y la moneda que se muestra (Zen, WCoinC, WCoinP, Goblin Point o Ruud).
namespace MuEmu::ItemPrice
{
// Precio custom del item: `goldType` 0 es compra y 1 venta, como en ItemValue(). Falso si no hay entrada.
bool Find(const ITEM* item, int goldType, int64_t& value);

// Nombre de la moneda del item, o nullptr si se paga con Zen (o no tiene precio custom).
const wchar_t* GetCurrencyName(const ITEM* item);
} // namespace MuEmu::ItemPrice
