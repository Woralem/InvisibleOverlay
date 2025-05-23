#include "network_client.h"
#include "utils.h"

// Важно: winsock2.h должен быть включен перед windows.h
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <windows.h>
#endif

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <sstream>
#include <memory>
#include <utility>

NetworkClient::NetworkClient(std::string server_host, int server_port)
    : host(std::move(server_host)), port(server_port) {
}

void NetworkClient::SendChatMessage(const std::wstring& message, const std::string& session_id, WindowHandle callback_window) const {
    std::string user_message_utf8 = wstring_to_utf8(message);

    nlohmann::json request_json;
    request_json["message"] = user_message_utf8;
    request_json["session_id"] = session_id;
    std::string request_body = request_json.dump();

    SendRequestAsync(std::move(request_body), callback_window);
}

void NetworkClient::SendRequestAsync(std::string request_body, WindowHandle callback_window) const {
    std::thread([this, request_body = std::move(request_body), callback_window]() {
        httplib::Client cli(host, port);
        cli.set_connection_timeout(10);
        cli.set_read_timeout(60);
        cli.set_write_timeout(10);

        auto response = std::make_unique<ChatResponse>();

        try {
            if (auto res = cli.Post("/chat", request_body, "application/json")) {
                if (res->status == 200) {
                    nlohmann::json response_json = nlohmann::json::parse(res->body);
                    if (response_json.contains("reply")) {
                        response->response_text = utf8_to_wstring(response_json["reply"].get<std::string>());
                        response->is_error = false;
                    } else if (response_json.contains("error")) {
                        response->response_text = L"Server Error: " +
                                                utf8_to_wstring(response_json["error"].get<std::string>());
                        response->is_error = true;
                    } else {
                        response->response_text = L"Error: Unexpected server response structure.";
                        response->is_error = true;
                    }
                } else {
                    std::wstringstream ss;
                    ss << L"HTTP Error: " << res->status;
                    if (!res->body.empty()) {
                        try {
                            nlohmann::json error_json = nlohmann::json::parse(res->body);
                            if(error_json.contains("error")) {
                                ss << L" - " << utf8_to_wstring(error_json["error"].get<std::string>());
                            } else {
                                ss << L" - Body: " << utf8_to_wstring(res->body.substr(0, 200));
                            }
                        } catch (const nlohmann::json::parse_error&) {
                            ss << L" - Body: " << utf8_to_wstring(res->body.substr(0, 200));
                        }
                    }
                    response->response_text = ss.str();
                    response->is_error = true;
                }
            } else {
                auto err = res.error();
                response->response_text = L"Request Error: " + utf8_to_wstring(httplib::to_string(err));
                response->is_error = true;
            }
        } catch (const nlohmann::json::parse_error& e) {
            response->response_text = L"JSON Parse Error: " + utf8_to_wstring(e.what());
            response->is_error = true;
        } catch (const std::exception& e) {
            response->response_text = L"Exception: " + utf8_to_wstring(e.what());
            response->is_error = true;
        }

        if(callback_window) {
            // Передаем владение указателем в основной поток
            // Приводим WindowHandle обратно к HWND
            HWND hwnd = static_cast<HWND>(callback_window);
            PostMessage(hwnd, WM_CHAT_RESPONSE,
                       reinterpret_cast<WPARAM>(response.release()), 0);
        }
    }).detach();
}