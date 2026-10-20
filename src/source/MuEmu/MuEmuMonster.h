#pragma once

// Monstruos y NPC custom de los clientes MuEmu (GetMainInfo: CustomMonster.txt y CustomNPCName.txt), leidos de
// Data/MuEmu. Cada clase custom usa su propio .bmd (en los modelos de monstruo que el cliente deja libres) y su
// nombre; CustomNPCName cambia el nombre de un NPC segun el mapa y la posicion.
namespace MuEmu::Monster
{
// Crea el personaje de una clase custom; nullptr si la clase no es custom o su modelo no se pudo abrir.
CHARACTER* Create(int type, int positionX, int positionY, int key);

// Despues de Setting_Monster: nombre y tipo (NPC o monstruo) de las clases custom y nombres de CustomNPCName.
void Apply(CHARACTER* c, int type, int positionX, int positionY);
} // namespace MuEmu::Monster
