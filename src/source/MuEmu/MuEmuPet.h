#pragma once

// Mascotas custom de los clientes MuEmu (GetMainInfo: CustomPet.txt), leidas de Data/MuEmu, como Pet.cpp y
// PetMontura.cpp de Main.dll. El modelo del item (Data/Item/<ModelName>.bmd) se registra con
// --muemu-import-models. Segun el tipo de la linea, la mascota:
// - 5, 6, 7 y 11: se monta y se mueve como Fenrir, Dark Horse, Uniria o Dinorant (animaciones del jugador incluidas).
// - el resto (8, Muun): camina detras del personaje.
namespace MuEmu::Pet
{
// Si el helper del personaje (`c->Helper.Type`) es una mascota custom, crea su montura con el modelo del item y,
// si se monta, deja en `c->Helper.Type` el helper original equivalente (el Dark Horse para el tipo 6) para que el
// cliente use sus animaciones y skills. Verdadero si era una mascota custom (el cliente no crea otra montura).
bool CreateMount(CHARACTER* c, OBJECT* o, OBJECT* pHelper);

// El helper original equivalente al modelo `helperType` si es una mascota custom que se monta; si no, el mismo.
int RiderHelperType(int helperType);

// Como se mueve la montura de tipo `mountType`: la montura original equivalente si es una mascota custom que se
// monta; si no, el mismo tipo.
int MoveType(int mountType);

// Movimiento de las mascotas custom que caminan detras del personaje.
void MoveFollower(OBJECT* o);

// Escala de la mascota custom (columna View de CustomPet; +0.3 en la seleccion de personaje, como Main.dll).
void ApplyScale(OBJECT* o, bool bForceRender);

// Numero de item (grupo * 512 + numero) de la montura `o` si es una mascota custom; -1 si no.
int GetItemType(const OBJECT* o);
} // namespace MuEmu::Pet
