#include "stdafx.h"
#include "MuEmu/MuEmuTools.h"
#include "MuEmu/MuEmuUiFeatures.h"

#include "MuEmu/MuEmuPanel.h"
#include "MuEmu/MuEmuUi.h"

#include "Core/Utilities/Log/MuLogger.h"

#include <chrono>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
constexpr const char* EventsId = "events";
constexpr const char* CommandsId = "commands";

// Los bonus del servidor (BonusManager) usan los indices 120 a 139.
constexpr int FirstBonus = 120;
constexpr int LastBonus = 139;
// Un bonus con una fecha muy lejana solo se activa a mano.
constexpr int ManualBonusSeconds = 365 * 86400;

struct EventTime
{
    int index = 0;
    int seconds = -1;
};

std::map<int, std::string> g_eventNames;
std::vector<EventTime> g_events;
bool g_namesLoaded = false;
std::chrono::steady_clock::time_point g_lastTick;

struct Command
{
    std::string color;
    std::string command;
    std::string description;
};

std::vector<Command> g_commands;
bool g_commandsLoaded = false;

std::vector<std::string> ReadQuoted(const std::string& line, std::string& rest)
{
    std::vector<std::string> texts;
    std::string::size_type at = 0;
    for (;;)
    {
        const auto open = line.find('"', at);
        const auto close = open == std::string::npos ? std::string::npos : line.find('"', open + 1);
        if (close == std::string::npos)
        {
            rest += line.substr(at);
            break;
        }
        rest += line.substr(at, open - at) + " ";
        texts.push_back(line.substr(open + 1, close - open - 1));
        at = close + 1;
    }
    return texts;
}

std::string StripComment(const std::string& line)
{
    bool quoted = false;
    for (std::string::size_type i = 0; i + 1 < line.size(); ++i)
    {
        if (line[i] == '"')
            quoted = !quoted;
        else if (!quoted && line[i] == '/' && line[i + 1] == '/')
            return line.substr(0, i);
    }
    return line;
}

void LoadEventNames()
{
    g_namesLoaded = true;
    const auto fileStream = MuEmu::OpenData("Data/MuEmu/CustomEventTimeText.txt");
    std::istream& file = *fileStream;
    for (std::string line; std::getline(file, line);)
    {
        line = StripComment(line);
        std::string rest;
        const auto texts = ReadQuoted(line, rest);
        std::istringstream tokens(rest);
        int index = -1;
        if (!texts.empty() && (tokens >> index) && index >= 0)
            g_eventNames[index] = texts[0];
    }
}

void LoadCommands()
{
    g_commandsLoaded = true;

    // Colores de Main.dll: 1 dorado, 2 blanco, 3 naranja, 4 rojo, 5 verde, 6 amarillo, 7 violeta.
    static const char* const colors[] = { "#ffffff", "#ffd23c", "#ffffff", "#ff9a3c", "#ff5050",
                                          "#60e060", "#ffff79", "#c080ff" };

    const auto fileStream = MuEmu::OpenData("Data/MuEmu/CustomCommandInfo.txt");

    std::istream& file = *fileStream;
    for (std::string line; std::getline(file, line);)
    {
        line = StripComment(line);
        std::string rest;
        const auto texts = ReadQuoted(line, rest);
        std::istringstream tokens(rest);
        int index = -1;
        int color = 2;
        if (texts.empty() || !(tokens >> index >> color))
            continue;

        Command command;
        command.color = colors[(color >= 1 && color <= 7) ? color : 2];
        command.command = texts[0];
        command.description = texts.size() > 1 ? texts[1] : std::string();
        g_commands.push_back(std::move(command));
    }
}

std::string EventName(const EventTime& event, int position)
{
    if (!g_namesLoaded)
        LoadEventNames();

    auto found = g_eventNames.find(event.index);
    if (found == g_eventNames.end())
        found = g_eventNames.find(position);
    if (found != g_eventNames.end())
        return found->second;
    return "Evento " + std::to_string(event.index);
}

