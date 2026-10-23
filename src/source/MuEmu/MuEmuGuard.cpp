#include "stdafx.h"
#include "MuEmu/MuEmuUi.h"
#include "MuEmu/MuEmuUiFeatures.h"

#include "Core/Utilities/Log/MuLogger.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <tlhelp32.h>

extern HWND g_hWnd;
#endif

// La evidencia de hack (HackCheck.cpp, GCHackEvidenceRequestRecv): cuando el GameServer detecta algo (speed hack,
// latencia, flood) o un GM la pide, manda C1 FB 75 (motivo 64, detalles 128) y el cliente contesta C2 FB 76 con
// la ventana activa, las ventanas abiertas, las DLL cargadas fuera de Windows y los procesos sospechosos. En un
// pedido manual ("ManualRequest") agrega la lista completa de procesos en partes C2 FB 77. El servidor lo guarda
// en Log/HACK_EVIDENCIAS. Mismo formato que Main.dll (estructuras sin empaquetar: FB 76 lleva los dos int en 8 y
// 12 y el texto en 16; FB 77 lleva parte y total en 5 y 6 y el texto en 7).
namespace
{
#ifdef _WIN32
constexpr size_t ReportMax = 1650;
constexpr size_t ProcChunk = 1700;
constexpr size_t ProcMaxParts = 32;

const char* const HackKeywords[] = {
    "cheat", "speed", "artmoney", "olly", "x64dbg", "x32dbg", "wpe", "hook",
    "inject", "hidetool", "autoclick", "auto_click", "engine", "sandbox",
    "trainer", "macro", "clicker", "bypass", "spk", "packet", "proxy",
    "peheader", "scylla", "processhacker", "procexp", "wireshark",
    "fiddler", "charles", "dnspy", "ilspy", "memscan", "speedgear",
};

const char* const SystemProcessNames[] = {"svchost.exe", "csrss.exe", "lsass.exe", "services.exe", "smss.exe", "winlogon.exe"};

// Texto de Windows (UTF-16) al ANSI del servidor, como lo manda Main.dll.
std::string Narrow(const wchar_t* text)
{
    if (text == nullptr || text[0] == L'\0')
        return {};
    const int length = WideCharToMultiByte(CP_ACP, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1)
        return {};
    std::string result(static_cast<size_t>(length - 1), '\0');
    WideCharToMultiByte(CP_ACP, 0, text, -1, result.data(), length, nullptr, nullptr);
    return result;
}

std::string Lower(std::string text)
{
    for (char& ch : text)
        ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
    return text;
}

bool HasHackKeyword(const std::string& lower)
{
    for (const char* keyword : HackKeywords)
    {
        if (lower.find(keyword) != std::string::npos)
            return true;
    }
    return false;
}

bool IsWindowsPath(const std::string& lower, bool withWinSxs)
{
    return lower.find("\\windows\\system32\\") != std::string::npos || lower.find("\\windows\\syswow64\\") != std::string::npos ||
           (withWinSxs && lower.find("\\windows\\winsxs\\") != std::string::npos);
}

std::string ProcessPath(DWORD pid)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process == nullptr)
        return {};
    wchar_t path[MAX_PATH] = {};
    DWORD length = MAX_PATH;
    std::string result;
    if (QueryFullProcessImageNameW(process, 0, path, &length) != FALSE)
        result = Narrow(path);
    CloseHandle(process);
    return result;
}

void Append(std::string& report, const std::string& line, size_t limit)
{
    if (report.size() + line.size() < limit)
        report += line;
}

struct WindowScan
{
    std::string* report;
    int count;
    int suspicious;
};

