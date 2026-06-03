import http.server
import socketserver
import os

PORT = 8000

class LiveTrackingHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        # Print a clean enterprise log to the terminal when the agent hits the server
        print(f"\n[+] Incoming Beacon Received from {self.client_address[0]}")
        print(f"    Requested Endpoint: {self.path}")
        print(f"    User-Agent: {self.headers.get('User-Agent')}")
        print(f"    Host Metadata: {self.headers.get('X-Agent-Metadata', 'not provided')}")
        
        # Construct a simple mock task instruction
        mock_instruction = b"MVP_INITIAL_TEST_OK"
        
        # Send professional HTTP response headers
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(mock_instruction)))
        self.end_headers()
        
        # Transmit the payload bytes down the stream
        self.wfile.write(mock_instruction)
        print("[-] Sent Instruction: MVP_INITIAL_TEST_OK")

if __name__ == "__main__":
    # Ensure Python operates out of the server directory
    print(f"[*] Starting Minimal C2 Listener on port {PORT}...")
    
    # Allow the port to be immediately reused if the server restarts
    socketserver.TCPServer.allow_reuse_address = True
    
    # Start the TCP server to listen for incoming connections
    with socketserver.TCPServer(("", PORT), LiveTrackingHandler) as httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\n[*] Shutting down C2 Listener smoothly.")
