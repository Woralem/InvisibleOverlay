#pragma once

#include <string>

// Используем void* вместо HWND для избежания включения windows.h
using WindowHandle = void*;

class NetworkClient {
public:
    explicit NetworkClient(std::string server_host = "127.0.0.1", int server_port = 5000);
    ~NetworkClient() = default;

    // Отправка сообщения на сервер
    void SendChatMessage(const std::wstring& message, const std::string& session_id, WindowHandle callback_window) const;

    // Структура для передачи ответа
    struct ChatResponse {
        std::wstring response_text;
        bool is_error;
    };

private:
    std::string host;
    int port;

    // Асинхронная отправка запроса
    void SendRequestAsync(std::string request_body, WindowHandle callback_window) const;
};