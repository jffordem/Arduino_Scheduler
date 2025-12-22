/*
  For skyrim upleveling.  Put desired spell (e.g. Muffle) in both hands and wear as much Magicka Regen as possible.

  Refactored to use an encoder wheel w/ built-in switch.  This works great!

  Wiring:
  The encoder should have power & ground connected, then the switch, clock and data lines.
  Connect those to any three digital pins and set them as btnPin, clkPin and dtPin.

  Using:
  Casting will be off by default.
  Press the encoder button to turn casting on/off.
  Turn the encoder wheel to make casting faster or slower.
*/

#include <Mouse.h>
#include <Keyboard.h>
#include <Scheduler.hpp>
#include <EncoderWheel.hpp>
#include <ButtonHandler.hpp>
#include <HIDIO.hpp>

#include <LeonardoConfig.hpp>
//#include <BreadboardConfig.hpp>

bool active = false;
long downTime = 750;
long upTime = 1800;
long keyPressDelay = 100;
long saveTime = 60000L;

MainSchedule schedule;

//#define DEBUG
#ifndef DEBUG
MouseButton leftMouseButton(MOUSE_LEFT);
MouseButton rightMouseButton(MOUSE_RIGHT);
KeyPress saveKey(KEY_F5);
#else
DummyButton leftMouseButton("LEFT", true);
DummyButton rightMouseButton("RIGHT", true);
DummyButton saveKey("SAVE", true);
#endif

PressFollower rightDelayButton(schedule, 150, rightMouseButton);
PressComposite mouseButtons(&leftMouseButton, &rightDelayButton);

ButtonController buttonController(schedule, upTime, downTime, mouseButtons);
ButtonController saveController(schedule, saveTime, keyPressDelay, saveKey);
EnableComposite controller(&buttonController, &saveController);

EncoderControl<long> castRate(schedule, Config.Left.Encoder, upTime, -20, upTime*2);
EncoderControl<long> castTime(schedule, Config.Right.Encoder, downTime, -20, downTime*2);

ToggleButton leftButton(schedule, Config.Left.Button, controller);

void setup() {
#ifdef DEBUG
  Serial.begin(9600);
#endif
  Mouse.begin();
  Keyboard.begin();
  controller.enable(false);
}

void loop() {
  schedule.poll();
}