#include "stdafx.h"
#include "MuEmu/MuEmuUiFeatures.h"

#include "MuEmu/MuEmuPanel.h"
#include "MuEmu/MuEmuUi.h"
#include "Character/CharacterManager.h"
#include "Engine/Object/ZzzCharacter.h"

#include <cstdlib>
#include <vector>

// Ventanas de las tiendas y sistemas custom de Main.dll que se manejan con listas del servidor:
// - Ruleta (Lucky Wheel V2, VongQuay): C1 D3 8C abre (lista C2 D3 8A), C1 D3 8B pide los premios de una ruleta
//   (C2 D3 8B), C1 D3 8A gira y el resultado llega en C1 D3 8C (StartRoll 1 = girando, 0 = premio IndexWin).
// - Tienda XShop (SPK_xShop): C1 D5 13 pide las tiendas (C2 D5 11), C1 D5 12 los items de una (C2 D5 12) y
//   C1 D5 11 compra (tienda, posicion).
// - Alquiler de banderas (ThueFlag): C1 FF 03 (tipo, valor): 0 pide la bandera n, 1 la compra o la usa, 3 la
//   muestra u oculta. El servidor contesta cada una con C1 FB 18 (bandera actual y datos de la pedida).
// Las posiciones salen de las estructuras del GameServer (empaquetado por defecto de MSVC).
namespace
{
using MuEmu::Ui::ReadInt;
using MuEmu::Ui::ReadText;

std::vector<uint8_t> Ints(std::initializer_list<int32_t> values)
{
    std::vector<uint8_t> payload;
    for (int32_t value : values)
        MuEmu::Ui::WriteInt(payload, value);
    return payload;
}

struct NamedEntry
{
    int index = 0;
    std::string name;
};

// Lista de (indice int, nombre de 90) de 96 bytes desde 6: la usan las ruletas y las tiendas XShop.
std::vector<NamedEntry> ReadNamedList(const uint8_t* buffer, int size)
{
    std::vector<NamedEntry> entries;
    const int count = size > 5 ? buffer[5] : 0;
    for (int n = 0; n < count && 6 + (n + 1) * 96 <= size; ++n)
    {
        NamedEntry entry;
        entry.index = ReadInt(buffer, 6 + n * 96);
        entry.name = ReadText(buffer, 6 + n * 96 + 4, 90);
        entries.push_back(std::move(entry));
    }
    return entries;
}

std::string ItemName(const uint8_t* item)
{
    const std::string name = MuEmu::Ui::Ranking::DescribeItem(item);
    return name.empty() ? "-" : name;
}

std::string ItemColor(const std::string& name)
{
    if (name.rfind("Exc", 0) == 0)
        return "#80ff80";
    if (name.rfind("Anc", 0) == 0)
        return "#60e060";
    return std::string();
}

// ------------------------------------------------------------------------------------------------ Ruleta
constexpr const char* WheelId = "lucky_wheel";

enum WheelAction
{
    WheelBack = 1,
    WheelSpin,
    WheelViewFirst = 100, // + fila de la lista de ruletas
};

std::vector<NamedEntry> g_wheels;
struct
{
    int wheel = 0;
    std::string name;
    int coin[3] = {};
    std::vector<std::string> prizes;
    std::string status;
    int winner = -1;
} g_wheel;

void ShowWheelList()
{
    MuEmu::Panel::Content content;
    content.title = "Lucky Wheel";
    if (g_wheels.empty())
        content.lines = { "El servidor no tiene ruletas." };
    content.header = { "Ruleta" };
    for (std::size_t n = 0; n < g_wheels.size(); ++n)
    {
        MuEmu::Panel::Row row;
        row.cells = { g_wheels[n].name };
        row.buttons = { { "Ver", WheelViewFirst + static_cast<int>(n) } };
        content.rows.push_back(std::move(row));
    }
    MuEmu::Panel::Show(WheelId, std::move(content), [](int action)
    {
        const int n = action - WheelViewFirst;
        if (n < 0 || n >= static_cast<int>(g_wheels.size()))
            return;
        g_wheel.wheel = g_wheels[static_cast<std::size_t>(n)].index;
        g_wheel.name = g_wheels[static_cast<std::size_t>(n)].name;
        MuEmu::Ui::Send(0xD3, 0x8B, Ints({ g_wheel.wheel }));
    });
}

void ShowWheel()
{
    static const char* Coins[3] = { "WCoinC", "WCoinP", "Goblin" };
    std::string price;
    for (int n = 0; n < 3; ++n)
    {
        if (g_wheel.coin[n] <= 0)
            continue;
        price += (price.empty() ? "" : " + ") + MuEmu::Ui::FormatNumber(g_wheel.coin[n]) + " " + Coins[n];
    }

    MuEmu::Panel::Content content;
    content.title = "Lucky Wheel - " + g_wheel.name;
    content.wide = true;
    content.lines = { "Cada giro cuesta " + (price.empty() ? std::string("nada") : price) + ". El premio llega al inventario." };
    if (!g_wheel.status.empty())
        content.lines.push_back(g_wheel.status);
    content.header = { "Premio" };
    for (std::size_t n = 0; n < g_wheel.prizes.size(); ++n)
    {
        MuEmu::Panel::Row row;
        row.cells = { g_wheel.prizes[n] };
        row.color = static_cast<int>(n) == g_wheel.winner ? "#ffd040" : ItemColor(g_wheel.prizes[n]);
        content.rows.push_back(std::move(row));
    }
    content.buttons = { { "Girar", WheelSpin }, { "Volver", WheelBack } };
    MuEmu::Panel::Show(WheelId, std::move(content), [](int action)
    {
        if (action == WheelBack)
        {
            ShowWheelList();
        }
        else if (action == WheelSpin)
        {
            MuEmu::Ui::Send(0xD3, 0x8A, Ints({ g_wheel.wheel, 1 }));
        }
    });
}

// ------------------------------------------------------------------------------------------------ XShop
constexpr const char* XShopId = "xshop";

enum XShopAction
{
    XShopBack = 1,
    XShopConfirm,
    XShopCancel,
    XShopOpenFirst = 100, // + fila de la lista de tiendas
    XShopBuyFirst = 1000, // + fila de los items
};

struct ShopItem
{
    int priceType = 0;
    int price = 0;
    std::string name;
    bool forMyClass = true;
};

std::vector<NamedEntry> g_shops;
int g_shop = 0;
std::string g_shopName;
std::vector<ShopItem> g_shopItems;
int g_shopPending = -1;

std::string ShopPrice(const ShopItem& item)
{
    static const char* Types[4] = { "Zen", "WCoinC", "WCoinP", "Goblin" };
    const char* type = item.priceType >= 0 && item.priceType < 4 ? Types[item.priceType] : "?";
    return MuEmu::Ui::FormatNumber(item.price) + " " + type;
}

void ShowShopList()
{
    MuEmu::Panel::Content content;
    content.title = "XShop";
    if (g_shops.empty())
        content.lines = { "El servidor no tiene tiendas." };
    content.header = { "Tienda" };
    for (std::size_t n = 0; n < g_shops.size(); ++n)
    {
        MuEmu::Panel::Row row;
        row.cells = { g_shops[n].name };
        row.buttons = { { "Abrir", XShopOpenFirst + static_cast<int>(n) } };
        content.rows.push_back(std::move(row));
    }
    MuEmu::Panel::Show(XShopId, std::move(content), [](int action)
    {
        const int n = action - XShopOpenFirst;
        if (n < 0 || n >= static_cast<int>(g_shops.size()))
            return;
        g_shop = g_shops[static_cast<std::size_t>(n)].index;
        g_shopName = g_shops[static_cast<std::size_t>(n)].name;
        MuEmu::Ui::Send(0xD5, 0x12, Ints({ g_shop }));
    });
}

void ShowShop();

void ConfirmShopBuy(int position)
{
    if (position < 0 || position >= static_cast<int>(g_shopItems.size()))
        return;
    g_shopPending = position;
    const ShopItem& item = g_shopItems[static_cast<std::size_t>(position)];

    MuEmu::Panel::Content content;
    content.title = "Confirmar compra";
    content.lines = { "Comprar " + item.name + "?", "Precio: " + ShopPrice(item) };
    if (!item.forMyClass)
        content.lines.push_back("Este item no es para tu clase.");
    content.buttons = { { "Comprar", XShopConfirm }, { "Cancelar", XShopCancel } };
    MuEmu::Panel::Show(XShopId, std::move(content), [](int action)
    {
        if (action == XShopConfirm && g_shopPending >= 0)
            MuEmu::Ui::Send(0xD5, 0x11, Ints({ g_shop, g_shopPending }));
        g_shopPending = -1;
        ShowShop();
    });
}

void ShowShop()
{
    MuEmu::Panel::Content content;
    content.title = "XShop - " + g_shopName;
    content.wide = true;
    if (g_shopItems.empty())
        content.lines = { "Esta tienda no tiene items." };
    content.header = { "Item", "Precio" };
    for (std::size_t n = 0; n < g_shopItems.size(); ++n)
    {
        MuEmu::Panel::Row row;
        row.cells = { g_shopItems[n].name, ShopPrice(g_shopItems[n]) };
        row.color = g_shopItems[n].forMyClass ? ItemColor(g_shopItems[n].name) : "#808080";
        row.buttons = { { "Comprar", XShopBuyFirst + static_cast<int>(n) } };
        content.rows.push_back(std::move(row));
    }
    content.buttons = { { "Volver", XShopBack } };
    MuEmu::Panel::Show(XShopId, std::move(content), [](int action)
    {
        if (action == XShopBack)
            ShowShopList();
        else if (action >= XShopBuyFirst)
            ConfirmShopBuy(action - XShopBuyFirst);
    });
}

// ------------------------------------------------------------------------------------------------ Banderas
constexpr const char* FlagId = "flag_rent";

enum FlagAction
{
    FlagToggle = 1,
    FlagUseFirst = 100, // + numero de bandera
};

struct FlagInfo
{
    int skin = 0;
    int price = 0;
    int attack = 0;
    int defense = 0;
    int life = 0;
    int mana = 0;
};

std::vector<FlagInfo> g_flags;
int g_flagCurrent = 0; // bandera puesta (negativa: oculta)
int g_flagUnlocked = -1; // la ultima comprada (las anteriores quedan disponibles)
bool g_flagLoading = false;

void ShowFlags()
{
    MuEmu::Panel::Content content;
    content.title = "Alquiler de banderas";
    content.wide = true;
    std::string current = "Sin bandera";
    for (std::size_t n = 0; n < g_flags.size(); ++n)
    {
        if (g_flags[n].skin == std::abs(g_flagCurrent) && g_flagCurrent != 0)
            current = "Bandera " + std::to_string(n + 1) + (g_flagCurrent < 0 ? " (oculta)" : "");
    }
    content.lines = { "Puesta: " + current, "Se compran en orden; las ya compradas se pueden volver a usar gratis." };
    if (g_flagLoading)
        content.lines.push_back("Cargando banderas...");

    content.header = { "Bandera", "Ataque", "Defensa", "Vida", "Mana", "Precio" };
    for (std::size_t n = 0; n < g_flags.size(); ++n)
    {
        const FlagInfo& flag = g_flags[n];
        const int number = static_cast<int>(n);
        const bool bought = number <= g_flagUnlocked;
        const bool worn = g_flagCurrent != 0 && flag.skin == std::abs(g_flagCurrent);

        MuEmu::Panel::Row row;
        row.cells = { "Bandera " + std::to_string(n + 1), std::to_string(flag.attack), std::to_string(flag.defense),
                      std::to_string(flag.life), std::to_string(flag.mana),
                      bought ? "Comprada" : MuEmu::Ui::FormatNumber(flag.price) + " WCoinC" };
        row.color = worn ? "#ffd040" : (bought ? "#80ff80" : (number == g_flagUnlocked + 1 ? "" : "#808080"));
        if (!worn && (bought || number == g_flagUnlocked + 1))
            row.buttons = { { bought ? "Usar" : "Comprar", FlagUseFirst + number } };
        content.rows.push_back(std::move(row));
    }
    if (g_flagCurrent != 0)
        content.buttons = { { g_flagCurrent < 0 ? "Mostrar bandera" : "Ocultar bandera", FlagToggle } };

    MuEmu::Panel::Show(FlagId, std::move(content), [](int action)
    {
        if (action == FlagToggle)
        {
            MuEmu::Ui::Send(0xFF, 0x03, { 3, 0, 0, 0, 1, 0, 0, 0 });
        }
        else if (action >= FlagUseFirst)
        {
            std::vector<uint8_t> payload = { 1, 0, 0, 0 };
            MuEmu::Ui::WriteInt(payload, action - FlagUseFirst);
            MuEmu::Ui::Send(0xFF, 0x03, payload);
        }
    });
}

void RequestFlag(int number)
{
    std::vector<uint8_t> payload = { 0, 0, 0, 0 }; // Type (1) y relleno hasta el int Value
    MuEmu::Ui::WriteInt(payload, number);
    MuEmu::Ui::Send(0xFF, 0x03, payload);
}
} // namespace

