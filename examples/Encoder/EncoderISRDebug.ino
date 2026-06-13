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

/* Encoder ISR debug demo.
 *
 * This example uses InterruptEncoderWheel to sample two encoder wheels via
 * attachInterrupt() on the encoder clock pin, and ButtonHandler to watch the
 * encoder push buttons.
 *
 * It prints the current encoder counts and button press state over Serial.
 *
 * The board config header is selected automatically for Leonardo/Pro Micro
 * versus R4 Minima. Update the pin mappings if you have a different wiring.
 */

#include <Scheduler.hpp>
#include <EncoderWheel.hpp>
#include <ButtonHandler.hpp>

#if defined(__AVR_ATmega32U4__)
  #include <LeonardoConfig.hpp>
  #define ENCODER_LEFT  Config.B
  #define ENCODER_RIGHT Config.A
  #define LEFT_BUTTON   Config.B.Button
  #define RIGHT_BUTTON  Config.A.Button
#else
  #include <MinimaConfig.hpp>
  #define ENCODER_LEFT  Config.Left
  #define ENCODER_RIGHT Config.Right
  #define LEFT_BUTTON   Config.Left.Button
  #define RIGHT_BUTTON  Config.Right.Button
#endif

MainSchedule schedule;

int leftEncoderValue = 0;
int rightEncoderValue = 0;
int leftButtonPresses = 0;
int rightButtonPresses = 0;
bool leftButtonDown = false;
bool rightButtonDown = false;
unsigned long lastReport = 0;

void onLeftButtonPress() {
    leftButtonDown = true;
    leftButtonPresses++;
    Serial.println("LEFT BUTTON PRESSED");
}

void onLeftButtonRelease() {
    leftButtonDown = false;
    Serial.println("LEFT BUTTON RELEASED");
}

void onRightButtonPress() {
    rightButtonDown = true;
    rightButtonPresses++;
    Serial.println("RIGHT BUTTON PRESSED");
}

void onRightButtonRelease() {
    rightButtonDown = false;
    Serial.println("RIGHT BUTTON RELEASED");
}

InterruptEncoderWheel leftEncoder(
    schedule,
    ENCODER_LEFT.Encoder.clockPin,
    ENCODER_LEFT.Encoder.dataPin,
    leftEncoderValue,
    1000,
    0);

InterruptEncoderWheel rightEncoder(
    schedule,
    ENCODER_RIGHT.Encoder.clockPin,
    ENCODER_RIGHT.Encoder.dataPin,
    rightEncoderValue,
    1000,
    1);

ButtonHandler leftEncoderButton(
    schedule,
    LEFT_BUTTON,
    onLeftButtonPress,
    onLeftButtonRelease);

ButtonHandler rightEncoderButton(
    schedule,
    RIGHT_BUTTON,
    onRightButtonPress,
    onRightButtonRelease);

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        ;
    }
    Serial.println("Encoder ISR Debug Example");
#if defined(__AVR_ATmega32U4__)
    Serial.println("Using Leonardo/Pro Micro config");
    Serial.print("Left encoder: clock=");
    Serial.print(ENCODER_LEFT.Encoder.clockPin);
    Serial.print(" data=");
    Serial.print(ENCODER_LEFT.Encoder.dataPin);
    Serial.print("  Right encoder: clock=");
    Serial.print(ENCODER_RIGHT.Encoder.clockPin);
    Serial.print(" data=");
    Serial.println(ENCODER_RIGHT.Encoder.dataPin);
#else
    Serial.println("Using R4 Minima config");
    Serial.print("Left encoder: clock=");
    Serial.print(ENCODER_LEFT.Encoder.clockPin);
    Serial.print(" data=");
    Serial.print(ENCODER_LEFT.Encoder.dataPin);
    Serial.print("  Right encoder: clock=");
    Serial.print(ENCODER_RIGHT.Encoder.clockPin);
    Serial.print(" data=");
    Serial.println(ENCODER_RIGHT.Encoder.dataPin);
#endif
    Serial.println("Press the left/right switches to see button state and press counts.");

    schedule.begin();
}

void loop() {
    schedule.poll();

    unsigned long now = millis();
    if (now - lastReport >= 500) {  // More frequent updates
        lastReport = now;
        Serial.print("T=");
        Serial.print(now);
        Serial.print(" L=");
        Serial.print(leftEncoderValue);
        Serial.print(" LBtn=");
        Serial.print(leftButtonDown ? "DN" : "UP");
        Serial.print("(");
        Serial.print(leftButtonPresses);
        Serial.print(") R=");
        Serial.print(rightEncoderValue);
        Serial.print(" RBtn=");
        Serial.print(rightButtonDown ? "DN" : "UP");
        Serial.print("(");
        Serial.print(rightButtonPresses);
        Serial.println(")");
    }
}
