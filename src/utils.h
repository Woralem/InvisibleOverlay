#pragma once

#include <string>
#include <windows.h>

// Константы приложения
constexpr size_t MAX_CHAT_HISTORY_STORAGE = 100;
constexpr size_t MAX_USER_INPUT_LENGTH = 10000;

// ID для горячих клавиш
constexpr int HOTKEY_ID_TOGGLE_VISIBILITY = 1;
constexpr int HOTKEY_ID_SEND_QUERY_F1 = 2;

// Сообщения приложения
constexpr UINT WM_CHAT_RESPONSE = WM_APP + 1;

// Функции конвертации кодировок
std::wstring utf8_to_wstring(const std::string& utf8_str);
std::string wstring_to_utf8(const std::wstring& wide_string);