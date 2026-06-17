#pragma once
#include <Arduino.h>

// Forked from jffordem_Scheduler/Clock.hpp Timer class.
// Changes from original:
//   - uint32_t throughout instead of long (matches millis() return type)
//   - No Expires base class or Scheduler dependency
//   - Added disarm() to suspend a timer indefinitely
class Timer {
    uint32_t _time;
    uint32_t _lastReset;
public:
    Timer(uint32_t time = 0) : _time(time), _lastReset(millis()) {}

    // Returns true once _time ms have elapsed since the last reset.
    // Uses subtraction so uint32_t rollover at ~49 days is handled correctly.
    bool expired() const { return (millis() - _lastReset) > _time; }

    void reset()               { _lastReset = millis(); }
    void reset(uint32_t time)  { _time = time; _lastReset = millis(); }

    // Park the timer so expired() never returns true until the next reset().
    void disarm()              { _time = UINT32_MAX; _lastReset = millis(); }
};
