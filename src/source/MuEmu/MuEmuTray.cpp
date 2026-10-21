#include "stdafx.h"
#include "MuEmu/MuEmuUi.h"

#ifdef _WIN32

#include "Core/Utilities/Log/MuLogger.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Scenes/SceneCore.h"

#include <shellapi.h>

#include <cwchar>

extern HWND g_hWnd;

// Main.dll TrayMode.cpp: F12 hides the window to the notification area, the icon's tip is the window name
// (and the character's in game), and a double click on the icon shows the window again.
namespace
{
constexpr UINT TrayIconId = WM_USER + 100;
constexpr UINT TrayMessage = WM_USER + 101;

bool g_hidden = false;
HICON g_icon = nullptr;

HICON Icon()
{
    if (g_icon == nullptr)
    {
        g_icon = static_cast<HICON>(LoadImageW(nullptr, L"Data\\Custom\\Main.ico", IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                               GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE | LR_DEFAULTCOLOR));
        if (g_icon == nullptr)
            g_icon = reinterpret_cast<HICON>(SendMessageW(g_hWnd, WM_GETICON, ICON_SMALL, 0));
        if (g_icon == nullptr)
            g_icon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    return g_icon;
}

void Notify(bool add)
{
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_hWnd;
    nid.uID = TrayIconId;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = TrayMessage;
    nid.hIcon = Icon();

    wchar_t title[96] = {};
    GetWindowTextW(g_hWnd, title, static_cast<int>(std::size(title)));
    if (SceneFlag == MAIN_SCENE && Hero != nullptr && Hero->ID[0] != L'\0')
        swprintf(nid.szTip, std::size(nid.szTip), L"%ls (%ls)", title, Hero->ID);
    else
        swprintf(nid.szTip, std::size(nid.szTip), L"%ls", title);

    Shell_NotifyIconW(add ? NIM_ADD : NIM_DELETE, &nid);
}
} // namespace

void MuEmu::Ui::TrayMode::Toggle()
{
    if (g_hWnd == nullptr)
        return;
    if (g_hidden)
    {
        ShowWindow(g_hWnd, SW_SHOW);
        SetForegroundWindow(g_hWnd);
        Notify(false);
        g_hidden = false;
    }
    else
    {
        ShowWindow(g_hWnd, SW_HIDE);
        Notify(true);
        g_hidden = true;
    }
    mu::log::Get("ui")->info("[MuEmu] TrayMode: {}", g_hidden ? "oculto" : "visible");
}

bool MuEmu::Ui::TrayMode::IsHidden()
{
    return g_hidden;
}

bool MuEmu::Ui::TrayMode::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Bit 30: the key was already down (auto-repeat), so a held F12 toggles once.
    if (msg == WM_KEYDOWN && wParam == VK_F12 && (lParam & (1 << 30)) == 0)
    {
        Toggle();
        return true;
    }
    if (msg == TrayMessage)
    {
        if (lParam == WM_LBUTTONDBLCLK && g_hidden)
            Toggle();
        return true;
    }
    return false;
}

void MuEmu::Ui::TrayMode::Remove()
{
    if (g_hidden)
    {
        Notify(false);
        g_hidden = false;
    }
}

#else

void MuEmu::Ui::TrayMode::Toggle() {}
bool MuEmu::Ui::TrayMode::IsHidden()
{
    return false;
}
void MuEmu::Ui::TrayMode::Remove() {}

#endif
