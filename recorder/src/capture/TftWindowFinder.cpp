#include "capture/TftWindowFinder.h"

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#include <string>
#endif

namespace tft {

TftWindowFinder::TftWindowFinder(WindowRules rules) : rules_(std::move(rules)) {}

bool TftWindowFinder::LoadRulesFromFile(const std::string& path, std::string* error) {
    WindowRuleConfig cfg = rules_.GetConfig();
    if (!WindowRuleConfig::LoadFromFile(path, cfg, error)) return false;
    rules_ = WindowRules(cfg);
    return true;
}

#if defined(_WIN32)
namespace {
std::string WideToUtf8(const wchar_t* w, int len = -1) {
    if (!w) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, w, len, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, len, out.data(), size, nullptr, nullptr);
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

std::string GetProcessName(DWORD pid) {
    std::string name;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return name;
    wchar_t buf[MAX_PATH];
    DWORD sz = MAX_PATH;
    if (QueryFullProcessImageNameW(h, 0, buf, &sz)) {
        std::wstring full(buf, sz);
        auto pos = full.find_last_of(L"\\/");
        std::wstring exe = (pos == std::wstring::npos) ? full : full.substr(pos + 1);
        name = WideToUtf8(exe.c_str(), static_cast<int>(exe.size()));
    }
    CloseHandle(h);
    return name;
}

BOOL CALLBACK EnumProc(HWND hwnd, LPARAM lparam) {
    auto* list = reinterpret_cast<std::vector<WindowInfo>*>(lparam);
    if (!IsWindow(hwnd)) return TRUE;
    if (!IsWindowVisible(hwnd)) return TRUE;
    wchar_t title[512] = {};
    wchar_t cls[256] = {};
    GetWindowTextW(hwnd, title, 512);
    GetClassNameW(hwnd, cls, 256);
    RECT rc{};
    GetWindowRect(hwnd, &rc);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    WindowInfo info;
    info.title = WideToUtf8(title);
    info.class_name = WideToUtf8(cls);
    info.process_name = GetProcessName(pid);
    info.visible = IsWindowVisible(hwnd) != FALSE;
    info.width = rc.right - rc.left;
    info.height = rc.bottom - rc.top;
    info.handle_id = reinterpret_cast<unsigned long long>(hwnd);
    if (!info.title.empty() || !info.process_name.empty()) {
        list->push_back(std::move(info));
    }
    return TRUE;
}
} // namespace

std::vector<WindowInfo> TftWindowFinder::EnumerateWindows() {
    std::vector<WindowInfo> list;
    EnumWindows(EnumProc, reinterpret_cast<LPARAM>(&list));
    return list;
}
#else
std::vector<WindowInfo> TftWindowFinder::EnumerateWindows() {
    return {};
}
#endif

MatchResult TftWindowFinder::Find() const {
    auto windows = EnumerateWindows();
    return rules_.SelectTftWindow(windows);
}

} // namespace tft
