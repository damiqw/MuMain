#include "stdafx.h"
#include "MuEmu/MuEmuMenuButton.h"

#include "MuEmu/MuEmuUi.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>

#include <memory>

namespace
{
struct MenuButtonModel
{
    Rml::String label = "Menu";
};

void BindModel(Rml::DataModelConstructor& c, MenuButtonModel& model)
{
    c.Bind("label", &model.label);
    c.BindEventCallback("muemu_menu_open",
        [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { MuEmu::Ui::ShowMenu(); });
}

using MenuButtonView = UI::RmlBridge::ThemedView<MenuButtonModel>;

MenuButtonView& View()
{
    static std::unique_ptr<MenuButtonView> view;
    if (!view)
    {
        view = std::make_unique<MenuButtonView>(
            "muemu_menu_button", [](Rml::DataModelConstructor& c, MenuButtonModel& model) { BindModel(c, model); },
            std::vector<UI::RmlBridge::ThemedDocumentSpec>{ { "Data/Interface/RmlUi/muemu_menu_button.rml" } });
    }
    return *view;
}
} // namespace

void MuEmu::MenuButton::Show()
{
    if (!View().Ensure())
        return;

    Rml::ElementDocument* document = View().Document();
    if (document != nullptr && !document->IsVisible())
        document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
}
