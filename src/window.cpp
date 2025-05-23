#include "window.h"
#include "utils.h"
#include <algorithm>

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

constexpr wchar_t CLASS_NAME[] = L"GeminiChatOverlayWindow";

OverlayWindow::OverlayWindow(HINSTANCE hInstance)
    : hInstance(hInstance)
    , hwnd(nullptr)
    , hbrBackground(nullptr)
    , isVisible(true)
    , sessionId("default_cpp_session") {
    chatRenderer = std::make_unique<ChatRenderer>();
    networkClient = std::make_unique<NetworkClient>();
}

OverlayWindow::~OverlayWindow() {
    if (hwnd) {
        UnregisterHotkeys();
        DestroyWindow(hwnd);
    }
    if (hbrBackground) {
        DeleteObject(hbrBackground);
    }
}

bool OverlayWindow::Create() {
    // Регистрация класса окна
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    hbrBackground = CreateSolidBrush(RGB(30, 30, 30));
    wc.hbrBackground = hbrBackground;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    if (!RegisterClassW(&wc)) {
        MessageBoxW(nullptr, L"Ошибка регистрации класса окна!", L"Ошибка", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    // Создание окна
    hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        CLASS_NAME,
        nullptr,
        WS_POPUP,
        100, 100, 600, 450,
        nullptr, nullptr, hInstance, this
    );

    if (!hwnd) {
        MessageBoxW(nullptr, L"Ошибка создания окна!", L"Ошибка", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    // Настройка прозрачности
    SetLayeredWindowAttributes(hwnd, 0, 230, LWA_ALPHA);

    // Исключение из захвата экрана
    if (!SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)) {
        SetWindowDisplayAffinity(hwnd, WDA_MONITOR);
    }

    RegisterHotkeys();

    if (isVisible) {
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
        SetFocus(hwnd);
    }

    UpdateWindow(hwnd);
    return true;
}

void OverlayWindow::Show(bool show) {
    isVisible = show;
    ShowWindow(hwnd, show ? SW_SHOW : SW_HIDE);
    if (show) {
        SetForegroundWindow(hwnd);
        SetFocus(hwnd);
        chatRenderer->SetStatusMessage(L"Overlay shown.");
    }
}
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void OverlayWindow::ProcessMessages() {
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

LRESULT CALLBACK OverlayWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    OverlayWindow* pThis = nullptr;

    if (uMsg == WM_NCCREATE) {
        auto pCreate = static_cast<CREATESTRUCT*>(reinterpret_cast<void*>(lParam));
        pThis = static_cast<OverlayWindow*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->hwnd = hwnd;
    } else {
        pThis = reinterpret_cast<OverlayWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->HandleMessage(uMsg, wParam, lParam);
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

LRESULT OverlayWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT:
            OnPaint();
            return 0;

        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProcW(hwnd, uMsg, wParam, lParam);
            return (hit == HTCLIENT) ? HTCAPTION : hit;
        }

        case WM_KEYDOWN:
            OnKeyDown(wParam);
            return 0;

        case WM_CHAR:
            OnChar(wParam);
            return 0;

        case WM_HOTKEY:
            OnHotkey(wParam);
            return 0;

        case WM_MOUSEWHEEL:
            OnMouseWheel(wParam);
            return 0;

        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;

        case WM_CHAT_RESPONSE:
            OnChatResponse(wParam);
            return 0;

        case WM_DESTROY:
            UnregisterHotkeys();
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
}
void OverlayWindow::OnPaint() const {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT clientRect;
    GetClientRect(hwnd, &clientRect);

    // Заливка фона
    FillRect(hdc, &ps.rcPaint, hbrBackground);

    // Отрисовка чата с передачей HWND
    chatRenderer->DrawChat(hdc, clientRect, hwnd);

    EndPaint(hwnd, &ps);
}

// Изменить метод OnKeyDown - добавить const
void OverlayWindow::OnKeyDown(WPARAM wParam) const {
    if (!isVisible || chatRenderer->IsWaitingForResponse()) {
        return;
    }

    if (bool ctrlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0; ctrlPressed) {
        switch (wParam) {
            case 'C':
                CopyInputToClipboard();
                break;
            case 'V':
                PasteFromClipboard();
                break;
            default:
                break;
        }
    } else {
        switch (wParam) {
            case VK_BACK: {
                std::wstring input = chatRenderer->GetUserInput();
                if (!input.empty()) {
                    input.pop_back();
                    chatRenderer->SetUserInput(input);
                    InvalidateRect(hwnd, nullptr, TRUE);
                }
                break;
            }

            case VK_RETURN:
                SendCurrentInput();
                break;

            case VK_ESCAPE:
                chatRenderer->ClearUserInput();
                InvalidateRect(hwnd, nullptr, TRUE);
                break;

            case VK_LEFT:
                chatRenderer->ScrollInputLeft();
                InvalidateRect(hwnd, nullptr, TRUE);
                break;

            case VK_RIGHT:
                chatRenderer->ScrollInputRight();
                InvalidateRect(hwnd, nullptr, TRUE);
                break;

            case VK_HOME:
                chatRenderer->ScrollInputHome();
                InvalidateRect(hwnd, nullptr, TRUE);
                break;

            case VK_END:
                chatRenderer->ScrollInputEnd();
                InvalidateRect(hwnd, nullptr, TRUE);
                break;

            default:
                break;
        }
    }
}
void OverlayWindow::OnChar(WPARAM wParam) const {
    if (!isVisible || chatRenderer->IsWaitingForResponse()) {
        return;
    }

    // Игнорируем управляющие символы
    if (wParam < 32 && wParam != L' ') {
        return;
    }

    if (iswprint(static_cast<wint_t>(wParam)) || wParam == L' ') {
        std::wstring input = chatRenderer->GetUserInput();
        if (input.length() < MAX_USER_INPUT_LENGTH) {
            input += static_cast<wchar_t>(wParam);
            chatRenderer->SetUserInput(input);
            InvalidateRect(hwnd, nullptr, TRUE);
        }
    }
}

void OverlayWindow::OnHotkey(WPARAM wParam) {
    switch (wParam) {
        case HOTKEY_ID_TOGGLE_VISIBILITY:
            Show(!isVisible);
            break;

        case HOTKEY_ID_SEND_QUERY_F1:
            if (isVisible && !chatRenderer->GetUserInput().empty() &&
                !chatRenderer->IsWaitingForResponse()) {
                SendCurrentInput();
            }
            break;

        default:
            break;
    }
}

void OverlayWindow::OnMouseWheel(WPARAM wParam) const {
    if (!isVisible) return;

    short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
    int scroll_lines_per_tick = 3;
    int line_height_approx = 16;
    int scroll_pixels = scroll_lines_per_tick * line_height_approx;

    if (zDelta > 0) {
        chatRenderer->ScrollUp(scroll_pixels);
    } else {
        chatRenderer->ScrollDown(scroll_pixels);
    }

    InvalidateRect(hwnd, nullptr, TRUE);
}

void OverlayWindow::OnChatResponse(WPARAM wParam) const {
    auto response = reinterpret_cast<NetworkClient::ChatResponse*>(wParam);

    if (response->is_error) {
        chatRenderer->AddMessage(response->response_text);
    } else {
        chatRenderer->AddMessage(L"Gemini: " + response->response_text);
        chatRenderer->ClearUserInput();
    }

    chatRenderer->SetStatusMessage(L"");
    chatRenderer->SetWaitingForResponse(false);
    chatRenderer->ScrollToBottom();

    delete response;
    InvalidateRect(hwnd, nullptr, TRUE);
}

void OverlayWindow::SendCurrentInput() const {
    std::wstring input = chatRenderer->GetUserInput();
    if (input.empty() || chatRenderer->IsWaitingForResponse()) {
        return;
    }

    chatRenderer->SetWaitingForResponse(true);
    chatRenderer->AddMessage(L"You: " + input);
    chatRenderer->ScrollToBottom();
    chatRenderer->SetStatusMessage(L"Отправка на сервер...");

    InvalidateRect(hwnd, nullptr, TRUE);

    // Приводим HWND к WindowHandle
    networkClient->SendChatMessage(input, sessionId, static_cast<WindowHandle>(hwnd));
}

void OverlayWindow::CopyInputToClipboard() const {
    std::wstring input = chatRenderer->GetUserInput();
    if (input.empty()) return;

    if (OpenClipboard(hwnd)) {
        EmptyClipboard();

        if (HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, (input.length() + 1) * sizeof(wchar_t))) {
            if (auto buffer = static_cast<wchar_t*>(GlobalLock(hg))) {
                wcscpy_s(buffer, input.length() + 1, input.c_str());
                GlobalUnlock(hg);
                SetClipboardData(CF_UNICODETEXT, hg);
                chatRenderer->SetStatusMessage(L"Текст скопирован!");
            } else {
                GlobalFree(hg);
            }
        }
        CloseClipboard();
        InvalidateRect(hwnd, nullptr, TRUE);
    }
}

void OverlayWindow::PasteFromClipboard() const {
    if (OpenClipboard(hwnd)) {
        if (HANDLE hData = GetClipboardData(CF_UNICODETEXT)) {
            if (auto pszText = static_cast<const wchar_t*>(GlobalLock(hData))) {
                std::wstring clipboardText = pszText;
                // Удаляем переносы строк
                clipboardText.erase(std::remove(clipboardText.begin(), clipboardText.end(), L'\r'),
                                  clipboardText.end());
                clipboardText.erase(std::remove(clipboardText.begin(), clipboardText.end(), L'\n'),
                                  clipboardText.end());

                std::wstring input = chatRenderer->GetUserInput();
                if (input.length() + clipboardText.length() <= MAX_USER_INPUT_LENGTH) {
                    input += clipboardText;
                } else if (input.length() < MAX_USER_INPUT_LENGTH) {
                    input += clipboardText.substr(0, MAX_USER_INPUT_LENGTH - input.length());
                }

                chatRenderer->SetUserInput(input);
                chatRenderer->ScrollInputEnd();
                chatRenderer->SetStatusMessage(L"Вставлено из буфера обмена.");

                GlobalUnlock(hData);
            }
        }
        CloseClipboard();
        InvalidateRect(hwnd, nullptr, TRUE);
    }
}

void OverlayWindow::RegisterHotkeys() const {
    if (!RegisterHotKey(hwnd, HOTKEY_ID_TOGGLE_VISIBILITY, 0, VK_INSERT)) {
        MessageBoxW(hwnd, L"Не удалось зарегистрировать горячую клавишу INSERT!",
                   L"Ошибка", MB_OK | MB_ICONERROR);
    }
    if (!RegisterHotKey(hwnd, HOTKEY_ID_SEND_QUERY_F1, 0, VK_F1)) {
        MessageBoxW(hwnd, L"Не удалось зарегистрировать горячую клавишу F1!",
                   L"Ошибка", MB_OK | MB_ICONERROR);
    }
}

void OverlayWindow::UnregisterHotkeys() const {
    UnregisterHotKey(hwnd, HOTKEY_ID_TOGGLE_VISIBILITY);
    UnregisterHotKey(hwnd, HOTKEY_ID_SEND_QUERY_F1);
}