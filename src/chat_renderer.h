#pragma once

#include <windows.h>
#include <string>
#include <vector>

class ChatRenderer {
public:
    ChatRenderer();
    ~ChatRenderer();

    // Отрисовка чата
    void DrawChat(HDC hdc, RECT clientRect, HWND hwnd);
    
    // Управление историей чата
    void AddMessage(const std::wstring& message);
    void ClearHistory();
    
    // Управление вводом пользователя
    void SetUserInput(const std::wstring& input);
    [[nodiscard]] std::wstring GetUserInput() const;
    void ClearUserInput();

    // Управление статусом
    void SetStatusMessage(const std::wstring& status);

    // Управление прокруткой
    void ScrollUp(int pixels);
    void ScrollDown(int pixels);
    void ScrollToBottom();
    void UpdateScrollOffset(int visible_height, int total_height);

    // Управление курсором ввода
    void ScrollInputLeft();
    void ScrollInputRight();
    void ScrollInputHome();
    void ScrollInputEnd();
    void UpdateInputScroll();

    // Состояние
    [[nodiscard]] bool IsWaitingForResponse() const { return waiting_for_response; }
    void SetWaitingForResponse(bool waiting) { waiting_for_response = waiting; }

private:
    std::vector<std::wstring> display_chat_history;
    std::wstring current_status_message;
    std::wstring user_input_buffer;
    
    int user_input_scroll_offset_chars;
    int user_input_visible_chars_estimate;
    
    int chat_scroll_offset_y;
    int total_text_block_height;
    int visible_chat_area_height;
    bool auto_scroll_to_bottom;
    bool waiting_for_response;
    
    HFONT hFont;
};