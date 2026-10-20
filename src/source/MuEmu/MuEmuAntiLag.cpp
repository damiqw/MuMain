#include "stdafx.h"
#include "MuEmu/MuEmuUiFeatures.h"

#include "MuEmu/MuEmuPanel.h"
#include "MuEmu/MuEmuTools.h"

#include <array>

// Panel AntiLag (Antilag de Main.dll), con las opciones que tienen sentido en este cliente: los efectos custom
// del servidor y los modelos que mas cuestan en zonas llenas. Cada opcion se guarda en config.ini [MuEmu]
// (AntiLagItemEffects, AntiLagSetEffects, AntiLagPlayers, AntiLagWings, AntiLagZen: 1 = oculto).
namespace
{
constexpr const char* AntiLagId = "antilag";

struct OptionInfo
{
    const char* key;
    const char* label;
};

constexpr std::array<OptionInfo, MuEmu::AntiLag::OptionCount> Options = { {
    { "AntiLagItemEffects", "Efectos de items custom" },
    { "AntiLagSetEffects", "Efectos de set y runa" },
    { "AntiLagPlayers", "Otros jugadores" },
    { "AntiLagWings", "Alas de otros jugadores" },
    { "AntiLagZen", "Zen en el piso" },
} };

std::array<int, MuEmu::AntiLag::OptionCount> g_hidden = {};
bool g_loaded = false;

void EnsureLoaded()
{
    if (g_loaded)
        return;
    g_loaded = true;
    for (std::size_t n = 0; n < Options.size(); ++n)
        g_hidden[n] = MuEmu::GetConfigInt(Options[n].key, 0) != 0 ? 1 : 0;
}
} // namespace

bool MuEmu::AntiLag::IsHidden(Option option)
{
    EnsureLoaded();
    return option >= 0 && option < OptionCount && g_hidden[static_cast<std::size_t>(option)] != 0;
}

void MuEmu::AntiLag::Open()
{
    EnsureLoaded();

    MuEmu::Panel::Content content;
    content.title = "AntiLag";
    content.wide = true;
    content.lines = { "Ocultar lo que no hace falta ver mejora los FPS en PCs lentas o zonas llenas." };
    content.header = { "Opcion", "Estado" };
    for (std::size_t n = 0; n < Options.size(); ++n)
    {
        MuEmu::Panel::Row row;
        const bool hidden = g_hidden[n] != 0;
        row.cells = { Options[n].label, hidden ? "Oculto" : "Visible" };
        row.color = hidden ? "#ff8060" : "#80ff80";
        row.buttons = { { hidden ? "Mostrar" : "Ocultar", static_cast<int>(n) + 1 } };
        content.rows.push_back(std::move(row));
    }

    MuEmu::Panel::Show(AntiLagId, std::move(content), [](int action)
    {
        const int n = action - 1;
        if (n < 0 || n >= OptionCount)
            return;
        g_hidden[static_cast<std::size_t>(n)] = g_hidden[static_cast<std::size_t>(n)] != 0 ? 0 : 1;
        MuEmu::SetConfigInt(Options[static_cast<std::size_t>(n)].key, g_hidden[static_cast<std::size_t>(n)]);
        MuEmu::AntiLag::Open();
    });
}
