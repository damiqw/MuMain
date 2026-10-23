#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

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
// La ventana propia del banco (temas legacy/ex700): cerrarla, si esta abierta (o la generica) y sus joyas 3D.
void CloseJewelBank();
bool IsJewelBankOpen();
void Frame();
// Shift + click en el inventario: deposita el item de esa posicion si es una joya del banco (o su paquete).
bool DepositAt(int position, int itemType);
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

namespace MuEmu::Ui::Wheel
{
// Ruleta (Lucky Wheel V2 de Main.dll): C1/C2 D3 8A, 8B y 8C.
void Open();
bool Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size);
} // namespace MuEmu::Ui::Wheel

namespace MuEmu::Ui::XShop
{
// Tienda XShop (SPK_xShop de Main.dll): C1/C2 D5 11, 12 y 13.
void Open();
bool Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size);
} // namespace MuEmu::Ui::XShop

namespace MuEmu::Ui::Flag
{
// Alquiler de banderas (ThueFlag de Main.dll): envia C1 FF 03, recibe C1 FB 18.
void Open();
bool Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size);
} // namespace MuEmu::Ui::Flag

namespace MuEmu::Ui::Ruud
{
// Tienda Ruud (NPCRuud de Main.dll): la abre el NPC (C1/C3 EE y lista C2 EA); compra C1 EE y cierra C1 31.
bool Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size);
} // namespace MuEmu::Ui::Ruud

namespace MuEmu::Ui::ItemColor
{
// Color de items (CustomItemColor de Main.dll): ventana del NPC (C1 EF 10/12) y colores de los jugadores (C2 EF 13).
bool Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size);
// Tine la luz de una parte del equipo (lugar del servidor, 0 a 11) del jugador con ese indice; falso si no tiene color.
bool Tint(int characterKey, int slot, float* light);
} // namespace MuEmu::Ui::ItemColor

namespace MuEmu::Ui::StatsAdvance
{
// Stats avanzados (CustomStatsAdvance de Main.dll): C1 F3 F4. Open abre o cierra la ventana pegada a la del
// personaje, en todos los temas.
void Open();
void Close();
void Frame();
bool Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size);
} // namespace MuEmu::Ui::StatsAdvance

namespace MuEmu::Ui::Ping
{
// Ping en pantalla (CustomPing de Main.dll): C1 F3 F1 cada 5 segundos, respuesta C1 F3 F3. [MuEmu] Ping = 0 lo apaga.
void Frame();
bool Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size);
} // namespace MuEmu::Ui::Ping

namespace MuEmu::Ui::Evidence
{
// Evidencia de hack (HackCheck.cpp de Main.dll): al pedido C1 FB 75 del servidor contesta C2 FB 76 (ventanas,
// DLL y procesos sospechosos) y, si es manual, la lista de procesos en C2 FB 77.
bool Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size);
} // namespace MuEmu::Ui::Evidence

namespace MuEmu::Ui::QuickStats
{
// Sumar puntos rapido (CustomQuickAddStats de Main.dll): C1 FF 0C.
void Open();
bool IsShown();
} // namespace MuEmu::Ui::QuickStats

namespace MuEmu::Ui::Register
{
// Crear cuenta desde el login (CreateAccount de Main.dll): C3 F1 12, respuesta C1 F1 12.
void Open();
bool Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size);
} // namespace MuEmu::Ui::Register

namespace MuEmu::Ui::PartyInfo
{
// Buffs de los miembros (C1 2E, PartyBuffs de Main.dll): decoradores RmlUi de sus iconos (hasta 8).
std::vector<std::string> BuffIcons(const std::string& name);
// Ayuda de asignacion (PartyAssignHelper): el lider mantiene Ctrl + S (C1 FF 38), los miembros lo siguen (C1 FB 38).
void Frame();
bool Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size);
} // namespace MuEmu::Ui::PartyInfo

namespace MuEmu::Ui::MiniMapInfo
{
// Minimapa con datos del servidor (MiniMapInfo de Main.dll): lugares (C1 E7 03) y party (C1 F3 F9).
void Frame(); // pide los datos al abrir el mapa (C1 E7 01) y avisa al cerrarlo (E7 02)
bool Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size);
// Los marcadores para agregar al mapa: tipo (1 NPC, 2 portal), casilla y nombre.
void ForEachMarker(const std::function<void(int kind, int x, int y, const std::wstring& name)>& add);
} // namespace MuEmu::Ui::MiniMapInfo

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
