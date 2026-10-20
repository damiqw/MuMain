#pragma once

// Efectos visuales de los clientes MuEmu (GetMainInfo), leidos de Data/MuEmu:
// - CustomEffectStatic.txt: sprites sobre los huesos de un item (alas, armas...), que reemplazan los del cliente.
// - CustomModelEffect.txt: efectos sobre el personaje que lleva un casco, armadura, pantalones o botas (nivel y
//   excelente minimos).
// - CustomRuneEffect.txt: runa en el piso segun VIP y PK, con los datos del servidor (C1 F3 EF).
// Las texturas usan los numeros de 1.04E, que coinciden con los del cliente desde 32001 (BITMAP_LIGHT = 32002).
namespace MuEmu::Effect
{
// Efectos del item `itemType` (grupo * 512 + numero) con el modelo `model` y el objeto `object`. Verdadero si el
// item tiene efectos custom (el cliente no dibuja los suyos).
bool RenderItem(int itemType, BMD* model, OBJECT* object);

// Efectos de set y runa del personaje (en la escena del juego).
void RenderCharacter(CHARACTER* c, OBJECT* o);

// Guarda los datos de la runa de un jugador (C1 F3 EF).
void ReceiveRune(const unsigned char* buffer, int size);
} // namespace MuEmu::Effect