void MuEmu::Ui::Wheel::Open()
{
    MuEmu::Ui::Send(0xD3, 0x8C, Ints({ 1 }));
}

bool MuEmu::Ui::Wheel::Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size)
{
    if (head != 0xD3 || (sub != 0x8A && sub != 0x8B && sub != 0x8C))
        return false;

    if (sub == 0x8A && buffer[0] == 0xC2)
    {
        g_wheels = ReadNamedList(buffer, size);
        ShowWheelList();
    }
    else if (sub == 0x8B && buffer[0] == 0xC2 && size >= 28)
    {
        // Cabecera de 28: cantidad (5), item pedido (8), cantidad pedida (12), WCoinC (16), WCoinP (20), Goblin (24).
        // Premios de 24 bytes: indice (short, 4), durabilidad (6), item de 12 (7) y tiempo (int, 20).
        g_wheel.coin[0] = ReadInt(buffer, 16);
        g_wheel.coin[1] = ReadInt(buffer, 20);
        g_wheel.coin[2] = ReadInt(buffer, 24);
        g_wheel.prizes.clear();
        g_wheel.status.clear();
        g_wheel.winner = -1;
        const int count = buffer[5];
        for (int n = 0; n < count && 28 + (n + 1) * 24 <= size; ++n)
            g_wheel.prizes.push_back(ItemName(buffer + 28 + n * 24 + 7));
        ShowWheel();
    }
    else if (sub == 0x8C && buffer[0] == 0xC1 && size >= 12)
    {
        const int startRoll = ReadInt(buffer, 4);
        const int winner = ReadInt(buffer, 8);
        if (startRoll == 1)
        {
            g_wheel.winner = -1;
            g_wheel.status = "Girando...";
        }
        else if (winner >= 0 && winner < static_cast<int>(g_wheel.prizes.size()))
        {
            g_wheel.winner = winner;
            g_wheel.status = "Premio: " + g_wheel.prizes[static_cast<std::size_t>(winner)];
        }
        if (MuEmu::Panel::IsShown(WheelId))
            ShowWheel();
    }
    return true;
}

