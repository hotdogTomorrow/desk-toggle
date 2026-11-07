#include "gesture.h"
#include <cstdio>
#include <cstdlib>

namespace {
int count = 0;
void Check(bool condition, const char* name) {
    ++count;
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", name); std::exit(1); }
}
desk::Gesture NewGesture() {
    desk::Gesture g;
    g.Configure(500, 8, 8, 4, 4);
    return g;
}
bool Click(desk::Gesture& g, desk::Point p, std::uint32_t time, bool blank = true,
           std::uintptr_t device = 1) {
    g.Down(p, time, device, blank);
    return g.Up(p, time + 20, device, blank);
}
}
int main() {
    auto g = NewGesture();
    Check(!Click(g, {100, 100}, 100), "single click does nothing");
    Check(Click(g, {101, 101}, 250), "double click toggles");
    Check(!Click(g, {100, 100}, 350), "triple click does not toggle twice");
    Check(Click(g, {100, 100}, 450), "four clicks toggle twice");
    g = NewGesture();
    Click(g, {0, 0}, 100);
    Check(!Click(g, {0, 0}, 601), "slow clicks do not toggle");
    Check(!Click(g, {5, 0}, 700), "outside double-click rectangle");
    g = NewGesture();
    Check(!Click(g, {0, 0}, 100, false), "icon click does nothing");
    Check(!Click(g, {0, 0}, 200), "icon followed by blank does not toggle");
    Click(g, {0, 0}, 250, false);
    Check(!Click(g, {0, 0}, 300), "icon interrupts blank click sequence");
    g = NewGesture();
    g.Down({0, 0}, 100, 1, true);
    g.Move({100, 100});
    g.Move({0, 0});
    Check(!g.Up({0, 0}, 200, 1, true), "drag out and back is rejected");
    Check(!Click(g, {0, 0}, 250), "drag cannot start double click");
    g = NewGesture();
    Click(g, {-1000, -200}, 100);
    Check(Click(g, {-998, -202}, 200), "negative monitor coordinates");
    g = NewGesture();
    Click(g, {0, 0}, 100);
    Check(!Click(g, {0, 0}, 200, true, 2), "different mouse devices do not form a pair");
    g = NewGesture();
    Click(g, {0, 0}, 0xfffffff0u);
    Check(Click(g, {0, 0}, 80), "32-bit system timestamp wrap");
    g = NewGesture();
    g.Down({0, 0}, 100, 1, true);
    Check(!g.Up({0, 0}, 700, 1, true), "long press is rejected");
    Check(!Click(g, {0, 0}, 750), "long press cannot start pair");
    g = NewGesture();
    Click(g, {0, 0}, 100);
    g.Reset();
    Check(!Click(g, {0, 0}, 200), "reset on settings/device change");
    g.Down({0, 0}, 250, 1, true);
    Check(!g.Up({0, 0}, 270, 1, false), "release on icon cancels candidate");
    Check(!g.Up({0, 0}, 280, 1, true), "orphan release is rejected");
    g = NewGesture();
    Click(g, {100, 100}, 100);
    Check(Click(g, {100, 100}, 200), "restore gesture completes normally");
    Check(!Click(g, {100, 100}, 10000), "delayed single click after restore does nothing");
    Check(!Click(g, {100, 100}, 20000), "separate later single clicks never form a pair");
    g = NewGesture();
    Click(g, {0, 0}, 100);
    g.Down({0, 0}, 200, 1, true);
    g.Move({100, 0});
    Check(!g.Up({0, 0}, 220, 1, true), "dragging second click cancels the pair");
    Check(!Click(g, {0, 0}, 300), "single click after cancelled pair does nothing");
    std::printf("%d gesture checks passed\n", count);
    return 0;
}
