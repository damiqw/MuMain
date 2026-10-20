#include "stdafx.h"
#include "MuEmu/MuEmuServerInfo.h"

#include "Core/Text/Utf8.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
struct ServerText
{
    int index = 0;
    std::wstring group;
    std::wstring text;
};

std::vector<ServerText> g_texts;
bool g_loaded = false;

void Load()
{
    g_loaded = true;

    std::ifstream file("Data/MuEmu/CustomServerInfo.txt");
    for (std::string line; std::getline(file, line);)
    {
        const auto comment = line.find("//");
        if (comment != std::string::npos)
            line.erase(comment);

        // Servidor, "grupo" y "texto".
        std::vector<std::string> texts;
        std::string rest;
        for (std::string::size_type at = 0;;)
        {
            const auto open = line.find('"', at);
            const auto close = open == std::string::npos ? std::string::npos : line.find('"', open + 1);
            if (close == std::string::npos)
            {
                rest += line.substr(at);
                break;
            }
            rest += line.substr(at, open - at);
            texts.push_back(line.substr(open + 1, close - open - 1));
            at = close + 1;
        }

        std::istringstream tokens(rest);
        ServerText entry;
        if (texts.size() < 2 || !(tokens >> entry.index))
            continue;

        entry.group = Core::Text::FromUtf8(texts[0]);
        entry.text = Core::Text::FromUtf8(texts[1]);
        g_texts.push_back(std::move(entry));
    }
}
} // namespace

const wchar_t* MuEmu::ServerInfo::GetText(const wchar_t* groupName, int index)
{
    if (!g_loaded)
        Load();

    for (const ServerText& entry : g_texts)
    {
        if (entry.index == index && groupName != nullptr && entry.group == groupName)
            return entry.text.c_str();
    }
    return nullptr;
}
