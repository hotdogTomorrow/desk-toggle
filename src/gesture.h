#pragma once
#include <cstdint>

namespace desk {
struct Point { long x = 0; long y = 0; };

// No allocation, timers, Windows dependencies, or work while the mouse is idle.
class Gesture {
public:
    void Configure(std::uint32_t interval, long doubleWidth, long doubleHeight,
                   long dragWidth, long dragHeight) {
        interval_ = interval;
        doubleX_ = doubleWidth / 2;
        doubleY_ = doubleHeight / 2;
        dragX_ = dragWidth;
        dragY_ = dragHeight;
        Reset();
    }
    void Reset() { held_ = first_ = second_ = false; }
    bool Held() const { return held_; }

    void Down(Point p, std::uint32_t time, std::uintptr_t device, bool blank) {
        if (!blank) { Reset(); return; }
        second_ = first_ && device == device_ && time - firstTime_ <= interval_ &&
                  Near(p, firstPoint_, doubleX_, doubleY_);
        first_ = false;
        held_ = true;
        down_ = p;
        downTime_ = time;
        device_ = device;
    }
    void Move(Point p) {
        if (held_ && !Near(p, down_, dragX_, dragY_)) Reset();
    }
    bool Up(Point p, std::uint32_t time, std::uintptr_t device, bool blank) {
        if (!held_ || device != device_ || !blank ||
            time - downTime_ > interval_ || !Near(p, down_, dragX_, dragY_)) {
            Reset(); return false;
        }
        held_ = false;
        if (second_) { Reset(); return true; }
        first_ = true;
        firstTime_ = downTime_;
        firstPoint_ = down_;
        return false;
    }
private:
    static bool Near(Point a, Point b, long x, long y) {
        const auto dx = static_cast<std::int64_t>(a.x) - b.x;
        const auto dy = static_cast<std::int64_t>(a.y) - b.y;
        return dx >= -x && dx <= x && dy >= -y && dy <= y;
    }
    std::uint32_t interval_ = 500, downTime_ = 0, firstTime_ = 0;
    long doubleX_ = 2, doubleY_ = 2, dragX_ = 4, dragY_ = 4;
    Point down_, firstPoint_;
    std::uintptr_t device_ = 0;
    bool held_ = false, first_ = false, second_ = false;
};
}
