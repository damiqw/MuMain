#include "stdafx.h"
#include "MuEmu/MuEmuUiFeatures.h"

#include "MuEmu/MuEmuPanel.h"
#include "MuEmu/MuEmuTools.h"
#include "MuEmu/MuEmuUi.h"
#include "Character/CharacterManager.h"
#include "Core/Utilities/StringUtils.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Network/Server/WSclient.h"
#include "Render/Text/CUIRenderText.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <vector>

extern BYTE Version[SIZE_PROTOCOLVERSION];
extern BYTE Serial[SIZE_PROTOCOLSERIAL + 1];

// Funciones chicas de Main.dll:
// - Ping (CustomPing): C1 F3 F1 cada 5 segundos, el servidor contesta C1 F3 F3; se muestra en pantalla.
// - Sumar puntos rapido (CustomQuickAddStats): C1 FF 0C con los puntos de cada stat (5 int desde 4).
// - Crear cuenta en el login (CreateAccount): C3 F1 12 (cuenta 10, clave 20, codigo personal 20 y email 50 con
//   Xor3, tick, version y serial); contesta C1 F1 12 (0 cuenta usada, 1 creada, 2 no disponible).
namespace
{
// ------------------------------------------------------------------------------------------------ Ping
using Clock = std::chrono::steady_clock;
constexpr auto PingInterval = std::chrono::seconds(5);

Clock::time_point g_pingSent;
Clock::time_point g_pingNext;
bool g_pingWaiting = false;
int g_pingMs = -1;

// ------------------------------------------------------------------------------------------------ Sumar puntos
constexpr const char* QuickStatsId = "quick_stats";

// ------------------------------------------------------------------------------------------------ Crear cuenta
constexpr const char* RegisterId = "register_account";

enum RegisterStep
{
    StepAccount,
    StepPassword,
    StepCode,
    StepEmail,
    StepCount,
};

struct
{
    std::string values[StepCount];
    int step = StepAccount;
    std::string error;
    std::string result;
} g_register;

void ShowRegister();

void XorArgument(uint8_t* out, const std::string& text, int length)
{
    static const uint8_t Key[3] = { 0xFC, 0xCF, 0xAB };
    for (int n = 0; n < length; ++n)
    {
        const uint8_t value = n < static_cast<int>(text.size()) ? static_cast<uint8_t>(text[static_cast<std::size_t>(n)]) : 0;
        out[n] = value ^ Key[n % 3];
    }
}

void SendRegister()
{
    std::vector<uint8_t> packet(132, 0);
    packet[0] = 0xC3;
    packet[1] = static_cast<uint8_t>(packet.size());
    packet[2] = 0xF1;
    packet[3] = 0x12;
    XorArgument(packet.data() + 4, g_register.values[StepAccount], 10);
    XorArgument(packet.data() + 14, g_register.values[StepPassword], 20);
    XorArgument(packet.data() + 34, g_register.values[StepCode], 20);
    XorArgument(packet.data() + 54, g_register.values[StepEmail], 50);
    const uint32_t tick = static_cast<uint32_t>(GetTickCount64());
    std::memcpy(packet.data() + 104, &tick, sizeof(tick));
    std::memcpy(packet.data() + 108, Version, 5);
    std::memcpy(packet.data() + 113, Serial, 16);
    MuEmu::Ui::SendRaw(packet);
}

bool IsValidField(int step, const std::string& value, std::string& error)
{
    const auto alnum = [](const std::string& text)
    {
        return std::all_of(text.begin(), text.end(), [](char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0; });
    };
    switch (step)
    {
    case StepAccount:
        if (value.size() < 4 || value.size() > 10 || !alnum(value))
            error = "La cuenta tiene que tener de 4 a 10 letras o numeros.";
        break;
    case StepPassword:
        if (value.size() < 4 || value.size() > 10 || !alnum(value))
            error = "La clave tiene que tener de 4 a 10 letras o numeros.";
        break;
    case StepCode:
        if (value.size() < 4 || value.size() > 10 || !alnum(value))
            error = "El codigo personal tiene que tener de 4 a 10 letras o numeros (sirve para borrar personajes).";
        break;
    case StepEmail:
        if (value.size() < 6 || value.size() > 49 || value.find('@') == std::string::npos)
            error = "Escribi un email valido.";
        break;
    default:
        break;
    }
    return error.empty();
}

void ShowRegister()
{
    static const char* Labels[StepCount] = { "Cuenta", "Clave", "Codigo personal", "Email" };

    MuEmu::Panel::Content content;
    content.title = "Crear cuenta";
    if (!g_register.result.empty())
    {
        content.lines = { g_register.result };
        content.buttons = { { "Cerrar", 3 } };
    }
    else
    {
        content.lines = { "Paso " + std::to_string(g_register.step + 1) + " de " + std::to_string(StepCount) };
        for (int n = 0; n < g_register.step; ++n)
            content.lines.push_back(std::string(Labels[n]) + ": " + (n == StepPassword ? std::string("******") : g_register.values[n]));
        if (!g_register.error.empty())
            content.lines.push_back(g_register.error);
        content.inputLabel = Labels[g_register.step];
        content.inputValue = g_register.values[g_register.step];
        content.buttons = { { g_register.step + 1 < StepCount ? "Siguiente" : "Crear cuenta", 1 } };
        if (g_register.step > 0)
            content.buttons.push_back({ "Volver", 2 });
    }

    MuEmu::Panel::Show(RegisterId, std::move(content), [](int action)
    {
        if (action == 3)
        {
            MuEmu::Panel::Hide();
            return;
        }
        if (action == 2 && g_register.step > 0)
        {
            g_register.values[g_register.step] = MuEmu::Panel::GetInput();
            g_register.error.clear();
            --g_register.step;
            ShowRegister();
            return;
        }
        if (action != 1)
            return;

        std::string value = MuEmu::Panel::GetInput();
        value.erase(0, value.find_first_not_of(' '));
        value.erase(value.find_last_not_of(' ') + 1);
        g_register.error.clear();
        if (!IsValidField(g_register.step, value, g_register.error))
        {
            g_register.values[g_register.step] = value;
            ShowRegister();
            return;
        }
        g_register.values[g_register.step] = value;
        if (g_register.step + 1 < StepCount)
        {
            ++g_register.step;
            ShowRegister();
            return;
        }
        SendRegister();
        g_register.result = "Enviando...";
        ShowRegister();
    });
}
} // namespace

