#pragma once

#include <string>
#include <string_view>
#include "environment.h"
#include "settings.h"

namespace Util::Dialog
{
    inline void ShowWarning(const std::wstring& a_title, const std::wstring& a_message)
    {
#ifdef _WIN32
        if (Settings::General::bWineCompatibility.GetValue() && Environment::IsRunningUnderWine()) {
            // Under Wine / CrossOver with D3DMetal/DXVK, modal message boxes deadlock the display server
            // and freeze fullscreen rendering. Suppress modal dialog and log safely.
            REX::WARN("Wine/CrossOver detected: modal dialog suppressed for safety. Title: '{}', Message: '{}'"sv,
                std::string(a_title.begin(), a_title.end()),
                std::string(a_message.begin(), a_message.end()));
            return;
        }

        REX::W32::MessageBoxW(nullptr, a_message.c_str(), a_title.c_str(), MB_OK);
#else
        (void)a_title;
        (void)a_message;
#endif
    }
}
