#include "stdafx.h"
#include "MuEmu/MuEmuUiFeatures.h"

#include "MuEmu/MuEmuPanel.h"
#include "MuEmu/MuEmuUi.h"
#include "UI/Core/WindowSystem.h"

#include <algorithm>
#include <cstring>
#include <deque>
#include <vector>

// Item en el chat (post_item de Main.dll): Ctrl + click sobre un item del inventario abre una ventana con el
// mensaje y lo publica en el chat de todos los servidores.
// - Envio C1 78 (80 bytes, PMSG_POST_ITEM_RECV del GameServer): posicion del item (int, 4), texto antes (35, 8)
//   y despues (35, 43).
// - Recibo C1 78 (104 bytes, PMSG_POST_ITEM_SEND): personaje (11, 3), texto antes (35, 14) y despues (35, 49),
//   indice (short, 84), durabilidad (86), item de 12 bytes (87) y tiempo del item temporal (int, 100).
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
};

std::deque<Post> g_recent;
int g_position = -1;

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
    if (g_pChatListBox != nullptr)
        g_pChatListBox->AddText(ToWide("[Item] " + post.name), ToWide(line), mu::ui::window::TYPE_GM_MESSAGE);

    g_recent.push_front(std::move(post));
    if (g_recent.size() > MaxRecent)
        g_recent.pop_back();
    if (MuEmu::Panel::IsShown(RecentId))
        OpenRecent();
    return true;
}
