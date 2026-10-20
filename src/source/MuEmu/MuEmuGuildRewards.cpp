#include "stdafx.h"
#include "MuEmu/MuEmuUiFeatures.h"

#include "MuEmu/MuEmuPanel.h"
#include "MuEmu/MuEmuUi.h"

#include <cstdio>
#include <cstring>
#include <iterator>
#include <string>
#include <vector>

// Recompensas de guild del servidor (GuildRewards): misiones de la semana (C2 FF 30) y cofre de niveles
// (C2 FF 32). El servidor los manda al hablar con su NPC; la ventana se abre sola.
namespace
{
constexpr const char* QuestsId = "guild_quests";
constexpr const char* VaultId = "guild_vault";

constexpr int MaxMissions = 6;
constexpr int MaxTiers = 10;
constexpr int MaxTopMembers = 10;
constexpr int MaxLevels = 8;

// Acciones de las misiones: 10 * mision + indice de la cantidad.
constexpr int DonateAmounts[] = { 1, 10 };
// Acciones del cofre.
constexpr int ClaimMember = 1;
constexpr int ClaimGuild = 2;
constexpr int ShowVault = 3;
constexpr int ShowQuests = 4;

float ReadFloat(const uint8_t* buffer, int offset)
{
    const int32_t raw = MuEmu::Ui::ReadInt(buffer, offset);
    float value = 0.f;
    std::memcpy(&value, &raw, sizeof(value));
    return value;
}

std::string Percent(float value)
{
    char text[16];
    std::snprintf(text, sizeof(text), "%.1f%%", value);
    return text;
}

std::string Rewards(const std::string& item, int count, int wcoinC, int wcoinP, int goblin)
{
    std::string text;
    if (!item.empty())
        text = (count > 1 ? std::to_string(count) + "x " : "") + item;
    auto add = [&text](int value, const char* name)
    {
        if (value > 0)
            text += (text.empty() ? "" : " + ") + MuEmu::Ui::FormatNumber(value) + " " + name;
    };
    add(wcoinC, "WCoinC");
    add(wcoinP, "WCoinP");
    add(goblin, "Goblin");
    return text;
}

std::vector<uint8_t> g_lastQuests;
std::vector<uint8_t> g_lastVault;

void Donate(int mission, int amount)
{
    // C1 FF 31: mision (byte 4) y cantidad (int en 8).
    std::vector<uint8_t> payload = { static_cast<uint8_t>(mission), 0, 0, 0 };
    MuEmu::Ui::WriteInt(payload, amount);
    MuEmu::Ui::Send(0xFF, 0x31, payload);
}

void Claim(uint8_t type)
{
    MuEmu::Ui::Send(0xFF, 0x33, { type });
}

void ShowQuestsWindow(const uint8_t* buffer, int size)
{
    using MuEmu::Ui::FormatNumber;
    using MuEmu::Ui::ReadInt;
    using MuEmu::Ui::ReadText;

    MuEmu::Panel::Content content;
    content.title = "Guild " + ReadText(buffer, 12, 11) + " - semana " + std::to_string(ReadInt(buffer, 8));
    content.lines = {
        ReadText(buffer, 23, 64),
        "Nivel " + std::to_string(ReadInt(buffer, 88)) + " - puntos " + FormatNumber(ReadInt(buffer, 92)) + " / " +
            FormatNumber(ReadInt(buffer, 96)),
        "Tu aporte: " + Percent(ReadFloat(buffer, 104)) + " - reinicio en " + MuEmu::Ui::FormatTime(ReadInt(buffer, 996)),
    };

    content.header = { "Mision", "Progreso", "Puntos" };
    for (int n = 0; n < MaxMissions; ++n)
    {
        const int target = ReadInt(buffer, 156 + n * 4);
        if (ReadInt(buffer, 108 + n * 4) <= 0 && target <= 0)
            continue;

        const int current = ReadInt(buffer, 180 + n * 4);
        MuEmu::Panel::Row row;
        row.cells = { ReadText(buffer, 228 + n * 128, 128), FormatNumber(current) + "/" + FormatNumber(target),
                      FormatNumber(ReadInt(buffer, 204 + n * 4)) };
        if (current >= target)
        {
            row.color = "#60e060";
        }
        else
        {
            for (int a = 0; a < static_cast<int>(std::size(DonateAmounts)); ++a)
                row.buttons.push_back({ "+" + std::to_string(DonateAmounts[a]), n * 10 + a });
        }
        content.rows.push_back(std::move(row));
    }

    // Premios por aporte y los que mas aportaron.
    const int tiers = ReadInt(buffer, 1000);
    for (int n = 0; n < tiers && n < MaxTiers; ++n)
    {
        const int at = 1004 + n * 68;
        content.lines.push_back("Desde " + std::to_string(ReadInt(buffer, at + 4)) + "%: " +
                                Rewards(ReadText(buffer, at + 24, 32), ReadInt(buffer, at + 20), ReadInt(buffer, at + 56),
                                        ReadInt(buffer, at + 60), ReadInt(buffer, at + 64)));
    }
    const int top = ReadInt(buffer, 1684);
    for (int n = 0; n < top && n < MaxTopMembers; ++n)
    {
        const int at = 1688 + n * 20;
        content.lines.push_back(std::to_string(n + 1) + ". " + ReadText(buffer, at, 11) + " " +
                                FormatNumber(ReadInt(buffer, at + 12)) + " (" + Percent(ReadFloat(buffer, at + 16)) + ")");
    }

    content.buttons = { { "Cofre", ShowVault } };
    MuEmu::Panel::Show(QuestsId, std::move(content), [](int action)
    {
        if (action == ShowVault)
        {
            if (!g_lastVault.empty())
            {
                const std::vector<uint8_t> vault = g_lastVault;
                MuEmu::Ui::GuildRewards::Receive(0xFF, 0x32, vault.data(), static_cast<int>(vault.size()));
            }
            return;
        }
        Donate(action / 10, DonateAmounts[action % 10]);
    });
}

void ShowVaultWindow(const uint8_t* buffer, int size)
{
    using MuEmu::Ui::FormatNumber;
    using MuEmu::Ui::ReadInt;
    using MuEmu::Ui::ReadText;

    MuEmu::Panel::Content content;
    content.title = "Cofre del guild";
    content.lines = {
        "Semana " + std::to_string(ReadInt(buffer, 8)) + ": nivel " + std::to_string(ReadInt(buffer, 12)) + " - puntos " +
            FormatNumber(ReadInt(buffer, 16)) + " - reinicio en " + MuEmu::Ui::FormatTime(ReadInt(buffer, 664)),
    };

    content.header = { "Nivel", "Puntos", "Premio" };
    const int levels = ReadInt(buffer, 20);
    for (int n = 0; n < levels && n < MaxLevels; ++n)
    {
        const int at = 24 + n * 80;
        MuEmu::Panel::Row row;
        row.cells = { std::to_string(ReadInt(buffer, at)), FormatNumber(ReadInt(buffer, at + 4)),
                      Rewards(ReadText(buffer, at + 36, 32), ReadInt(buffer, at + 32), ReadInt(buffer, at + 68),
                              ReadInt(buffer, at + 72), ReadInt(buffer, at + 76)) };
        if (ReadInt(buffer, at) <= ReadInt(buffer, 12))
            row.color = "#60e060";
        content.rows.push_back(std::move(row));
    }

    // Semana anterior: lo que se puede reclamar.
    const bool guildClaimed = buffer[676] != 0;
    const bool isGuildMaster = buffer[688] != 0;
    content.lines.push_back("Semana anterior (" + std::to_string(ReadInt(buffer, 668)) + "): nivel " +
                            std::to_string(ReadInt(buffer, 672)) + ", tu aporte " + Percent(ReadFloat(buffer, 692)) +
                            ", premios desbloqueados " + std::to_string(buffer[700]));
    if (guildClaimed)
        content.lines.push_back("Premio del guild reclamado por " + ReadText(buffer, 677, 11));

    content.buttons = { { "Reclamar mis premios", ClaimMember } };
    if (isGuildMaster && !guildClaimed)
        content.buttons.push_back({ "Reclamar premio del guild", ClaimGuild });
    content.buttons.push_back({ "Misiones", ShowQuests });

    MuEmu::Panel::Show(VaultId, std::move(content), [](int action)
    {
        if (action == ClaimMember)
            Claim(0);
        else if (action == ClaimGuild)
            Claim(1);
        else if (action == ShowQuests && !g_lastQuests.empty())
        {
            const std::vector<uint8_t> quests = g_lastQuests;
            MuEmu::Ui::GuildRewards::Receive(0xFF, 0x30, quests.data(), static_cast<int>(quests.size()));
        }
    });
}
} // namespace

bool MuEmu::Ui::GuildRewards::Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size)
{
    if (head != 0xFF || buffer[0] != 0xC2)
        return false;

    if (sub == 0x30)
    {
        if (size >= 1888)
        {
            g_lastQuests.assign(buffer, buffer + size);
            ShowQuestsWindow(buffer, size);
        }
        return true;
    }
    if (sub == 0x32)
    {
        if (size >= 701)
        {
            g_lastVault.assign(buffer, buffer + size);
            ShowVaultWindow(buffer, size);
        }
        return true;
    }
    return false;
}
