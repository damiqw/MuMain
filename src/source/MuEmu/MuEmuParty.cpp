#include "stdafx.h"
#include "MuEmu/MuEmuUiFeatures.h"

#include "MuEmu/MuEmuBuff.h"
#include "MuEmu/MuEmuTools.h"
#include "MuEmu/MuEmuUi.h"
#include "Core/Input/KeyState.h"
#include "Core/Utilities/StringUtils.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInventory.h"
#include "GameLogic/Automation/Movement.h"
#include "MUHelper/MuHelper.h"
#include "UI/Core/WindowSystem.h"
#include "World/MapInfra/MapManager.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// Party y minimapa de Main.dll con datos del servidor:
// - Buffs de la party (PartyBuffs): C1 2E con el nombre (11, 3), la cantidad (14) y buffs de 5 bytes desde 15
//   (efecto y tiempo). Se ven en la lista de la party (PartyListWindow).
// - Ayuda de asignacion (PartyAssignHelper): el lider mantiene Ctrl + S y manda C1 FF 38 (accion, x, y); los
//   miembros reciben C1 FB 38, caminan hasta el lider y al soltar prenden el MU Helper. [MuEmu] AssignHelper.
// - Minimapa (MiniMapInfo): al abrir el mapa (Tab) se manda C1 E7 01 y al cerrarlo E7 02. El servidor manda sus
//   lugares (C1 E7 03: indice 4, grupo 5, tipo 6, x 8, y 9 y texto de 31 en 10; grupo 0 = portal) y la party
//   (C1 F3 F9: cantidad 4 y miembros de 14 bytes desde 5: nombre 11, mapa, x, y).
namespace
{
using Clock = std::chrono::steady_clock;

// ------------------------------------------------------------------------------------------------ Buffs de la party
struct PartyBuffs
{
    std::vector<int> effects;
};

std::map<std::string, PartyBuffs> g_partyBuffs;

// Igual que BuildIconDecorator() de BuffStrip.cpp: el tile del atlas de iconos de buffs.
std::string IconDecorator(int buff)
{
    const int buffId = MuEmu::Buff::GetIcon(buff);
    const bool isAtlas1 = buffId < 81;
    const int rawIndex = isAtlas1 ? (buffId - 1) : (buffId - 81);
    const int tileIndex = std::min(std::max(rawIndex, 0), 89);
    return "image(" + std::string(isAtlas1 ? "atlas1-" : "atlas2-") + std::to_string(tileIndex) + ")";
}

// ------------------------------------------------------------------------------------------------ Ayuda de asignacion
struct
{
    bool holding = false;
    Clock::time_point lastSend;
    int lastX = -1;
    int lastY = -1;
} g_leader;

struct
{
    bool following = false;
    int x = 0;
    int y = 0;
    Clock::time_point lastWalk;
} g_member;

bool AssignHelperEnabled()
{
    static const bool enabled = MuEmu::GetConfigInt("AssignHelper", 1) != 0;
    return enabled;
}

bool IsPartyLeader()
{
    return Hero != nullptr && PartyNumber > 1 && wcscmp(Party[0].Name, Hero->ID) == 0;
}

void SendAssign(int action)
{
    MuEmu::Ui::Send(0xFF, 0x38, { static_cast<uint8_t>(action), static_cast<uint8_t>(Hero->PositionX),
                                  static_cast<uint8_t>(Hero->PositionY) });
}

// ------------------------------------------------------------------------------------------------ Minimapa
struct Spot
{
    int group = 0;
    int x = 0;
    int y = 0;
    std::wstring text;
};

struct PartyPosition
{
    std::wstring name;
    int map = 0;
    int x = 0;
    int y = 0;
};

std::vector<Spot> g_spots;
std::vector<PartyPosition> g_partyPositions;
bool g_mapOpen = false;
} // namespace

std::vector<std::string> MuEmu::Ui::PartyInfo::BuffIcons(const std::string& name)
{
    std::vector<std::string> icons;
    const auto found = g_partyBuffs.find(name);
    if (found == g_partyBuffs.end())
        return icons;
    for (int effect : found->second.effects)
    {
        if (icons.size() >= 8)
            break;
        icons.push_back(IconDecorator(effect));
    }
    return icons;
}

