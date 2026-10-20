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
