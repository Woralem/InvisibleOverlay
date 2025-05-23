#include "chat_renderer.h"
#include "utils.h"
#include <algorithm>
#include <sstream>

ChatRenderer::ChatRenderer() 
    : user_input_scroll_offset_chars(0)
    , user_input_visible_chars_estimate(30)
    , chat_scroll_offset_y(0)
    , total_text_block_height(0)
    , visible_chat_area_height(0)
    , auto_scroll_to_bottom(true)
    , waiting_for_response(false)
    , hFont(nullptr) {
    current_status_message = L"Press INSERT. Ctrl+C/V. Enter/F1 to send. L/R Arrow for input scroll.";
}

ChatRenderer::~ChatRenderer() {
    if (hFont && hFont != GetStockObject(DEFAULT_GUI_FONT)) {
        DeleteObject(hFont);
    }
}

void ChatRenderer::DrawChat(HDC hdc, RECT clientRect, HWND hwnd) {
    SetTextColor(hdc, RGB(220, 220, 220));
    SetBkMode(hdc, TRANSPARENT);

    if (!hFont) {
        hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        if (!hFont) hFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    }

    auto hOldFont = static_cast<HFONT>(SelectObject(hdc, hFont));

    // Область чата
    RECT chat_content_rect = {10, 10, clientRect.right - 10, clientRect.bottom - 40};
    visible_chat_area_height = chat_content_rect.bottom - chat_content_rect.top;

    // Подготовка текста для отрисовки
    std::wstring full_text_for_calc_and_draw;
    for(const auto& line : display_chat_history) {
        full_text_for_calc_and_draw += line + L"\r\n";
    }
    if(!current_status_message.empty() && (waiting_for_response || display_chat_history.empty())) {
        full_text_for_calc_and_draw += current_status_message + L"\r\n";
    }
    if (full_text_for_calc_and_draw.empty() && !waiting_for_response) {
        full_text_for_calc_and_draw = L"Chat history is empty. Type your message below.\r\n";
    }
    else if (full_text_for_calc_and_draw.empty() && waiting_for_response) {
        full_text_for_calc_and_draw = L" ";
    }

    // Расчет высоты текста
    RECT calcRect = chat_content_rect;
    DrawTextW(hdc, full_text_for_calc_and_draw.c_str(), -1, &calcRect,
              DT_CALCRECT | DT_WORDBREAK | DT_EDITCONTROL);
    total_text_block_height = calcRect.bottom - calcRect.top;

    UpdateScrollOffset(visible_chat_area_height, total_text_block_height);

    // Отрисовка текста чата
    RECT drawing_rect = chat_content_rect;
    drawing_rect.top -= chat_scroll_offset_y;
    drawing_rect.bottom = drawing_rect.top + total_text_block_height;
    DrawTextW(hdc, full_text_for_calc_and_draw.c_str(), -1, &drawing_rect,
              DT_TOP | DT_LEFT | DT_WORDBREAK | DT_EDITCONTROL);

    // Отрисовка поля ввода
    std::wstring input_prompt_prefix = L"Input: ";
    RECT input_field_background_rect = {10, clientRect.bottom - 30,
                                       clientRect.right - 10, clientRect.bottom - 10};

    SIZE prefixSize;
    GetTextExtentPoint32W(hdc, input_prompt_prefix.c_str(),
                         static_cast<int>(input_prompt_prefix.length()), &prefixSize);
    TextOutW(hdc, input_field_background_rect.left + 2,
            input_field_background_rect.top + (input_field_background_rect.bottom -
            input_field_background_rect.top - prefixSize.cy) / 2,
            input_prompt_prefix.c_str(), static_cast<int>(input_prompt_prefix.length()));

    // Область видимого текста ввода
    RECT text_input_visible_area_rect = input_field_background_rect;
    text_input_visible_area_rect.left += prefixSize.cx + 5;
    text_input_visible_area_rect.right -= 2;
    text_input_visible_area_rect.top += 2;
    text_input_visible_area_rect.bottom -= 2;

    // Расчет видимых символов
    if (prefixSize.cx > 0) {
        TEXTMETRICW tm;
        GetTextMetricsW(hdc, &tm);
        int avg_char_w = tm.tmAveCharWidth > 0 ? tm.tmAveCharWidth : 8;
        user_input_visible_chars_estimate = (text_input_visible_area_rect.right -
                                           text_input_visible_area_rect.left) / avg_char_w;
        if (user_input_visible_chars_estimate < 1) user_input_visible_chars_estimate = 1;
    }

    // Обновление прокрутки ввода
    int max_scroll_offset = static_cast<int>(user_input_buffer.length()) - user_input_visible_chars_estimate;
    if (max_scroll_offset < 0) max_scroll_offset = 0;
    user_input_scroll_offset_chars = std::max(0, std::min(user_input_scroll_offset_chars, max_scroll_offset));

    // Получение видимой части ввода
    std::wstring visible_input_part;
    if (user_input_buffer.length() > static_cast<size_t>(user_input_scroll_offset_chars)) {
        visible_input_part = user_input_buffer.substr(user_input_scroll_offset_chars,
                                                     user_input_visible_chars_estimate);
    }

    // Добавление курсора
    std::wstring text_to_draw_in_field = visible_input_part;
    if (hwnd && GetFocus() == hwnd) {
        static bool show_cursor_char = true;
        static DWORD last_blink_time = 0;
        if (GetTickCount() - last_blink_time > 500) {
            show_cursor_char = !show_cursor_char;
            last_blink_time = GetTickCount();
            InvalidateRect(hwnd, &input_field_background_rect, FALSE);
        }

        if (show_cursor_char) {
            int relative_cursor_pos = static_cast<int>(user_input_buffer.length()) -
                                    user_input_scroll_offset_chars;
            if(relative_cursor_pos >= 0 && relative_cursor_pos <= static_cast<int>(text_to_draw_in_field.length())) {
                text_to_draw_in_field.insert(relative_cursor_pos, L"_");
            } else if (text_to_draw_in_field.empty() && user_input_buffer.empty() &&
                      user_input_scroll_offset_chars == 0) {
                text_to_draw_in_field = L"_";
            } else {
                text_to_draw_in_field += L"_";
            }
        }
    }

    // Отрисовка текста ввода с обрезкой
    HRGN hRgn = CreateRectRgn(text_input_visible_area_rect.left, text_input_visible_area_rect.top,
                             text_input_visible_area_rect.right, text_input_visible_area_rect.bottom);
    SelectClipRgn(hdc, hRgn);
    DrawTextW(hdc, text_to_draw_in_field.c_str(), -1, &text_input_visible_area_rect,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    SelectClipRgn(hdc, nullptr);
    DeleteObject(hRgn);

    SelectObject(hdc, hOldFont);
}

void ChatRenderer::AddMessage(const std::wstring& message) {
    display_chat_history.push_back(message);
    if (display_chat_history.size() > MAX_CHAT_HISTORY_STORAGE) {
        auto elements_to_remove = static_cast<std::vector<std::wstring>::difference_type>(
            display_chat_history.size() - MAX_CHAT_HISTORY_STORAGE);
        display_chat_history.erase(display_chat_history.begin(),
                                 display_chat_history.begin() + elements_to_remove);
    }
    auto_scroll_to_bottom = true;
}

void ChatRenderer::ClearHistory() {
    display_chat_history.clear();
}

void ChatRenderer::SetUserInput(const std::wstring& input) {
    user_input_buffer = input;
    UpdateInputScroll();
}

std::wstring ChatRenderer::GetUserInput() const {
    return user_input_buffer;
}

void ChatRenderer::ClearUserInput() {
    user_input_buffer.clear();
    user_input_scroll_offset_chars = 0;
}

void ChatRenderer::SetStatusMessage(const std::wstring& status) {
    current_status_message = status;
}

void ChatRenderer::ScrollUp(int pixels) {
    chat_scroll_offset_y -= pixels;
    auto_scroll_to_bottom = false;
}

void ChatRenderer::ScrollDown(int pixels) {
    chat_scroll_offset_y += pixels;
    auto_scroll_to_bottom = false;
}

void ChatRenderer::ScrollToBottom() {
    auto_scroll_to_bottom = true;
}

void ChatRenderer::UpdateScrollOffset(int visible_height, int total_height) {
    if (auto_scroll_to_bottom && total_height > visible_height) {
        chat_scroll_offset_y = total_height - visible_height;
    }
    if (total_height <= visible_height) {
        chat_scroll_offset_y = 0;
    } else {
        chat_scroll_offset_y = std::max(0, std::min(chat_scroll_offset_y, total_height - visible_height));
    }
}

void ChatRenderer::ScrollInputLeft() {
    if (user_input_scroll_offset_chars > 0) {
        user_input_scroll_offset_chars--;
    }
}

void ChatRenderer::ScrollInputRight() {
    int max_scroll = static_cast<int>(user_input_buffer.length()) - user_input_visible_chars_estimate;
    if (user_input_scroll_offset_chars < std::max(0, max_scroll)) {
        user_input_scroll_offset_chars++;
    }
}

void ChatRenderer::ScrollInputHome() {
    user_input_scroll_offset_chars = 0;
}

void ChatRenderer::ScrollInputEnd() {
    int max_scroll = static_cast<int>(user_input_buffer.length()) - user_input_visible_chars_estimate;
    user_input_scroll_offset_chars = std::max(0, max_scroll);
}

void ChatRenderer::UpdateInputScroll() {
    int current_text_len = static_cast<int>(user_input_buffer.length());
    if (current_text_len > user_input_scroll_offset_chars + user_input_visible_chars_estimate) {
        user_input_scroll_offset_chars = current_text_len - user_input_visible_chars_estimate;
    }
}