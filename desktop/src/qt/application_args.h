#pragma once
#include <optional>
#include <string>
#include <QString>

struct ApplicationArguments
{
    std::optional<std::string> rom_path;
    std::optional<std::string> save_path;
    std::optional<bool> server_mode;
    bool no_menu = false;
    std::optional<std::string> host;
    std::optional<uint16_t> port;
};
