#include "desktop.h"
#include "startup.h"
#include <objbase.h>
#include <cstdio>
#include <cwchar>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
int StartupRoundtrip() {
    constexpr wchar_t keyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, keyPath, 0, nullptr, 0,
                       KEY_QUERY_VALUE | KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return 1;
    DWORD type = 0, bytes = 0;
    const LSTATUS prior = RegQueryValueExW(key, L"DeskToggle", nullptr, &type, nullptr, &bytes);
    if (prior != ERROR_SUCCESS && prior != ERROR_FILE_NOT_FOUND) { RegCloseKey(key); return 1; }
    std::vector<BYTE> original(bytes);
    if (prior == ERROR_SUCCESS && RegQueryValueExW(key, L"DeskToggle", nullptr, &type,
            original.data(), &bytes) != ERROR_SUCCESS) { RegCloseKey(key); return 1; }

    bool enabled = true;
    bool passed = desk::SetStartup(false) == ERROR_SUCCESS &&
                  desk::StartupRegistered(enabled) == ERROR_SUCCESS && !enabled;
    passed = desk::SetStartup(true) == ERROR_SUCCESS && passed;
    passed = desk::StartupRegistered(enabled) == ERROR_SUCCESS && enabled && passed;
    wchar_t value[32768]{};
    DWORD length = sizeof(value);
    const LSTATUS query = RegGetValueW(key, nullptr, L"DeskToggle", RRF_RT_REG_SZ,
                                     nullptr, value, &length);
    passed = query == ERROR_SUCCESS && value == (L"\"" + desk::ExecutablePath() + L"\"") && passed;
    passed = desk::SetStartup(false) == ERROR_SUCCESS && passed;
    passed = desk::StartupRegistered(enabled) == ERROR_SUCCESS && !enabled && passed;
    const LSTATUS restore = prior == ERROR_SUCCESS ? RegSetValueExW(key, L"DeskToggle", 0, type,
        original.data(), bytes) : RegDeleteValueW(key, L"DeskToggle");
    passed = (restore == ERROR_SUCCESS || (prior == ERROR_FILE_NOT_FOUND && restore == ERROR_FILE_NOT_FOUND)) && passed;
    RegCloseKey(key);
    std::printf("startup_roundtrip_passed=%d\n", passed);
    return passed ? 0 : 1;
}
}

// Developer-only probe, excluded from distribution. No names, paths or item contents.
int wmain(int argc, wchar_t** argv) {
    if (argc == 2 && std::wcscmp(argv[1], L"--startup-roundtrip") == 0) return StartupRoundtrip();
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 1;
    int result = 0;
    bool initial = false;
    HRESULT hr = desk::GetIconsHidden(initial);
    if (FAILED(hr)) {
        std::printf("shell_error=0x%08lX\n", static_cast<unsigned long>(hr));
        result = 1;
    } else {
        std::printf("icons_hidden=%d\n", initial);
        if (argc == 2 && std::wcscmp(argv[1], L"--roundtrip") == 0) {
            hr = desk::SetIconsHidden(!initial);
            bool changed = initial;
            const HRESULT query = desk::GetIconsHidden(changed);
            const HRESULT restore = desk::SetIconsHidden(initial);
            bool finalState = !initial;
            const HRESULT finalQuery = desk::GetIconsHidden(finalState);
            const bool passed = SUCCEEDED(hr) && SUCCEEDED(query) && changed != initial &&
                SUCCEEDED(restore) && SUCCEEDED(finalQuery) && finalState == initial;
            std::printf("roundtrip_passed=%d\n", passed);
            result = passed ? 0 : 1;
        } else if (argc == 4 && std::wcscmp(argv[1], L"--point") == 0) {
            const POINT p{std::wcstol(argv[2], nullptr, 10), std::wcstol(argv[3], nullptr, 10)};
            std::printf("desktop_background=%d\n", desk::IsDesktopBackground(p));
        }
    }
    CoUninitialize();
    return result;
}
