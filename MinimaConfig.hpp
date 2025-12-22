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

#ifndef BREADBOARDCONFIG_HPP
#define BREADBOARDCONFIG_HPP

#include <Led.hpp>
#include <EncoderWheel.hpp>
#include <ButtonHandler.hpp>

/*
Note: the PWM pins on the Pro Micro are 3, 5, 6, 9, 10.
*/

struct {
  const LedConfig DefaultLed = { .pin = LED_BUILTIN, .lowIsOn = false };
  struct {
    const ButtonConfig Button = { .pin = 4, .lowIsPressed = true };
    const EncoderConfig Encoder = { .clockPin = 6, .dataPin = 5 };
  } Left;
  struct {
    const ButtonConfig Button = { .pin = 4, .lowIsPressed = true };
    const EncoderConfig Encoder = { .clockPin = 6, .dataPin = 5 };
  } A; // Copy of Left
  struct {
    const ButtonConfig Button = { .pin = 7, .lowIsPressed = true };
    const EncoderConfig Encoder = { .clockPin = 9, .dataPin = 8 };
  } Right;
  struct {
    const ButtonConfig Button = { .pin = 7, .lowIsPressed = true };
    const EncoderConfig Encoder = { .clockPin = 9, .dataPin = 8 };
  } B; // Copy of Right
} Config;

#endif
