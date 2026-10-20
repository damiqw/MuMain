#pragma once

// Numeros de textura de los clientes MuEmu (1.04E) en este cliente.
// Coinciden hasta 32274; desde ahi 1.04E tiene texturas que este cliente no carga (y al reves), asi que los
// numeros se corren (por ejemplo ShockWave.jpg es 32279 en 1.04E y BITMAP_SHOCK_WAVE = 32278 aca).
namespace MuEmu::Texture
{
// Textura de este cliente para el numero `id` de 1.04E; -1 si este cliente no la tiene.
int From104E(int id);
} // namespace MuEmu::Texture
