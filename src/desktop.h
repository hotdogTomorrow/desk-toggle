#pragma once
#include <windows.h>

namespace desk {
HRESULT GetIconsHidden(bool& hidden);
HRESULT SetIconsHidden(bool hidden);
HRESULT ToggleIcons();
bool IsDesktopBackground(POINT point);
}
