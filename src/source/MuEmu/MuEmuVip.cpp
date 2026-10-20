#include "stdafx.h"
#include "MuEmu/MuEmuUiFeatures.h"

#include "MuEmu/MuEmuPanel.h"
#include "MuEmu/MuEmuUi.h"

#include <vector>

// Comprar VIP (BuyVip de Main.dll): el servidor manda los planes (C2 F3 FB: cantidad en 5 y 60 bytes por plan
// desde 6: indice, exp, drop, dias, WCoinC, WCoinP, Goblin y nombre de 32) y el cliente compra con C1 F3 F0
// (int, el plan del 1 al 3). El servidor cobra las monedas y avisa el resultado.
namespace
{
constexpr const char* VipId = "buy_vip";
constexpr const char* ConfirmId = "buy_vip_confirm";
constexpr int EntrySize = 60;

enum VipAction
{
    ActionBuyFirst = 1,   // + plan
    ActionConfirm = 100,
    ActionCancel,
};

struct Plan
{
    int index = 0;
    int exp = 0;
    int drop = 0;
    int days = 0;
    int coin[3] = {};
    std::string name;
};

std::vector<Plan> g_plans;
int g_pending = 0;

std::string Price(const Plan& plan)
{
    static const char* Coins[3] = { "WCoinC", "WCoinP", "Goblin" };
    std::string text;
    for (int n = 0; n < 3; ++n)
    {
        if (plan.coin[n] <= 0)
            continue;
        if (!text.empty())
            text += " + ";
        text += MuEmu::Ui::FormatNumber(plan.coin[n]) + " " + Coins[n];
    }
    return text.empty() ? "Gratis" : text;
}

void Confirm(int plan)
{
    if (plan < 1 || plan > static_cast<int>(g_plans.size()))
        return;
    g_pending = plan;
    const Plan& info = g_plans[static_cast<std::size_t>(plan - 1)];

    MuEmu::Panel::Content content;
    content.title = "Confirmar compra";
    content.lines = { "Comprar " + info.name + " por " + std::to_string(info.days) + " dia(s)?", "Costo: " + Price(info) };
    content.buttons = { { "Comprar", ActionConfirm }, { "Cancelar", ActionCancel } };
    MuEmu::Panel::Show(ConfirmId, std::move(content), [](int action)
    {
        if (action == ActionConfirm && g_pending > 0)
        {
            std::vector<uint8_t> payload;
            MuEmu::Ui::WriteInt(payload, g_pending);
            MuEmu::Ui::Send(0xF3, 0xF0, payload);
        }
        g_pending = 0;
        MuEmu::Ui::Vip::Open();
    });
}
} // namespace

void MuEmu::Ui::Vip::Open()
{
    MuEmu::Panel::Content content;
    content.title = "Comprar VIP";
    content.wide = true;
    if (g_plans.empty())
        content.lines = { "El servidor no mando los planes VIP." };

    content.header = { "Plan", "Exp", "Drop", "Tiempo", "Precio" };
    for (std::size_t n = 0; n < g_plans.size() && n < 3; ++n)
    {
        const Plan& plan = g_plans[n];
        MuEmu::Panel::Row row;
        row.cells = { plan.name, "+" + std::to_string(plan.exp) + "%", "+" + std::to_string(plan.drop) + "%",
                      std::to_string(plan.days) + " dia(s)", Price(plan) };
        row.buttons = { { "Comprar", ActionBuyFirst + static_cast<int>(n) + 1 } };
        content.rows.push_back(std::move(row));
    }

    MuEmu::Panel::Show(VipId, std::move(content), [](int action)
    {
        if (action > ActionBuyFirst && action <= ActionBuyFirst + 3)
            Confirm(action - ActionBuyFirst);
    });
}

bool MuEmu::Ui::Vip::Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size)
{
    if (head != 0xF3 || sub != 0xFB || buffer[0] != 0xC2)
        return false;

    g_plans.clear();
    const int count = size > 5 ? buffer[5] : 0;
    for (int n = 0; n < count && 6 + (n + 1) * EntrySize <= size; ++n)
    {
        const int at = 6 + n * EntrySize;
        Plan plan;
        plan.index = ReadInt(buffer, at);
        plan.exp = ReadInt(buffer, at + 4);
        plan.drop = ReadInt(buffer, at + 8);
        plan.days = ReadInt(buffer, at + 12);
        for (int c = 0; c < 3; ++c)
            plan.coin[c] = ReadInt(buffer, at + 16 + c * 4);
        plan.name = ReadText(buffer, at + 28, 32);
        g_plans.push_back(std::move(plan));
    }
    if (MuEmu::Panel::IsShown(VipId))
        Open();
    return true;
}
