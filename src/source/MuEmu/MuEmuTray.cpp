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
// (and the character's in game), and a double click on the icon shows the window again. The icon talks to
// a message-only window of its own: the shell may send its messages straight to the window procedure, and
// the game window's procedure is SDL's, which would drop them (only pumped messages reach WndProc).
namespace
{
constexpr UINT TrayIconId = 1;
constexpr UINT TrayMessage = WM_USER + 101;
constexpr UINT MenuRestore = 1;
constexpr UINT MenuExit = 2;

bool g_hidden = false;
HICON g_icon = nullptr;
HWND g_trayWnd = nullptr;

void ShowMenu(HWND hwnd)
{
    HMENU menu = CreatePopupMenu();
    if (menu == nullptr)
        return;
    AppendMenuW(menu, MF_STRING, MenuRestore, L"Restaurar");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, MenuExit, L"Salir");
    SetMenuDefaultItem(menu, MenuRestore, FALSE);

    POINT pt{};
    GetCursorPos(&pt);
    // Without the foreground the menu would not close on a click elsewhere (KB135788).
    SetForegroundWindow(hwnd);
    const UINT chosen = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
    PostMessageW(hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);

    if (chosen == MenuRestore)
        MuEmu::Ui::TrayMode::Toggle();
    else if (chosen == MenuExit && g_hWnd != nullptr)
        PostMessageW(g_hWnd, WM_CLOSE, 0, 0); // SDL turns it into the quit event
}

LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == TrayMessage)
    {
        switch (LOWORD(lParam))
        {
        case WM_LBUTTONDBLCLK:
            if (g_hidden)
                MuEmu::Ui::TrayMode::Toggle();
            break;
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            ShowMenu(hwnd);
            break;
        default:
            break;
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

HWND TrayWindow()
{
    if (g_trayWnd == nullptr)
    {
        const HINSTANCE instance = GetModuleHandleW(nullptr);
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = TrayWndProc;
        wc.hInstance = instance;
        wc.lpszClassName = L"MuEmuTrayMode";
        RegisterClassExW(&wc);
        g_trayWnd = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, nullptr);
    }
    return g_trayWnd;
}

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

bool Notify(bool add)
{
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = TrayWindow();
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

    return nid.hWnd != nullptr && Shell_NotifyIconW(add ? NIM_ADD : NIM_DELETE, &nid) != FALSE;
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
        // Without its icon the window could not come back.
        if (!Notify(true))
        {
            mu::log::Get("ui")->warn("[MuEmu] TrayMode: no se pudo crear el icono");
            return;
        }
        ShowWindow(g_hWnd, SW_HIDE);
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
