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
#include <Wire.h>
#include <Scheduler.hpp>
#include <EncoderWheel.hpp>
#include <ButtonHandler.hpp>
#include <HIDIO.hpp>

#include <LK204_25.hpp>
#include <Display.hpp>
#include <MenuUI.hpp>
#include <KeypadHandler.hpp>

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

ButtonController leftCastController(schedule, upTime, downTime, leftMouseButton);
ButtonController rightCastController(schedule, upTime, downTime, rightDelayButton);
ButtonController saveController(schedule, saveTime, keyPressDelay, saveKey);
EnableComposite controller(&leftCastController, &rightCastController, &saveController);

EncoderControl<long> castRate(schedule, Config.Left.Encoder, upTime, -20, upTime*2);
EncoderControl<long> castTime(schedule, Config.Right.Encoder, downTime, -20, downTime*2);

ToggleButton leftButton(schedule, Config.Left.Button, controller);

Enabled &leftCasting = leftCastController;
Enabled &rightCasting = rightCastController;

LK204_25_LCD lcd;
LK204_25_Keypad keypad;

static void menuBack(MenuContext &ctx) { ctx.pop(); }

MenuItem timingItems[] = {
	MenuItem::EditLong("Set up-time", upTime, 25),
	MenuItem::EditLong("Set down-time", downTime, 25),
	MenuItem::Action("Back", &menuBack)
};
MenuScreen timingMenu("Timing", timingItems, (int)(sizeof(timingItems) / sizeof(timingItems[0])));

MenuItem rootItems[] = {
	MenuItem::Submenu("Timing...", &timingMenu),
	MenuItem::Toggle("Toggle left cast", leftCasting),
	MenuItem::Toggle("Toggle right cast", rightCasting),
	MenuItem::Action("Back", &menuBack)
};
MenuScreen rootMenu("Skyrim Dual Cast", rootItems, (int)(sizeof(rootItems) / sizeof(rootItems[0])));

MenuContext menu(rootMenu);
MenuRenderer<LK204_25_LCD, 4, 20> menuRenderer(menu);
MainDisplay<LK204_25_LCD, 4, 20> mainDisplay(schedule, lcd, menuRenderer, 125, 5000L);

MenuKeymap keymap = {
	/*up*/ KeypadKey_2,
	/*down*/ KeypadKey_8,
	/*back*/ KeypadKey_D,
	/*select*/ KeypadKey_A,
	/*line1*/ KeypadKey_A,
	/*line2*/ KeypadKey_B,
	/*line3*/ KeypadKey_C,
	/*line4*/ KeypadKey_D
};
MenuKeypadController menuKeys(menu, keymap);
KeypadHandler keypadHandler(schedule, keypad, menuKeys);

void setup() {
#ifdef DEBUG
  Serial.begin(9600);
#endif
  Mouse.begin();
  Keyboard.begin();

	keypad.begin();
	mainDisplay.begin();

	leftCasting.enable(false);
	rightCasting.enable(false);
	controller.enable(false);
	schedule.begin();
}

void loop() {
  schedule.poll();
}