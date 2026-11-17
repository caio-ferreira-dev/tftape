#pragma once

#include <chrono>

namespace tft {

using ClockTimePoint = std::chrono::system_clock::time_point;

/// Relógio injetável para tornar logs/testes determinísticos.
class IClock {
public:
    virtual ~IClock() = default;
    virtual ClockTimePoint Now() const = 0;
};

class SystemClock final : public IClock {
public:
    ClockTimePoint Now() const override { return std::chrono::system_clock::now(); }
};

class FakeClock final : public IClock {
public:
    explicit FakeClock(ClockTimePoint initial = ClockTimePoint{}) : now_(initial) {}

    void Set(ClockTimePoint value) { now_ = value; }
    void Advance(std::chrono::milliseconds delta) { now_ += delta; }
    ClockTimePoint Now() const override { return now_; }

private:
    ClockTimePoint now_{};
};

inline SystemClock& SystemClockInstance() {
    static SystemClock clock;
    return clock;
}

} // namespace tft
