#pragma once
#include <Arduino.h>

void webui_begin();
void webui_push(const String& json);
void webui_cleanup();
