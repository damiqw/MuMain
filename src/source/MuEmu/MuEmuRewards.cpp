#include "stdafx.h"
#include "MuEmu/MuEmuUiFeatures.h"

#include "MuEmu/MuEmuPanel.h"
#include "MuEmu/MuEmuUi.h"

#include "Engine/Object/ZzzInfomation.h"
#include "World/MapInfra/MapManager.h"

#include <array>
#include <chrono>
#include <cstring>
#include <string>
#include <vector>

namespace
{
constexpr const char* DailyId = "daily";
constexpr const char* InvasionId = "invasion";
constexpr const char* PartyId = "party_search";

// ------------------------------------------------ recompensa diaria

enum DailyRequest : uint8_t
{
    DailyInfo = 0,  // dias del mes y cuales se reclamaron
    DailyDay = 1,   // premio de un dia
    DailyClaim = 2, // reclamar un dia
};

// Acciones: ver = dia, reclamar = 100 + dia.
constexpr int ClaimBase = 100;

struct DailyDayInfo
{
    int day = 0;
    int itemType = -1;
    int wcoin = 0;
    int zen = 0;
    std::string text;
};

struct DailyReward
{
    int maxDay = 0;
    int currentDay = 0;
    std::array<uint8_t, 31> claimed{};
    bool received = false;
    int selectedDay = 0;
    DailyDayInfo selected;
};

DailyReward g_daily;

void SendDaily(DailyRequest type, int value)
{
    // C1 FF 02: tipo (byte 4) y valor (int en 8).
    std::vector<uint8_t> payload = { type, 0, 0, 0 };
    MuEmu::Ui::WriteInt(payload, value);
    MuEmu::Ui::Send(0xFF, 0x02, payload);
}

std::string ItemName(int itemType)
{
    if (itemType < 0 || itemType >= MAX_ITEM || ItemAttribute[itemType].Name[0] == 0)
        return std::string();
    return MuEmu::Panel::ToUtf8(ItemAttribute[itemType].Name);
}

MuEmu::Panel::Content BuildDaily()
{
    MuEmu::Panel::Content content;
    content.title = "Recompensa diaria";
    if (!g_daily.received)
    {
        content.lines = { "Esperando los datos del servidor..." };
        return content;
    }

    if (g_daily.selectedDay > 0 && g_daily.selected.day == g_daily.selectedDay)
    {
        const DailyDayInfo& info = g_daily.selected;
        content.lines.push_back("Dia " + std::to_string(info.day) + ":");
        const std::string item = ItemName(info.itemType);
        if (!item.empty())
            content.lines.push_back("Item: " + item);
        if (info.wcoin > 0)
            content.lines.push_back("WCoin: " + MuEmu::Ui::FormatNumber(info.wcoin));
        if (info.zen > 0)
            content.lines.push_back("Zen: " + MuEmu::Ui::FormatNumber(info.zen));
        if (!info.text.empty())
            content.lines.push_back(info.text);
    }

    content.header = { "Dia", "Estado" };
    for (int day = 1; day <= g_daily.maxDay && day <= 31; ++day)
    {
        MuEmu::Panel::Row row;
        const bool claimed = g_daily.claimed[day - 1] != 0;
        std::string state;
        if (claimed)
        {
            state = "Reclamado";
            row.color = "#a0a0a0";
        }
        else if (day <= g_daily.currentDay)
        {
            state = "Disponible";
            row.color = "#60e060";
            row.buttons.push_back({ "Reclamar", ClaimBase + day });
        }
        else
        {
            state = "Pendiente";
        }
        if (day == g_daily.currentDay)
            state += " (hoy)";
        row.buttons.insert(row.buttons.begin(), { "Ver", day });
        row.cells = { std::to_string(day), state };
        content.rows.push_back(std::move(row));
    }
    return content;
}

// ------------------------------------------------ invasiones

constexpr int MaxInvasions = 30;
constexpr int InvasionStart = 2;

struct Invasion
{
    int index = 0;
    int state = 0;
    int seconds = 0;
    std::string name;
};

struct InvasionMonster
{
    int monsterClass = 0;
    int count = 0;
    int countMax = 0;
};

std::vector<Invasion> g_invasions;
std::vector<InvasionMonster> g_invasionMonsters;
int g_invasionShown = -1;
bool g_invasionsReceived = false;

std::string MonsterName(int monsterClass)
{
    for (int n = 0; n < MAX_MONSTER; ++n)
    {
        if (MonsterScript[n].Type == monsterClass && MonsterScript[n].Name[0] != 0)
            return MuEmu::Panel::ToUtf8(MonsterScript[n].Name);
    }
    return "Monstruo " + std::to_string(monsterClass);
}

MuEmu::Panel::Content BuildInvasions()
{
    MuEmu::Panel::Content content;
    content.title = "Invasiones";
    if (!g_invasionsReceived)
    {
        content.lines = { "Esperando las invasiones del servidor..." };
        return content;
    }

    for (const Invasion& invasion : g_invasions)
    {
        if (invasion.name.empty())
            continue;

        MuEmu::Panel::Row row;
        if (invasion.state == InvasionStart)
        {
            row.cells = { invasion.name, "En curso" };
            row.color = "#60e060";
            row.buttons = { { "Ver", invasion.index } };
        }
        else
        {
            row.cells = { invasion.name, invasion.seconds > 0 ? MuEmu::Ui::FormatTime(invasion.seconds) : "-" };
        }
        content.rows.push_back(std::move(row));
    }

    if (g_invasionShown >= 0)
    {
        for (const InvasionMonster& monster : g_invasionMonsters)
        {
            if (monster.countMax <= 0)
                continue;
            content.lines.push_back(MonsterName(monster.monsterClass) + ": " + std::to_string(monster.count) + "/" +
                                    std::to_string(monster.countMax));
        }
    }

    if (content.rows.empty())
        content.lines.push_back("No hay invasiones programadas.");
    return content;
}

// ------------------------------------------------ busqueda de party

struct PartyEntry
{
    std::string leader;
    std::string rawLeader; // como lo manda el servidor, para pedir unirse
    int map = 0;
    int x = 0;
    int y = 0;
    int level = 0;
    int members = 0;
    bool password = false;
};

std::vector<PartyEntry> g_parties;
bool g_partiesReceived = false;

void JoinParty(const PartyEntry& party)
{
    // C1 FF 05: nombre del lider (11), sin clave y la clave vacia (20).
    std::vector<uint8_t> payload(11 + 1 + 20, 0);
    std::memcpy(payload.data(), party.rawLeader.c_str(), party.rawLeader.size() < 10 ? party.rawLeader.size() : 10);
    MuEmu::Ui::Send(0xFF, 0x05, payload);
}

MuEmu::Panel::Content BuildParties()
{
    MuEmu::Panel::Content content;
    content.title = "Buscar party";
    if (!g_partiesReceived)
    {
        content.lines = { "Esperando la lista del servidor..." };
        return content;
    }

    content.header = { "Lider", "Mapa", "Nivel", "Miembros" };
    for (std::size_t n = 0; n < g_parties.size(); ++n)
    {
        const PartyEntry& party = g_parties[n];
        MuEmu::Panel::Row row;
        const wchar_t* map = gMapManager.GetMapName(party.map);
        row.cells = { party.leader, MuEmu::Panel::ToUtf8(map) + " " + std::to_string(party.x) + "," + std::to_string(party.y),
                      std::to_string(party.level), std::to_string(party.members) };
        if (party.password)
            row.cells.back() += " (clave)";
        else
            row.buttons = { { "Unirse", static_cast<int>(n) } };
        content.rows.push_back(std::move(row));
    }

    if (content.rows.empty())
        content.lines = { "No hay partys publicadas." };
    content.buttons = { { "Actualizar", -1 } };
    return content;
}
} // namespace

