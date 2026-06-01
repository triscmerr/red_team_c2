#include <windows.h>
#include <wininet.h>
#include <stdio.h>

// Configuration for the MVP
#define C2_SERVER      "127.0.0.1"
#define C2_PORT        8000
#define C2_ENDPOINT    "/index.html"
#define SLEEP_INTERVAL 5000 // 5 seconds
#define BUFFER_SIZE    1024

/**
 * Executes a single network poll to the Python testing server.
 * Handles allocation and teardown of Windows network handles cleanly.
 */
void PerformNetworkPoll(void) {
    HINTERNET hInternet = NULL;
    HINTERNET hConnect = NULL;
    HINTERNET hRequest = NULL;
    char buffer[BUFFER_SIZE];
    DWORD bytesRead = 0;
    BOOL success = FALSE;

    // Zero out the buffer before use to guarantee no garbage memory is leaked or printed
    SecureZeroMemory(buffer, sizeof(buffer));

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

    // 4. Send the request to the server
    success = HttpSendRequestA(hRequest, NULL, 0, NULL, 0);
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

int main(void) {
    printf("[*] =========================================\n");
    printf("[*] C2 MVP Agent Active (Milestone 1)\n");
    printf("[*] Target C2 Node: %s:%d%s\n", C2_SERVER, C2_PORT, C2_ENDPOINT);
    printf("[*] =========================================\n\n");

    // Infinite execution loop
    while (TRUE) {
        PerformNetworkPoll();
        
        printf("[*] Loop cycle complete. Sleeping for %dms...\n\n", SLEEP_INTERVAL);
        Sleep(SLEEP_INTERVAL);
    }

    return 0;
}