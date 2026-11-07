#include <windows.h>
#include <objbase.h>
#include <windowsx.h>
#include <shellapi.h>
#include <strsafe.h>
#include "desktop.h"
#include "startup.h"
#include "gesture.h"

namespace {
constexpr wchar_t kClass[] = L"DeskToggle.Tray.v1";
constexpr wchar_t kMutex[] = L"Local\\DeskToggle.Singleton.v1";
constexpr UINT kTray = WM_APP + 1;
constexpr UINT kToggle = 100, kShow = 101, kStartup = 102, kAbout = 103, kExit = 104;
desk::Gesture gesture;
NOTIFYICONDATAW tray{};
bool trayAdded = false, hiddenByUs = false;
USHORT primaryDown = RI_MOUSE_LEFT_BUTTON_DOWN, primaryUp = RI_MOUSE_LEFT_BUTTON_UP;
UINT taskbarCreated = 0;
HINSTANCE instance = nullptr;

void Error(HWND owner, const wchar_t* description, HRESULT error) {
    wchar_t text[512]{};
    StringCchPrintfW(text, ARRAYSIZE(text), L"%s\n\n错误代码：0x%08lX", description,
                     static_cast<unsigned long>(error));
    MessageBoxW(owner, text, L"DeskToggle", MB_OK | MB_ICONWARNING);
}

void ConfigureGesture() {
    const bool swapped = GetSystemMetrics(SM_SWAPBUTTON) != 0;
    primaryDown = swapped ? RI_MOUSE_RIGHT_BUTTON_DOWN : RI_MOUSE_LEFT_BUTTON_DOWN;
    primaryUp = swapped ? RI_MOUSE_RIGHT_BUTTON_UP : RI_MOUSE_LEFT_BUTTON_UP;
    gesture.Configure(GetDoubleClickTime(), GetSystemMetrics(SM_CXDOUBLECLK),
                       GetSystemMetrics(SM_CYDOUBLECLK), GetSystemMetrics(SM_CXDRAG),
                       GetSystemMetrics(SM_CYDRAG));
}

bool AddTray(HWND window) {
    tray = {};
    tray.cbSize = sizeof(tray);
    tray.hWnd = window;
    tray.uID = 1;
    tray.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    tray.uCallbackMessage = kTray;
    tray.hIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));
    StringCchCopyW(tray.szTip, ARRAYSIZE(tray.szTip), L"DeskToggle · 双击桌面空白处隐藏／显示图标");
    trayAdded = Shell_NotifyIconW(NIM_ADD, &tray) != FALSE;
    if (trayAdded) {
        tray.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &tray);
    }
    return trayAdded;
}

void Toggle(HWND window) {
    gesture.Reset();
    bool hidden = false;
    HRESULT hr = desk::GetIconsHidden(hidden);
    if (SUCCEEDED(hr)) hr = desk::SetIconsHidden(!hidden);
    if (SUCCEEDED(hr)) hiddenByUs = !hidden;
    else Error(window, L"无法切换桌面图标。请确认 Windows 资源管理器正在运行。", hr);
}

