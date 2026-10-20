#include "stdafx.h"
#include "MuEmu/MuEmuUi.h"

#include "MuEmu/MuEmuMenuButton.h"
#include "MuEmu/MuEmuPanel.h"
#include "MuEmu/MuEmuUiFeatures.h"

#include "Network/Server/WSclient.h"
#include "Core/Utilities/Log/MuLogger.h"
#include "UI/Core/WindowCommon.h"

#include <cstdio>

namespace
{
// El boton Menu de la pantalla (como el menu de Main.dll).
enum MenuAction
{
    MenuEvents = 1,
    MenuCommands,
    MenuRanking,
    MenuJewelBank,
    MenuDaily,
    MenuInvasion,
    MenuWarehouse,
    MenuDps,
};

// La N: la party, como en Main.dll.
enum PartyAction
{
    PartyPublish = 1,
    PartyRemove,
    PartySearch,
};

constexpr const char* MenuId = "menu";
constexpr const char* PartyMenuId = "party_menu";

void Toggle(const char* id, void (*open)())
{
    if (MuEmu::Panel::IsShown(id))
        MuEmu::Panel::Hide();
    else
        open();
}

bool IsValidUtf8(const std::string& text)
{
    for (std::size_t i = 0; i < text.size();)
    {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        const int extra = c < 0x80 ? 0 : (c >> 5) == 0x6 ? 1 : (c >> 4) == 0xE ? 2 : (c >> 3) == 0x1E ? 3 : -1;
        if (extra < 0 || (extra > 0 && i + static_cast<std::size_t>(extra) >= text.size()))
            return false;
        for (int n = 1; n <= extra; ++n)
        {
            if ((static_cast<unsigned char>(text[i + n]) >> 6) != 0x2)
                return false;
        }
        i += extra + 1;
    }
    return true;
}
} // namespace

bool MuEmu::Ui::Receive(const uint8_t* buffer, int size)
{
    if (buffer == nullptr || size < 4)
        return false;

    // C1/C3: cabecera en [2] y subcodigo en [3]; C2/C4: en [3] y [4].
    const bool shortHeader = (buffer[0] % 2) == 1;
    const uint8_t head = shortHeader ? buffer[2] : buffer[3];
    const uint8_t sub = shortHeader ? buffer[3] : (size > 4 ? buffer[4] : 0);

    return Events::Receive(head, sub, buffer, size) || Bank::Receive(head, sub, buffer, size) ||
           Ranking::Receive(head, sub, buffer, size) || Rewards::Receive(head, sub, buffer, size) ||
           GuildRewards::Receive(head, sub, buffer, size);
}

bool MuEmu::Ui::HandleKeys()
{
    // Las teclas de Main.dll: N party, H eventos y J banco de joyas.
    if (mu::ui::window::IsPress('N'))
    {
        mu::log::Get("ui")->info("[MuEmu] N: menu de party");
        Toggle(PartyMenuId, ShowPartyMenu);
        return true;
    }
    if (mu::ui::window::IsPress('H'))
    {
        mu::log::Get("ui")->info("[MuEmu] H: horarios de eventos");
        Toggle("events", Events::Open);
        return true;
    }
    if (mu::ui::window::IsPress('J'))
    {
        mu::log::Get("ui")->info("[MuEmu] J: banco de joyas");
        Toggle("jewel_bank", Bank::OpenJewelBank);
        return true;
    }
    return false;
}

void MuEmu::Ui::Frame()
{
    MenuButton::Show();
    Events::Frame();
    Ranking::Frame();
    Rewards::Frame();
}