MuEmu::Panel::Content BuildEvents()
{
    MuEmu::Panel::Content content;
    content.title = "Eventos";
    content.header = { "Evento", "Comienza en" };

    std::vector<MuEmu::Panel::Row> bonus;
    for (std::size_t n = 0; n < g_events.size(); ++n)
    {
        const EventTime& event = g_events[n];
        if (event.seconds < 0)
            continue;

        const bool isBonus = event.index >= FirstBonus && event.index <= LastBonus;
        MuEmu::Panel::Row row;
        std::string time;
        if (event.seconds == 0)
        {
            time = isBonus ? "Activo" : "Online";
            row.color = "#60e060";
        }
        else if (isBonus && event.seconds > ManualBonusSeconds)
        {
            time = "Inactivo";
            row.color = "#a0a0a0";
        }
        else
        {
            time = MuEmu::Ui::FormatTime(event.seconds);
            if (event.seconds < 300)
                row.color = "#ffd23c";
        }

        row.cells = { EventName(event, static_cast<int>(n)), time };
        (isBonus ? bonus : content.rows).push_back(std::move(row));
    }

    if (!bonus.empty())
    {
        MuEmu::Panel::Row title;
        title.cells = { "Bonus", "" };
        title.color = "#ffd23c";
        content.rows.push_back(std::move(title));
        content.rows.insert(content.rows.end(), bonus.begin(), bonus.end());
    }

    if (content.rows.empty())
        content.lines = { "Esperando los horarios del servidor..." };
    return content;
}
} // namespace

void MuEmu::Ui::Events::Open()
{
    g_events.clear();
    Send(0xF3, 0xE8);
    MuEmu::Panel::Show(EventsId, BuildEvents(), nullptr);
}

void MuEmu::Ui::Events::OpenCommands()
{
    if (!g_commandsLoaded)
        LoadCommands();

    MuEmu::Panel::Content content;
    content.title = "Comandos";
    content.header = { "Comando", "Descripcion" };
    for (const Command& command : g_commands)
    {
        MuEmu::Panel::Row row;
        row.cells = { command.command, command.description };
        row.color = command.color;
        content.rows.push_back(std::move(row));
    }
    if (content.rows.empty())
        content.lines = { "Copiar CustomCommandInfo.txt a Data/MuEmu." };

    MuEmu::Panel::Show(CommandsId, std::move(content), nullptr);
}

bool MuEmu::Ui::Events::Receive(uint8_t head, uint8_t sub, const uint8_t* buffer, int size)
{
    // C2 F3 E8: cantidad (int en el byte 8) y por evento indice y segundos (int cada uno, desde el 12).
    if (head != 0xF3 || sub != 0xE8 || buffer[0] != 0xC2)
        return false;

    constexpr int HeaderSize = 12;
    if (size < HeaderSize)
        return true;

    const int count = ReadInt(buffer, 8);
    g_events.clear();
    for (int n = 0; n < count && HeaderSize + (n + 1) * 8 <= size; ++n)
    {
        EventTime event;
        event.index = ReadInt(buffer, HeaderSize + n * 8);
        event.seconds = ReadInt(buffer, HeaderSize + n * 8 + 4);
        g_events.push_back(event);
    }

    g_lastTick = std::chrono::steady_clock::now();
    mu::log::Get("ui")->info("[MuEmu] eventos recibidos: {}", g_events.size());
    MuEmu::Panel::Update(EventsId, BuildEvents());
    return true;
}

void MuEmu::Ui::Events::Frame()
{
    if (g_events.empty())
        return;

    // Como Main.dll, el reloj sigue en el cliente: un segundo menos por segundo.
    const auto now = std::chrono::steady_clock::now();
    if (now - g_lastTick < std::chrono::seconds(1))
        return;

    g_lastTick = now;
    for (EventTime& event : g_events)
    {
        if (event.seconds > 0 && event.seconds <= ManualBonusSeconds)
            --event.seconds;
    }
    MuEmu::Panel::Update(EventsId, BuildEvents());
}