void MuEmu::Ui::XShop::Open()
{
    MuEmu::Ui::Send(0xD5, 0x13, Ints({ 1 }));
}

bool MuEmu::Ui::XShop::Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size)
{
    if (head != 0xD5 || buffer[0] != 0xC2 || (sub != 0x11 && sub != 0x12))
        return false;

    if (sub == 0x11)
    {
        g_shops = ReadNamedList(buffer, size);
        ShowShopList();
        return true;
    }

    // Items de 60 bytes desde 6: tipo de precio (4), precio (8), clases (7 int, 12), indice (short, 40),
    // tipo (42), durabilidad (43), item de 12 (44) y tiempo (int, 56).
    const int myClass = Hero != nullptr ? gCharacterManager.GetBaseClass(Hero->Class) : -1;
    g_shopItems.clear();
    const int count = size > 5 ? buffer[5] : 0;
    for (int n = 0; n < count && 6 + (n + 1) * 60 <= size; ++n)
    {
        const uint8_t* entry = buffer + 6 + n * 60;
        ShopItem item;
        item.priceType = ReadInt(entry, 4);
        item.price = ReadInt(entry, 8);
        item.name = ItemName(entry + 44);
        if (myClass >= 0 && myClass < 7)
            item.forMyClass = ReadInt(entry, 12 + myClass * 4) != 0;
        g_shopItems.push_back(std::move(item));
    }
    ShowShop();
    return true;
}

