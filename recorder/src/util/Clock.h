#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

namespace tft {

using ClockTimePoint = std::chrono::system_clock::time_point;
using SteadyTimePoint = std::chrono::steady_clock::time_point;

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

/// Relógio monotonic usado para elapsed/timestamps de mídia (não sofre ajustes de NTP).
class ISteadyClock {
public:
    virtual ~ISteadyClock() = default;
    virtual SteadyTimePoint Now() const = 0;
};

class SteadyClock final : public ISteadyClock {
public:
    SteadyTimePoint Now() const override { return std::chrono::steady_clock::now(); }
};

class FakeSteadyClock final : public ISteadyClock {
public:
    explicit FakeSteadyClock(SteadyTimePoint initial = SteadyTimePoint{}) : now_(initial) {}

    void Set(SteadyTimePoint value) { now_ = value; }
    void Advance(std::chrono::milliseconds delta) { now_ += delta; }
    SteadyTimePoint Now() const override { return now_; }

private:
    SteadyTimePoint now_{};
};

inline SystemClock& SystemClockInstance() {
    static SystemClock clock;
    return clock;
}

inline SteadyClock& SteadyClockInstance() {
    static SteadyClock clock;
    return clock;
}

inline std::int64_t Elapsed100ns(const ISteadyClock& clock, SteadyTimePoint start) {
    const auto delta = std::chrono::duration_cast<std::chrono::nanoseconds>(clock.Now() - start);
    return delta.count() / 100; // 1 tick = 100 ns, mesmo formato dos frames de mídia.
}

inline std::string FormatUtcIso8601(ClockTimePoint tp) {
    const auto seconds = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &seconds);
#else
    gmtime_r(&seconds, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

} // namespace tft