void MuEmu::Ui::Rewards::OpenDaily()
{
    SendDaily(DailyInfo, 0);
    MuEmu::Panel::Show(DailyId, BuildDaily(), [](int action)
    {
        if (action > ClaimBase)
        {
            SendDaily(DailyClaim, action - ClaimBase);
        }
        else if (action > 0)
        {
            g_daily.selectedDay = action;
            SendDaily(DailyDay, action);
        }
    });
}

void MuEmu::Ui::Rewards::OpenInvasion()
{
    g_invasionShown = -1;
    MuEmu::Panel::Show(InvasionId, BuildInvasions(), [](int action)
    {
        g_invasionShown = action;
        g_invasionMonsters.clear();
        std::vector<uint8_t> payload;
        WriteInt(payload, action);
        Send(0xD3, 0x4C, payload);
    });
}

void MuEmu::Ui::Rewards::OpenPartySearch()
{
    Send(0xFF, 0x07);
    MuEmu::Panel::Show(PartyId, BuildParties(), [](int action)
    {
        if (action < 0)
            Send(0xFF, 0x07);
        else if (action < static_cast<int>(g_parties.size()))
            JoinParty(g_parties[static_cast<std::size_t>(action)]);
    });
}

void MuEmu::Ui::Rewards::PublishParty(bool publish)
{
    // C1 FF 04 (44 bytes): activa (4), solo guild (5), solo alianza (6), una clase (7), las 7 clases (8-14),
    // nivel minimo (int en 16), con clave (20) y la clave (20 desde 21).
    std::vector<uint8_t> payload(40, 0);
    payload[0] = publish ? 1 : 0;
    for (int n = 4; n <= 10; ++n)
        payload[n] = 1;
    payload[12] = 1;
    Send(0xFF, 0x04, payload);
}

