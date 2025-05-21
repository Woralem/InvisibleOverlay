#define UNICODE
#define _UNICODE

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <thread>
#include <algorithm>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <fcntl.h>
#include <io.h>

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

#define HOTKEY_ID_TOGGLE_VISIBILITY 1
#define HOTKEY_ID_SEND_QUERY_F1    2

HINSTANCE hInst;
HWND hwndOverlay;
const wchar_t CLASS_NAME[] = L"GeminiChatOverlayScrollableClass";
HBRUSH g_hbrBackground = NULL;

std::vector<std::wstring> display_chat_history;
std::wstring current_status_message = L"Press INSERT to toggle. Type/scroll. Enter/F1 to send.";
std::wstring user_input_buffer = L"";
bool is_overlay_visible = true;
bool waiting_for_response = false;
std::string current_session_id = "default_cpp_session_scroll";

int chat_scroll_offset_y = 0;
int total_text_block_height = 0;
int visible_chat_area_height = 0;
bool auto_scroll_to_bottom = true;

const size_t MAX_CHAT_HISTORY_STORAGE = 100;

std::wstring utf8_to_wstring(const std::string& utf8_str) {
    if (utf8_str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &utf8_str[0], (int)utf8_str.size(), NULL, 0);
    if (size_needed == 0) return std::wstring();
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &utf8_str[0], (int)utf8_str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}
std::string wstring_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    if (size_needed == 0) return std::string();
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

