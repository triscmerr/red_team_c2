import socket
import threading

PORT = 8000
TRANSFORM_KEY = [0x00]

# Track whether we have already sent the initial response
INITIAL_RESPONSE_SENT = False

def ApplyXorTransform(raw_bytes: bytearray) -> bytearray:
    key_length = len(TRANSFORM_KEY)
    for i in range(len(raw_bytes)):
        raw_bytes[i] ^= TRANSFORM_KEY[i % key_length]
    return raw_bytes

def handle_client(client_socket, client_address):
    global INITIAL_RESPONSE_SENT
    print(f"[*] Local test server listening on port {PORT}...")
    try:
        while True:
            scrambled_packet = client_socket.recv(2048)
            if not scrambled_packet:
                break

            # 1. Decode incoming data from the client
            decrypted_bytes = ApplyXorTransform(bytearray(scrambled_packet))
            plaintext_http = decrypted_bytes.decode('utf-8', errors='ignore')

            # 2. Parse basic HTTP attributes from the raw string stream
            http_lines = plaintext_http.split("\r\n")
            request_line = http_lines[0] if len(http_lines) > 0 else ""

            # Locate the endpoint inside the request (e.g., POST /index.html HTTP/1.1)
            endpoint = "/index.html"
            if "POST" in request_line:
                endpoint = request_line.split(" ")[1]

            # Extract the body payload (located after the empty line sequence)
            body_payload = "UNKNOWN"
            if "\r\n\r\n" in plaintext_http:
                body_payload = plaintext_http.split("\r\n\r\n")[1]

            print(f"[+] Connected: {client_address[0]} -> Path: {endpoint}")

            # 3. Handle response selection based on the target path
            command_to_send = "PING"  # Default fallback response

            if "/api/v1/results" in endpoint:
                print("========================= PRIMARY RESPONSE CAPTURE =========================")
                print(f"[!] Client output received: {body_payload}")
                print("==========================================================================")
                command_to_send = "PING"  # Return to the default response once data is recorded
            else:
                # If it's a standard check-in, send the initial info response exactly once
                if not INITIAL_RESPONSE_SENT:
                    print("[*] Check-in recognized. Issuing initial response: INFO")
                    command_to_send = "INFO"
                    INITIAL_RESPONSE_SENT = True
                else:
                    print("[*] Check-in recognized. No queued work. Issuing: PING")
                    command_to_send = "PING"

            # 4. Craft the HTTP response payload
            http_response = (
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                f"Content-Length: {len(command_to_send)}\r\n"
                "Connection: close\r\n"
                "\r\n"
                f"{command_to_send}"
            )

            # 5. Transform the server response before sending it over the raw socket
            response_bytes = bytearray(http_response.encode('utf-8'))
            transformed_response = ApplyXorTransform(response_bytes)

            client_socket.sendall(transformed_response)
            break
    except Exception as e:
        print(f"Client error {client_address}: {e}")
    finally:
        client_socket.close()
        print(f"Client disconnected: {client_address}")


def StartLocalServer():
    global INITIAL_RESPONSE_SENT
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind(("0.0.0.0", PORT))
    server_socket.listen(5)

    try:
        while True:
            client_socket, client_address = server_socket.accept()
            thread = threading.Thread(
                target=handle_client,
                args=(client_socket, client_address),
                daemon=True
            )
            thread.start()
    except KeyboardInterrupt:
        print("\n[*] Shutting down Server.")
    finally:
        server_socket.close()

if __name__ == "__main__":
    StartLocalServer()
