/*
MIT License

Copyright (c) 2022-2025 jffordem

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

/* I want a driver module for a nema 17 stepper motor.  It needs a fine, and coarse stepping mode, and should work with common stepper libraries. */

#include <Arduino.hpp>
#include <Scheduler.hpp>
#include <PinIO.hpp>
#include <Mapper.hpp>

class IStepper {
public:
    virtual void step(int steps) = 0;
};

// Poll-based 4-wire full-step stepper. Each call to step() queues steps;
// poll() advances one phase per stepMs without blocking.
class StepperControl : public IStepper, private Scheduled {
    int _pins[4];
    int _remaining;
    int _phase;
    long _stepMs;
    Timer _timer;

    void applyPhase() {
        int active = (_remaining > 0) ? _phase : (3 - _phase);
        for (int i = 0; i < 4; i++) {
            digitalWrite(_pins[i], i == active ? HIGH : LOW);
        }
    }
public:
    StepperControl(Schedule &schedule, int pin1, int pin2, int pin3, int pin4, long stepMs = 10) :
        Scheduled(schedule), _remaining(0), _phase(0), _stepMs(stepMs), _timer(0) {
        _pins[0] = pin1; _pins[1] = pin2; _pins[2] = pin3; _pins[3] = pin4;
        for (int i = 0; i < 4; i++) pinMode(_pins[i], OUTPUT);
    }
    void step(int steps) override { _remaining += steps; }
    int stepsRemaining() const { return _remaining; }
    void poll() override {
        if (_remaining == 0 || !_timer.expired()) return;
        _timer.reset(_stepMs);
        applyPhase();
        _phase = (_phase + 1) % 4;
        if (_remaining > 0) _remaining--;
        else _remaining++;
    }
};