void Menu(HWND window, POINT position) {
    gesture.Reset();
    bool registered = false;
    const LSTATUS startupStatus = desk::StartupRegistered(registered);
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, kToggle, L"隐藏／显示桌面图标");
    AppendMenuW(menu, MF_STRING, kShow, L"显示桌面图标");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (registered ? MF_CHECKED : MF_UNCHECKED) |
        (startupStatus == ERROR_SUCCESS ? 0 : MF_GRAYED), kStartup, L"开机自启（登录后启动）");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kAbout, L"关于 DeskToggle");
    AppendMenuW(menu, MF_STRING, kExit, L"退出（恢复本程序隐藏的图标）");
    SetForegroundWindow(window);
    const UINT command = static_cast<UINT>(TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY |
        TPM_RIGHTBUTTON, position.x, position.y, 0, window, nullptr));
    DestroyMenu(menu);
    PostMessageW(window, WM_NULL, 0, 0);
    switch (command) {
    case kToggle: Toggle(window); break;
    case kShow: {
        const HRESULT hr = desk::SetIconsHidden(false);
        if (FAILED(hr)) Error(window, L"无法显示桌面图标。", hr);
        else hiddenByUs = false;
        break;
    }
    case kStartup: {
        const LSTATUS status = desk::SetStartup(!registered);
        if (status != ERROR_SUCCESS) Error(window, L"无法修改开机自启设置。", HRESULT_FROM_WIN32(status));
        break;
    }
    case kAbout:
        MessageBoxW(window, L"DeskToggle 0.1.0\n\n双击桌面空白处隐藏／显示图标。\n"
                    L"右键托盘图标可设置开机自启或退出。\n\n"
                    L"原生 C++ / Win32 · 无联网 · 无遥测\nMIT License",
                    L"关于 DeskToggle", MB_OK | MB_ICONINFORMATION);
        break;
    case kExit: PostMessageW(window, WM_CLOSE, 0, 0); break;
    default: break;
    }
    gesture.Reset();
}

bool ModifiedClick() {
    return (GetAsyncKeyState(VK_CONTROL) & 0x8000) || (GetAsyncKeyState(VK_SHIFT) & 0x8000) ||
           (GetAsyncKeyState(VK_MENU) & 0x8000) || (GetAsyncKeyState(VK_LWIN) & 0x8000) ||
           (GetAsyncKeyState(VK_RWIN) & 0x8000);
}

void RawMouse(HWND window, HRAWINPUT handle) {
    RAWINPUT input{};
    UINT size = sizeof(input);
    const UINT read = GetRawInputData(handle, RID_INPUT, &input, &size, sizeof(RAWINPUTHEADER));
    if (read == static_cast<UINT>(-1) || read < sizeof(RAWINPUTHEADER) + sizeof(RAWMOUSE) ||
        input.header.dwType != RIM_TYPEMOUSE) return;
    const USHORT buttons = input.data.mouse.usButtonFlags;
    // Mouse movement without a held candidate is the hot path: no desktop/COM query,
    // allocation, cursor lookup, or redraw. Raw Input never blocks another app's input.
    if (!buttons && !gesture.Held()) return;
    if (buttons & ~(primaryDown | primaryUp)) {
        gesture.Reset(); return;
    }
    POINT point{};
    if (!GetCursorPos(&point)) { gesture.Reset(); return; }
    const desk::Point p{point.x, point.y};
    const auto time = static_cast<std::uint32_t>(GetMessageTime());
    const auto device = reinterpret_cast<std::uintptr_t>(input.header.hDevice);
    if (buttons & primaryDown) {
        gesture.Down(p, time, device, !ModifiedClick() && desk::IsDesktopBackground(point));
    }
    if (!buttons) gesture.Move(p);
    if (buttons & primaryUp) {
        const bool blank = gesture.Held() && !ModifiedClick() && desk::IsDesktopBackground(point);
        if (gesture.Up(p, time, device, blank)) Toggle(window);
    }
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (taskbarCreated && message == taskbarCreated) {
        gesture.Reset();
        AddTray(window); // Shell broadcasts this after Explorer recreates its taskbar.
        return 0;
    }
    switch (message) {
    case WM_INPUT:
        RawMouse(window, reinterpret_cast<HRAWINPUT>(lParam));
        // Required cleanup for RIM_INPUT; harmless for RIM_INPUTSINK.
        return DefWindowProcW(window, message, wParam, lParam);
    case WM_INPUT_DEVICE_CHANGE:
    case WM_POWERBROADCAST:
        gesture.Reset(); break;
    case WM_SETTINGCHANGE:
    case WM_DISPLAYCHANGE:
        ConfigureGesture(); break;
    case kTray:
        switch (LOWORD(lParam)) {
        case WM_CONTEXTMENU:
            Menu(window, POINT{GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam)}); break;
        case NIN_SELECT:
        case NIN_KEYSELECT: {
            POINT point{};
            GetCursorPos(&point);
            Menu(window, point);
            break;
        }
        default: break;
        }
        return 0;
    case WM_CLOSE:
        if (hiddenByUs) {
            const HRESULT hr = desk::SetIconsHidden(false);
            if (FAILED(hr)) {
                Error(window, L"无法恢复图标。可在桌面右键 → 查看 → 显示桌面图标中恢复。", hr);
            }
        }
        DestroyWindow(window);
        return 0;
    case WM_QUERYENDSESSION: return TRUE;
    case WM_ENDSESSION:
        // Do not call Explorer/COM while Windows is tearing down the session.
        if (wParam) DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        if (trayAdded) Shell_NotifyIconW(NIM_DELETE, &tray);
        PostQuitMessage(0); return 0;
    default: break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
}

