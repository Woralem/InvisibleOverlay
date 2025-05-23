#pragma once

#include <windows.h>
#include <memory>
#include "chat_renderer.h"
#include "network_client.h"

class OverlayWindow {
public:
    OverlayWindow(HINSTANCE hInstance);
    ~OverlayWindow();
    
    // Создание и отображение окна
    bool Create();
    void Show(bool show);
    
    // Получение дескриптора окна
    [[nodiscard]] HWND GetHWND() const { return hwnd; }

    // Обработка сообщений
    void ProcessMessages();

private:
    HINSTANCE hInstance;
    HWND hwnd;
    HBRUSH hbrBackground;

    std::unique_ptr<ChatRenderer> chatRenderer;
    std::unique_ptr<NetworkClient> networkClient;

    bool isVisible;
    std::string sessionId;

    // Обработчик сообщений окна
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

    // Обработчики событий
    void OnPaint() const;
    void OnKeyDown(WPARAM wParam) const;  // Добавить const
    void OnChar(WPARAM wParam) const;
    void OnHotkey(WPARAM wParam);
    void OnMouseWheel(WPARAM wParam) const;
    void OnChatResponse(WPARAM wParam) const;

    // Вспомогательные методы
    void SendCurrentInput() const;
    void CopyInputToClipboard() const;
    void PasteFromClipboard() const;
    void RegisterHotkeys() const;
    void UnregisterHotkeys() const;
};