#include "stdafx.h"
#include "MuEmu/MuEmuUiFeatures.h"

#include "MuEmu/MuEmuPanel.h"
#include "MuEmu/MuEmuTools.h"
#include "MuEmu/MuEmuUi.h"

#include "Camera/CameraProjection.h"
#include "Camera/CameraState.h"
#include "Core/Utilities/StringUtils.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Engine/Object/ZzzInterface.h"
#include "Render/Terrain/ZzzLodTerrain.h"
#include "Render/Text/CUIRenderText.h"
#include "UI/Core/WindowCommon.h"
#include "Render/Textures/ZzzOpenglUtil.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace
{
constexpr const char* RankingId = "ranking";

struct RankingEntry
{
    std::string name;
    std::string rawName; // como lo manda el servidor, para pedir su equipo
    std::string value;
    bool online = false;
};

struct Ranking
{
    int type = 0;
    int typeCount = 0;
    std::string title;
    std::string column1;
    std::string column2;
    std::vector<RankingEntry> entries;
    bool received = false;
};

struct RankUser
{
    std::string title;
    std::string vip;
    int level = 0;
    int resets = 0;
    int masterResets = 0;
};

struct DpsUser
{
    std::string name;
    int damage = 0;
    int percent = 0;
};

struct DpsMeter
{
    std::string monster;
    int life = 0;
    int maxLife = 0;
    bool dead = false;
    std::vector<DpsUser> users;
    std::chrono::steady_clock::time_point updated;
    bool received = false;
};

enum RankingAction
{
    ActionPrevious = 1,
    ActionNext,
    ActionBack,
    ActionViewFirst = 1000, // + fila del ranking: ver el equipo del personaje
};

constexpr const char* CharacterId = "ranking_character";

// Los 12 lugares del equipo, en el orden del servidor.
constexpr const char* SlotNames[12] = { "Arma derecha", "Arma izquierda", "Casco", "Armadura", "Pantalones", "Guantes",
                                        "Botas", "Alas", "Mascota", "Collar", "Anillo derecho", "Anillo izquierdo" };

// El medidor se oculta si el servidor deja de mandarlo.
constexpr auto DpsTimeout = std::chrono::seconds(10);

Ranking g_ranking;
std::map<int, RankUser> g_rankUsers;
DpsMeter g_dps;
bool g_dpsEnabled = true;
bool g_dpsLoaded = false;

void RequestRanking(int type)
{
    g_ranking.type = type;
    MuEmu::Ui::Send(0xF3, 0xE6, { static_cast<uint8_t>(type) });
}

MuEmu::Panel::Content BuildRanking()
{
    MuEmu::Panel::Content content;
    content.title = g_ranking.received && !g_ranking.title.empty() ? g_ranking.title : "Rankings";
    if (!g_ranking.received)
    {
        content.lines = { "Esperando el ranking del servidor..." };
        return content;
    }

    content.wide = true;
    content.header = { "#", g_ranking.column1.empty() ? "Nombre" : g_ranking.column1,
                       g_ranking.column2.empty() ? "Valor" : g_ranking.column2 };
    for (std::size_t n = 0; n < g_ranking.entries.size(); ++n)
    {
        const RankingEntry& entry = g_ranking.entries[n];
        MuEmu::Panel::Row row;
        row.cells = { std::to_string(n + 1), entry.name, entry.value };
        row.buttons = { { "Ver", ActionViewFirst + static_cast<int>(n) } };
        if (entry.online)
            row.color = "#60e060";
        else if (n < 3)
            row.color = "#ffd23c";
        content.rows.push_back(std::move(row));
    }

    if (g_ranking.typeCount > 1)
    {
        content.lines = { "Ranking " + std::to_string(g_ranking.type + 1) + " de " + std::to_string(g_ranking.typeCount) +
                          " (verde: en linea)" };
        content.buttons = { { "<", ActionPrevious }, { ">", ActionNext } };
    }
    return content;
}

bool IsVisible(OBJECT* o, CHARACTER* c)
{
    return o->Live && o->Visible && o->Alpha > 0.f && c->Dead == 0;
}

bool ToScreen(OBJECT* o, float height, int& x, int& y)
{
    vec3_t position;
    Vector(o->Position[0], o->Position[1], o->Position[2] + o->BoundingBoxMax[2] + height, position);

    vec3_t transformed;
    VectorTransform(position, g_Camera.Matrix, transformed);
    if (transformed[2] >= 0)
        return false;

    CameraProjection::WorldToScreen(g_Camera, position, &x, &y);
    return x > -100 && y > -100 && x < REFERENCE_WIDTH + 100 && y < REFERENCE_HEIGHT + 100;
}

void RenderLine(int x, int y, const std::string& text, BYTE r, BYTE g, BYTE b)
{
    const std::wstring wide = StringUtils::NarrowToWide(text);
    g_pRenderText->SetTextColor(r, g, b, 255);
    g_pRenderText->SetBgColor(0, 0, 0, 0);
    g_pRenderText->RenderText(x, y, wide.c_str(), 0, 0, RT3_WRITE_CENTER);
}

// Los interruptores de CustomRankUser de MainInfo.ini (SvenInfo.bmd o [MuEmu] de config.ini). Sin archivo, todo
// visible como hasta ahora.
struct RankSwitches
{
    int type = 0;            // RankUserType: 4 = solo el rango sobre la cabeza, sin los datos del jugador
    bool overHead = true;    // RankUserShowOverHead: el rango sobre la cabeza de cada jugador
    bool needAlt = false;    // RankUserNeedAltKey: los datos del jugador solo con Alt apretado
    bool safeZone = false;   // RankUserOnlyOnSafeZone: solo de los jugadores en zona segura
    bool showReset = true;   // RankUserShowReset
    bool showMasterReset = true; // RankUserShowMasterReset
};

const RankSwitches& Switches()
{
    static const RankSwitches switches = []
    {
        RankSwitches s;
        s.type = MuEmu::GetConfigInt("RankType", 0);
        s.overHead = MuEmu::GetConfigInt("RankOverHead", 1) != 0;
        s.needAlt = MuEmu::GetConfigInt("RankNeedAlt", 0) != 0;
        s.safeZone = MuEmu::GetConfigInt("RankSafeZone", 0) != 0;
        s.showReset = MuEmu::GetConfigInt("RankShowReset", 1) != 0;
        s.showMasterReset = MuEmu::GetConfigInt("RankShowMasterReset", 1) != 0;
        return s;
    }();
    return switches;
}

bool InSafeZone(const OBJECT* o)
{
    const int x = static_cast<int>(o->Position[0] / TERRAIN_SCALE);
    const int y = static_cast<int>(o->Position[1] / TERRAIN_SCALE);
    return (TerrainWall[TERRAIN_INDEX_REPEAT(x, y)] & TW_SAFEZONE) == TW_SAFEZONE;
}

// El rango sobre cada jugador y, en el seleccionado, su nivel, resets y VIP (como CustomRankUser).
void RenderRankUsers()
{
    const RankSwitches& switches = Switches();
    for (const auto& [key, user] : g_rankUsers)
    {
        const int index = FindCharacterIndex(key);
        if (index >= MAX_CHARACTERS_CLIENT)
            continue;

        CHARACTER* c = &CharactersClient[index];
        OBJECT* o = &c->Object;
        if (o->Type != MODEL_PLAYER || !IsVisible(o, c))
            continue;

        if (switches.safeZone && !InSafeZone(o))
            continue;

        int x = 0;
        int y = 0;
        if (!ToScreen(o, 150.f, x, y))
            continue;

        const bool selected = index == SelectedCharacter;
        if (!user.title.empty() && (switches.overHead || (selected && switches.type == 4)))
            RenderLine(x, y - 12, user.title, 255, 210, 60);

        if (selected && switches.type != 4 && (!switches.needAlt || mu::ui::window::IsRepeat(VK_MENU)))
        {
            std::string info = "Lvl " + std::to_string(user.level);
            if (switches.showReset)
                info += "  Res " + std::to_string(user.resets);
            if (switches.showMasterReset && user.masterResets > 0)
                info += "  MRes " + std::to_string(user.masterResets);
            if (!switches.overHead && !user.title.empty())
                info = user.title + "  " + info;
            RenderLine(x, y - 24, info, 255, 255, 255);
            if (!user.vip.empty())
                RenderLine(x, y - 36, user.vip, 120, 200, 255);
        }
    }
}

// El medidor de dano del monstruo que se esta atacando (DamageMeter del servidor).
void RenderDps()
{
    if (!g_dpsLoaded)
    {
        g_dpsLoaded = true;
        g_dpsEnabled = MuEmu::GetConfigInt("DpsMeter", 1) != 0;
    }
    if (!g_dpsEnabled || !g_dps.received || std::chrono::steady_clock::now() - g_dps.updated > DpsTimeout)
        return;

    constexpr float Left = 4.f;
    constexpr float Top = 120.f;
    constexpr float Width = 130.f;
    constexpr float LineHeight = 11.f;
    const float height = LineHeight * (2.f + static_cast<float>(g_dps.users.size())) + 4.f;

    EnableAlphaBlend();
    RenderColorQuadARGB(Left, Top, Width, height, 0x99000000u);
    if (g_dps.maxLife > 0)
    {
        const float rate = static_cast<float>(g_dps.life) / static_cast<float>(g_dps.maxLife);
        RenderColorQuadARGB(Left + 2.f, Top + LineHeight + 2.f, (Width - 4.f) * (rate < 0.f ? 0.f : rate), 3.f, 0xCCFF0000u);
    }
    DisableAlphaBlend();

    g_pRenderText->SetBgColor(0, 0, 0, 0);
    g_pRenderText->SetTextColor(255, 210, 60, 255);
    const std::wstring title = StringUtils::NarrowToWide(g_dps.monster + (g_dps.dead ? " (muerto)" : ""));
    g_pRenderText->RenderText(static_cast<int>(Left + 3.f), static_cast<int>(Top + 2.f), title.c_str());

    float y = Top + LineHeight * 2.f;
    for (std::size_t n = 0; n < g_dps.users.size(); ++n)
    {
        const DpsUser& user = g_dps.users[n];
        const std::wstring line = StringUtils::NarrowToWide(std::to_string(n + 1) + ". " + user.name + "  " +
                                                            MuEmu::Ui::FormatNumber(user.damage) + " (" +
                                                            std::to_string(user.percent) + "%)");
        g_pRenderText->SetTextColor(255, 255, 255, 255);
        g_pRenderText->RenderText(static_cast<int>(Left + 3.f), static_cast<int>(y), line.c_str());
        y += LineHeight;
    }
}

// C1 D3 40: nombre del personaje (11) y un indice (int, 0), como GetInfoCharTop de Main.dll.
void RequestCharacter(int row)
{
    if (row < 0 || row >= static_cast<int>(g_ranking.entries.size()))
        return;

    const std::string& name = g_ranking.entries[static_cast<std::size_t>(row)].rawName;
    std::vector<uint8_t> payload(11 + 1 + 4, 0); // nombre, relleno y el indice en 16
    std::memcpy(payload.data(), name.c_str(), std::min<std::size_t>(name.size(), 10));
    MuEmu::Ui::Send(0xD3, 0x40, payload);
}

// Item de 12 bytes del servidor: el nombre con su nivel y opciones, o vacio.
std::string DescribeItemBytes(const uint8_t* item)
{
    bool empty = true;
    for (int n = 0; n < 6; ++n)
        empty = empty && item[n] == 0xFF;
    if (empty)
        return std::string();

    const int type = item[0] | ((item[3] & 0x80) << 1) | ((item[5] & 0xF0) << 5);
    const int level = (item[1] >> 3) & 15;
    const int option = (item[1] & 3) + ((item[3] & 0x40) ? 4 : 0);
    const bool skill = (item[1] & 0x80) != 0;
    const bool luck = (item[1] & 0x04) != 0;
    const bool excellent = (item[3] & 0x3F) != 0;
    const bool ancient = (item[4] & 0x0F) != 0;

    std::string text;
    if (excellent)
        text += "Exc ";
    if (ancient)
        text += "Anc ";
    if (type >= 0 && type < MAX_ITEM && ItemAttribute[type].Name[0] != 0)
        text += StringUtils::WideToNarrow(ItemAttribute[type].Name);
    else
        text += "Item " + std::to_string(type / MAX_ITEM_INDEX) + "," + std::to_string(type % MAX_ITEM_INDEX);
    if (level > 0)
        text += " +" + std::to_string(level);
    if (skill)
        text += " +Skill";
    if (luck)
        text += " +Luck";
    if (option > 0)
        text += " +" + std::to_string(option * 4);
    return text;
}

// C2 D3 40: nombre (5), guild (16), marca (27), resets (64), nivel (68), master (72) y 12 items de 16 bytes (76).
void ShowCharacter(const uint8_t* buffer, int size)
{
    MuEmu::Panel::Content content;
    content.title = "Equipo de " + MuEmu::Ui::ReadText(buffer, 5, 11);
    content.wide = true;

    const std::string guild = MuEmu::Ui::ReadText(buffer, 16, 11);
    content.lines = { "Nivel " + std::to_string(MuEmu::Ui::ReadInt(buffer, 68)) + " - Master " +
                          std::to_string(MuEmu::Ui::ReadInt(buffer, 72)) + " - Resets " +
                          std::to_string(MuEmu::Ui::ReadInt(buffer, 64)),
                      guild.empty() ? "Sin guild" : "Guild: " + guild };

    content.header = { "Lugar", "Item" };
    for (int n = 0; n < 12 && 76 + (n + 1) * 16 <= size; ++n)
    {
        const std::string item = DescribeItemBytes(buffer + 76 + n * 16);
        MuEmu::Panel::Row row;
        row.cells = { SlotNames[n], item.empty() ? "-" : item };
        if (item.rfind("Exc", 0) == 0 || item.rfind("Anc", 0) == 0)
            row.color = item.rfind("Anc", 0) == 0 || item.find(" Anc ") != std::string::npos ? "#60e060" : "#80ff80";
        if (item.empty())
            row.color = "#808080";
        content.rows.push_back(std::move(row));
    }
    content.buttons = { { "Volver al ranking", ActionBack } };

    MuEmu::Panel::Show(CharacterId, std::move(content), [](int action)
    {
        if (action == ActionBack)
            MuEmu::Ui::Ranking::Open();
    });
}
} // namespace

