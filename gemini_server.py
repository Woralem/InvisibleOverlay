import os
import requests
from flask import Flask, request, jsonify
import traceback


GEMINI_API_KEY = "AIzaSyC06Mpo8SKihNugyw0AqFwHPARRKQRkRac"
MODEL_NAME = "gemini-2.5-flash-preview-05-20"

print("Running without explicit proxy configuration in the script. System-level VPN should handle traffic.")

test_url_ip = "https://ifconfig.me/ip"
print(f"Attempting to fetch current external IP from {test_url_ip} (should be VPN's IP)...")
try:
    response_ip_test = requests.get(test_url_ip, timeout=15)
    current_external_ip = response_ip_test.text.strip()
    print(f"-> Current external IP as seen by {test_url_ip}: {current_external_ip}")
except requests.exceptions.RequestException as e:
    print(f"!!! Could not fetch external IP: {e}")

app = Flask(__name__)

chat_histories = {}

def send_to_gemini_rest(session_id, user_message_text):
    if session_id not in chat_histories:
        chat_histories[session_id] = []

    current_history = list(chat_histories[session_id])
    current_history.append({"role": "user", "parts": [{"text": user_message_text}]})

    api_url = f"https://generativelanguage.googleapis.com/v1beta/models/{MODEL_NAME}:generateContent?key={GEMINI_API_KEY}"

    request_body = {
        "contents": current_history
    }
    headers = {
        "Content-Type": "application/json"
    }

    print(f"Sending to Gemini REST API. URL: {api_url}")

    try:
        response = requests.post(api_url, json=request_body, headers=headers, timeout=60)
        response.raise_for_status()

        response_data = response.json()

        if response_data.get("candidates") and response_data["candidates"][0].get("content"):
            model_content = response_data["candidates"][0]["content"]
            if model_content.get("parts") and model_content["parts"][0].get("text"):
                model_reply_text = model_content["parts"][0]["text"]

                chat_histories[session_id].append({"role": "user", "parts": [{"text": user_message_text}]})
                chat_histories[session_id].append({"role": "model", "parts": [{"text": model_reply_text}]})

                return model_reply_text, len(chat_histories[session_id])
            else:
                error_msg = "Error: Could not extract text from Gemini REST response (missing parts/text)."
                print(error_msg)
                return error_msg, len(chat_histories[session_id])
        elif response_data.get("error"):
            error_details = response_data["error"]
            error_msg = f"Gemini API Error: {error_details.get('message', 'Unknown error')} (Code: {error_details.get('code', 'N/A')})"
            print(error_msg)
            return error_msg, len(chat_histories[session_id])
        else:
            error_msg = "Error: Unexpected Gemini REST response structure (missing candidates/content)."
            print(error_msg)
            return error_msg, len(chat_histories[session_id])

    except requests.exceptions.HTTPError as http_err:
        error_message = f"HTTP error occurred with Gemini API: {http_err} - Response: {http_err.response.text}"
        print(error_message)
        try:
            err_json = http_err.response.json()
            if err_json.get("error") and err_json["error"].get("message"):
                api_err_msg = err_json["error"]["message"]
                if "User location is not supported" in api_err_msg:
                    return f"Gemini API Error: User location is not supported. VPN might not be active or routing correctly.", len(chat_histories[session_id])
                return f"Gemini API Error: {api_err_msg}", len(chat_histories[session_id])
        except ValueError:
            pass
        return error_message, len(chat_histories[session_id])
    except requests.exceptions.RequestException as req_err:
        error_message = f"Request error with Gemini API: {req_err}"
        print(error_message)
        return error_message, len(chat_histories[session_id])
    except Exception as e:
        print(f"!!! UNEXPECTED EXCEPTION in send_to_gemini_rest for session '{session_id}' !!!")
        traceback.print_exc()
        return f"Unexpected server error: {e}", len(chat_histories[session_id])

@app.route('/chat', methods=['POST'])
def handle_chat():
    data = request.get_json()
    if not data or 'message' not in data:
        return jsonify({"error": "Missing 'message' in request body"}), 400

    user_message = data['message']
    session_id = data.get('session_id', "default_session")

    print(f"Received message for session '{session_id}': {user_message}")

    model_reply, history_len = send_to_gemini_rest(session_id, user_message)

    if "Error:" in model_reply or "error occurred" in model_reply or "not supported" in model_reply:
        return jsonify({"error": model_reply}), 500

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
    ip_seen = "N/A (could not fetch)"
    try:
        r_health_ip = requests.get("https://ifconfig.me/ip", timeout=5)
        ip_seen = r_health_ip.text.strip()
    except:
        pass

    return jsonify({"status": "ok",
                    "proxy_enabled_in_script": False,
                    "mode": "REST_API",
                    "external_ip_seen_by_server": ip_seen
                    }), 200

if __name__ == '__main__':
    if not GEMINI_API_KEY or GEMINI_API_KEY == "YOUR_GEMINI_API_KEY" or "AIzaSyC06Mpo8SKihNugyw0AqFwHPARRKQRkRac" == GEMINI_API_KEY:
        print("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
        print("!!! WARNING: GEMINI_API_KEY is placeholder or not set correctly.         !!!")
        print("!!! Please set your actual GEMINI_API_KEY in the script.                 !!!")
        print("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
    else:
        print("GEMINI_API_KEY seems to be set.")

    app.run(host='127.0.0.1', port=5000, debug=True)