#include "stdafx.h"
#include "MuEmu/MuEmuTools.h"

#include "Core/Utilities/Log/MuLogger.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

// Data/Local/SvenInfo.bmd, el archivo que arma GetMainInfo_SVEN.exe con el MainInfo.ini y los Custom\*.txt del
// servidor (como Main.bmd para Main.dll): "SVEN" y la version en claro; el resto cifrado como Main.bmd con la
// cantidad de entradas y cada una con su nombre (WORD + texto) y sus datos (DWORD + bytes). La entrada
// "SvenInfo.ini" trae la conexion ([Connection]) y los interruptores ([MuEmu]); las demas son los Custom\*.txt.
// Sin el archivo el cliente sigue con config.ini y Data/MuEmu como antes.
namespace
{
constexpr uint32_t FormatVersion = 1;

struct Info
{
    bool loaded = false;
    std::map<std::string, std::string> files;                         // nombre en minusculas -> contenido
    std::map<std::string, std::map<std::string, std::string>> values; // seccion -> clave -> valor (en minusculas)
};

std::string Lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return text;
}

std::string Trim(const std::string& text)
{
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

void ParseIni(Info& info, const std::string& text)
{
    std::istringstream stream(text);
    std::string section;
    for (std::string line; std::getline(stream, line);)
    {
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '/')
            continue;
        if (line.front() == '[' && line.back() == ']')
        {
            section = Lower(line.substr(1, line.size() - 2));
            continue;
        }
        const auto separator = line.find('=');
        if (separator != std::string::npos)
            info.values[section][Lower(Trim(line.substr(0, separator)))] = Trim(line.substr(separator + 1));
    }
}

bool Parse(Info& info, std::vector<uint8_t>& data)
{
    if (data.size() < 12 || std::string(data.begin(), data.begin() + 4) != "SVEN")
        return false;
    const uint32_t version = data[4] | (data[5] << 8) | (data[6] << 16) | (static_cast<uint32_t>(data[7]) << 24);
    if (version != FormatVersion)
        return false;

    // El cifrado de Main.bmd al reves (GetMainInfo: xor 0xDA^LOBYTE(n) y resta 0x95^HIBYTE(n)).
    uint8_t* body = data.data() + 8;
    const size_t size = data.size() - 8;
    for (size_t n = 0; n < size; ++n)
    {
        body[n] += static_cast<uint8_t>(0x95 ^ ((n >> 8) & 0xFF));
        body[n] ^= static_cast<uint8_t>(0xDA ^ (n & 0xFF));
    }

    size_t at = 0;
    auto read = [&](size_t bytes) -> const uint8_t*
    {
        if (at + bytes > size)
            return nullptr;
        const uint8_t* p = body + at;
        at += bytes;
        return p;
    };
    const uint8_t* countBytes = read(4);
    if (countBytes == nullptr)
        return false;
    const uint32_t count = countBytes[0] | (countBytes[1] << 8) | (countBytes[2] << 16) | (static_cast<uint32_t>(countBytes[3]) << 24);
    for (uint32_t n = 0; n < count; ++n)
    {
        const uint8_t* nameSize = read(2);
        if (nameSize == nullptr)
            return false;
        const uint8_t* name = read(nameSize[0] | (nameSize[1] << 8));
        const uint8_t* dataSize = name != nullptr ? read(4) : nullptr;
        if (dataSize == nullptr)
            return false;
        const size_t length = dataSize[0] | (dataSize[1] << 8) | (dataSize[2] << 16) | (static_cast<size_t>(dataSize[3]) << 24);
        const uint8_t* content = read(length);
        if (content == nullptr)
            return false;
        info.files[Lower(std::string(reinterpret_cast<const char*>(name), nameSize[0] | (nameSize[1] << 8)))] =
            std::string(reinterpret_cast<const char*>(content), length);
    }

    const auto ini = info.files.find("sveninfo.ini");
    if (ini != info.files.end())
        ParseIni(info, ini->second);
    return true;
}

const Info& Get()
{
    static Info info;
    static bool tried = false;
    if (tried)
        return info;
    tried = true;

    std::ifstream file("Data/Local/SvenInfo.bmd", std::ios::binary);
    if (!file)
        return info;
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (Parse(info, data))
    {
        info.loaded = true;
        mu::log::Get("data")->info("[MuEmu] SvenInfo.bmd: {} archivos", info.files.size());
    }
    else
    {
        info = Info{};
        mu::log::Get("data")->warn("[MuEmu] SvenInfo.bmd invalido: se usan config.ini y Data/MuEmu");
    }
    return info;
}
} // namespace

bool MuEmu::GetInfoValue(const char* section, const char* key, std::string& value)
{
    const Info& info = Get();
    const auto found = info.values.find(Lower(section));
    if (found == info.values.end())
        return false;
    const auto entry = found->second.find(Lower(key));
    if (entry == found->second.end())
        return false;
    value = entry->second;
    return true;
}

std::unique_ptr<std::istream> MuEmu::OpenData(const char* path)
{
    std::string name = path;
    const auto slash = name.find_last_of("/\\");
    if (slash != std::string::npos)
        name = name.substr(slash + 1);

    const Info& info = Get();
    const auto found = info.files.find(Lower(name));
    if (found != info.files.end())
        return std::make_unique<std::istringstream>(found->second);
    return std::make_unique<std::ifstream>(path);
}