std::string MuEmu::Ui::Ranking::DescribeItem(const uint8_t* item)
{
    return DescribeItemBytes(item);
}

void MuEmu::Ui::Ranking::Open()
{
    Send(0xF3, 0xE7);
    RequestRanking(g_ranking.type);
    MuEmu::Panel::Show(RankingId, BuildRanking(), [](int action)
    {
        if (action >= ActionViewFirst)
        {
            RequestCharacter(action - ActionViewFirst);
            return;
        }
        if (g_ranking.typeCount <= 1)
            return;
        const int step = action == ActionNext ? 1 : -1;
        RequestRanking((g_ranking.type + step + g_ranking.typeCount) % g_ranking.typeCount);
    });
}

void MuEmu::Ui::Ranking::ToggleDps()
{
    g_dpsLoaded = true;
    g_dpsEnabled = !g_dpsEnabled;
}

bool MuEmu::Ui::Ranking::Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size)
{
    if (head != 0xF3)
        return false;

    switch (sub)
    {
    case 0xE7: // C1: cantidad de rankings (int en 4)
        if (buffer[0] == 0xC1 && size >= 8)
        {
            g_ranking.typeCount = ReadInt(buffer, 4);
            MuEmu::Panel::Update(RankingId, BuildRanking());
        }
        return buffer[0] == 0xC1;

    case 0xE6: // C2: tipo (8), titulo (12), columnas (32, 52), cantidad (72) y entradas de 81 bytes desde 76
    {
        if (buffer[0] != 0xC2)
            return false;
        constexpr int HeaderSize = 76;
        constexpr int EntrySize = 81;
        if (size < HeaderSize)
            return true;

        g_ranking.type = ReadInt(buffer, 8);
        g_ranking.title = ReadText(buffer, 12, 20);
        g_ranking.column1 = ReadText(buffer, 32, 20);
        g_ranking.column2 = ReadText(buffer, 52, 20);
        g_ranking.entries.clear();
        const int count = ReadInt(buffer, 72);
        for (int n = 0; n < count && HeaderSize + (n + 1) * EntrySize <= size; ++n)
        {
            const int at = HeaderSize + n * EntrySize;
            RankingEntry entry;
            entry.name = ReadText(buffer, at, 20);
            entry.rawName.assign(reinterpret_cast<const char*>(buffer + at), strnlen(reinterpret_cast<const char*>(buffer + at), 20));
            entry.value = ReadText(buffer, at + 20, 60);
            entry.online = buffer[at + 80] != 0;
            g_ranking.entries.push_back(std::move(entry));
        }
        g_ranking.received = true;
        MuEmu::Panel::Update(RankingId, BuildRanking());
        return true;
    }

    case 0xE5: // C1: titulo (4), indice (24), nivel (28), resets (32), master resets (36), VIP (76)
        if (buffer[0] != 0xC1)
            return false;
        if (size >= 96)
        {
            RankUser user;
            user.title = ReadText(buffer, 4, 20);
            user.level = ReadInt(buffer, 28);
            user.resets = ReadInt(buffer, 32);
            user.masterResets = ReadInt(buffer, 36);
            user.vip = ReadText(buffer, 76, 20);
            g_rankUsers[ReadInt(buffer, 24)] = std::move(user);
        }
        return true;

    case 0xF7: // C1: monstruo (8), vida (40, 44), muerto (48), cantidad (49) y 5 jugadores de 20 bytes desde 52
        if (buffer[0] != 0xC1)
            return false;
        if (size >= 52)
        {
            g_dps.monster = ReadText(buffer, 8, 32);
            g_dps.life = ReadInt(buffer, 40);
            g_dps.maxLife = ReadInt(buffer, 44);
            g_dps.dead = buffer[48] != 0;
            g_dps.users.clear();
            const int count = buffer[49];
            for (int n = 0; n < count && n < 5 && 52 + (n + 1) * 20 <= size; ++n)
            {
                const int at = 52 + n * 20;
                DpsUser user;
                user.name = ReadText(buffer, at, 11);
                user.damage = ReadInt(buffer, at + 12);
                user.percent = buffer[at + 16];
                g_dps.users.push_back(std::move(user));
            }
            g_dps.updated = std::chrono::steady_clock::now();
            g_dps.received = true;
        }
        return true;

    default:
        return false;
    }
}

bool MuEmu::Ui::Ranking::ReceiveCharacter(uint8_t head, uint8_t sub, const uint8_t* buffer, int size)
{
    if (head != 0xD3 || sub != 0x40 || buffer[0] != 0xC2)
        return false;
    if (size >= 76 + 12 * 16)
        ShowCharacter(buffer, size);
    return true;
}

void MuEmu::Ui::Ranking::Frame()
{
    RenderRankUsers();
    RenderDps();
}