BOOL CALLBACK ScanWindow(HWND hwnd, LPARAM lParam)
{
    if (!IsWindowVisible(hwnd) || hwnd == g_hWnd)
        return TRUE;

    wchar_t titleW[256] = {};
    if (GetWindowTextW(hwnd, titleW, 256) <= 0)
        return TRUE;
    const std::string title = Narrow(titleW);
    if (title == "Program Manager" || title == "Default IME" || title == "MSCTFIME UI" || title == "Settings" ||
        title == "Windows Input Experience")
        return TRUE;

    auto* scan = reinterpret_cast<WindowScan*>(lParam);
    if (HasHackKeyword(Lower(title)))
    {
        scan->suspicious++;
        Append(*scan->report, " [ALERTA VENTANA] " + title + "\r\n", 1600);
    }
    else if (scan->count < 7)
    {
        scan->count++;
        Append(*scan->report, " [Ventana] " + title + "\r\n", 1600);
    }
    return TRUE;
}

void PutInt(std::vector<uint8_t>& packet, size_t offset, int value)
{
    for (int n = 0; n < 4; ++n)
        packet[offset + n] = static_cast<uint8_t>((static_cast<uint32_t>(value) >> (8 * n)) & 0xFF);
}

// C2: tamaño en big endian en [1] y [2], como PSWMSG_HEAD.
std::vector<uint8_t> NewC2(uint8_t sub, size_t size)
{
    std::vector<uint8_t> packet(size, 0);
    packet[0] = 0xC2;
    packet[1] = static_cast<uint8_t>((size >> 8) & 0xFF);
    packet[2] = static_cast<uint8_t>(size & 0xFF);
    packet[3] = 0xFB;
    packet[4] = sub;
    return packet;
}