void MuEmu::Ui::Flag::Open()
{
    g_flags.clear();
    g_flagLoading = true;
    RequestFlag(0);
    ShowFlags();
}

bool MuEmu::Ui::Flag::Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size)
{
    if (head != 0xFB || sub != 0x18 || buffer[0] != 0xC1)
        return false;
    if (size < 40)
        return true;

    // Bandera puesta (4) y la pedida (8): id, skin, precio, ataque, defensa, vida, mana y la ultima comprada.
    g_flagCurrent = ReadInt(buffer, 4);
    const int id = ReadInt(buffer, 8);
    if (id < 0)
    {
        g_flagLoading = false; // no hay mas banderas
    }
    else
    {
        FlagInfo flag;
        flag.skin = ReadInt(buffer, 12);
        flag.price = ReadInt(buffer, 16);
        flag.attack = ReadInt(buffer, 20);
        flag.defense = ReadInt(buffer, 24);
        flag.life = ReadInt(buffer, 28);
        flag.mana = ReadInt(buffer, 32);
        g_flagUnlocked = ReadInt(buffer, 36);
        if (id >= static_cast<int>(g_flags.size()))
            g_flags.resize(static_cast<std::size_t>(id) + 1);
        g_flags[static_cast<std::size_t>(id)] = flag;

        // Mientras se abre la ventana se piden todas, una tras otra, hasta que el servidor conteste -1.
        if (g_flagLoading && id < 100)
            RequestFlag(id + 1);
    }
    if (MuEmu::Panel::IsShown(FlagId))
        ShowFlags();
    return true;
}
