#include "stdafx.h"
#include "MuEmu/MuEmuUiFeatures.h"

#include "MuEmu/MuEmuUi.h"
#include "MUHelper/MuHelper.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <vector>

// /offhelper de los servidores MuEmu: el servidor pide la configuracion del MU Helper (C1 FB 3E) y el cliente
// contesta con ella (C1 FB 3D, 248 bytes, la estructura CG_OFFMODE_RESULT del GameServer) para seguir cazando
// sin conexion, como cMuOffHelper::RunningOffHelper de Main.dll.
namespace
{
constexpr int PacketSize = 248;
constexpr int ExtraItems = 12;
constexpr int ExtraItemLength = 16;

void WriteWord(std::vector<uint8_t>& packet, int offset, int value)
{
    const auto word = static_cast<uint16_t>(value);
    packet[offset] = static_cast<uint8_t>(word & 0xFF);
    packet[offset + 1] = static_cast<uint8_t>(word >> 8);
}

void SendOffHelperConfiguration()
{
    using MUHelper::ConfigData;
    const ConfigData config = MUHelper::g_MuHelper.GetConfig();

    std::vector<uint8_t> packet(PacketSize, 0);
    packet[0] = 0xC1;
    packet[1] = PacketSize;
    packet[2] = 0xFB;
    packet[3] = 0x3D;

    WriteWord(packet, 4, config.iHuntingRange);
    // Pociones, curacion (elfa) y drenar vida (summoner): el servidor usa el porcentaje.
    packet[6] = config.bUseHealPotion ? 1 : 0;
    packet[7] = static_cast<uint8_t>(std::clamp(config.iPotionThreshold, 0, 100));
    packet[8] = config.bAutoHeal ? 1 : 0;
    packet[9] = static_cast<uint8_t>(std::clamp(config.iHealThreshold, 0, 100));
    packet[10] = config.bUseDrainLife ? 1 : 0;
    packet[11] = static_cast<uint8_t>(std::clamp(config.iHealThreshold, 0, 100));
    // Ataque
    packet[12] = config.bLongRangeCounterAttack ? 1 : 0;
    packet[13] = config.bReturnToOriginalPosition ? 1 : 0;
    WriteWord(packet, 14, config.iMaxSecondsAway);
    WriteWord(packet, 16, static_cast<int>(config.aiSkill[0]));
    WriteWord(packet, 18, static_cast<int>(config.aiSkill[1]));
    packet[20] = (config.aiSkillCondition[1] & MUHelper::ON_TIMER) ? 1 : 0;
    WriteWord(packet, 22, static_cast<int>(config.aiSkillInterval[1]));
    WriteWord(packet, 24, static_cast<int>(config.aiSkill[2]));
    packet[26] = (config.aiSkillCondition[2] & MUHelper::ON_TIMER) ? 1 : 0;
    WriteWord(packet, 28, static_cast<int>(config.aiSkillInterval[2]));
    packet[30] = config.bUseCombo ? 1 : 0;
    // Party
    packet[31] = config.bSupportParty ? 1 : 0;
    packet[32] = config.bAutoHealParty ? 1 : 0;
    packet[33] = static_cast<uint8_t>(std::clamp(config.iHealPartyThreshold, 0, 100));
    packet[34] = config.bBuffDurationParty ? 1 : 0;
    // Buffs
    packet[35] = config.bBuffDuration ? 1 : 0;
    for (int n = 0; n < 3; ++n)
        WriteWord(packet, 36 + n * 2, static_cast<int>(config.aiBuff[n]));
    // Recoleccion
    WriteWord(packet, 42, config.iObtainingRange);
    packet[44] = config.bRepairItem ? 1 : 0;
    packet[45] = config.bPickAllItems ? 1 : 0;
    packet[46] = config.bPickSelectItems ? 1 : 0;
    packet[47] = config.bPickZen ? 1 : 0;
    packet[48] = config.bPickJewel ? 1 : 0;
    packet[49] = config.bPickExcellent ? 1 : 0;
    packet[50] = config.bPickAncient ? 1 : 0;
    packet[51] = config.bPickExtraItems ? 1 : 0;

    int extraCount = 0;
    for (const std::wstring& item : config.aExtraItems)
    {
        if (extraCount >= ExtraItems)
            break;
        char name[ExtraItemLength] = {};
        const size_t length = std::wcstombs(name, item.c_str(), ExtraItemLength - 1);
        if (length == static_cast<size_t>(-1))
            continue;
        std::memcpy(packet.data() + 56 + extraCount * ExtraItemLength, name, ExtraItemLength - 1);
        ++extraCount;
    }
    packet[52] = static_cast<uint8_t>(extraCount);

    MuEmu::Ui::SendRaw(packet);
    MuEmu::Ui::MarkOfflineDisconnect();
}
} // namespace

bool MuEmu::Ui::OffHelper::Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size)
{
    if (head != 0xFB || sub != 0x3E || buffer[0] != 0xC1)
        return false;

    SendOffHelperConfiguration();
    return true;
}
