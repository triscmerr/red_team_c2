# local_client_server_demo
Project for a local client/server networking exercise.


Below is the structured Requirements Summary for your Milestone 1 MVP ("Walking Skeleton").

📋 MVP Requirements Document: Milestone 1
1. Objective & Scope
The goal of Milestone 1 is to establish a stable, leak-free, end-to-end communication loop between a low-level network polling client written in C and a lightweight local listener written in Python. This phase focuses on network transport stability, bounded string handling, and basic handle orchestration.

2. MVP Client (C) Requirements
Category	Requirement Description	Technical Target
Network I/O	Must natively establish an HTTP connection to a pre-configured IP/Port without external third-party dependencies.	Windows WinINet or WinHTTP libraries.
Polling Loop	Must run an infinite loop that queries the server at a strict interval (e.g., every 5 seconds).	Standard while(TRUE) with native OS Sleep().
Resource Mgmt	Every opened network, connection, and request handle must be explicitly closed within the loop iteration to prevent memory leaks.	Proper ordering of InternetCloseHandle().
String Handling	Any string formatting or buffer manipulation must use bounded functions to eliminate memory corruption vectors.	snprintf(), strncpy() instead of sprintf(), strcpy().
Error Isolation	If a network request fails (e.g., server offline), the client must fail gracefully, sleep, and attempt the next poll without crashing.	Basic conditional checks on function return values.
3. MVP Testing Server (Python Backend) Requirements
Endpoint Availability: Must expose a single HTTP network interface accessible via a standard web utility (e.g., GET /index.html).

Static Response Delivery: Must serve a simple, plaintext payload string (e.g., "PING") when queried by the client.

Operational Logging: Must print a timestamped log to the terminal console every time the C client submits a connection request.

Zero-Configuration Deployment: Must run locally utilizing standard native libraries to avoid environment configuration bottlenecks during early development.

4. Communication Protocol Specification
Transport Layer: Standard HTTP over TCP.

Network Addressing: Hardcoded to loopback adapter (127.0.0.1) on a non-standard port (e.g., 8000).

Data Format: Raw plaintext strings. The client sends a standard web request header, and the server returns a plaintext string indicator.

5. Success & Acceptance Criteria
The "Walking Skeleton" definition of done:

The Python server is initialized and listening on port 8000.

The compiled C binary is executed on the local system.

The server terminal logs consecutive check-ins from the client at the designated interval.

The client successfully prints the plaintext response hosted on the server.

The client application runs continuously for 15 minutes without escalating memory allocation or crashing the thread.