void MuEmu::Ui::Ping::Frame()
{
    static const bool enabled = MuEmu::GetConfigInt("Ping", 1) != 0;
    if (!enabled)
        return;

    const auto now = Clock::now();
    if (now >= g_pingNext && (!g_pingWaiting || now - g_pingSent > PingInterval * 3))
    {
        g_pingSent = now;
        g_pingNext = now + PingInterval;
        g_pingWaiting = true;
        Send(0xF3, 0xF1);
    }
    if (g_pingMs < 0)
        return;

    const std::wstring text = L"Ping: " + std::to_wstring(g_pingMs) + L" ms";
    if (g_pingMs < 100)
        g_pRenderText->SetTextColor(120, 230, 120, 255);
    else if (g_pingMs < 250)
        g_pRenderText->SetTextColor(240, 210, 80, 255);
    else
        g_pRenderText->SetTextColor(240, 90, 80, 255);
    g_pRenderText->SetBgColor(0, 0, 0, 120);
    g_pRenderText->RenderText(4, 108, text.c_str());
    g_pRenderText->SetBgColor(0, 0, 0, 0);
}

bool MuEmu::Ui::Ping::Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int)
{
    if (head != 0xF3 || sub != 0xF3 || buffer[0] != 0xC1)
        return false;
    if (g_pingWaiting)
    {
        g_pingWaiting = false;
        g_pingMs = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - g_pingSent).count());
    }
    return true;
}

void MuEmu::Ui::QuickStats::Open()
{
    if (CharacterAttribute == nullptr)
        return;

    const bool lord = Hero != nullptr && gCharacterManager.GetBaseClass(Hero->Class) == CLASS_DARK_LORD;
    MuEmu::Panel::Content content;
    content.title = "Sumar puntos";
    content.lines = { "Puntos libres: " + MuEmu::Ui::FormatNumber(CharacterAttribute->LevelUpPoint),
                      "Escribi la cantidad y elegi el stat." };
    content.header = { "Stat", "Actual" };
    const struct
    {
        const char* name;
        DWORD value;
    } stats[5] = { { "Fuerza", CharacterAttribute->Strength },   { "Agilidad", CharacterAttribute->Dexterity },
                   { "Vitalidad", CharacterAttribute->Vitality }, { "Energia", CharacterAttribute->Energy },
                   { "Comando", CharacterAttribute->Charisma } };
    for (int n = 0; n < (lord ? 5 : 4); ++n)
    {
        MuEmu::Panel::Row row;
        row.cells = { stats[n].name, MuEmu::Ui::FormatNumber(stats[n].value) };
        row.buttons = { { "Sumar", 100 + n } };
        content.rows.push_back(std::move(row));
    }
    content.inputLabel = "Cantidad";
    content.inputValue = "1";
    content.buttons = { { "Todo a Fuerza", 200 }, { "Todo a Agilidad", 201 }, { "Todo a Vitalidad", 202 },
                        { "Todo a Energia", 203 } };

    MuEmu::Panel::Show(QuickStatsId, std::move(content), [](int action)
    {
        if (CharacterAttribute == nullptr)
            return;
        int stat = -1;
        long long amount = 0;
        if (action >= 100 && action < 105)
        {
            stat = action - 100;
            amount = std::atoll(MuEmu::Panel::GetInput().c_str());
        }
        else if (action >= 200 && action < 204)
        {
            stat = action - 200;
            amount = CharacterAttribute->LevelUpPoint;
        }
        amount = std::min<long long>(amount, CharacterAttribute->LevelUpPoint);
        if (stat < 0 || amount <= 0)
            return;

        std::vector<uint8_t> payload;
        for (int n = 0; n < 5; ++n)
            MuEmu::Ui::WriteInt(payload, n == stat ? static_cast<int32_t>(amount) : 0);
        MuEmu::Ui::Send(0xFF, 0x0C, payload);
    });
}

bool MuEmu::Ui::QuickStats::IsShown()
{
    return MuEmu::Panel::IsShown(QuickStatsId);
}

void MuEmu::Ui::Register::Open()
{
    g_register = {};
    ShowRegister();
}

bool MuEmu::Ui::Register::Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size)
{
    if (head != 0xF1 || sub != 0x12 || buffer[0] != 0xC1)
        return false;
    if (size < 5)
        return true;
    switch (buffer[4])
    {
    case 0: g_register.result = "Esa cuenta ya esta registrada."; break;
    case 1: g_register.result = "Cuenta creada. Ya podes entrar con ella."; break;
    case 2: g_register.result = "La creacion de cuentas no esta disponible ahora."; break;
    default: g_register.result = "No se pudo crear la cuenta (codigo " + std::to_string(buffer[4]) + ")."; break;
    }
    ShowRegister();
    return true;
}
