#include "stdafx.h"
#include "MuEmu/MuEmuUiFeatures.h"

#include "MuEmu/MuEmuPanel.h"
#include "MuEmu/MuEmuUi.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Engine/Object/ZzzInventory.h"
#include "Render/RmlUi/RmlUiRuntime.h"
#include "UI/Character/CharacterInfoWindow.h"
#include "UI/Core/WindowSystem.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <memory>

#include <algorithm>
#include <array>
#include <cstdio>
#include <map>
#include <vector>

// Ventanas custom de Main.dll que abre un NPC o el servidor:
// - Tienda Ruud (NPCRuud): al hablar con el NPC llega C1/C3 EE (resultado 100) y la lista C2 EA (cantidad en 5 y
//   items de 20 bytes desde 6: precio en Ruud (4) e item de 12 (8)). Se compra con C1 EE (pagina, lugar) y al
//   cerrar hay que mandar C1 31 (el servidor deja al personaje "en el NPC" hasta entonces).
// - Color de items (CustomItemColor): el NPC manda C1 EF 10 (tipo de moneda, precio y el precio de los 8 lugares
//   en 12); se pide con C1 EF 10 (lugar, R, G, B), contesta C1 EF 12 y los colores de cada jugador llegan en
//   C2 EF 13 (indice del jugador en 6, cantidad en 8, lugares de 20 bytes desde 10: lugar, R, G, B).
// - Stats avanzados (CustomStatsAdvance): C1 F3 F4 con 52 int desde 8, cada vez que el servidor recalcula.
namespace
{
using MuEmu::Ui::ReadInt;

// ------------------------------------------------------------------------------------------------ Ruud
constexpr const char* RuudId = "ruud_shop";
constexpr int RuudPerPage = 5;

struct RuudItem
{
    int price = 0;
    std::string name;
};

std::vector<RuudItem> g_ruudItems;
int g_ruudPending = -1;

void CloseRuud()
{
    MuEmu::Ui::SendRaw({ 0xC1, 0x03, 0x31 });
}

void ShowRuud();

void ConfirmRuud(int position)
{
    if (position < 0 || position >= static_cast<int>(g_ruudItems.size()))
        return;
    g_ruudPending = position;

    MuEmu::Panel::Content content;
    content.title = "Confirmar compra";
    content.lines = { "Comprar " + g_ruudItems[static_cast<std::size_t>(position)].name + "?",
                      "Precio: " + MuEmu::Ui::FormatNumber(g_ruudItems[static_cast<std::size_t>(position)].price) + " Ruud" };
    content.buttons = { { "Comprar", 1 }, { "Cancelar", 2 } };
    MuEmu::Panel::Show(RuudId, std::move(content), [](int action)
    {
        if (action == 1 && g_ruudPending >= 0)
        {
            const int page = g_ruudPending / RuudPerPage + 1;
            const int slot = g_ruudPending % RuudPerPage + 1;
            MuEmu::Ui::SendRaw({ 0xC1, 0x05, 0xEE, static_cast<uint8_t>(page), static_cast<uint8_t>(slot) });
        }
        g_ruudPending = -1;
        ShowRuud();
    });
    MuEmu::Panel::SetOnClose(CloseRuud);
}

void ShowRuud()
{
    MuEmu::Panel::Content content;
    content.title = "Tienda Ruud";
    content.wide = true;
    content.header = { "Item", "Precio" };
    for (std::size_t n = 0; n < g_ruudItems.size(); ++n)
    {
        if (g_ruudItems[n].name == "-")
            continue;
        MuEmu::Panel::Row row;
        row.cells = { g_ruudItems[n].name, MuEmu::Ui::FormatNumber(g_ruudItems[n].price) + " Ruud" };
        row.buttons = { { "Comprar", 100 + static_cast<int>(n) } };
        content.rows.push_back(std::move(row));
    }
    if (content.rows.empty())
        content.lines = { "Esta tienda no tiene items para tu clase." };
    MuEmu::Panel::Show(RuudId, std::move(content), [](int action)
    {
        if (action >= 100)
            ConfirmRuud(action - 100);
    });
    MuEmu::Panel::SetOnClose(CloseRuud);
}

// ------------------------------------------------------------------------------------------------ Color de items
constexpr const char* ColorId = "item_color";

struct Rgb
{
    uint8_t r = 255, g = 255, b = 255;
};

// Colores puestos de cada jugador (indice del servidor) por lugar del equipo.
std::map<int, std::map<int, Rgb>> g_colors;

struct
{
    int costType = 0;
    std::array<int, 8> slotCost = {};
    int slot = -1;
} g_colorShop;

struct Preset
{
    const char* name;
    Rgb color;
};

constexpr std::array<Preset, 12> Presets = { {
    { "Rojo", { 220, 30, 30 } },     { "Naranja", { 240, 130, 20 } }, { "Amarillo", { 240, 220, 40 } },
    { "Verde", { 40, 200, 60 } },    { "Celeste", { 60, 200, 240 } }, { "Azul", { 40, 80, 230 } },
    { "Violeta", { 150, 60, 220 } }, { "Rosa", { 240, 110, 190 } },   { "Blanco", { 255, 255, 255 } },
    { "Gris", { 130, 130, 130 } },   { "Negro", { 35, 35, 35 } },     { "Dorado", { 210, 170, 60 } },
} };

constexpr const char* SlotNames[8] = { "Arma derecha", "Arma izquierda", "Casco", "Armadura",
                                       "Pantalones",   "Guantes",        "Botas", "Alas" };

std::string CostText(int slot)
{
    static const char* Types[4] = { "WCoinC", "WCoinP", "Goblin", "Zen" };
    const int cost = slot >= 0 && slot < 8 ? g_colorShop.slotCost[static_cast<std::size_t>(slot)] : 0;
    if (cost <= 0)
        return "Gratis";
    return MuEmu::Ui::FormatNumber(cost) + " " +
           (g_colorShop.costType >= 0 && g_colorShop.costType < 4 ? Types[g_colorShop.costType] : "?");
}

void SendColor(const Rgb& color)
{
    if (g_colorShop.slot < 0)
        return;
    MuEmu::Ui::Send(0xEF, 0x10, { static_cast<uint8_t>(g_colorShop.slot), color.r, color.g, color.b });
}

void ShowColorSlots();

void ShowColorPicker()
{
    const int slot = g_colorShop.slot;
    MuEmu::Panel::Content content;
    content.title = std::string("Color: ") + SlotNames[slot];
    content.lines = { "Precio: " + CostText(slot), "Elegi un color o escribi R G B (0 a 255) y Aplicar." };
    for (std::size_t n = 0; n < Presets.size(); ++n)
        content.buttons.push_back({ Presets[n].name, 100 + static_cast<int>(n) });
    content.buttons.push_back({ "Aplicar", 1 });
    content.buttons.push_back({ "Volver", 2 });
    content.inputLabel = "R G B";
    content.inputValue = "255 255 255";
    MuEmu::Panel::Show(ColorId, std::move(content), [](int action)
    {
        if (action == 2)
        {
            ShowColorSlots();
        }
        else if (action == 1)
        {
            int r = 255, g = 255, b = 255;
            if (std::sscanf(MuEmu::Panel::GetInput().c_str(), "%d %d %d", &r, &g, &b) == 3)
                SendColor({ static_cast<uint8_t>(std::clamp(r, 0, 255)), static_cast<uint8_t>(std::clamp(g, 0, 255)),
                            static_cast<uint8_t>(std::clamp(b, 0, 255)) });
        }
        else if (action >= 100 && action < 100 + static_cast<int>(Presets.size()))
        {
            SendColor(Presets[static_cast<std::size_t>(action - 100)].color);
        }
    });
}

void ShowColorSlots()
{
    MuEmu::Panel::Content content;
    content.title = "Color de items";
    content.wide = true;
    content.lines = { "Elegi la parte del equipo a tenir. Solo algunos items se pueden tenir." };
    content.header = { "Lugar", "Item", "Precio" };
    for (int slot = 0; slot < 8; ++slot)
    {
        const ITEM* item = CharacterMachine != nullptr ? &CharacterMachine->Equipment[slot] : nullptr;
        if (item == nullptr || item->Type == -1)
            continue;
        MuEmu::Panel::Row row;
        std::string name = item->Type >= 0 && item->Type < MAX_ITEM && ItemAttribute[item->Type].Name[0] != 0
                               ? MuEmu::Panel::ToUtf8(ItemAttribute[item->Type].Name)
                               : std::string("Item");
        row.cells = { SlotNames[slot], name, CostText(slot) };
        row.buttons = { { "Tenir", 100 + slot } };
        content.rows.push_back(std::move(row));
    }
    if (content.rows.empty())
        content.lines.push_back("No tenes items equipados.");
    MuEmu::Panel::Show(ColorId, std::move(content), [](int action)
    {
        if (action >= 100 && action < 108)
        {
            g_colorShop.slot = action - 100;
            ShowColorPicker();
        }
    });
}

// ------------------------------------------------------------------------------------------------ Stats avanzados
constexpr const char* StatsId = "stats_advance";

struct StatLabel
{
    const char* name;
    bool percent;
};

// El orden de STATS_ADVANCE_DATA del GameServer.
constexpr std::array<StatLabel, 52> StatLabels = { {
    { "Nivel", false }, { "Resets", false }, { "Grand resets", false }, { "Reflejo de dano", true },
    { "Reflejo total", true }, { "Critico", true }, { "Critico PvM", false }, { "Critico PvP", false },
    { "Excelente", true }, { "Excelente PvM", false }, { "Excelente PvP", false }, { "Dano doble", true },
    { "Dano triple", true }, { "Reduccion de dano", true }, { "Reduccion skill escudo", true },
    { "Reduccion de dano SD", true }, { "Disminucion dano SD", true }, { "Ignorar defensa", true },
    { "Ignorar SD", true }, { "Dano PvP", false }, { "Defensa PvP", false }, { "Resist. dano doble", true },
    { "Resist. ignorar def.", true }, { "Resist. ignorar SD", true }, { "Resist. critico", true },
    { "Resist. excelente", true }, { "Resist. aturdir", true }, { "Hielo", false }, { "Veneno", false },
    { "Rayo", false }, { "Fuego", false }, { "Tierra", false }, { "Viento", false }, { "Agua", false },
    { "Resist. hielo", false }, { "Resist. veneno", false }, { "Resist. rayo", false }, { "Resist. fuego", false },
    { "Resist. tierra", false }, { "Resist. viento", false }, { "Resist. agua", false },
    { "Recuperar HP (def.)", true }, { "Recuperar MP (def.)", true }, { "Recuperar SD (def.)", true },
    { "Recuperar HP", true }, { "Recuperar MP", true }, { "Recuperar SD", true }, { "Vel. de ataque", false },
    { "Vel. de magia", false }, { "Dano minimo", false }, { "Dano maximo", false }, { "Tasa de acierto", false },
} };

std::array<int, 52> g_stats = {};
bool g_statsReceived = false;

// ------------------------------------------------------------------------------------------------ Ventana
// Como Main.dll: una ventana de 213x422 pegada a la izquierda de la del personaje, a su misma escala.
struct AdvancedRow
{
    std::string label;
    std::string value;
};

struct StatsAdvanceModel
{
    std::string title = "ADVANCED STATUS";
    std::vector<AdvancedRow> rows;
};

using StatsAdvanceView = UI::RmlBridge::ThemedView<StatsAdvanceModel>;
bool g_statsShown = false;

void BindStatsAdvance(Rml::DataModelConstructor& c, StatsAdvanceModel& model)
{
    if (auto row = c.RegisterStruct<AdvancedRow>())
    {
        row.RegisterMember("label", &AdvancedRow::label);
        row.RegisterMember("value", &AdvancedRow::value);
    }
    c.RegisterArray<std::vector<AdvancedRow>>();
    c.Bind("title", &model.title);
    c.Bind("rows", &model.rows);
    c.BindEventCallback("sa_close",
        [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { MuEmu::Ui::StatsAdvance::Close(); });
}

StatsAdvanceView& StatsWindow()
{
    static std::unique_ptr<StatsAdvanceView> view;
    if (!view)
        view = std::make_unique<StatsAdvanceView>(
            "muemu_stats_advance", [](Rml::DataModelConstructor& c, StatsAdvanceModel& model) { BindStatsAdvance(c, model); },
            std::vector<UI::RmlBridge::ThemedDocumentSpec>{ { "Data/Interface/RmlUi/muemu_stats_advance.rml" } });
    return *view;
}

// Las lineas en el orden de Main.dll (PrintAdvanceStatus): indices de STATS_ADVANCE_DATA.
std::vector<AdvancedRow> BuildAdvancedRows()
{
    const auto value = [](int index, bool percent)
    {
        const int v = g_stats[static_cast<std::size_t>(index)];
        return v == 0 ? std::string("-") : MuEmu::Ui::FormatNumber(v) + (percent ? "%" : "");
    };
    const auto pair = [](int inc, int res)
    {
        const int a = g_stats[static_cast<std::size_t>(inc)];
        const int b = g_stats[static_cast<std::size_t>(res)];
        if (a == 0 && b == 0)
            return std::string("-");
        return std::to_string(a) + " / " + std::to_string(b) + "%";
    };
    std::vector<AdvancedRow> rows = {
        { "Total Dmg Reflect", value(3, true) },          { "Full Dmg Reflect Rate", value(4, true) },
        { "Crit Dmg Rate", value(5, true) },              { "Crit Dmg (PVM)", value(6, false) },
        { "Crit Dmg (PVP)", value(7, false) },            { "Excellent Dmg Rate", value(8, true) },
        { "Excellent Dmg (PVM)", value(9, false) },       { "Excellent Dmg (PVP)", value(10, false) },
        { "Double Dmg Rate", value(11, true) },           { "Triple Dmg Rate", value(12, true) },
        { "Dmg Reduction Rate", value(13, true) },        { "Shield Skill Dmg Redc Rate", value(14, true) },
        { "SD Dmg Reduction Rate", value(15, true) },     { "SD Decrease Dmg Rate", value(16, true) },
        { "Ignore Def Rate", value(17, true) },           { "Ignore SD Rate", value(18, true) },
        { "Inc Dmg PvP", value(19, false) },              { "Inc Def PvP", value(20, false) },
        { "Resist Double Dmg Rate", value(21, true) },    { "Resist Ignore Def Rate", value(22, true) },
        { "Resist Ignore SD Rate", value(23, true) },     { "Resist Crit Dmg Rate", value(24, true) },
        { "Resist Excellent Dmg Rate", value(25, true) }, { "Resist Stun Rate", value(26, true) },
        { "Ice Inc / Resist Rate", pair(27, 34) },        { "Poison Inc / Resist Rate", pair(28, 35) },
        { "Lighting Inc / Resist Rate", pair(29, 36) },   { "Fire Inc / Resist Rate", pair(30, 37) },
        { "Earth Inc / Resist Rate", pair(31, 38) },      { "Wind Inc / Resist Rate", pair(32, 39) },
        { "Water Inc / Resist Rate", pair(33, 40) },
    };
    return rows;
}

// La ventana del personaje, o null si no se ve.
Rml::Element* CharacterPanel(Rml::Context* context)
{
    for (int n = 0; n < context->GetNumDocuments(); ++n)
    {
        Rml::ElementDocument* other = context->GetDocument(n);
        if (other != nullptr && other->IsVisible() && other->GetSourceURL().find("character_info.rml") != Rml::String::npos)
            return other->GetElementById("panel");
    }
    return nullptr;
}

void SyncStatsWindow()
{
    if (!g_statsShown)
        return;
    StatsAdvanceModel& model = StatsWindow().GetModel();
    model.rows = BuildAdvancedRows();
    StatsWindow().MarkDirty("rows");
}

MuEmu::Panel::Content BuildStats()
{
    MuEmu::Panel::Content content;
    content.title = "Stats avanzados";
    content.wide = true;
    if (!g_statsReceived)
        content.lines = { "El servidor todavia no mando los stats." };
    content.header = { "Stat", "Valor", "Stat", "Valor" };
    const auto value = [](std::size_t n)
    { return MuEmu::Ui::FormatNumber(g_stats[n]) + (StatLabels[n].percent ? "%" : ""); };
    const std::size_t half = (StatLabels.size() + 1) / 2;
    for (std::size_t n = 0; n < half; ++n)
    {
        MuEmu::Panel::Row row;
        row.cells = { StatLabels[n].name, value(n) };
        if (n + half < StatLabels.size())
        {
            row.cells.push_back(StatLabels[n + half].name);
            row.cells.push_back(value(n + half));
        }
        content.rows.push_back(std::move(row));
    }
    return content;
}
} // namespace

bool MuEmu::Ui::Ruud::Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size)
{
    if (head == 0xEE && (buffer[0] == 0xC1 || buffer[0] == 0xC3) && size >= 5 && sub == 100)
    {
        g_ruudItems.clear();
        return true; // la lista llega enseguida (C2 EA)
    }
    if (head != 0xEA || buffer[0] != 0xC2)
        return false;

    g_ruudItems.clear();
    const int count = size > 5 ? buffer[5] : 0;
    for (int n = 0; n < count && 6 + (n + 1) * 20 <= size; ++n)
    {
        const uint8_t* entry = buffer + 6 + n * 20;
        RuudItem item;
        item.price = ReadInt(entry, 4);
        item.name = MuEmu::Ui::Ranking::DescribeItem(entry + 8);
        if (item.name.empty())
            item.name = "-";
        g_ruudItems.push_back(std::move(item));
    }
    ShowRuud();
    return true;
}

