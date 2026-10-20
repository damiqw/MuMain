#include "stdafx.h"
#include "MuEmu/MuEmuUiFeatures.h"

#include "MuEmu/MuEmuPanel.h"
#include "MuEmu/MuEmuUi.h"

#include "Core/Utilities/Log/MuLogger.h"
#include "Engine/Object/ZzzInfomation.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Inventory/InventoryCtrl.h"
#include "UI/Inventory/MyInventory.h"

#include <array>
#include <set>
#include <iterator>
#include <string>

namespace
{
constexpr const char* JewelBankId = "jewel_bank";
constexpr const char* WarehouseId = "warehouse";

// Las 30 joyas del banco, en el orden del servidor (CustomJewelBank de Main.dll).
constexpr int JewelCount = 30;
constexpr std::array<std::array<int, 2>, JewelCount> Jewels = { {
    { 14, 13 }, { 14, 14 }, { 14, 16 }, { 14, 22 }, { 14, 31 }, { 14, 41 }, { 14, 42 }, { 12, 15 }, { 14, 43 },
    { 14, 44 }, { 14, 400 }, { 14, 401 }, { 14, 402 }, { 14, 403 }, { 14, 404 }, { 14, 405 }, { 14, 406 },
    { 14, 407 }, { 14, 408 }, { 14, 415 }, { 14, 409 }, { 14, 410 }, { 14, 411 }, { 14, 412 }, { 14, 413 },
    { 14, 414 }, { 14, 57 }, { 14, 51 }, { 14, 121 }, { 14, 122 },
} };

// Paquetes de 10 de las primeras 10 joyas (grupo 12): valen 10 * (nivel + 1), como en el servidor.
constexpr int BundleCount = 10;
constexpr std::array<int, BundleCount> Bundles = { 30, 31, 136, 137, 138, 139, 140, 141, 142, 143 };

// Cantidades que se pueden retirar, como en Main.dll; 99 es "lo que entre" (hasta el maximo del servidor).
constexpr int WithdrawCounts[] = { 1, 10, 20, 30, 99 };

// Acciones: deposito = slot, retiro = 100 + slot * 10 + indice de la cantidad.
constexpr int WithdrawBase = 100;
constexpr int ActionRefresh = -1;
constexpr int ActionPreviousPage = -2;
constexpr int ActionNextPage = -3;

constexpr int JewelsPerPage = 10;
constexpr int PageCount = (JewelCount + JewelsPerPage - 1) / JewelsPerPage;

struct JewelBank
{
    std::array<int, JewelCount> counts{};
    bool enabled = false;
    bool received = false;
};

// C1 FB D5: el baul actual se muestra tal cual ("[2/20]") y el ultimo campo dice si el servidor usa los baules.
struct Warehouse
{
    int current = 0;
    int count = 0;
    bool enabled = false;
    bool received = false;
};

JewelBank g_bank;
Warehouse g_warehouse;
int g_bankPage = 0;

int ItemType(int slot)
{
    return (Jewels[slot][0] * MAX_ITEM_INDEX) + Jewels[slot][1];
}

std::string ItemName(int itemType)
{
    if (itemType < 0 || itemType >= MAX_ITEM || ItemAttribute[itemType].Name[0] == 0)
        return "Item " + std::to_string(itemType / MAX_ITEM_INDEX) + "," + std::to_string(itemType % MAX_ITEM_INDEX);
    return MuEmu::Panel::ToUtf8(ItemAttribute[itemType].Name);
}

int BundleType(int slot)
{
    return slot < BundleCount ? (12 * MAX_ITEM_INDEX) + Bundles[slot] : -1;
}

// Joyas de la ranura en el inventario: sueltas y en paquetes.
int InventoryCount(int slot)
{
    mu::ui::window::CInventoryCtrl* ctrl = g_pMyInventory->GetInventoryCtrl();
    if (ctrl == nullptr)
        return 0;

    const int single = ItemType(slot);
    const int bundle = BundleType(slot);
    std::set<const ITEM*> seen;
    int count = 0;
    for (int y = 0; y < ctrl->GetNumberOfRow(); ++y)
    {
        for (int x = 0; x < ctrl->GetNumberOfColumn(); ++x)
        {
            const ITEM* item = ctrl->FindItem(x, y);
            if (item == nullptr || !seen.insert(item).second)
                continue;
            if (item->Type == single)
                ++count;
            else if (bundle >= 0 && item->Type == bundle)
                count += 10 * (item->Level + 1);
        }
    }
    return count;
}

// Deposita una joya del inventario (el servidor recibe la posicion del item).
void Deposit(int slot)
{
    mu::ui::window::CInventoryCtrl* ctrl = g_pMyInventory->GetInventoryCtrl();
    int position = ctrl->FindItemIndex(static_cast<short>(ItemType(slot)), -1);
    if (position < 0 && BundleType(slot) >= 0)
        position = ctrl->FindItemIndex(static_cast<short>(BundleType(slot)), -1);
    if (position < 0)
        return;

    std::vector<uint8_t> payload;
    MuEmu::Ui::WriteInt(payload, position);
    MuEmu::Ui::Send(0xF3, 0xF5, payload);
}

void Withdraw(int slot, int count)
{
    std::vector<uint8_t> payload;
    MuEmu::Ui::WriteInt(payload, slot);
    MuEmu::Ui::WriteInt(payload, count);
    MuEmu::Ui::Send(0xF3, 0xF6, payload);
}

MuEmu::Panel::Content BuildJewelBank()
{
    MuEmu::Panel::Content content;
    content.title = "Jewel Bank";
    content.wide = true;
    if (!g_bank.received)
    {
        content.lines = { "Esperando el banco del servidor..." };
        return content;
    }
    if (!g_bank.enabled)
    {
        content.lines = { "El banco de joyas esta desactivado." };
        return content;
    }

    // Como Main.dll: 10 joyas por pagina, todas aunque esten en cero.
    const int first = g_bankPage * JewelsPerPage;
    content.header = { "Joya", "Banco", "Inv." };
    for (int slot = first; slot < first + JewelsPerPage && slot < JewelCount; ++slot)
    {
        const int inventory = InventoryCount(slot);
        MuEmu::Panel::Row row;
        row.cells = { ItemName(ItemType(slot)), MuEmu::Ui::FormatNumber(g_bank.counts[slot]), std::to_string(inventory) };
        if (g_bank.counts[slot] == 0 && inventory == 0)
            row.color = "#909090";
        row.buttons.push_back({ "Dep.", slot });
        for (int n = 0; n < static_cast<int>(std::size(WithdrawCounts)); ++n)
            row.buttons.push_back({ WithdrawCounts[n] == 99 ? "Max" : std::to_string(WithdrawCounts[n]),
                                    WithdrawBase + slot * 10 + n });
        content.rows.push_back(std::move(row));
    }

    content.lines = { "Pagina " + std::to_string(g_bankPage + 1) + " / " + std::to_string(PageCount) +
                      " - Dep.: guarda las joyas (o el paquete) del inventario; los numeros retiran." };
    content.buttons = { { "< Anterior", ActionPreviousPage }, { "Siguiente >", ActionNextPage },
                        { "Actualizar", ActionRefresh } };
    return content;
}

MuEmu::Panel::Content BuildWarehouse()
{
    MuEmu::Panel::Content content;
    content.title = "Baul";
    if (!g_warehouse.received)
    {
        content.lines = { "Abri el baul para recibir los datos del servidor." };
        return content;
    }

    content.lines = { "Baul actual: " + std::to_string(g_warehouse.current) + " de " +
                      std::to_string(g_warehouse.count) };
    if (!g_warehouse.enabled)
        content.lines.push_back("El servidor no tiene baules extra.");

    for (int n = 0; n < g_warehouse.count; ++n)
    {
        MuEmu::Panel::Row row;
        row.cells = { "Baul " + std::to_string(n + 1) };
        if (n == g_warehouse.current)
            row.color = "#ffd23c";
        else if (g_warehouse.enabled)
            row.buttons = { { "Usar", n } };
        content.rows.push_back(std::move(row));
    }
    return content;
}
} // namespace

