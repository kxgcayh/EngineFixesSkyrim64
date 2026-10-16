#pragma once

#include <cctype>
#include <filesystem>
#include <string>

namespace Util::CoSaves
{
    inline bool HasSKSEExtension(const std::filesystem::path& a_path)
    {
        std::string ext = a_path.extension().string();
        for (auto& c : ext) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return ext == ".skse";
    }

    bool Clean();
}