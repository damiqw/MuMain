#include "stdafx.h"
#include "MuEmu/MuEmuUiFeatures.h"

#include "MuEmu/MuEmuPanel.h"
#include "MuEmu/MuEmuUi.h"

#include "Engine/Object/ZzzInfomation.h"
#include "World/MapInfra/MapManager.h"

#include <algorithm>
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

std::string g_partyPassword;

void JoinParty(const PartyEntry& party)
{
    // Nombre del lider (11), con clave (1) y la clave (20): sin clave C1 FF 05, con clave C1 FF 13.
    std::vector<uint8_t> payload(11 + 1 + 20, 0);
    std::memcpy(payload.data(), party.rawLeader.c_str(), party.rawLeader.size() < 10 ? party.rawLeader.size() : 10);
    if (party.password)
    {
        payload[11] = 1;
        std::memcpy(payload.data() + 12, g_partyPassword.c_str(), std::min<std::size_t>(g_partyPassword.size(), 19));
    }
    MuEmu::Ui::Send(0xFF, party.password ? 0x13 : 0x05, payload);
}

MuEmu::Panel::Content BuildParties()
{
    MuEmu::Panel::Content content;
    content.title = "Buscar party";
    if (MuEmu::Panel::IsShown(PartyId))
        g_partyPassword = MuEmu::Panel::GetInput(); // lo que se esta escribiendo

    if (!g_partiesReceived)
    {
        content.lines = { "Esperando la lista del servidor..." };
        return content;
    }

    content.wide = true;
    content.header = { "Lider", "Mapa", "Posicion", "Nivel", "Miembros", "Clave" };
    for (std::size_t n = 0; n < g_parties.size(); ++n)
    {
        const PartyEntry& party = g_parties[n];
        MuEmu::Panel::Row row;
        row.cells = { party.leader, MuEmu::Panel::ToUtf8(gMapManager.GetMapName(party.map)),
                      std::to_string(party.x) + ", " + std::to_string(party.y), std::to_string(party.level),
                      std::to_string(party.members) + "/5", party.password ? "Si" : "No" };
        if (party.password)
            row.color = "#d0c080";
        row.buttons = { { "Unirse", static_cast<int>(n) } };
        content.rows.push_back(std::move(row));
    }

    if (content.rows.empty())
        content.lines = { "No hay partys publicadas." };
    content.inputLabel = "Clave (para las partys con clave):";
    content.inputValue = g_partyPassword;
    content.buttons = { { "Actualizar", -1 } };
    return content;
}
// ------------------------------------------------ publicar la party propia

constexpr const char* PartySettingsId = "party_settings";
constexpr int ClassCount = 7;
constexpr const char* ClassNames[ClassCount] = { "Dark Wizard", "Dark Knight", "Elf", "Magic Gladiator", "Dark Lord",
                                                 "Summoner", "Rage Fighter" };

struct PartySettings
{
    bool onlyGuild = false;
    bool onlyAlliance = false;
    bool oneClass = false;
    std::array<bool, ClassCount> classes{ true, true, true, true, true, true, true };
    int level = 1;
    bool password = false;
    std::string passwordText;
};

PartySettings g_partySettings;

enum PartySettingsAction
{
    ToggleOnlyGuild = 1,
    ToggleOnlyAlliance,
    ToggleOneClass,
    ToggleClassFirst, // + clase (0 a 6)
    TogglePassword = ToggleClassFirst + ClassCount,
    LevelMinus100 = 20,
    LevelMinus10,
    LevelPlus10,
    LevelPlus100,
    ActionPublish = 30,
    ActionRemove,
    ActionCancel,
};

void SendPartySettings(bool publish)
{
    // C1 FF 04 (44 bytes): activa (4), solo guild (5), solo alianza (6), una clase (7), las 7 clases (8-14),
    // nivel minimo (int en 16), con clave (20) y la clave (20 desde 21).
    const PartySettings& settings = g_partySettings;
    std::vector<uint8_t> payload(40, 0);
    payload[0] = publish ? 1 : 0;
    payload[1] = settings.onlyGuild ? 1 : 0;
    payload[2] = settings.onlyAlliance ? 1 : 0;
    payload[3] = settings.oneClass ? 1 : 0;
    for (int n = 0; n < ClassCount; ++n)
        payload[4 + n] = settings.classes[n] ? 1 : 0;
    const auto level = static_cast<uint32_t>(settings.level);
    for (int n = 0; n < 4; ++n)
        payload[12 + n] = static_cast<uint8_t>((level >> (8 * n)) & 0xFF);
    const bool password = settings.password && !settings.passwordText.empty();
    payload[16] = password ? 1 : 0;
    if (password)
        std::memcpy(payload.data() + 17, settings.passwordText.c_str(), std::min<std::size_t>(settings.passwordText.size(), 19));
    MuEmu::Ui::Send(0xFF, 0x04, payload);
}

std::string YesNo(bool value)
{
    return value ? "Si" : "No";
}

void ShowPartySettings();

