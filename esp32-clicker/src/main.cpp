#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include "secrets.h"
#include "ClickerCmd.h"
#include "HID.h"
#include "ClickerMode.h"
#include "WebUI.h"
#include "AutoCast.h"
#include "Hold.h"

static AutoCast      autoCastMode;
static Hold          holdMode;
static ClickerMode*  modes[]   = { &autoCastMode, &holdMode };
static int           modeCount = 2;

QueueHandle_t  cmdQueue;
static ClickerMode* activeMode  = nullptr;
static uint32_t     lastWsPush  = 0;
static uint32_t     lastIpPrint = 0;

// ── Mode registry ────────────────────────────────────────────────────────────

static ClickerMode* findMode(const char* modeName) {
    for (int i = 0; i < modeCount; i++)
        if (strcmp(modes[i]->name(), modeName) == 0)
            return modes[i];
    return nullptr;
}

// ── Command handler (Core 1 only) ────────────────────────────────────────────

static void applyCommand(const ClickerCmd& cmd) {
    switch (cmd.type) {

        case ClickerCmd::SET_MODE: {
            ClickerMode* next = findMode(cmd.paramVal);
            Serial.printf("[CMD] SET_MODE \"%s\" -> %s\n", cmd.paramVal, next ? "found" : "NOT FOUND");
            if (!next || next == activeMode) break;
            if (activeMode && activeMode->running()) activeMode->stop();
            HID::releaseAll();
            activeMode = next;
            // Push updated status immediately so UI reflects the switch.
            webui_push(activeMode->statusJson());
            break;
        }

        case ClickerCmd::START:
            Serial.printf("[CMD] START (activeMode=%s, running=%s)\n",
                activeMode ? activeMode->name() : "null",
                (activeMode && activeMode->running()) ? "yes" : "no");
            if (activeMode && !activeMode->running()) activeMode->start();
            break;

        case ClickerCmd::STOP:
            Serial.printf("[CMD] STOP (activeMode=%s, running=%s)\n",
                activeMode ? activeMode->name() : "null",
                (activeMode && activeMode->running()) ? "yes" : "no");
            if (activeMode && activeMode->running()) {
                activeMode->stop();
                HID::releaseAll();
            }
            break;

        case ClickerCmd::SET_PARAM:
            Serial.printf("[CMD] SET_PARAM \"%s\"=\"%s\"\n", cmd.paramKey, cmd.paramVal);
            if (activeMode) {
                activeMode->setParam(cmd.paramKey, cmd.paramVal);
                webui_push(activeMode->statusJson());
            }
            break;
    }
}

// ── setup / loop ─────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\nESP32 Clicker starting...");

    cmdQueue = xQueueCreate(16, sizeof(ClickerCmd));

#ifdef USB_HID_MODE
    HID::begin();
    Serial.println("USB HID initialised.");
#else
    Serial.println("HID stub mode (dev build) — USB Serial active.");
#endif

    // Phase 6 will add a captive-portal fallback here.
    Serial.printf("Connecting to %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) {
        delay(250);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        // Temporary: halt until Phase 6 adds the AP fallback.
        Serial.println("WiFi failed. Check secrets.h. Halting.");
        while (true) delay(1000);
    }
    Serial.printf("Connected. Open http://%s/ on your phone.\n",
                  WiFi.localIP().toString().c_str());

    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed — re-run uploadfs.");
    }

    webui_begin(modes, modeCount);
    activeMode = modes[0];
    webui_push(activeMode->statusJson());
    Serial.println("Web server started.");
}

void loop() {
    // Drain the command queue before polling so all state changes are applied
    // on Core 1 before poll() reads them. This is the thread-safety boundary.
    ClickerCmd cmd;
    while (xQueueReceive(cmdQueue, &cmd, 0) == pdTRUE)
        applyCommand(cmd);

    uint32_t now = millis();

    // Re-print IP every 5 s so a late-connecting serial monitor still sees it.
    if (now - lastIpPrint >= 5000) {
        Serial.printf("http://%s/\n", WiFi.localIP().toString().c_str());
        lastIpPrint = now;
    }

    if (activeMode) activeMode->poll(now);

    // Push status heartbeat every 200 ms while a client is connected.
    if (now - lastWsPush >= 200) {
        if (activeMode) webui_push(activeMode->statusJson());
        lastWsPush = now;
    }

    webui_cleanup();
}
