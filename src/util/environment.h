#pragma once

#include <string>
#include <string_view>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace Util::Environment
{
    inline bool IsRunningUnderWine()
    {
        static const bool s_isWine = []() {
            const auto ntdll = ::GetModuleHandleA("ntdll.dll");
            return ntdll && (::GetProcAddress(ntdll, "wine_get_version") != nullptr);
        }();
        return s_isWine;
    }

    inline std::string GetWineVersion()
    {
        const auto ntdll = ::GetModuleHandleA("ntdll.dll");
        if (ntdll) {
            if (const auto get_ver = reinterpret_cast<const char*(__cdecl*)()>(::GetProcAddress(ntdll, "wine_get_version"))) {
                const char* ver = get_ver();
                return ver ? std::string(ver) : std::string();
            }
        }
        return {};
    }

    inline std::string GetWineHostSysname()
    {
        const auto ntdll = ::GetModuleHandleA("ntdll.dll");
        if (ntdll) {
            if (const auto get_host = reinterpret_cast<void(__cdecl*)(const char**, const char**)>(::GetProcAddress(ntdll, "wine_get_host_version"))) {
                const char* sysname = nullptr;
                const char* release = nullptr;
                get_host(&sysname, &release);
                if (sysname) {
                    std::string res = sysname;
                    if (release) {
                        res += " ";
                        res += release;
                    }
                    return res;
                }
            }
        }
        return {};
    }

    inline bool IsRunningOnDarwin()
    {
        const auto host = GetWineHostSysname();
        return host.find("Darwin") != std::string::npos;
    }
}
#else
namespace Util::Environment
{
    inline bool IsRunningUnderWine() { return false; }
    inline std::string GetWineVersion() { return {}; }
    inline std::string GetWineHostSysname() { return {}; }
    inline bool IsRunningOnDarwin() { return true; }
}
#endif
