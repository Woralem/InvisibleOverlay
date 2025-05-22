import os
import requests
from flask import Flask, request, jsonify
import traceback

# --- НАСТРОЙКИ ---
GEMINI_API_KEY = os.environ.get("AIzaSyD2z6napwv9r2RLy5uD3ESfM9jtf_zNFOw", "AIzaSyD2z6napwv9r2RLy5uD3ESfM9jtf_zNFOw") # Замените на свой ключ или используйте переменную окружения
MODEL_NAME = "gemini-2.5-flash-preview-05-20" # <--- ИЗМЕНИТЕ ЭТУ СТРОКУ

# Локальный SOCKS5 прокси, предоставляемый NekoBox
NEKOBOX_SOCKS_PROXY_URL = "socks5://127.0.0.1:2080" # 'h' для DNS через прокси

proxies_for_requests = {
    "http": NEKOBOX_SOCKS_PROXY_URL,
    "https": NEKOBOX_SOCKS_PROXY_URL,
}
print(f"Python script will use SOCKS Proxy for ALL its requests: {NEKOBOX_SOCKS_PROXY_URL}")
print("Ensure NekoBox is running, SOCKS proxy is enabled on 127.0.0.1:2080,")
print("TUN mode is OFF, and only ONE VLESS profile is active in NekoBox.")
print("Also, ensure custom routing rules in NekoBox are CLEARED/EMPTY.")

# Проверка IP через прокси (если настроен)
test_url_ip = "https://ifconfig.me/ip"
print(f"Attempting to fetch current external IP from {test_url_ip} (should be VLESS server's IP)...")
try:
    response_ip_test = requests.get(test_url_ip, timeout=15, proxies=proxies_for_requests)
    current_external_ip = response_ip_test.text.strip()
    print(f"-> Current external IP as seen by this script via NekoBox: {current_external_ip}")
except requests.exceptions.RequestException as e:
    print(f"!!! Could not fetch external IP via NekoBox: {e}")
    print("!!! Check if NekoBox is running and configured correctly (SOCKS on 127.0.0.1:2080, one VLESS active).")

app = Flask(__name__)
chat_histories = {} # Хранение истории чата для каждой сессии

def send_to_gemini_rest(session_id, user_message_text):
    if session_id not in chat_histories:
        chat_histories[session_id] = []

    # Добавляем текущее сообщение пользователя в историю ДЛЯ ЗАПРОСА
    current_request_history = list(chat_histories[session_id]) # Копируем, чтобы не менять основную историю до успешного ответа
    current_request_history.append({"role": "user", "parts": [{"text": user_message_text}]})

    api_url = f"https://generativelanguage.googleapis.com/v1beta/models/{MODEL_NAME}:generateContent?key={GEMINI_API_KEY}"

    request_body = {
        "contents": current_request_history,
        # Можно добавить generationConfig, safetySettings если нужно
        # "generationConfig": {
        #     "temperature": 0.7,
        #     "topK": 1,
        #     "topP": 1,
        #     "maxOutputTokens": 2048,
        # },
        # "safetySettings": [
        #     {"category": "HARM_CATEGORY_HARASSMENT", "threshold": "BLOCK_MEDIUM_AND_ABOVE"},
        #     # ... другие настройки безопасности
        # ]
    }
    headers = {
        "Content-Type": "application/json"
    }

    print(f"Sending to Gemini REST API. URL: {api_url}")
    print(f"Using proxies: {proxies_for_requests}")

    try:
        response = requests.post(
            api_url,
            json=request_body,
            headers=headers,
            timeout=60, # Таймаут для запроса
            proxies=proxies_for_requests, # Используем настроенный прокси
            verify=False
        )
        response.raise_for_status() # Вызовет исключение для HTTP-ошибок (4xx, 5xx)

        response_data = response.json()

        # Извлечение ответа модели
        if response_data.get("candidates") and \
                response_data["candidates"][0].get("content") and \
                response_data["candidates"][0]["content"].get("parts") and \
                response_data["candidates"][0]["content"]["parts"][0].get("text"):

            model_reply_text = response_data["candidates"][0]["content"]["parts"][0]["text"]

            # Только после успешного ответа обновляем основную историю
            chat_histories[session_id].append({"role": "user", "parts": [{"text": user_message_text}]})
            chat_histories[session_id].append({"role": "model", "parts": [{"text": model_reply_text}]})

            # Ограничение размера истории (опционально)
            # MAX_HISTORY_PAIRS = 10 # Например, 10 пар вопрос-ответ
            # while len(chat_histories[session_id]) > MAX_HISTORY_PAIRS * 2:
            #    chat_histories[session_id].pop(0) # Удаляем самые старые сообщения (пару)

            return model_reply_text, len(chat_histories[session_id])
        elif response_data.get("promptFeedback") and \
                response_data["promptFeedback"].get("blockReason"):
            reason = response_data["promptFeedback"]["blockReason"]
            details = response_data["promptFeedback"].get("safetyRatings", "")
            error_msg = f"Content blocked by Gemini. Reason: {reason}. Details: {details}"
            print(error_msg)
            # Не добавляем в историю заблокированный запрос/ответ
            return error_msg, len(chat_histories[session_id])
        elif response_data.get("error"):
            error_details = response_data["error"]
            error_msg = f"Gemini API Error: {error_details.get('message', 'Unknown error')} (Code: {error_details.get('code', 'N/A')})"
            print(error_msg)
            return error_msg, len(chat_histories[session_id])
        else:
            error_msg = "Error: Unexpected Gemini REST response structure."
            print(error_msg)
            print("Response Data:", response_data) # Для отладки
            return error_msg, len(chat_histories[session_id])

    except requests.exceptions.HTTPError as http_err:
        error_message = f"HTTP error occurred with Gemini API: {http_err} - Response: {http_err.response.text if http_err.response else 'No response body'}"
        print(error_message)
        if http_err.response:
            try:
                err_json = http_err.response.json()
                if err_json.get("error") and err_json["error"].get("message"):
                    api_err_msg = err_json["error"]["message"]
                    if "User location is not supported" in api_err_msg or "API key not valid" in api_err_msg: # Добавил проверку ключа
                        return f"Gemini API Error: {api_err_msg}. Check VPN/proxy or API Key.", len(chat_histories[session_id])
                    return f"Gemini API Error: {api_err_msg}", len(chat_histories[session_id])
            except ValueError:
                pass
        return error_message, len(chat_histories[session_id])
    except requests.exceptions.ProxyError as proxy_err:
        error_message = f"Proxy error: {proxy_err}. Check if NekoBox is running, SOCKS on {NEKOBOX_SOCKS_PROXY_URL}, and one VLESS profile is active."
        print(error_message)
        return error_message, len(chat_histories[session_id])
    except requests.exceptions.RequestException as req_err: # Более общий обработчик ошибок запросов
        error_message = f"Request error with Gemini API: {req_err}"
        print(error_message)
        return error_message, len(chat_histories[session_id])
    except Exception as e:
        print(f"!!! UNEXPECTED EXCEPTION in send_to_gemini_rest for session '{session_id}' !!!")
        traceback.print_exc() # Печатаем полный traceback для неожиданных ошибок
        return f"Unexpected server error: {str(e)}", len(chat_histories[session_id])