void MuEmu::Ui::Bank::OpenJewelBank()
{
    Send(0xF3, 0xF4);
    MuEmu::Panel::Show(JewelBankId, BuildJewelBank(), [](int action)
    {
        if (action == ActionRefresh)
            Send(0xF3, 0xF4);
        else if (action == ActionPreviousPage || action == ActionNextPage)
        {
            g_bankPage = (g_bankPage + (action == ActionNextPage ? 1 : PageCount - 1)) % PageCount;
            MuEmu::Panel::Update(JewelBankId, BuildJewelBank());
        }
        else if (action >= WithdrawBase)
            Withdraw((action - WithdrawBase) / 10, WithdrawCounts[(action - WithdrawBase) % 10]);
        else if (action >= 0 && action < JewelCount)
            Deposit(action);
    });
}

void MuEmu::Ui::Bank::OpenWarehouse()
{
    MuEmu::Panel::Show(WarehouseId, BuildWarehouse(), [](int action)
    {
        std::vector<uint8_t> payload;
        WriteInt(payload, action);
        Send(0xF3, 0xF7, payload);
    });
}

std::wstring MuEmu::Ui::Bank::WarehouseTitle(const wchar_t* storage)
{
    if (!g_warehouse.received || !g_warehouse.enabled)
        return std::wstring();
    return std::wstring(storage != nullptr ? storage : L"Storage") + L" [" + std::to_wstring(g_warehouse.current) +
           L"/" + std::to_wstring(g_warehouse.count) + L"]";
}

void MuEmu::Ui::Bank::ChangeWarehouse(int step)
{
    // Como Main.dll: de 0 al total, sin dar la vuelta.
    if (!g_warehouse.received || !g_warehouse.enabled)
        return;

    const int next = g_warehouse.current + step;
    if (next < 0 || next > g_warehouse.count)
        return;
    std::vector<uint8_t> payload;
    WriteInt(payload, next);
    Send(0xF3, 0xF7, payload);
}

bool MuEmu::Ui::Bank::Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size)
{
    if (head == 0xF3 && sub == 0xF5 && buffer[0] == 0xC1)
    {
        // C1 F3 F5: 30 cantidades (int desde el byte 4), activo (124), maximo (125) y paginas (126).
        if (size >= 127)
        {
            for (int slot = 0; slot < JewelCount; ++slot)
                g_bank.counts[slot] = ReadInt(buffer, 4 + slot * 4);
            g_bank.enabled = buffer[124] != 0;
            g_bank.received = true;
            MuEmu::Panel::Update(JewelBankId, BuildJewelBank());
        }
        return true;
    }

    if (head == 0xFB && sub == 0xD5 && buffer[0] == 0xC1)
    {
        // C1 FB D5: baul actual (int en 4), cantidad (8) y bloqueo (12).
        if (size >= 13)
        {
            g_warehouse.current = ReadInt(buffer, 4);
            g_warehouse.count = ReadInt(buffer, 8);
            g_warehouse.enabled = buffer[12] != 0;
            mu::log::Get("ui")->info("[MuEmu] baul {} de {} (activo {})", g_warehouse.current, g_warehouse.count, buffer[12]);
            g_warehouse.received = true;
            MuEmu::Panel::Update(WarehouseId, BuildWarehouse());
        }
        return true;
    }
    return false;
}