bool MuEmu::Ui::ItemColor::Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size)
{
    if (head != 0xEF || (sub != 0x10 && sub != 0x12 && sub != 0x13))
        return false;

    if (sub == 0x10 && buffer[0] == 0xC1 && size >= 44)
    {
        g_colorShop.costType = ReadInt(buffer, 4);
        for (std::size_t n = 0; n < 8; ++n)
            g_colorShop.slotCost[n] = ReadInt(buffer, 12 + static_cast<int>(n) * 4);
        ShowColorSlots();
    }
    else if (sub == 0x12 && buffer[0] == 0xC1 && size >= 9)
    {
        if (buffer[4] == 1 && Hero != nullptr)
            g_colors[Hero->Key][buffer[5]] = { buffer[6], buffer[7], buffer[8] };
        if (MuEmu::Panel::IsShown(ColorId))
            ShowColorSlots();
    }
    else if (sub == 0x13 && buffer[0] == 0xC2 && size >= 10)
    {
        const int target = buffer[6] | (buffer[7] << 8);
        const int count = buffer[8];
        auto& slots = g_colors[target];
        slots.clear();
        for (int n = 0; n < count && 10 + (n + 1) * 20 <= size; ++n)
        {
            const uint8_t* node = buffer + 10 + n * 20;
            if (node[0] < 12)
                slots[node[0]] = { node[1], node[2], node[3] };
        }
    }
    return true;
}

