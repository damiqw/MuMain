#include "stdafx.h"
#include "MuEmu/MuEmuUiFeatures.h"

#include "MuEmu/MuEmuPanel.h"
#include "MuEmu/MuEmuUi.h"

#include "Engine/Object/ZzzInfomation.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Inventory/MyInventory.h"

#include <array>
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

// Cantidades que se pueden retirar, como en Main.dll.
constexpr int WithdrawCounts[] = { 1, 10, 20, 30 };

// Acciones: deposito = slot, retiro = 100 + slot * 10 + indice de la cantidad.
constexpr int WithdrawBase = 100;

struct JewelBank
{
    std::array<int, JewelCount> counts{};
    bool enabled = false;
    bool received = false;
};

struct Warehouse
{
    int current = 0;
    int count = 0;
    bool locked = false;
    bool received = false;
};

JewelBank g_bank;
Warehouse g_warehouse;

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

// Deposita una joya del inventario (el servidor recibe la posicion del item).
void Deposit(int slot)
{
    const int position = g_pMyInventory->GetInventoryCtrl()->FindItemIndex(static_cast<short>(ItemType(slot)), -1);
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
    content.title = "Banco de joyas";
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

    content.header = { "Joya", "Banco", "Inv." };
    for (int slot = 0; slot < JewelCount; ++slot)
    {
        const int inventory = g_pMyInventory->GetNumItemByType(static_cast<short>(ItemType(slot)));
        if (g_bank.counts[slot] == 0 && inventory == 0)
            continue;

        MuEmu::Panel::Row row;
        row.cells = { ItemName(ItemType(slot)), MuEmu::Ui::FormatNumber(g_bank.counts[slot]), std::to_string(inventory) };
        if (inventory > 0)
            row.buttons.push_back({ "Dep.", slot });
        for (int n = 0; n < static_cast<int>(std::size(WithdrawCounts)); ++n)
        {
            if (g_bank.counts[slot] >= WithdrawCounts[n])
                row.buttons.push_back({ std::to_string(WithdrawCounts[n]), WithdrawBase + slot * 10 + n });
        }
        content.rows.push_back(std::move(row));
    }

    if (content.rows.empty())
        content.lines = { "No hay joyas en el banco ni en el inventario." };
    content.buttons = { { "Actualizar", -1 } };
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

    content.lines = { "Baul actual: " + std::to_string(g_warehouse.current + 1) + " de " +
                      std::to_string(g_warehouse.count) };
    if (g_warehouse.locked)
        content.lines.push_back("El baul esta bloqueado.");

    for (int n = 0; n < g_warehouse.count; ++n)
    {
        MuEmu::Panel::Row row;
        row.cells = { "Baul " + std::to_string(n + 1) };
        if (n == g_warehouse.current)
            row.color = "#ffd23c";
        else if (!g_warehouse.locked)
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
        if (action == -1)
            Send(0xF3, 0xF4);
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
            g_warehouse.locked = buffer[12] != 0;
            g_warehouse.received = true;
            MuEmu::Panel::Update(WarehouseId, BuildWarehouse());
        }
        return true;
    }
    return false;
}
