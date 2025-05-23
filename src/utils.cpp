#include "utils.h"

std::wstring utf8_to_wstring(const std::string& utf8_str) {
    if (utf8_str.empty()) return {};
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8_str.data(), static_cast<int>(utf8_str.size()), nullptr, 0);
    if (size_needed == 0) return {};
    std::wstring wide_string(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8_str.data(), static_cast<int>(utf8_str.size()), wide_string.data(), size_needed);
    return wide_string;
}

std::string wstring_to_utf8(const std::wstring& wide_str) {
    if (wide_str.empty()) return {};
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wide_str.data(), static_cast<int>(wide_str.size()), nullptr, 0, nullptr, nullptr);
    if (size_needed == 0) return {};
    std::string utf8_string(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide_str.data(), static_cast<int>(wide_str.size()), utf8_string.data(), size_needed, nullptr, nullptr);
    return utf8_string;
}