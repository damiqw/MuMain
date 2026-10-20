#include "stdafx.h"
#include "MuEmu/MuEmuUiFeatures.h"

#include "MuEmu/MuEmuPanel.h"
#include "MuEmu/MuEmuUi.h"
#include "Data/GameData/ItemData/ItemDatabase.h"
#include "Engine/Object/ZzzInventory.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Inventory/ItemMng.h"

#include <algorithm>
#include <cstring>
#include <deque>
#include <memory>
#include <span>
#include <vector>

// Item en el chat (post_item de Main.dll): Ctrl + click sobre un item del inventario abre una ventana con el
// mensaje y lo publica en el chat de todos los servidores.
// - Envio C1 78 (80 bytes, PMSG_POST_ITEM_RECV del GameServer): posicion del item (int, 4), texto antes (35, 8)
//   y despues (35, 43).
// - Recibo C1 78 (104 bytes, PMSG_POST_ITEM_SEND): personaje (11, 3), texto antes (35, 14) y despues (35, 49),
//   indice (short, 84), durabilidad (86), item de 12 bytes (87) y tiempo del item temporal (int, 100).
// Pasar el cursor por la linea del chat muestra el tooltip del item, como Main.dll.
namespace
{
constexpr const char* PostId = "post_item";
constexpr const char* RecentId = "post_item_recent";
constexpr int SendSize = 80;
constexpr int ReceiveSize = 104;
constexpr int TextLength = 35;
constexpr std::size_t MaxRecent = 20;

enum PostAction
{
    ActionPublish = 1,
};

struct Post
{
    std::string name;
    std::string text;
    std::string item;
    int durability = 0;
    // La linea del chat (remitente y texto) y el item para su tooltip.
    std::wstring chatId;
    std::wstring chatText;
    std::shared_ptr<ITEM> object;
};

std::deque<Post> g_recent;
int g_position = -1;
std::shared_ptr<ITEM> g_hovered;

// Item de 12 bytes del servidor -> formato extendido que lee el cliente (igual que SvenConvertItem del
// GameServer), para crear el item con todas sus opciones.
std::shared_ptr<ITEM> CreateItem(const uint8_t* info)
{
    const int index = info[0] + ((info[3] & 0x80) * 2) + ((info[5] & 0xF0) * 32);
    const int group = index / MAX_ITEM_INDEX;
    const int number = index % MAX_ITEM_INDEX;
    const int option = (info[1] & 3) + ((info[3] & 0x40) ? 4 : 0);
    const bool wing = g_ItemDatabase.GetSlot(index) == Data::Items::ItemSlot::Wings;
    const uint8_t excellent = info[3] & (wing ? 0x0F : 0x3F);
    const uint8_t optionType = wing ? ((info[3] >> 4) & 3) : 0;

    int sockets = 0;
    for (int n = 0; n < 5; ++n)
    {
        if (info[7 + n] != 0xFF)
            sockets = n + 1;
    }

    uint8_t flags = 0;
    flags |= option != 0 ? 0x01 : 0;
    flags |= (info[1] & 4) != 0 ? 0x02 : 0;
    flags |= (info[1] & 128) != 0 ? 0x04 : 0;
    flags |= excellent != 0 ? 0x08 : 0;
    flags |= (info[4] & 3) != 0 ? 0x10 : 0;
    flags |= (sockets == 0 && info[6] != 0 && info[6] != 0xFF) ? 0x20 : 0;
    flags |= (info[5] & 0x08) != 0 ? 0x40 : 0;
    flags |= sockets != 0 ? 0x80 : 0;

    std::vector<BYTE> data = { static_cast<BYTE>((group << 4) | ((number >> 8) & 0x0F)),
                               static_cast<BYTE>(number & 0xFF), static_cast<BYTE>((info[1] >> 3) & 0x0F), info[2],
                               flags };
    if (flags & 0x01)
        data.push_back(static_cast<BYTE>((optionType << 4) | (option & 0x0F)));
    if (flags & 0x08)
        data.push_back(excellent);
    if (flags & 0x10)
        data.push_back(static_cast<BYTE>((info[4] & 3) | (((info[4] >> 2) & 3) << 4)));
    if (flags & 0x20)
        data.push_back(info[6]);
    if (flags & 0x80)
    {
        data.push_back(static_cast<BYTE>((((info[6] == 0xFF) ? 0x0F : (info[6] & 0x0F)) << 4) | sockets));
        for (int n = 0; n < sockets; ++n)
            data.push_back(info[7 + n]);
    }

    if (g_pNewItemMng == nullptr)
        return nullptr;
    ITEM* created = g_pNewItemMng->CreateItemExtended(std::span<const BYTE>(data.data(), data.size()));
    if (created == nullptr)
        return nullptr;
    // Copia propia: el administrador de items puede borrar los suyos al cambiar de escena.
    auto copy = std::make_shared<ITEM>(*created);
    g_pNewItemMng->DeleteItem(created);
    return copy;
}

// El servidor guarda los textos en ANSI (la pagina de codigos de Windows).
std::string ToAnsi(const std::string& utf8)
{
    if (utf8.empty())
        return std::string();
    const int wideLength = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(std::max(wideLength, 1)), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wide.data(), wideLength);
    const int length = WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string ansi(static_cast<std::size_t>(std::max(length, 1)), '\0');
    WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, ansi.data(), length, nullptr, nullptr);
    ansi.resize(std::strlen(ansi.c_str()));
    return ansi;
}

