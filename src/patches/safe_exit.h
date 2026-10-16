#pragma once
#include "fixes/precomputed_paths.h"
#include "fixes/texture_load_crash.h"
#include "util/dialog.h"

#include <spdlog/spdlog.h>

namespace Patches::SafeExit
{
    namespace detail
    {
        inline void Shutdown()
        {
            if (Settings::Warnings::bTextureLoadFailed.GetValue() && Fixes::TextureLoadCrash::detail::TotalLoadFails > 0) {
                REX::WARN("a total of {} textures failed to load in this session"sv, Fixes::TextureLoadCrash::detail::TotalLoadFails);
                std::wostringstream warningString;
                warningString << L"WARNING: " << Fixes::TextureLoadCrash::detail::TotalLoadFails << L" textures failed to load during this session. Please check EngineFixes.log for more details."sv;
                Util::Dialog::ShowWarning(L"Engine Fixes for Skyrim Special Edition", warningString.str());
            }
            if (Settings::Warnings::bPrecomputedPathHasErrors.GetValue() && Fixes::PrecomputedPaths::detail::HasIssues) {
                std::wostringstream warningString;
                warningString << L"WARNING: a precomputed path had issues, please check your EngineFixes.log for more details."sv;
                Util::Dialog::ShowWarning(L"Engine Fixes for Skyrim Special Edition", warningString.str());
            }
            spdlog::default_logger()->flush();
            ::TerminateProcess(::GetCurrentProcess(), EXIT_SUCCESS);
        }
    }

    inline void Install()
    {
        // Main::Shutdown called by WinMain
        REL::Relocation target{ RELOCATION_ID(35545, 36544), VAR_NUM(0x35, 0x1AE) };
        target.write_call<5>(detail::Shutdown);

        REX::INFO("installed safe exit patch"sv);
    }
}