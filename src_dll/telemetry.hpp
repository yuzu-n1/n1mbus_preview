// telemetry.hpp - N1mbus HWID telemetry sender
// Sends a unique HWID ping to the nimbus backend on first launch.
// Uses WinHTTP for HTTPS support (no libcurl dependency).
#pragma once
#include <windows.h>
#include <winhttp.h>
#include <iphlpapi.h>
#include <string>
#include <thread>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "iphlpapi.lib")

namespace Telemetry {

// Build a stable hardware ID from volume serial + MAC address
inline std::string GetHWID() {
    // 1. Boot drive volume serial
    DWORD volSerial = 0;
    GetVolumeInformationA("C:\\", nullptr, 0, &volSerial, nullptr, nullptr, nullptr, 0);

    // 2. First physical MAC address
    ULONG bufLen = 15000;
    BYTE* macBuf = new BYTE[bufLen];
    std::string macStr = "000000000000";
    if (GetAdaptersInfo((PIP_ADAPTER_INFO)macBuf, &bufLen) == NO_ERROR) {
        PIP_ADAPTER_INFO adapter = (PIP_ADAPTER_INFO)macBuf;
        while (adapter) {
            if (adapter->Type == MIB_IF_TYPE_ETHERNET || adapter->Type == IF_TYPE_IEEE80211) {
                char mac[32];
                snprintf(mac, sizeof(mac), "%02X%02X%02X%02X%02X%02X",
                    adapter->Address[0], adapter->Address[1], adapter->Address[2],
                    adapter->Address[3], adapter->Address[4], adapter->Address[5]);
                macStr = mac;
                break;
            }
            adapter = adapter->Next;
        }
    }
    delete[] macBuf;

    // 3. Combine: VOL{serial_hex}-MAC{mac}
    char hwid[64];
    snprintf(hwid, sizeof(hwid), "VOL%08X-MAC%s", volSerial, macStr.c_str());
    return std::string(hwid);
}

// Send HWID ping asynchronously (fire and forget, never blocks game startup)
inline void SendPing(const std::string& version = "preview") {
    std::thread([version]() {
        std::string hwid = GetHWID();

        // Build JSON body
        std::string body = "{\"hwid\":\"" + hwid + "\",\"version\":\"" + version + "\"}";

        HINTERNET hSession = WinHttpOpen(
            L"N1mbus/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0
        );
        if (!hSession) return;

        HINTERNET hConnect = WinHttpConnect(
            hSession,
            L"nimbus-9ym.pages.dev",
            INTERNET_DEFAULT_HTTPS_PORT,
            0
        );
        if (!hConnect) { WinHttpCloseHandle(hSession); return; }

        HINTERNET hRequest = WinHttpOpenRequest(
            hConnect,
            L"POST",
            L"/api/telemetry/log",
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE
        );
        if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return; }

        LPCWSTR contentType = L"Content-Type: application/json\r\n";
        WinHttpAddRequestHeaders(hRequest, contentType, (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD);

        WinHttpSendRequest(
            hRequest,
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0,
            (LPVOID)body.c_str(),
            (DWORD)body.size(),
            (DWORD)body.size(),
            0
        );
        WinHttpReceiveResponse(hRequest, nullptr);

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
    }).detach();
}

} // namespace Telemetry
