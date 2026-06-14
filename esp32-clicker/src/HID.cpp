#include "HID.h"

#ifdef USB_HID_MODE
static USBHIDKeyboard _kb;
static USBHIDMouse    _ms;
#endif

namespace HID {

void begin() {
#ifdef USB_HID_MODE
    _kb.begin();
    _ms.begin();
    USB.begin();
#endif
}

void pressKey(uint8_t code) {
    Serial.printf("[HID] KEY PRESS   0x%02X\n", code);
#ifdef USB_HID_MODE
    _kb.press(code);
#endif
}

void releaseKey(uint8_t code) {
    Serial.printf("[HID] KEY RELEASE 0x%02X\n", code);
#ifdef USB_HID_MODE
    _kb.release(code);
#endif
}

void tapKey(uint8_t code) {
    pressKey(code);
    delay(20);
    releaseKey(code);
}

static const char* mouseButtonName(uint8_t button) {
    return button == MOUSE_LEFT ? "LEFT" : button == MOUSE_RIGHT ? "RIGHT" : "MIDDLE";
}

void pressMouse(uint8_t button) {
    Serial.printf("[HID] MOUSE PRESS   %s\n", mouseButtonName(button));
#ifdef USB_HID_MODE
    _ms.press(button);
#endif
}

void releaseMouse(uint8_t button) {
    Serial.printf("[HID] MOUSE RELEASE %s\n", mouseButtonName(button));
#ifdef USB_HID_MODE
    _ms.release(button);
#endif
}

void releaseAll() {
    Serial.println("[HID] RELEASE ALL");
#ifdef USB_HID_MODE
    _kb.releaseAll();
    _ms.release(MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE);
#endif
}

} // namespace HID
