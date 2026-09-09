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
    std::optional<uint32_t> test_auto_close_ms;
    std::optional<std::string> host;
    std::optional<uint16_t> port;
};
