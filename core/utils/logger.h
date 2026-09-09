#pragma once

#include <format>
#include <print>
#include <cstdlib>

class Log
{
public:
    template <typename... Args>
    static void Info(std::format_string<Args...> fmt, Args&&... args)
    {
#ifndef NDEBUG
        std::println("[INFO] {}", std::format(fmt, std::forward<Args>(args)...));
#endif
    }

    template <typename... Args>
    static void Warn(std::format_string<Args...> fmt, Args&&... args)
    {
#ifndef NDEBUG
        std::println(stderr, "[WARN] {}", std::format(fmt, std::forward<Args>(args)...));
#endif
    }

    template <typename... Args>
    static void Error(std::format_string<Args...> fmt, Args&&... args)
    {
#ifndef NDEBUG
        std::println(stderr, "[ERROR] {}", std::format(fmt, std::forward<Args>(args)...));
#endif
    }

    template <typename... Args>
    static void Fatal(std::format_string<Args...> fmt, Args&&... args)
    {
#ifndef NDEBUG
        std::println(stderr, "[FATAL] {}", std::format(fmt, std::forward<Args>(args)...));
#endif
        std::abort();
    }
};
