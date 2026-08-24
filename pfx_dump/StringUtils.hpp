#pragma once

#include <string>
#include <algorithm>
#include <cctype>

inline bool containsIgnoreCase(const std::string& str, const std::string& subStr)
{
    if (subStr.empty()) return true;
    auto it = std::search(
        str.begin(), str.end(),
        subStr.begin(), subStr.end(),
        [](char ch1, char ch2) { return std::tolower(ch1) == std::tolower(ch2); }
    );
    return it != str.end();
}