bool MuEmu::Ui::ItemColor::Tint(int characterKey, int slot, float* light)
{
    const auto player = g_colors.find(characterKey);
    if (player == g_colors.end())
        return false;
    const auto color = player->second.find(slot);
    if (color == player->second.end())
        return false;
    // Como Main.dll (HookRenderPartObjectBody): la luz de la pieza ES el color, sin la del lugar, asi se ve
    // vivo de dia y de noche.
    light[0] = color->second.r / 255.f;
    light[1] = color->second.g / 255.f;
    light[2] = color->second.b / 255.f;
    return true;
}

void MuEmu::Ui::StatsAdvance::Open()
{
    if (g_statsShown)
    {
        Close();
        return;
    }
    if (!StatsWindow().Ensure())
        return;
    // Va pegada a la ventana del personaje: si no esta abierta, se abre.
    if (g_pNewUISystem != nullptr && !g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_CHARACTER))
        g_pNewUISystem->Show(mu::ui::window::INTERFACE_CHARACTER);
    g_statsShown = true;
    SyncStatsWindow();
    if (Rml::ElementDocument* document = StatsWindow().Document())
    {
        document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        document->PullToFront();
    }
}

void MuEmu::Ui::StatsAdvance::Close()
{
    if (!g_statsShown)
        return;
    g_statsShown = false;
    StatsWindow().Hide();
}