@app.route('/chat', methods=['POST'])
def handle_chat():
    data = request.get_json()
    if not data or 'message' not in data:
        return jsonify({"error": "Missing 'message' in request body"}), 400

    user_message = data['message']
    session_id = data.get('session_id', "default_session") # Получаем session_id или используем дефолтный

    print(f"Received message for session '{session_id}': {user_message}")

    model_reply, history_len = send_to_gemini_rest(session_id, user_message)

    # Проверяем, содержит ли ответ явное указание на ошибку
    if "Error:" in model_reply or \
            "error occurred" in model_reply or \
            "not supported" in model_reply or \
            "blocked by Gemini" in model_reply or \
            "Proxy error" in model_reply:
        return jsonify({"error": model_reply}), 500 # Возвращаем ошибку сервера

    print(f"Gemini reply for session '{session_id}': {model_reply}")
    return jsonify({"reply": model_reply, "history_length": history_len})

@app.route('/clear_history', methods=['POST'])
def clear_chat_history():
    data = request.get_json()
    session_id = data.get('session_id', "default_session") if data else "default_session"

    if session_id in chat_histories:
        del chat_histories[session_id]
        message = f"Chat history for session '{session_id}' cleared."
        print(message)
        return jsonify({"message": message}), 200
    else:
        message = f"No active chat session found for id '{session_id}' to clear."
        print(message)
        return jsonify({"message": message}), 404

@app.route('/health', methods=['GET'])
def health_check():
    ip_seen_by_script = "N/A (could not fetch)"
    try:
        r_health_ip = requests.get("https://ifconfig.me/ip", timeout=5, proxies=proxies_for_requests)
        ip_seen_by_script = r_health_ip.text.strip()
    except:
        pass # Ошибки уже логируются при старте

    return jsonify({
        "status": "ok",
        "proxy_settings": {
            "type": "SOCKS5 via NekoBox (assumed)",
            "url": NEKOBOX_SOCKS_PROXY_URL,
            "note": "This script directs ALL its requests through this proxy."
        },
        "gemini_model": MODEL_NAME,
        "external_ip_seen_by_this_script": ip_seen_by_script
    }), 200

if __name__ == '__main__':
    if not GEMINI_API_KEY or "YOUR_GEMINI_API_KEY" in GEMINI_API_KEY:
        print("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
        print("!!! WARNING: GEMINI_API_KEY is not set or is a placeholder.              !!!")
        print("!!! Please set your actual GEMINI_API_KEY as an environment variable     !!!")
        print("!!! or directly in the script (less secure).                           !!!")
        print("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
    else:
        print("GEMINI_API_KEY seems to be set.")

    try:
        import socks # Проверка, что PySocks доступен
    except ImportError:
        print("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
        print("!!! WARNING: PySocks library not found. SOCKS proxy will not work.       !!!")
        print("!!! Please install it: pip install requests[socks]  OR  pip install PySocks !!!")
        print("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
        # Можно решить завершить работу, если PySocks критичен для работы.
        # exit(1)

    # Для Windows может быть полезно для корректного вывода UTF-8 в консоль, если запускается не из IDE
    # try:
    #    if os.name == 'nt':
    #        import sys
    #        sys.stdout.reconfigure(encoding='utf-8')
    #        sys.stderr.reconfigure(encoding='utf-8')
    # except AttributeError: # reconfigure доступен с Python 3.7
    #    pass

    app.run(host='127.0.0.1', port=5000, debug=True) # debug=True для разработки