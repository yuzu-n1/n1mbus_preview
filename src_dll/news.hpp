#pragma once
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#pragma comment(lib, "winhttp.lib")

struct NewsItem {
    std::string date;
    std::string title;
    std::string content;
};

extern std::vector<NewsItem> g_NewsItems;
extern bool g_NewsLoaded;
extern bool g_NewsFailed;
extern std::mutex g_NewsMutex;

namespace NewsAPI {
    inline void FetchNewsAsync() {
        std::thread([]() {
            HINTERNET hSession = WinHttpOpen(L"N1mbus/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            if (!hSession) { g_NewsFailed = true; return; }

            HINTERNET hConnect = WinHttpConnect(hSession, L"www.n1mbus.xyz", INTERNET_DEFAULT_HTTPS_PORT, 0);
            if (!hConnect) { WinHttpCloseHandle(hSession); g_NewsFailed = true; return; }

            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/api/news", nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
            if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); g_NewsFailed = true; return; }

            if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                WinHttpReceiveResponse(hRequest, nullptr)) {
                
                std::string response;
                DWORD size = 0, downloaded = 0;
                do {
                    WinHttpQueryDataAvailable(hRequest, &size);
                    if (size == 0) break;
                    char* buf = new char[size + 1];
                    ZeroMemory(buf, size + 1);
                    if (WinHttpReadData(hRequest, (LPVOID)buf, size, &downloaded)) {
                        response.append(buf, downloaded);
                    }
                    delete[] buf;
                } while (size > 0);

                // Simple JSON string parsing since we might not have a json lib
                std::vector<NewsItem> items;
                size_t pos = 0;
                while ((pos = response.find("{\"id\"", pos)) != std::string::npos || (pos = response.find("{\"date\"", pos)) != std::string::npos) {
                    NewsItem item;
                    
                    auto extractField = [&](const std::string& key) -> std::string {
                        std::string searchKey = "\"" + key + "\":\"";
                        size_t start = response.find(searchKey, pos);
                        if (start != std::string::npos) {
                            start += searchKey.length();
                            size_t end = response.find("\"", start);
                            if (end != std::string::npos) {
                                std::string val = response.substr(start, end - start);
                                // Very basic unescape (e.g. \n)
                                size_t escapePos = 0;
                                while ((escapePos = val.find("\\n", escapePos)) != std::string::npos) {
                                    val.replace(escapePos, 2, "\n");
                                    escapePos += 1;
                                }
                                return val;
                            }
                        }
                        return "";
                    };

                    item.date = extractField("date");
                    item.title = extractField("title");
                    item.content = extractField("content");

                    if (!item.title.empty()) {
                        items.push_back(item);
                    }
                    pos += 5; // move forward
                }

                std::lock_guard<std::mutex> lock(g_NewsMutex);
                g_NewsItems = items;
                g_NewsLoaded = true;
                g_NewsFailed = false;
            } else {
                g_NewsFailed = true;
            }

            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
        }).detach();
    }
}
