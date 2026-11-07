#include "desktop.h"
#include <ole2.h>
#include <exdisp.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <servprov.h>
#include <oleacc.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace {
HRESULT DesktopView(ComPtr<IFolderView2>& view) {
    ComPtr<IShellWindows> windows;
    HRESULT hr = CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_LOCAL_SERVER,
                                  IID_PPV_ARGS(&windows));
    if (FAILED(hr)) return hr;
    VARIANT location{};
    location.vt = VT_I4;
    location.lVal = CSIDL_DESKTOP;
    VARIANT root{};
    long handle = 0;
    ComPtr<IDispatch> dispatch;
    hr = windows->FindWindowSW(&location, &root, SWC_DESKTOP, &handle,
                               SWFO_NEEDDISPATCH, &dispatch);
    if (FAILED(hr)) return hr;
    if (!dispatch) return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    ComPtr<IServiceProvider> services;
    hr = dispatch.As(&services);
    if (FAILED(hr)) return hr;
    ComPtr<IShellBrowser> browser;
    hr = services->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(&browser));
    if (FAILED(hr)) return hr;
    ComPtr<IShellView> shellView;
    hr = browser->QueryActiveShellView(&shellView);
    return FAILED(hr) ? hr : shellView.As(&view);
}

bool HasClass(HWND window, const wchar_t* name) {
    wchar_t actual[64]{};
    return GetClassNameW(window, actual, ARRAYSIZE(actual)) && lstrcmpW(actual, name) == 0;
}
}

namespace desk {
HRESULT GetIconsHidden(bool& hidden) {
    ComPtr<IFolderView2> view;
    HRESULT hr = DesktopView(view);
    DWORD flags = 0;
    if (SUCCEEDED(hr)) hr = view->GetCurrentFolderFlags(&flags);
    if (SUCCEEDED(hr)) hidden = (flags & FWF_NOICONS) != 0;
    return hr;
}

HRESULT SetIconsHidden(bool hidden) {
    // Reacquire on each operation: no stale Explorer proxy after a shell restart.
    ComPtr<IFolderView2> view;
    HRESULT hr = DesktopView(view);
    if (SUCCEEDED(hr)) hr = view->SetCurrentFolderFlags(FWF_NOICONS, hidden ? FWF_NOICONS : 0);
    if (FAILED(hr)) return hr;
    DWORD flags = 0;
    hr = view->GetCurrentFolderFlags(&flags);
    if (SUCCEEDED(hr) && ((flags & FWF_NOICONS) != 0) != hidden) return E_FAIL;
    return hr;
}

HRESULT ToggleIcons() {
    bool hidden = false;
    HRESULT hr = GetIconsHidden(hidden);
    return FAILED(hr) ? hr : SetIconsHidden(!hidden);
}

bool IsDesktopBackground(POINT point) {
    const HWND hit = WindowFromPoint(point);
    if (!hit) return false;
    const HWND root = GetAncestor(hit, GA_ROOT);
    if (!HasClass(root, L"Progman") && !HasClass(root, L"WorkerW")) return false;
    DWORD shellProcess = 0, hitProcess = 0;
    GetWindowThreadProcessId(GetShellWindow(), &shellProcess);
    GetWindowThreadProcessId(hit, &hitProcess);
    if (!shellProcess || shellProcess != hitProcess) return false;

    if (HasClass(hit, L"SysListView32")) {
        // An arbitrary Explorer list-view is not the desktop.
        if (!HasClass(GetParent(hit), L"SHELLDLL_DefView")) return false;
        ComPtr<IAccessible> accessible;
        if (FAILED(AccessibleObjectFromWindow(hit, static_cast<DWORD>(OBJID_CLIENT),
                                              IID_PPV_ARGS(&accessible)))) return false;
        VARIANT child{};
        const HRESULT hr = accessible->accHitTest(point.x, point.y, &child);
        const bool blank = hr == S_OK && child.vt == VT_I4 && child.lVal == CHILDID_SELF;
        VariantClear(&child);
        return blank;
    }
    // With icons hidden the mouse lands on the desktop host instead of its list-view.
    return hit == root || HasClass(hit, L"SHELLDLL_DefView");
}
}
