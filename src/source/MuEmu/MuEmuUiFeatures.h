#pragma once

#include <cstdint>
#include <string>

// Las ventanas de MuEmu::Ui, cada una en su archivo. Receive() devuelve verdadero si el paquete era suyo.
namespace MuEmu::Ui::Events
{
void Open();         // horarios de eventos (C1 F3 E8)
void OpenCommands(); // CustomCommandInfo.txt
bool Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size);
void Frame();
} // namespace MuEmu::Ui::Events

namespace MuEmu::Ui::Bank
{
void OpenJewelBank(); // banco de joyas (C1 F3 F4/F5/F6)
void OpenWarehouse(); // cambio de baul (C1 F3 F7)
// Titulo del baul ("Storage [2/20]") o vacio si el servidor no mando los baules (C1 FB D5).
std::wstring WarehouseTitle(const wchar_t* storage);
// Pasa al baul anterior (-1) o siguiente (1).
void ChangeWarehouse(int step);
bool Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size);
} // namespace MuEmu::Ui::Bank

namespace MuEmu::Ui::Ranking
{
void Open();       // rankings (C1 F3 E6/E7)
void ToggleDps();  // medidor de dano (C1 F3 F7)
bool Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size);
void Frame();      // rango sobre los jugadores y medidor de dano
// Equipo de un personaje del ranking (C2 D3 40), pedido con el boton Ver.
bool ReceiveCharacter(uint8_t head, uint8_t sub, const uint8_t* buffer, int size);
// Item de 12 bytes del servidor: nombre, nivel y opciones ("Exc Espada +9 +Skill"), o vacio.
std::string DescribeItem(const uint8_t* item);
} // namespace MuEmu::Ui::Ranking

namespace MuEmu::Ui::Rewards
{
void OpenDaily();       // recompensa diaria (C1 FF 02 / FB 03-04)
void OpenInvasion();    // invasiones (FA F8/F9)
void OpenPartySearch(); // busqueda de party (FF 05/07, F3 F0)
void OpenPublishParty(); // opciones de la party propia y publicarla (FF 04/06, FB 06)
bool Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size);
void Frame();
} // namespace MuEmu::Ui::Rewards

namespace MuEmu::Ui::GuildRewards
{
// Misiones (C2 FF 30) y cofre (C2 FF 32) del guild: el servidor los manda desde su NPC.
bool Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size);
} // namespace MuEmu::Ui::GuildRewards

namespace MuEmu::Ui::OffHelper
{
// /offhelper: el servidor pide la configuracion del MU Helper (C1 FB 3E) y se le manda (C1 FB 3D).
bool Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size);
} // namespace MuEmu::Ui::OffHelper

namespace MuEmu::Ui::PostItem
{
// Ventana para publicar el item en esa posicion del inventario (Ctrl + click).
void Open(int position);
// Los ultimos items publicados en el chat.
void OpenRecent();
// Item publicado por un jugador (C1 78): va al chat.
bool Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size);
// La linea del chat bajo el cursor (remitente y texto), o nada: si es un item publicado se ve su tooltip.
void SetHoveredLine(const std::wstring* id, const std::wstring* text);
void Frame();
} // namespace MuEmu::Ui::PostItem

namespace MuEmu::Ui::Store
{
// Tienda personal abierta o cerrada (C1 F3 EC, como OffTrade de Main.dll): con la tienda abierta se ofrecen
// cerrarla y dejarla offline.
bool Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size);
bool IsActive();
} // namespace MuEmu::Ui::Store

namespace MuEmu::Ui::Vip
{
// Comprar VIP (BuyVip de Main.dll): planes del servidor (C2 F3 FB) y compra (C1 F3 F0).
void Open();
bool Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size);
} // namespace MuEmu::Ui::Vip

namespace MuEmu::AntiLag
{
// Panel AntiLag (Antilag de Main.dll): apaga efectos y modelos para PCs lentas. Se guarda en config.ini [MuEmu].
enum Option
{
    CustomItemEffects, // efectos de los items custom (CustomEffectStatic)
    SetEffects,        // efectos de set y runa (CustomModelEffect, CustomRuneEffect)
    OtherPlayers,      // los demas jugadores
    OtherWings,        // alas de los demas jugadores
    GroundZen,         // zen en el piso
    OptionCount,
};
// Verdadero si la opcion esta apagada (no se dibuja).
bool IsHidden(Option option);
void Open();
} // namespace MuEmu::AntiLag

namespace MuEmu::Ui::Stats
{
// Stats de 32 bits de los servidores MuEmu (C1 F3 E0 y E1).
bool Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size);
} // namespace MuEmu::Ui::Stats
