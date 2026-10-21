#include "stdafx.h"
#include "MuEmu/MuEmuUiFeatures.h"

#include "MuEmu/MuEmuPanel.h"
#include "MuEmu/MuEmuUi.h"

#include "Core/Utilities/Log/MuLogger.h"
#include "Engine/Object/ZzzInfomation.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Inventory/InventoryCtrl.h"
#include "UI/Inventory/MyInventory.h"
#include "Audio/DSPlaySound.h"
#include "Engine/Object/ZzzInventory.h"
#include "UI/Inventory/ItemCameraTarget.h"
#include "UI/RmlBridge/RmlDraggable.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>

#include <array>
#include <memory>
#include <vector>
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
    int max = 0; // cuantas se pueden retirar de una vez (el ultimo boton)
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

// ------------------------------------------------------------------------------------------------ Ventana propia
// Con los temas que la tienen (legacy, ex700...) el banco usa su ventana con el aspecto de Main.dll
// (muemu_jewel_bank.rml); el tema modern sigue con la ventana generica de MuEmu.
struct JewelRowEntry
{
    std::string name;
    std::string count;
    bool empty = true;
    int slot = 0;
};

struct JewelBankModel
{
    std::string title = "Jewel Bank";
    std::string message;
    std::vector<JewelRowEntry> rows;
    std::vector<std::string> amounts;
    std::string pageText;
    std::string hint = "SHIFT + CLICK";
};

using JewelBankView = UI::RmlBridge::ThemedView<JewelBankModel>;

bool g_windowShown = false;
std::array<std::unique_ptr<UI::Items::ItemCameraTarget>, JewelsPerPage> g_icons;

bool UseThemedWindow()
{
    return UI::RmlBridge::GetActiveThemeName() != "modern";
}

void SyncWindow();