void MuEmu::Ui::PartyInfo::Frame()
{
    if (Hero == nullptr || !AssignHelperEnabled())
        return;

    // Lider: Ctrl + S mantenido (sin escribir en el chat).
    const bool typing = g_pChatInputBox != nullptr && g_pChatInputBox->HaveFocus();
    const bool holding = IsPartyLeader() && !typing && Core::Input::IsKeyDown(VK_CONTROL) && Core::Input::IsKeyDown('S');
    const auto now = Clock::now();
    if (holding)
    {
        if (!g_leader.holding || now - g_leader.lastSend >= std::chrono::milliseconds(250) ||
            g_leader.lastX != Hero->PositionX || g_leader.lastY != Hero->PositionY)
        {
            g_leader.holding = true;
            g_leader.lastSend = now;
            g_leader.lastX = Hero->PositionX;
            g_leader.lastY = Hero->PositionY;
            SendAssign(1);
        }
    }
    else if (g_leader.holding)
    {
        g_leader.holding = false;
        SendAssign(0);
    }

    // Miembro: caminar hasta el lider (a 2 casillas).
    if (g_member.following && now - g_member.lastWalk >= std::chrono::milliseconds(300))
    {
        g_member.lastWalk = now;
        const int dx = Hero->PositionX - g_member.x;
        const int dy = Hero->PositionY - g_member.y;
        if (dx * dx + dy * dy > 4)
            GameLogic::Automation::WalkTo(g_member.x, g_member.y);
    }
}

bool MuEmu::Ui::PartyInfo::Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size)
{
    // C1 2E: buffs de un miembro (el subcodigo es el primer byte del nombre).
    if (head == 0x2E && buffer[0] == 0xC1)
    {
        if (size < 15)
            return true;
        const std::string name = ReadText(buffer, 3, 10);
        PartyBuffs& buffs = g_partyBuffs[name];
        buffs.effects.clear();
        const int count = buffer[14];
        for (int n = 0; n < count && 15 + (n + 1) * 5 <= size; ++n)
        {
            const int effect = buffer[15 + n * 5];
            if (effect != 0)
                buffs.effects.push_back(effect);
        }
        return true;
    }

    if (head == 0xFB && sub == 0x38 && buffer[0] == 0xC1)
    {
        if (size < 7 || Hero == nullptr || !AssignHelperEnabled() || IsPartyLeader())
            return true;
        if (buffer[4] == 1)
        {
            g_member.x = buffer[5];
            g_member.y = buffer[6];
            if (!g_member.following && MUHelper::g_MuHelper.IsActive())
                MUHelper::g_MuHelper.TriggerStop();
            g_member.following = true;
        }
        else
        {
            g_member.following = false;
            if (!MUHelper::g_MuHelper.IsActive())
                MUHelper::g_MuHelper.TriggerStart();
        }
        return true;
    }
    return false;
}

void MuEmu::Ui::MiniMapInfo::Frame()
{
    const bool open = g_pNewUISystem != nullptr && g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_MINI_MAP);
    if (open == g_mapOpen)
        return;
    g_mapOpen = open;
    if (open)
        g_spots.clear();
    MuEmu::Ui::SendRaw({ 0xC1, 0x04, 0xE7, static_cast<uint8_t>(open ? 0x01 : 0x02) });
}

bool MuEmu::Ui::MiniMapInfo::Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size)
{
    if (head == 0xE7 && sub == 0x03 && buffer[0] == 0xC1)
    {
        if (size < 41)
            return true;
        if (buffer[4] == 0)
            g_spots.clear();
        Spot spot;
        spot.group = buffer[5];
        spot.x = buffer[8];
        spot.y = buffer[9];
        spot.text = StringUtils::NarrowToWide(ReadText(buffer, 10, 31));
        g_spots.push_back(std::move(spot));
        return true;
    }

    if (head == 0xF3 && sub == 0xF9 && buffer[0] == 0xC1)
    {
        g_partyPositions.clear();
        const int count = size > 4 ? buffer[4] : 0;
        for (int n = 0; n < count && 5 + (n + 1) * 14 <= size; ++n)
        {
            const uint8_t* entry = buffer + 5 + n * 14;
            PartyPosition member;
            member.name = StringUtils::NarrowToWide(ReadText(entry, 0, 11));
            member.map = entry[11];
            member.x = entry[12];
            member.y = entry[13];
            g_partyPositions.push_back(std::move(member));
        }
        return true;
    }
    return false;
}

void MuEmu::Ui::MiniMapInfo::ForEachMarker(const std::function<void(int kind, int x, int y, const std::wstring& name)>& add)
{
    for (const Spot& spot : g_spots)
        add(spot.group == 0 ? 2 : 1, spot.x, spot.y, spot.text);
    for (const PartyPosition& member : g_partyPositions)
    {
        if (Hero != nullptr && member.name == Hero->ID)
            continue;
        if (member.map == gMapManager.WorldActive)
            add(1, member.x, member.y, L"[Party] " + member.name);
    }
}
