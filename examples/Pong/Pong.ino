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

/*
 * Classic Pong game.
 * 
 * Hardware configuration:
 * Two encoder wheels (part#)
 * Bitmap display (part#)
 */

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <Graphics.hpp>

#include <BreadboardConfig.hpp>

//#define DEBUG

#include "Paddle.hpp"
#include "Ball.hpp"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

MainSchedule schedule;
MainWindow window(schedule, display);

#define PADDLE_SIZE 10
Paddle player1(schedule, window, Config.Left.Encoder, 0, PADDLE_SIZE, SCREEN_HEIGHT);
Paddle player2(schedule, window, Config.Right.Encoder, SCREEN_WIDTH - 1, PADDLE_SIZE, SCREEN_HEIGHT);
Ball ball(schedule, window, player1, player2, SCREEN_WIDTH, SCREEN_HEIGHT);
ButtonHandler newGameButton(schedule, Config.Left.Button, &onNewGamePressed);

void onNewGamePressed() {
  ball.newgame();
}

void setup() {
  randomSeed(analogRead(A0));
#ifdef DEBUG
  Serial.begin(9600);
#endif
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  display.display();
  delay(2000);
  schedule.begin();
  window.enable(true);
}

void loop() {
  schedule.poll();
}