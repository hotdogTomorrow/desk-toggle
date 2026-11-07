#pragma once
#include <windows.h>
#include <string>

namespace desk {
std::wstring ExecutablePath();
LSTATUS StartupRegistered(bool& registered);
LSTATUS SetStartup(bool enabled);
}