void MuEmu::Ui::ShowMenu()
{
    if (Panel::IsShown(MenuId))
    {
        Panel::Hide();
        return;
    }

    Panel::Content content;
    content.title = "Menu";

    const struct
    {
        const char* label;
        int action;
    } entries[] = {
        { "Eventos (H)", MenuEvents },
        { "Comandos", MenuCommands },
        { "Rankings", MenuRanking },
        { "Banco de joyas (J)", MenuJewelBank },
        { "Recompensa diaria", MenuDaily },
        { "Invasiones", MenuInvasion },
        { "Cambiar baul", MenuWarehouse },
        { "Medidor de dano", MenuDps },
    };
    for (const auto& entry : entries)
        content.buttons.push_back({ entry.label, entry.action });

    Panel::Show(MenuId, std::move(content), [](int action)
    {
        switch (action)
        {
        case MenuEvents: Events::Open(); break;
        case MenuCommands: Events::OpenCommands(); break;
        case MenuRanking: Ranking::Open(); break;
        case MenuJewelBank: Bank::OpenJewelBank(); break;
        case MenuDaily: Rewards::OpenDaily(); break;
        case MenuInvasion: Rewards::OpenInvasion(); break;
        case MenuWarehouse: Bank::OpenWarehouse(); break;
        case MenuDps: Ranking::ToggleDps(); break;
        default: break;
        }
    });
}

void MuEmu::Ui::ShowPartyMenu()
{
    Panel::Content content;
    content.title = "Party";
    content.buttons = { { "Publicar party", PartyPublish }, { "Quitar publicacion", PartyRemove },
                        { "Buscar party", PartySearch } };
    Panel::Show(PartyMenuId, std::move(content), [](int action)
    {
        switch (action)
        {
        case PartyPublish: Rewards::PublishParty(true); break;
        case PartyRemove: Rewards::PublishParty(false); break;
        case PartySearch: Rewards::OpenPartySearch(); break;
        default: break;
        }
    });
}

void MuEmu::Ui::Send(uint8_t head, uint8_t sub, const std::vector<uint8_t>& payload)
{
    if (SocketClient == nullptr)
        return;

    std::vector<uint8_t> packet = { 0xC1, 0, head, sub };
    packet.insert(packet.end(), payload.begin(), payload.end());
    packet[1] = static_cast<uint8_t>(packet.size());
    SocketClient->Send(packet.data(), static_cast<int32_t>(packet.size()));
}

int32_t MuEmu::Ui::ReadInt(const uint8_t* buffer, int offset)
{
    const uint8_t* v = buffer + offset;
    return static_cast<int32_t>(static_cast<uint32_t>(v[0]) | (static_cast<uint32_t>(v[1]) << 8) |
                                (static_cast<uint32_t>(v[2]) << 16) | (static_cast<uint32_t>(v[3]) << 24));
}

std::string MuEmu::Ui::ReadText(const uint8_t* buffer, int offset, int length)
{
    std::string text;
    for (int n = 0; n < length && buffer[offset + n] != 0; ++n)
        text += static_cast<char>(buffer[offset + n]);

    if (IsValidUtf8(text))
        return text;

    // Texto ANSI (Windows-1252, como los nombres con acentos del servidor): a UTF-8.
    std::string utf8;
    for (const char ch : text)
    {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c < 0x80)
        {
            utf8 += ch;
        }
        else
        {
            utf8 += static_cast<char>(0xC0 | (c >> 6));
            utf8 += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    return utf8;
}

void MuEmu::Ui::WriteInt(std::vector<uint8_t>& payload, int32_t value)
{
    const auto v = static_cast<uint32_t>(value);
    payload.push_back(static_cast<uint8_t>(v & 0xFF));
    payload.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    payload.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    payload.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

std::string MuEmu::Ui::FormatNumber(int64_t value)
{
    std::string digits = std::to_string(value < 0 ? -value : value);
    for (int at = static_cast<int>(digits.size()) - 3; at > 0; at -= 3)
        digits.insert(static_cast<std::size_t>(at), ".");
    return value < 0 ? "-" + digits : digits;
}

std::string MuEmu::Ui::FormatTime(int seconds)
{
    if (seconds < 0)
        seconds = 0;

    char text[32];
    const int days = seconds / 86400;
    if (days > 0)
    {
        std::snprintf(text, sizeof(text), "%dd %02dh", days, (seconds / 3600) % 24);
        return text;
    }
    std::snprintf(text, sizeof(text), "%02d:%02d:%02d", seconds / 3600, (seconds / 60) % 60, seconds % 60);
    return text;
}