void MuEmu::Ui::StatsAdvance::Frame()
{
    if (!g_statsShown)
        return;
    Rml::ElementDocument* document = StatsWindow().Document();
    Rml::Context* context = RmlUiRuntime::Instance().GetContext();
    if (document == nullptr || context == nullptr)
        return;
    // Se cierra con la ventana del personaje y va pegada a su izquierda, a su escala.
    Rml::Element* anchor = CharacterPanel(context);
    if (anchor == nullptr)
    {
        Close();
        return;
    }
    Rml::Element* panel = document->GetElementById("panel");
    if (panel == nullptr)
        return;
    float scale = g_pCharacterInfoWindow != nullptr ? g_pCharacterInfoWindow->GetPlacedScale() : 0.f;
    if (scale <= 0.f)
        scale = 1.f;
    const Rml::Vector2f at = anchor->GetAbsoluteOffset(Rml::BoxArea::Border);
    const float width = panel->GetBox().GetSize(Rml::BoxArea::Border).x * scale;
    panel->SetProperty(Rml::PropertyId::Left, Rml::Property(std::max(0.f, at.x - width), Rml::Unit::PX));
    panel->SetProperty(Rml::PropertyId::Top, Rml::Property(at.y, Rml::Unit::PX));
    panel->SetProperty("transform", "scale(" + std::to_string(scale) + ")");
}

bool MuEmu::Ui::StatsAdvance::Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size)
{
    if (head != 0xF3 || sub != 0xF4 || buffer[0] != 0xC1)
        return false;
    if (size >= 8 + 52 * 4)
    {
        for (std::size_t n = 0; n < g_stats.size(); ++n)
            g_stats[n] = ReadInt(buffer, 8 + static_cast<int>(n) * 4);
        g_statsReceived = true;
        MuEmu::Panel::Update(StatsId, BuildStats());
        SyncStatsWindow();
    }
    return true;
}