void OnPartySettingsAction(int action)
{
    PartySettings& settings = g_partySettings;
    settings.passwordText = MuEmu::Panel::GetInput();

    if (action >= ToggleClassFirst && action < ToggleClassFirst + ClassCount)
        settings.classes[action - ToggleClassFirst] = !settings.classes[action - ToggleClassFirst];

    switch (action)
    {
    case ToggleOnlyGuild: settings.onlyGuild = !settings.onlyGuild; break;
    case ToggleOnlyAlliance: settings.onlyAlliance = !settings.onlyAlliance; break;
    case ToggleOneClass: settings.oneClass = !settings.oneClass; break;
    case TogglePassword: settings.password = !settings.password; break;
    case LevelMinus100: settings.level -= 100; break;
    case LevelMinus10: settings.level -= 10; break;
    case LevelPlus10: settings.level += 10; break;
    case LevelPlus100: settings.level += 100; break;
    case ActionPublish:
        SendPartySettings(true);
        MuEmu::Panel::Hide();
        return;
    case ActionRemove:
        SendPartySettings(false);
        MuEmu::Panel::Hide();
        return;
    case ActionCancel:
        MuEmu::Panel::Hide();
        return;
    default: break;
    }

    settings.level = std::clamp(settings.level, 1, 400);
    ShowPartySettings();
}

void ShowPartySettings()
{
    const PartySettings& settings = g_partySettings;
    MuEmu::Panel::Content content;
    content.title = "Publicar party";
    content.header = { "Opcion", "Valor" };

    auto add = [&content](const std::string& name, const std::string& value, bool on, std::vector<MuEmu::Panel::Button> buttons)
    {
        MuEmu::Panel::Row row;
        row.cells = { name, value };
        row.color = on ? "#60e060" : "#c0c0c0";
        row.buttons = std::move(buttons);
        content.rows.push_back(std::move(row));
    };

    add("Solo miembros del guild", YesNo(settings.onlyGuild), settings.onlyGuild, { { "Cambiar", ToggleOnlyGuild } });
    add("Solo miembros de la alianza", YesNo(settings.onlyAlliance), settings.onlyAlliance, { { "Cambiar", ToggleOnlyAlliance } });
    add("Una vez cada clase", YesNo(settings.oneClass), settings.oneClass, { { "Cambiar", ToggleOneClass } });
    for (int n = 0; n < ClassCount; ++n)
        add(ClassNames[n], settings.classes[n] ? "Puede entrar" : "No", settings.classes[n], { { "Cambiar", ToggleClassFirst + n } });
    add("Nivel minimo", std::to_string(settings.level), true,
        { { "-100", LevelMinus100 }, { "-10", LevelMinus10 }, { "+10", LevelPlus10 }, { "+100", LevelPlus100 } });
    add("Pedir clave", YesNo(settings.password), settings.password, { { "Cambiar", TogglePassword } });

    if (settings.password)
    {
        content.inputLabel = "Clave:";
        content.inputValue = settings.passwordText;
    }

    content.buttons = { { "Publicar", ActionPublish }, { "Quitar publicacion", ActionRemove }, { "Cancelar", ActionCancel } };
    content.wide = true;
    MuEmu::Panel::Show(PartySettingsId, std::move(content), OnPartySettingsAction);
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
        g_partyPassword = MuEmu::Panel::GetInput();
        if (action < 0)
            Send(0xFF, 0x07);
        else if (action < static_cast<int>(g_parties.size()))
            JoinParty(g_parties[static_cast<std::size_t>(action)]);
    });
}

void MuEmu::Ui::Rewards::OpenPublishParty()
{
    Send(0xFF, 0x06); // opciones guardadas (C1 FB 06)
    ShowPartySettings();
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
        // Primero las que se pueden unir sin clave, despues por mapa y lider.
        std::stable_sort(g_parties.begin(), g_parties.end(), [](const PartyEntry& a, const PartyEntry& b)
        {
            if (a.password != b.password)
                return !a.password;
            if (a.map != b.map)
                return a.map < b.map;
            return a.leader < b.leader;
        });
        g_partiesReceived = true;
        MuEmu::Panel::Update(PartyId, BuildParties());
        return true;
    }

    if (head == 0xFB && sub == 0x06 && buffer[0] == 0xC1)
    {
        // C1 FB 06: las opciones guardadas, con el mismo formato que FF 04.
        if (size >= 41)
        {
            PartySettings& settings = g_partySettings;
            settings.onlyGuild = buffer[5] != 0;
            settings.onlyAlliance = buffer[6] != 0;
            settings.oneClass = buffer[7] != 0;
            for (int n = 0; n < ClassCount; ++n)
                settings.classes[n] = buffer[8 + n] != 0;
            settings.level = std::clamp(ReadInt(buffer, 16), 1, 400);
            settings.password = buffer[20] != 0;
            settings.passwordText.assign(reinterpret_cast<const char*>(buffer + 21),
                                         strnlen(reinterpret_cast<const char*>(buffer + 21), 20));
            if (MuEmu::Panel::IsShown(PartySettingsId))
                ShowPartySettings();
        }
        return true;
    }
    return false;
}

void MuEmu::Ui::Rewards::Frame()
{
    // Las invasiones llegan cada segundo con el tiempo ya actualizado: nada que contar aca.
}