void SendToServerHttpLib(const std::wstring& user_message_wstr) {
    if (waiting_for_response) {
        current_status_message = L"Waiting for previous response...";
        if (hwndOverlay) InvalidateRect(hwndOverlay, NULL, TRUE);
        return;
    }
    waiting_for_response = true;
    display_chat_history.push_back(L"You: " + user_message_wstr);
    if (display_chat_history.size() > MAX_CHAT_HISTORY_STORAGE) {
        display_chat_history.erase(display_chat_history.begin(),
                                   display_chat_history.begin() + (display_chat_history.size() - MAX_CHAT_HISTORY_STORAGE));
    }
    auto_scroll_to_bottom = true;
    current_status_message = L"Sending to server (httplib)...";
    if (hwndOverlay) InvalidateRect(hwndOverlay, NULL, TRUE);

    std::string user_message_utf8 = wstring_to_utf8(user_message_wstr);
    nlohmann::json request_json;
    request_json["message"] = user_message_utf8;
    request_json["session_id"] = current_session_id;
    std::string request_body_str = request_json.dump();

    std::thread([request_body_str]() {
        httplib::Client cli("127.0.0.1", 5000);
        cli.set_connection_timeout(10);
        cli.set_read_timeout(60);
        cli.set_write_timeout(10);
        std::wstring new_response_text_for_ui;
        try {
            if (auto res = cli.Post("/chat", request_body_str, "application/json")) {
                if (res->status == 200) {
                    nlohmann::json response_json = nlohmann::json::parse(res->body);
                    if (response_json.contains("reply")) {
                        new_response_text_for_ui = utf8_to_wstring(response_json["reply"].get<std::string>());
                    } else if (response_json.contains("error")) {
                        new_response_text_for_ui = L"Server Error (httplib): " + utf8_to_wstring(response_json["error"].get<std::string>());
                    } else { new_response_text_for_ui = L"Error (httplib): Unexpected server response structure."; }
                } else {
                    std::wstringstream ss; ss << L"HTTP Error (httplib): " << res->status;
                    if (!res->body.empty()) {
                         try {
                            nlohmann::json error_json_body = nlohmann::json::parse(res->body);
                            if(error_json_body.contains("error")) { ss << L" - " << utf8_to_wstring(error_json_body["error"].get<std::string>()); }
                            else { ss << L" - Body: " << utf8_to_wstring(res->body.substr(0,200)); }
                         } catch (const nlohmann::json::parse_error&) { ss << L" - Body: " << utf8_to_wstring(res->body.substr(0,200)); }
                    } new_response_text_for_ui = ss.str();
                }
            } else { auto err = res.error(); new_response_text_for_ui = L"Request Error (httplib): " + utf8_to_wstring(httplib::to_string(err)); }
        } catch (const nlohmann::json::parse_error& e) { new_response_text_for_ui = L"JSON Parse Error (httplib): " + utf8_to_wstring(e.what()); }
        catch (const std::exception& e) { new_response_text_for_ui = L"Exception (httplib/std): " + utf8_to_wstring(e.what()); }
        struct UpdatePayload { std::wstring response_for_ui; };
        UpdatePayload* payload = new UpdatePayload(); payload->response_for_ui = new_response_text_for_ui;
        if(hwndOverlay) PostMessage(hwndOverlay, WM_APP + 1, reinterpret_cast<WPARAM>(payload), 0);
        waiting_for_response = false;
    }).detach();
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateOverlayWindow(HINSTANCE hInstance);

void AdjustScrollOffset(int visible_height, int total_height) {
    if (auto_scroll_to_bottom && total_height > visible_height) {
        chat_scroll_offset_y = total_height - visible_height;
    }
    if (total_height <= visible_height) {
        chat_scroll_offset_y = 0;
    } else {
        chat_scroll_offset_y = std::max(0, std::min(chat_scroll_offset_y, total_height - visible_height));
    }
}

void DrawChat(HDC hdc, RECT clientRect) {
    SetTextColor(hdc, RGB(220, 220, 220));
    SetBkMode(hdc, TRANSPARENT);
    HFONT hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    if (!hFont) hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    RECT chat_content_rect = {10, 10, clientRect.right - 10, clientRect.bottom - 40}; // Область для текста чата
    visible_chat_area_height = chat_content_rect.bottom - chat_content_rect.top;

    std::wstring full_text_for_calc_and_draw;
    for(const auto& line : display_chat_history) {
        full_text_for_calc_and_draw += line + L"\r\n";
    }
    if(!current_status_message.empty() && (waiting_for_response || display_chat_history.empty())) {
         full_text_for_calc_and_draw += current_status_message + L"\r\n";
    }
    if (full_text_for_calc_and_draw.empty()) full_text_for_calc_and_draw = L" ";

    RECT calcRect = chat_content_rect;
    DrawTextW(hdc, full_text_for_calc_and_draw.c_str(), -1, &calcRect, DT_CALCRECT | DT_WORDBREAK | DT_EDITCONTROL);
    total_text_block_height = calcRect.bottom - calcRect.top;

    AdjustScrollOffset(visible_chat_area_height, total_text_block_height);

    RECT drawing_rect = chat_content_rect;
    drawing_rect.top -= chat_scroll_offset_y;
    drawing_rect.bottom = drawing_rect.top + total_text_block_height;

    DrawTextW(hdc, full_text_for_calc_and_draw.c_str(), -1, &drawing_rect, DT_TOP | DT_LEFT | DT_WORDBREAK | DT_EDITCONTROL);

    std::wstring input_prompt = L"Input: " + user_input_buffer;
    if (is_overlay_visible) input_prompt += L"_";
    RECT input_rect = {10, clientRect.bottom - 30, clientRect.right - 10, clientRect.bottom - 10};
    DrawTextW(hdc, input_prompt.c_str(), -1, &input_rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    SelectObject(hdc, hOldFont);
    if (hFont != GetStockObject(DEFAULT_GUI_FONT)) DeleteObject(hFont);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT cr; GetClientRect(hwnd, &cr);
            FillRect(hdc, &ps.rcPaint, g_hbrBackground ? g_hbrBackground : (HBRUSH)GetStockObject(DKGRAY_BRUSH));
            DrawChat(hdc, cr);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProcW(hwnd, uMsg, wParam, lParam);
            return (hit == HTCLIENT) ? HTCAPTION : hit;
        }
        case WM_CHAR: {
            if (is_overlay_visible && !waiting_for_response) {
                if (wParam == VK_BACK) { if (!user_input_buffer.empty()) user_input_buffer.pop_back(); }
                else if (wParam == VK_RETURN) { if (!user_input_buffer.empty()) SendToServerHttpLib(user_input_buffer); }
                else if (wParam == VK_ESCAPE) { user_input_buffer.clear(); }
                else if (iswprint(static_cast<wint_t>(wParam)) || wParam == L' ') {
                     if(user_input_buffer.length() < 200) user_input_buffer += static_cast<wchar_t>(wParam);
                }
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }
        case WM_HOTKEY: {
            if (wParam == HOTKEY_ID_TOGGLE_VISIBILITY) {
                is_overlay_visible = !is_overlay_visible;
                ShowWindow(hwndOverlay, is_overlay_visible ? SW_SHOW : SW_HIDE);
                if (is_overlay_visible) { current_status_message = L"Overlay shown. Type/scroll. Enter/F1."; InvalidateRect(hwnd, NULL, TRUE); }
            } else if (wParam == HOTKEY_ID_SEND_QUERY_F1) {
                 if (is_overlay_visible && !user_input_buffer.empty() && !waiting_for_response) SendToServerHttpLib(user_input_buffer);
            }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            if (!is_overlay_visible) return 0;

            short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            int scroll_lines_per_tick = 3;
            int scroll_pixels = scroll_lines_per_tick * 16;

            if (zDelta > 0) {
                chat_scroll_offset_y -= scroll_pixels;
            } else {
                chat_scroll_offset_y += scroll_pixels;
            }

            auto_scroll_to_bottom = false;
            InvalidateRect(hwndOverlay, NULL, TRUE);
            return 0;
        }
        case WM_APP + 1: {
            struct UpdatePayload { std::wstring response_for_ui; };
            UpdatePayload* payload = reinterpret_cast<UpdatePayload*>(wParam);

            bool is_error = (payload->response_for_ui.find(L"Error:") != std::wstring::npos || payload->response_for_ui.find(L"Exception:") != std::wstring::npos);

            if (is_error) {
                display_chat_history.push_back(payload->response_for_ui);
            } else {
                display_chat_history.push_back(L"Gemini: " + payload->response_for_ui);
                user_input_buffer.clear();
            }
            current_status_message = L"";

            if(display_chat_history.size() > MAX_CHAT_HISTORY_STORAGE) {
                display_chat_history.erase(display_chat_history.begin(),
                                           display_chat_history.begin() + (display_chat_history.size() - MAX_CHAT_HISTORY_STORAGE));
            }

            auto_scroll_to_bottom = true;
            delete payload;
            if(hwndOverlay) InvalidateRect(hwndOverlay, NULL, TRUE);
            return 0;
        }
        case WM_DESTROY: { /* ... без изменений ... */
            UnregisterHotKey(hwnd, HOTKEY_ID_TOGGLE_VISIBILITY);
            UnregisterHotKey(hwnd, HOTKEY_ID_SEND_QUERY_F1);
            if (g_hbrBackground) { DeleteObject(g_hbrBackground); g_hbrBackground = NULL; }
            PostQuitMessage(0);
            return 0;
        }
        default:
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
}

void CreateOverlayWindow(HINSTANCE hInstance) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    if (g_hbrBackground == NULL) g_hbrBackground = CreateSolidBrush(RGB(30, 30, 30));
    wc.hbrBackground = g_hbrBackground;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClassW(&wc)) { /* ... */ return; }
    hwndOverlay = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW, CLASS_NAME, NULL, WS_POPUP, 100, 100, 600, 450, NULL, NULL, hInstance, NULL );
    if (hwndOverlay == NULL) { /* ... */ return; }
    SetLayeredWindowAttributes(hwndOverlay, 0, 230, LWA_ALPHA);
    if (!SetWindowDisplayAffinity(hwndOverlay, WDA_EXCLUDEFROMCAPTURE)) {
        SetWindowDisplayAffinity(hwndOverlay, WDA_MONITOR);
    }
    if (is_overlay_visible) ShowWindow(hwndOverlay, SW_SHOW);
    UpdateWindow(hwndOverlay);
}

bool IsConsolePresent() { return ::GetConsoleWindow() != NULL; }
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    hInst = hInstance;
    CreateOverlayWindow(hInstance);
    if (hwndOverlay == NULL) return 1;
    if (!RegisterHotKey(hwndOverlay, HOTKEY_ID_TOGGLE_VISIBILITY, 0, VK_INSERT)) { /* ... */ }
    if (!RegisterHotKey(hwndOverlay, HOTKEY_ID_SEND_QUERY_F1, 0, VK_F1)) { /* ... */ }
    MSG msg = {};
    while (GetMessageW(&msg, NULL, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return (int)msg.wParam;
}