bool MuEmu::Ui::Rewards::Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size)
{
    if (head == 0xFB && sub == 0x03 && buffer[0] == 0xC1)
    {
        // C1 FB 03: ultimo dia (4), hoy (5), primer dia de la semana (6) y reclamados (31 bytes desde 7).
        if (size >= 38)
        {
            g_daily.maxDay = buffer[4];
            g_daily.currentDay = buffer[5];
            for (int n = 0; n < 31; ++n)
                g_daily.claimed[n] = buffer[7 + n];
            g_daily.received = true;
            MuEmu::Panel::Update(DailyId, BuildDaily());
        }
        return true;
    }

    if (head == 0xFB && sub == 0x04 && buffer[0] == 0xC1)
    {
        // C1 FB 04: item (short en 4), WCoin (24), Zen (28) y texto (100 desde 32).
        if (size >= 132)
        {
            DailyDayInfo info;
            info.day = g_daily.selectedDay;
            info.itemType = static_cast<int16_t>(buffer[4] | (buffer[5] << 8));
            info.wcoin = ReadInt(buffer, 24);
            info.zen = ReadInt(buffer, 28);
            info.text = ReadText(buffer, 32, 100);
            g_daily.selected = std::move(info);
            MuEmu::Panel::Update(DailyId, BuildDaily());
        }
        return true;
    }

    if (head == 0xFA && sub == 0xF8 && buffer[0] == 0xC2)
    {
        // C2 FA F8: 30 invasiones de 144 bytes desde el 8 (indice, estado, segundos, nombre de 128, pagina).
        constexpr int EntrySize = 144;
        g_invasions.clear();
        for (int n = 0; n < MaxInvasions && 8 + (n + 1) * EntrySize <= size; ++n)
        {
            const int at = 8 + n * EntrySize;
            Invasion invasion;
            invasion.index = ReadInt(buffer, at);
            invasion.state = ReadInt(buffer, at + 4);
            invasion.seconds = ReadInt(buffer, at + 8);
            invasion.name = ReadText(buffer, at + 12, 128);
            g_invasions.push_back(std::move(invasion));
        }
        g_invasionsReceived = true;
        MuEmu::Panel::Update(InvasionId, BuildInvasions());
        return true;
    }

    if (head == 0xFA && sub == 0xF9 && buffer[0] == 0xC2)
    {
        // C2 FA F9: 20 grupos de 24 bytes desde el 8 (clase, cantidad, maximo...).
        g_invasionMonsters.clear();
        for (int n = 0; n < 20 && 8 + (n + 1) * 24 <= size; ++n)
        {
            const int at = 8 + n * 24;
            InvasionMonster monster;
            monster.monsterClass = ReadInt(buffer, at);
            monster.count = ReadInt(buffer, at + 4);
            monster.countMax = ReadInt(buffer, at + 8);
            g_invasionMonsters.push_back(monster);
        }
        MuEmu::Panel::Update(InvasionId, BuildInvasions());
        return true;
    }

    if (head == 0xF3 && sub == 0xF0 && buffer[0] == 0xC2)
    {
        // C2 F3 F0: cantidad (int en 8) y partys de 44 bytes desde el 12.
        constexpr int EntrySize = 44;
        g_parties.clear();
        const int count = size >= 12 ? ReadInt(buffer, 8) : 0;
        for (int n = 0; n < count && 12 + (n + 1) * EntrySize <= size; ++n)
        {
            const int at = 12 + n * EntrySize;
            PartyEntry party;
            party.leader = ReadText(buffer, at, 11);
            party.rawLeader.assign(reinterpret_cast<const char*>(buffer + at), strnlen(reinterpret_cast<const char*>(buffer + at), 11));
            party.map = ReadInt(buffer, at + 12);
            party.x = ReadInt(buffer, at + 16);
            party.y = ReadInt(buffer, at + 20);
            party.level = ReadInt(buffer, at + 24);
            party.password = buffer[at + 39] != 0;
            party.members = ReadInt(buffer, at + 40);
            g_parties.push_back(std::move(party));
        }
        g_partiesReceived = true;
        MuEmu::Panel::Update(PartyId, BuildParties());
        return true;
    }

    // Opciones de la party propia (C1 FB 06): sin ventana todavia.
    return head == 0xFB && sub == 0x06;
}

void MuEmu::Ui::Rewards::Frame()
{
    // Las invasiones llegan cada segundo con el tiempo ya actualizado: nada que contar aca.
}
