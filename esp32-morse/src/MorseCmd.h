#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

struct MorseCmd {
    enum Type { SET_PARAM, START, STOP } type;
    char key[24];
    char val[64];
};

extern QueueHandle_t cmdQueue;
