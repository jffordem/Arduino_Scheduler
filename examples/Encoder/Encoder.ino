/* Encoder wheel and switch demo.
 *   
 * Wire encoder wheel's button, clock and data lines to three digital pins on the Arduino. 
 * Configure them as btnPin, clkPin and dtPin.
 * 
 * The button pauses flashing and the wheel changes the rate.  Have fun!
 * 
 * Going to try to add a display - SSD1306 - to this for fun.
 * https://www.arduino.cc/reference/en/libraries/adafruit-ssd1306/
 * Be sure to install the Adafruit libraries for SSD1306 in the Tools | Manage Libraries menu.
 */

#include <Scheduler.hpp>
#include <Clock.hpp>
#include <Led.hpp>
#include <EncoderWheel.hpp>
#include <Mapper.hpp>
#include <ButtonHandler.hpp>

#include <LeonardoConfig.hpp>
//#include <BreadboardConfig.hpp>

long onTime = 125;
long offTime = 125;

// Examples of OO-based composition
class Blinky : public Clock, private DigitalLED {
  bool _ledState;
public:
  Blinky(Schedule &schedule, long &offTime, long &onTime, const LedConfig &ledConfig) :
    Clock(schedule, offTime, onTime, _ledState),
    DigitalLED(schedule, _ledState, ledConfig),
    _ledState(LOW) { }
};

MainSchedule schedule;

Blinky blinky(schedule, offTime, onTime, Config.DefaultLed);
EncoderControl<long> offControl(schedule, Config.Left.Encoder, offTime, -15, offTime*2);
EncoderControl<long> onControl(schedule, Config.Right.Encoder, onTime, -15, onTime*2);
ToggleButton encoderButton(schedule, Config.Left.Button, blinky);

void setup() {
  schedule.begin();
  blinky.enable(true);
}

void loop() {
  schedule.poll();
}