int WINAPI wWinMain(HINSTANCE module, HINSTANCE, PWSTR command, int) {
    instance = module;
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(init)) return 1;
    if (lstrcmpW(command, L"--quit") == 0) {
        if (HWND window = FindWindowW(kClass, nullptr)) PostMessageW(window, WM_CLOSE, 0, 0);
        CoUninitialize(); return 0;
    }
    if (lstrcmpW(command, L"--show") == 0 || lstrcmpW(command, L"--toggle") == 0) {
        const HRESULT hr = lstrcmpW(command, L"--show") == 0 ? desk::SetIconsHidden(false) : desk::ToggleIcons();
        if (FAILED(hr)) Error(nullptr, L"无法修改桌面图标显示状态。", hr);
        CoUninitialize(); return FAILED(hr) ? 1 : 0;
    }
    if (*command) {
        MessageBoxW(nullptr, L"用法：DeskToggle.exe [--show | --toggle | --quit]",
                     L"DeskToggle", MB_OK | MB_ICONINFORMATION);
        CoUninitialize(); return 1;
    }
    HANDLE mutex = CreateMutexW(nullptr, FALSE, kMutex);
    if (!mutex) { CoUninitialize(); return 1; }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"DeskToggle 已在运行。请右键任务栏通知区域中的 DeskToggle 图标。",
                     L"DeskToggle", MB_OK | MB_ICONINFORMATION);
        CloseHandle(mutex); CoUninitialize(); return 0;
    }
    WNDCLASSW wc{};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = module;
    wc.lpszClassName = kClass;
    wc.hIcon = LoadIconW(module, MAKEINTRESOURCEW(1));
    RegisterClassW(&wc);
    taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    // A hidden top-level window, not HWND_MESSAGE: receive Explorer broadcasts.
    HWND window = CreateWindowExW(WS_EX_TOOLWINDOW, kClass, L"DeskToggle", WS_POPUP,
                                  0, 0, 0, 0, nullptr, nullptr, module, nullptr);
    int result = 1;
    if (window) {
        ConfigureGesture();
        const RAWINPUTDEVICE mouse{0x01, 0x02, RIDEV_INPUTSINK | RIDEV_DEVNOTIFY, window};
        if (!RegisterRawInputDevices(&mouse, 1, sizeof(mouse))) {
            Error(window, L"无法注册鼠标输入。", HRESULT_FROM_WIN32(GetLastError()));
        } else if (!AddTray(window)) {
            Error(window, L"无法创建托盘图标。请在资源管理器就绪后重试。", E_FAIL);
        } else {
            MSG message{};
            BOOL status = 0;
            while ((status = GetMessageW(&message, nullptr, 0, 0)) > 0) {
                DispatchMessageW(&message);
            }
            result = status == 0 ? 0 : 1;
        }
        if (IsWindow(window)) DestroyWindow(window);
    }
    CloseHandle(mutex);
    CoUninitialize();
    return result;
}
