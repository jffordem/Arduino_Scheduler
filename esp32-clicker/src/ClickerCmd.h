#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Commands pushed to cmdQueue by WebSocket callbacks (Core 0)
// and drained by loop() (Core 1) before calling poll().
struct ClickerCmd {
    enum Type : uint8_t { SET_MODE, START, STOP, SET_PARAM } type;
    char paramKey[20];   // SET_PARAM: parameter name
    char paramVal[64];   // SET_PARAM: value serialized as string
                         // SET_MODE:  mode name (in paramVal, paramKey unused)
};

// Defined in main.cpp; declared here so WebUI.cpp and main.cpp share one queue.
extern QueueHandle_t cmdQueue;
