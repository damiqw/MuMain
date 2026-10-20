#include "stdafx.h"
#include "MuEmu/MuEmuPanel.h"

#include "Core/Utilities/StringUtils.h"
#include "UI/RmlBridge/RmlDraggable.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>

#include <algorithm>
#include <memory>

namespace
{
using MuEmu::Panel::Button;
using MuEmu::Panel::Content;
using MuEmu::Panel::Row;

struct PanelModel
{
    std::string title;
    std::vector<std::string> lines;
    std::vector<std::string> header;
    std::vector<Row> rows;
    std::vector<Button> buttons;
    bool wide = false;
    int buttonsWidth = 0;
};

std::string g_shownId;
MuEmu::Panel::ActionHandler g_onAction;

void BindModel(Rml::DataModelConstructor& c, PanelModel& model)
{
    c.RegisterArray<std::vector<std::string>>();

    if (auto button = c.RegisterStruct<Button>())
    {
        button.RegisterMember("label", &Button::label);
        button.RegisterMember("action", &Button::action);
    }
    c.RegisterArray<std::vector<Button>>();

    if (auto row = c.RegisterStruct<Row>())
    {
        row.RegisterMember("cells", &Row::cells);
        row.RegisterMember("color", &Row::color);
        row.RegisterMember("buttons", &Row::buttons);
    }
    c.RegisterArray<std::vector<Row>>();

    c.Bind("title", &model.title);
    c.Bind("lines", &model.lines);
    c.Bind("header", &model.header);
    c.Bind("rows", &model.rows);
    c.Bind("buttons", &model.buttons);
    c.Bind("wide", &model.wide);
    c.Bind("buttons_width", &model.buttonsWidth);

    c.BindEventCallback("muemu_action",
        [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
        {
            if (!args.empty() && g_onAction)
            {
                // Copia: la accion puede abrir otra ventana y reemplazar el manejador.
                const MuEmu::Panel::ActionHandler onAction = g_onAction;
                onAction(args[0].Get<int>());
            }
        });
    c.BindEventCallback("muemu_close",
        [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { MuEmu::Panel::Hide(); });
}

using PanelView = UI::RmlBridge::ThemedView<PanelModel>;

PanelView& View()
{
    static std::unique_ptr<PanelView> view;
    if (!view)
    {
        view = std::make_unique<PanelView>(
            "muemu_panel", [](Rml::DataModelConstructor& c, PanelModel& model) { BindModel(c, model); },
            std::vector<UI::RmlBridge::ThemedDocumentSpec>{ { "Data/Interface/RmlUi/muemu_panel.rml" } });
        view->SetAfterBuild([]
        {
            Rml::ElementDocument* document = View().Document();
            if (document == nullptr)
                return;
            if (Rml::Element* panel = document->GetElementById("panel"))
                UI::RmlBridge::MakeDraggable(panel, panel, nullptr, [panel] { UI::RmlBridge::KeepInsideWindow(panel); });
        });
    }
    return *view;
}

void Apply(Content content)
{
    PanelModel& model = View().GetModel();
    model.title = std::move(content.title);
    model.lines = std::move(content.lines);
    model.header = std::move(content.header);
    model.rows = std::move(content.rows);
    model.buttons = std::move(content.buttons);
    model.wide = content.wide;

    // La columna de botones de la tabla, del ancho de la fila con mas botones (texto aproximado en dp).
    model.buttonsWidth = 0;
    for (const Row& row : model.rows)
    {
        int width = 0;
        for (const Button& button : row.buttons)
            width += 10 + static_cast<int>(button.label.size()) * 6;
        model.buttonsWidth = std::max(model.buttonsWidth, width);
    }

    for (const char* field : { "title", "lines", "header", "rows", "buttons", "wide", "buttons_width" })
        View().MarkDirty(field);
}
} // namespace

void MuEmu::Panel::Show(const std::string& id, Content content, ActionHandler onAction)
{
    if (!View().Ensure())
        return;

    g_shownId = id;
    g_onAction = std::move(onAction);
    Apply(std::move(content));

    if (Rml::ElementDocument* document = View().Document())
    {
        document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        document->PullToFront();
    }
}

void MuEmu::Panel::Update(const std::string& id, Content content)
{
    if (IsShown(id))
        Apply(std::move(content));
}

bool MuEmu::Panel::IsShown(const std::string& id)
{
    Rml::ElementDocument* document = View().IsBuilt() ? View().Document() : nullptr;
    return document != nullptr && document->IsVisible() && g_shownId == id;
}

void MuEmu::Panel::Hide()
{
    g_shownId.clear();
    g_onAction = nullptr;
    if (View().IsBuilt())
        View().Hide();
}

std::string MuEmu::Panel::ToUtf8(const wchar_t* text)
{
    return text != nullptr ? StringUtils::WideToNarrow(text) : std::string();
}
