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

#include <EEPROM.h>
#include <Scheduler.hpp>
#include <Clock.hpp>

/*
PersistentValue<T> loads a value from EEPROM on startup and saves it back
whenever it changes. Writes are deferred by writeDelayMs (default 500ms) so
rapid changes (e.g. encoder spinning) don't wear the EEPROM.

EEPROM has a finite write cycle lifetime (~100k writes per byte), so the
write delay is important for values that change frequently.

Address layout: each instance occupies sizeof(T) bytes starting at `address`.
Use PersistentValue<T>::Size to compute the next address:

  long speed = 50;
  int  mode  = 0;
  PersistentValue<long> savedSpeed(schedule, 0,                    speed);
  PersistentValue<int>  savedMode (schedule, PersistentValue<long>::Size, mode);

On first flash EEPROM contains uninitialized bytes — call reset(defaultValue)
to write a known-good starting value.
*/

template <class T>
class PersistentValue : private Scheduled {
    int _address;
    T &_value;
    T _last;
    T _saved;
    long _writeDelayMs;
    Timer _writeTimer;
public:
    static const int Size = sizeof(T);

    PersistentValue(Schedule &schedule, int address, T &value, long writeDelayMs = 500) :
        Scheduled(schedule),
        _address(address), _value(value),
        _last(value), _saved(value),
        _writeDelayMs(writeDelayMs), _writeTimer(0) {
        EEPROM.get(_address, _value);
        _last = _value;
        _saved = _value;
    }

    // Write defaultValue to EEPROM immediately and set value.
    // Call this on first flash to initialise the stored value.
    void reset(T defaultValue) {
        _value = defaultValue;
        save();
    }

    // Force an immediate write regardless of the delay timer.
    void save() {
        EEPROM.put(_address, _value);
        _last = _value;
        _saved = _value;
    }

    // Re-read from EEPROM, discarding any unsaved in-memory change.
    void load() {
        EEPROM.get(_address, _value);
        _last = _value;
        _saved = _value;
    }

    void poll() override {
        if (_value != _last) {
            _last = _value;
            _writeTimer.reset(_writeDelayMs);
        } else if (_last != _saved && _writeTimer.expired()) {
            EEPROM.put(_address, _last);
            _saved = _last;
        }
    }
};
