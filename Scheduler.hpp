/*
MIT License

Copyright (c) 2022 jffordem

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

#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

/*
MainSchedule provides a single pollable object for all pollers.
Current uses hardcoded limit of 50 pollable objects to avoid dynamic allocation.
Change the MAX_POLLERS if it's insufficient.  Unfortuantely there really isn't a good
way to tell when you've exceeded the max at this point.

All pollable objects should take schedule as the first parameter, and add themselves
to ensure they get into the polling loop.

Example:
MainSchedule schedule;
SomePollableClass myobject(schedule, a, b, c);
void setup() {
	schedule.begin();
}
void loop() {
	schedule.poll();
}
*/

#include <Arduino.hpp>
#include <LinkedList.hpp>

const long MAX_LONG = 2147483647L;
const unsigned long MAX_ULONG =  4294967295UL;

const int MAX_INT = 32767;
const unsigned int MAX_UINT = 65535U;

class Pressable {
public:
	virtual void press() = 0;
	virtual void release() = 0;
};

class Poller {
public:
	virtual void poll() = 0;
};

class Enabled {
public:
	virtual void enable(bool value) = 0;
	virtual void toggle() = 0;
	virtual bool enabled() const = 0;
};

template <class T>
class Composite : public List<T*>, public T {
public:
	Composite(T *items[] = NULL, int count = 0) : List<T*>(items, count) { }
};

class PressComposite : public Composite<Pressable> {
public:
	PressComposite(Pressable *itemsZ[] = NULL) :
		Composite<Pressable>(itemsZ, countZ(itemsZ)) { }
	void press() {
		for (int i = 0; i < length(); i++) {
			item(i)->press();
		}
	}
	void release() {
		for (int i = 0; i < length(); i++) {
			item(i)->release();
		}
	}
};

class EnableComposite : public Composite<Enabled> {
public:
	EnableComposite(Enabled *itemsZ[] = NULL) :
		Composite<Enabled>(itemsZ, countZ(itemsZ)) { }
	void enable(bool value) {
		for (int i = 0; i < length(); i++) {
			item(i)->enable(value);
		}
	}
	void toggle() {
		for (int i = 0; i < length(); i++) {
			item(i)->toggle();
		}
	}
	bool enabled() const {
		for (int i = 0; i < length(); i++) {
			if (item(i)->enabled()) {
				return true;
			}
		}
		return false;
	}
};

class PollerComposite : public Composite<Poller> {
public:
	PollerComposite(Poller *itemsZ[] = NULL) :
		Composite<Poller>(itemsZ, countZ(itemsZ)) { }
	void poll() {
		for (int i = 0; i < length(); i++) {
			item(i)->poll();
		}
	}
};

typedef PollerComposite Schedule;

class MainSchedule : public Schedule {
public:
	MainSchedule() { }
	void begin() {
		// Let transient effects work themselves out.
		for (int i = 0; i < 25; i++) {
			poll();
		}
	}
};

class Scheduled : public Poller {
public:
	Scheduled(Schedule &schedule) {
		schedule.add(this);
	}
};

class PollGroup : public PollerComposite, public Scheduled, public Enabled {
	bool _enabled;
public:
	PollGroup(Schedule &schedule) : Scheduled(schedule) { }
	void enable(bool value) { _enabled = value; }
	void toggle() { enable(!_enabled); }
	bool enabled() const { return _enabled; }
	void poll() {
		if (_enabled) {
			PollerComposite::poll();
		}
	}
};

#endif