std::wstring ToWide(const std::string& utf8)
{
    if (utf8.empty())
        return std::wstring();
    const int length = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(std::max(length, 1)), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wide.data(), length);
    wide.resize(std::wcslen(wide.c_str()));
    return wide;
}

void Publish(int position, const std::string& message)
{
    std::vector<uint8_t> packet(SendSize, 0);
    packet[0] = 0xC1;
    packet[1] = static_cast<uint8_t>(SendSize);
    packet[2] = 0x78;
    std::memcpy(packet.data() + 4, &position, sizeof(position));
    const std::string text = ToAnsi(message);
    std::memcpy(packet.data() + 8, text.c_str(), std::min<std::size_t>(text.size(), TextLength - 1));
    MuEmu::Ui::SendRaw(packet);
}
} // namespace

void MuEmu::Ui::PostItem::Open(int position)
{
    g_position = position;

    MuEmu::Panel::Content content;
    content.title = "Mostrar item en el chat";
    content.lines = { "El item se publica en el chat de todos los jugadores.",
                      "El servidor puede cobrar o pedir un nivel minimo." };
    content.inputLabel = "Mensaje (opcional)";
    content.buttons = { { "Publicar", ActionPublish } };

    MuEmu::Panel::Show(PostId, std::move(content), [](int action)
    {
        if (action != ActionPublish || g_position < 0)
            return;
        Publish(g_position, MuEmu::Panel::GetInput());
        g_position = -1;
        MuEmu::Panel::Hide();
    });
}

void MuEmu::Ui::PostItem::OpenRecent()
{
    MuEmu::Panel::Content content;
    content.title = "Items publicados";
    content.wide = true;
    if (g_recent.empty())
        content.lines = { "Todavia no se publico ningun item. Ctrl + click sobre un item del inventario para publicarlo." };

    content.header = { "Personaje", "Item", "Mensaje" };
    for (const Post& post : g_recent)
    {
        MuEmu::Panel::Row row;
        row.cells = { post.name, post.item + " (" + std::to_string(post.durability) + ")", post.text };
        if (post.item.rfind("Exc", 0) == 0)
            row.color = "#80ff80";
        else if (post.item.rfind("Anc", 0) == 0)
            row.color = "#60e060";
        content.rows.push_back(std::move(row));
    }
    MuEmu::Panel::Show(RecentId, std::move(content), [](int) {});
}

bool MuEmu::Ui::PostItem::Receive(uint8_t head, uint8_t, const uint8_t* buffer, int size)
{
    if (head != 0x78 || buffer[0] != 0xC1)
        return false;
    if (size < ReceiveSize)
        return true;

    Post post;
    post.name = ReadText(buffer, 3, 11);
    const std::string before = ReadText(buffer, 14, TextLength);
    const std::string after = ReadText(buffer, 49, TextLength);
    post.text = before + (before.empty() || after.empty() ? "" : " ") + after;
    post.item = Ranking::DescribeItem(buffer + 87);
    post.durability = buffer[86];

    std::string line = before;
    if (!line.empty())
        line += " ";
    line += "[" + post.item + "]";
    if (!after.empty())
        line += " " + after;
    post.chatId = ToWide("[Item] " + post.name);
    post.chatText = ToWide(line);
    post.object = CreateItem(buffer + 87);
    if (g_pChatListBox != nullptr)
        g_pChatListBox->AddText(post.chatId, post.chatText, mu::ui::window::TYPE_GM_MESSAGE);

    g_recent.push_front(std::move(post));
    if (g_recent.size() > MaxRecent)
        g_recent.pop_back();
    if (MuEmu::Panel::IsShown(RecentId))
        OpenRecent();
    return true;
}

void MuEmu::Ui::PostItem::SetHoveredLine(const std::wstring* id, const std::wstring* text)
{
    g_hovered.reset();
    if (id == nullptr || text == nullptr || text->empty())
        return;
    // Una linea larga se parte en dos: la primera conserva el remitente y el comienzo del texto.
    for (const Post& post : g_recent)
    {
        if (post.object && post.chatId == *id && post.chatText.compare(0, text->size(), *text) == 0)
        {
            g_hovered = post.object;
            return;
        }
    }
}

void MuEmu::Ui::PostItem::Frame()
{
    if (g_hovered)
        RenderItemInfoAtPx(g_fWindowMouseX, g_fWindowMouseY - 20.f, g_hovered.get(), false);
}
