#include "startup.h"
#include <vector>

namespace {
constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kValue[] = L"DeskToggle";
}
namespace desk {
std::wstring ExecutablePath() {
    std::vector<wchar_t> buffer(32768);
    const DWORD size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (!size || size >= buffer.size()) return {};
    return {buffer.data(), size};
}

LSTATUS StartupRegistered(bool& registered) {
    registered = false;
    DWORD bytes = 0;
    LSTATUS status = RegGetValueW(HKEY_CURRENT_USER, kRunKey, kValue, RRF_RT_REG_SZ,
                                 nullptr, nullptr, &bytes);
    if (status == ERROR_FILE_NOT_FOUND) return ERROR_SUCCESS;
    if (status != ERROR_SUCCESS) return status;
    std::vector<wchar_t> value(bytes / sizeof(wchar_t) + 1);
    status = RegGetValueW(HKEY_CURRENT_USER, kRunKey, kValue, RRF_RT_REG_SZ,
                          nullptr, value.data(), &bytes);
    if (status == ERROR_SUCCESS) registered = value[0] != L'\0';
    return status;
}

LSTATUS SetStartup(bool enabled) {
    if (!enabled) {
        const LSTATUS status = RegDeleteKeyValueW(HKEY_CURRENT_USER, kRunKey, kValue);
        return status == ERROR_FILE_NOT_FOUND ? ERROR_SUCCESS : status;
    }
    const std::wstring path = ExecutablePath();
    if (path.empty()) return ERROR_BAD_PATHNAME;
    const std::wstring command = L"\"" + path + L"\"";
    // Run keys document a 260-character command limit. Never silently truncate.
    if (command.size() > 260) return ERROR_FILENAME_EXCED_RANGE;
    HKEY key = nullptr;
    LSTATUS status = RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0,
                                    KEY_SET_VALUE, nullptr, &key, nullptr);
    if (status != ERROR_SUCCESS) return status;
    status = RegSetValueExW(key, kValue, 0, REG_SZ,
                            reinterpret_cast<const BYTE*>(command.c_str()),
                            static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return status;
}
}
