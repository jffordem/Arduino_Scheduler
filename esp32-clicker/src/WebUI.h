#pragma once
#include <Arduino.h>
#include "ClickerMode.h"

// Initialize the web server, WebSocket, and LittleFS routes.
// modes / modeCount used to serve GET /macros (Phase 5); pass nullptr / 0 for now.
void webui_begin(ClickerMode** modes, int modeCount);

// Broadcast a JSON status string to all connected WebSocket clients.
// Also caches it to send to clients that connect later.
// Call from loop() (Core 1) only.
void webui_push(const String& json);

// Must be called every loop() to free closed WebSocket connections.
void webui_cleanup();
