/* Keypad key handler for LK204_25 keypad.
 * The keypad geometry is this.
 * A[1] B[2] C[3] D[A]
 * F[4] G[5] H[6] I[B]
 * K[7] L[8] M[9] N[C]
 * P[*] Q[0] R[#] S[D]
 * So, here's how you'd map the [A] key (which is read as 'D') to toggle something.
 * MainSchedule schedule;
 * LK204_25_Keypad keypad;
 * SomethingEnabled enabled();
 * ToggleKeypadKeyHandler toggler('D', enabled);
 * KeypadHandler handller(schedule, keypad, toggler);
*/

#ifndef KEYPAD_HANDLER_HPP
#define KEYPAD_HANDLER_HPP

#include <Scheduler.hpp>
#include <LK204_25.hpp>

// Here's the keymapping for handlers - or should this go in LM204_25?
const char KeypadKey_1 = 'A';
const char KeypadKey_2 = 'B';
const char KeypadKey_3 = 'C';
const char KeypadKey_A = 'D';
const char KeypadKey_4 = 'F';
const char KeypadKey_5 = 'G';
const char KeypadKey_6 = 'H';
const char KeypadKey_B = 'I';
const char KeypadKey_7 = 'K';
const char KeypadKey_8 = 'L';
const char KeypadKey_9 = 'M';
const char KeypadKey_C = 'N';
const char KeypadKey_Asterisk = 'P';
const char KeypadKey_0 = 'Q';
const char KeypadKey_Pound = 'R';
const char KeypadKey_Hash = 'R';
const char KeypadKey_Octothorpe = 'R';
const char KeypadKey_D = 'S';

class KeypadKeyHandler {
public:
  virtual bool handle_key(char ch) = 0;
};

class KeypadKeyHandlerComposite : public Composite<KeypadKeyHandler> {
public:
  KeypadKeyHandlerComposite(KeypadKeyHandler *keyHandlerZ[]) : Composite<KeypadKeyHandler>(keyHandlerZ, countZ(keyHandlerZ)) { }
  virtual bool handle_key(char ch) {
    const int count = length();
    for (int i = 0; i < count; i++) {
      if (item(i)->handle_key(ch)) {
        return;
      }
    }
  }
};

class ToggleKeypadKeyHandler : public KeypadKeyHandler {
  char _toggleKey;
  Enabled &_enabled;
public:
  ToggleKeypadKeyHandler(char toggleKey, Enabled &enabled) : _toggleKey(toggleKey), _enabled(enabled) { }
  virtual bool handle_key(char ch) {
    if (ch == _toggleKey) {
      _enabled.toggle();
      return true;
    }
    return false;
  }
};

class EnableKeypadKeyHandler : public KeypadKeyHandler {
  char _enableKey;
  Enabled &_enabled;
  bool _enable;
public:
  EnableKeypadKeyHandler(char enableKey, Enabled &enabled, bool enable = true) : _enableKey(enableKey), _enabled(enabled), _enable(enable) { }
  virtual bool handle_key(char ch) {
    if (ch == _enableKey) {
      _enabled.enable(_enable);
      return true;
    }
    return false;
  }
};

class KeypadHandler : public Scheduled {
  IKeypad &_keypad;
  KeypadKeyHandler &_keyHandler;
public:
  KeypadHandler(Schedule &schedule, IKeypad &keypad, KeypadKeyHandler &keyHandler) : 
    Scheduled(schedule), _keypad(keypad), _keyHandler(keyHandler) { }
  void poll() {
    char ch = _keypad.read();
    _keyHandler.handle_key(ch);
  }
};

#endif // KEYPAD_HANDLER_HPP
