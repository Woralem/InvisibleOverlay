#define UNICODE

#include <windows.h>
#include <memory>
#include "window.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    // Создание окна оверлея
    auto overlayWindow = std::make_unique<OverlayWindow>(hInstance);

    if (!overlayWindow->Create()) {
        return 1;
    }

    // Обработка сообщений
    overlayWindow->ProcessMessages();

    return 0;
}