void BindJewelBank(Rml::DataModelConstructor& c, JewelBankModel& model)
{
    if (auto row = c.RegisterStruct<JewelRowEntry>())
    {
        row.RegisterMember("name", &JewelRowEntry::name);
        row.RegisterMember("count", &JewelRowEntry::count);
        row.RegisterMember("empty", &JewelRowEntry::empty);
        row.RegisterMember("slot", &JewelRowEntry::slot);
    }
    c.RegisterArray<std::vector<JewelRowEntry>>();
    c.RegisterArray<std::vector<std::string>>();
    c.Bind("title", &model.title);
    c.Bind("message", &model.message);
    c.Bind("rows", &model.rows);
    c.Bind("amounts", &model.amounts);
    c.Bind("page_text", &model.pageText);
    c.Bind("hint", &model.hint);
    c.BindEventCallback("jb_withdraw",
        [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
        {
            if (args.size() < 2)
                return;
            const int slot = args[0].Get<int>();
            const int index = args[1].Get<int>();
            if (slot >= 0 && slot < JewelCount && index >= 0 && index < static_cast<int>(std::size(WithdrawCounts)))
            {
                const int count = WithdrawCounts[index] == 99 && g_bank.max > 0 ? g_bank.max : WithdrawCounts[index];
                Withdraw(slot, count);
                PlayBuffer(SOUND_CLICK01);
            }
        });
    c.BindEventCallback("jb_page",
        [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
        {
            if (args.empty())
                return;
            g_bankPage = (g_bankPage + (args[0].Get<int>() > 0 ? 1 : PageCount - 1)) % PageCount;
            PlayBuffer(SOUND_CLICK01);
            SyncWindow();
        });
    c.BindEventCallback("jb_close",
        [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { MuEmu::Ui::Bank::CloseJewelBank(); });
}

JewelBankView& Window()
{
    static std::unique_ptr<JewelBankView> view;
    if (!view)
    {
        view = std::make_unique<JewelBankView>(
            "muemu_jewel_bank",
            [](Rml::DataModelConstructor& c, JewelBankModel& model) { BindJewelBank(c, model); },
            std::vector<UI::RmlBridge::ThemedDocumentSpec>{ { "Data/Interface/RmlUi/muemu_jewel_bank.rml" } });
        view->SetAfterBuild([]
        {
            Rml::ElementDocument* document = Window().Document();
            if (document == nullptr)
                return;
            Rml::Element* panel = document->GetElementById("panel");
            Rml::Element* title = document->GetElementById("jb_title");
            if (panel != nullptr)
                UI::RmlBridge::MakeDraggable(panel, title != nullptr ? title : panel, nullptr,
                                             [panel] { UI::RmlBridge::KeepInsideWindow(panel); });
        });
        for (int n = 0; n < JewelsPerPage; ++n)
        {
            g_icons[static_cast<std::size_t>(n)] = std::make_unique<UI::Items::ItemCameraTarget>(
                [n](const Rml::Vector2f& offset, const Rml::Vector2f& size)
                {
                    const int slot = g_bankPage * JewelsPerPage + n;
                    if (slot < JewelCount)
                        RenderItem3D(offset.x, offset.y, size.x, size.y, ItemType(slot), 0, 0, 0, false);
                });
        }
    }
    return *view;
}

void SyncWindow()
{
    if (!g_windowShown)
        return;
    JewelBankModel& model = Window().GetModel();
    model.rows.clear();
    model.message.clear();
    if (!g_bank.received)
        model.message = "Cargando...";
    else if (!g_bank.enabled)
        model.message = "El banco de joyas esta desactivado.";
    else
    {
        const int first = g_bankPage * JewelsPerPage;
        for (int slot = first; slot < first + JewelsPerPage && slot < JewelCount; ++slot)
        {
            JewelRowEntry row;
            row.name = ItemName(ItemType(slot));
            row.count = MuEmu::Ui::FormatNumber(g_bank.counts[static_cast<std::size_t>(slot)]);
            row.empty = g_bank.counts[static_cast<std::size_t>(slot)] == 0;
            row.slot = slot;
            model.rows.push_back(std::move(row));
        }
    }
    model.amounts.clear();
    for (int count : WithdrawCounts)
        model.amounts.push_back(count == 99 ? (g_bank.max > 0 ? std::to_string(g_bank.max) : std::string("Max"))
                                            : std::to_string(count));
    model.pageText = std::to_string(g_bankPage + 1) + " / " + std::to_string(PageCount);
    for (const char* field : { "rows", "message", "amounts", "page_text" })
        Window().MarkDirty(field);
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
    if (UseThemedWindow())
    {
        if (g_windowShown)
        {
            CloseJewelBank();
            return;
        }
        if (!Window().Ensure())
            return;
        g_windowShown = true;
        SyncWindow();
        if (Rml::ElementDocument* document = Window().Document())
        {
            document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
            document->PullToFront();
        }
        return;
    }
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
    // Como Main.dll: el numero cambia ya; el servidor manda los items del baul nuevo.
    g_warehouse.current = next;
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
            g_bank.max = buffer[125];
            g_bank.received = true;
            MuEmu::Panel::Update(JewelBankId, BuildJewelBank());
            SyncWindow();
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

void MuEmu::Ui::Bank::CloseJewelBank()
{
    if (!g_windowShown)
        return;
    g_windowShown = false;
    for (auto& icon : g_icons)
    {
        if (icon)
            icon->Disable();
    }
    Window().Hide();
}

bool MuEmu::Ui::Bank::IsJewelBankOpen()
{
    return g_windowShown || MuEmu::Panel::IsShown(JewelBankId);
}

void MuEmu::Ui::Bank::Frame()
{
    if (!g_windowShown)
        return;
    Rml::ElementDocument* document = Window().Document();
    if (document == nullptr)
        return;
    for (int n = 0; n < JewelsPerPage; ++n)
    {
        auto& icon = g_icons[static_cast<std::size_t>(n)];
        if (icon)
            icon->Sync(document->GetElementById("jb_icon_" + std::to_string(n)), g_bankPage * JewelsPerPage + n < JewelCount);
    }
}

bool MuEmu::Ui::Bank::DepositAt(int position, int itemType)
{
    if (!IsJewelBankOpen())
        return false;
    for (int slot = 0; slot < JewelCount; ++slot)
    {
        if (itemType == Jewels[slot][0] * MAX_ITEM_INDEX + Jewels[slot][1] || itemType == BundleType(slot))
        {
            std::vector<uint8_t> payload;
            WriteInt(payload, position);
            Send(0xF3, 0xF5, payload);
            return true;
        }
    }
    return false;
}