// La lista completa de procesos (solo en los pedidos manuales), en partes C2 FB 77.
void SendFullProcessList()
{
    std::vector<std::string> parts;
    std::string current;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        if (Process32FirstW(snapshot, &entry))
        {
            do
            {
                const std::string path = ProcessPath(entry.th32ProcessID);
                char line[MAX_PATH * 2 + 96];
                std::snprintf(line, sizeof(line), " PID %5lu | PPID %5lu | Hilos %3lu | %s -> %s\r\n", entry.th32ProcessID,
                              entry.th32ParentProcessID, entry.cntThreads, Narrow(entry.szExeFile).c_str(),
                              path.empty() ? "[ruta no accesible]" : path.c_str());
                if (current.size() + std::strlen(line) > ProcChunk)
                {
                    parts.push_back(current);
                    current.clear();
                    if (parts.size() >= ProcMaxParts)
                        break;
                }
                current += line;
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
    }

    if (!current.empty() && parts.size() < ProcMaxParts)
        parts.push_back(current);
    if (parts.empty())
        parts.push_back(" [No se pudo obtener la lista de procesos]\r\n");

    for (size_t n = 0; n < parts.size(); ++n)
    {
        std::vector<uint8_t> packet = NewC2(0x77, 7 + parts[n].size() + 1);
        packet[5] = static_cast<uint8_t>(n);
        packet[6] = static_cast<uint8_t>(parts.size());
        std::memcpy(packet.data() + 7, parts[n].c_str(), parts[n].size() + 1);
        MuEmu::Ui::SendRaw(packet);
    }
}

void SendEvidence(bool manual)
{
    std::string report;
    report.reserve(1800);
    int processCount = 0;
    int moduleCount = 0;

    // 1. La ventana activa.
    wchar_t foregroundW[128] = {};
    if (HWND foreground = GetForegroundWindow(); foreground != nullptr)
        GetWindowTextW(foreground, foregroundW, 128);
    const std::string foregroundTitle = Narrow(foregroundW);
    report += "Ventana activa: [" + (foregroundTitle.empty() ? std::string("Desconocida") : foregroundTitle) + "]\r\n";

    // 2. Las ventanas abiertas del escritorio.
    report += "--- VENTANAS DEL ESCRITORIO (SEGUNDO PLANO) ---\r\n";
    WindowScan scan{&report, 0, 0};
    EnumWindows(ScanWindow, reinterpret_cast<LPARAM>(&scan));
    if (scan.count == 0 && scan.suspicious == 0)
        report += " [Ninguna ventana adicional visible]\r\n";

    // 3. Las DLL cargadas en el cliente que no son de Windows (posibles inyecciones).
    report += "--- MODULOS FUERA DE WINDOWS (POSIBLES INYECCIONES) ---\r\n";
    int suspiciousModules = 0;
    HANDLE modules = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (modules != INVALID_HANDLE_VALUE)
    {
        MODULEENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        if (Module32FirstW(modules, &entry))
        {
            do
            {
                moduleCount++;
                const std::string path = Narrow(entry.szExePath);
                if (!IsWindowsPath(Lower(path), true))
                {
                    suspiciousModules++;
                    Append(report, " [" + Narrow(entry.szModule) + "] -> " + path + "\r\n", 650);
                }
            } while (Module32NextW(modules, &entry));
        }
        CloseHandle(modules);
    }
    if (suspiciousModules == 0)
        report += " [Ninguna DLL sospechosa detectada]\r\n";

    // 4. Procesos sospechosos: por nombre, o de sistema pero fuera de System32 (camuflados).
    report += "--- PROCESOS SOSPECHOSOS EN WINDOWS ---\r\n";
    int suspiciousProcesses = 0;
    HANDLE processes = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (processes != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        if (Process32FirstW(processes, &entry))
        {
            do
            {
                processCount++;
                const std::string name = Narrow(entry.szExeFile);
                const std::string nameLower = Lower(name);
                char line[MAX_PATH * 2 + 100];
                if (HasHackKeyword(nameLower))
                {
                    suspiciousProcesses++;
                    const std::string path = ProcessPath(entry.th32ProcessID);
                    if (!path.empty())
                        std::snprintf(line, sizeof(line), " [ALERTA PID: %5lu] %s -> %s\r\n", entry.th32ProcessID, name.c_str(), path.c_str());
                    else
                        std::snprintf(line, sizeof(line), " [ALERTA PID: %5lu] %s\r\n", entry.th32ProcessID, name.c_str());
                    Append(report, line, 1600);
                    continue;
                }

                bool systemName = false;
                for (const char* systemProcess : SystemProcessNames)
                    systemName = systemName || nameLower == systemProcess;
                if (!systemName)
                    continue;
                const std::string path = ProcessPath(entry.th32ProcessID);
                if (!path.empty() && !IsWindowsPath(Lower(path), false))
                {
                    suspiciousProcesses++;
                    std::snprintf(line, sizeof(line), " [ALERTA PROCESO CAMUFLADO PID: %5lu] %s -> %s\r\n", entry.th32ProcessID,
                                  name.c_str(), path.c_str());
                    Append(report, line, 1600);
                }
            } while (Process32NextW(processes, &entry));
        }
        CloseHandle(processes);
    }
    if (suspiciousProcesses == 0)
        report += " [Ningun proceso sospechoso detectado]\r\n";

    char summary[128];
    std::snprintf(summary, sizeof(summary), "Total procesos: %d | Total modulos: %d\r\n", processCount, moduleCount);
    report += summary;
    if (report.size() > ReportMax)
        report.resize(ReportMax);

    // sizeof(SDHP_HACK_EVIDENCE_SEND) es 20 (el texto en 16): el paquete mide 20 - 1 + texto + 1.
    std::vector<uint8_t> packet = NewC2(0x76, 20 + report.size());
    PutInt(packet, 8, processCount);
    PutInt(packet, 12, moduleCount);
    std::memcpy(packet.data() + 16, report.c_str(), report.size() + 1);
    MuEmu::Ui::SendRaw(packet);

    mu::log::Get("ui")->info("[MuEmu] Evidencia enviada al servidor ({} procesos, {} modulos{})", processCount, moduleCount,
                             manual ? ", pedido manual" : "");
    if (manual)
        SendFullProcessList();
}
#endif
} // namespace

bool MuEmu::Ui::Evidence::Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size)
{
    // C1 FB 75: motivo (64) en 4 y detalles (128) en 68.
    if (head != 0xFB || sub != 0x75 || buffer[0] != 0xC1)
        return false;
#ifdef _WIN32
    const std::string reason = size >= 4 + 64 ? ReadText(buffer, 4, 64) : std::string();
    SendEvidence(reason == "ManualRequest");
#endif
    return true;
}

