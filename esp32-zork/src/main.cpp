#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include "secrets.h"
#include "../lib/frotz/zork_frotz.h"

/*
 * Story file to load from LittleFS.
 * LittleFS is mounted at /littlefs/ by LittleFS.begin(), so frotz's fopen()
 * finds the file there via the VFS layer. Change to switch games.
 */
static const char* STORY_FILE = "/littlefs/zork1.z3";

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");

/* Forward declaration — used in send callback below. */
static void send_to_client(uint32_t id, const char* text, size_t len);

// ── WebSocket handler ─────────────────────────────────────────────────────────

static void onWsEvent(AsyncWebSocket*, AsyncWebSocketClient* client,
                      AwsEventType type, void* arg, uint8_t* data, size_t len) {

    if (type == WS_EVT_CONNECT) {
        Serial.printf("[WS] #%u connected\n", client->id());

        if (zork_client_id != 0) {
            /* Already playing — reject extra connections. */
            client->text("Game in progress. Try again later.");
            client->close();
            return;
        }

        zork_client_id = client->id();
        xTaskCreatePinnedToCore(
            zork_interpreter_task, "frotz",
            24 * 1024,          /* stack bytes — frotz needs headroom for recursion */
            nullptr, 1,
            &zork_task_handle, 1    /* pin to core 1 */
        );

    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("[WS] #%u disconnected\n", client->id());

        if (client->id() == zork_client_id) {
            /*
             * Force-kill the interpreter task. This is safe here because the
             * task is either blocking on xQueueReceive (idle, waiting for input)
             * or running frotz opcode logic with no locks held.
             */
            if (zork_task_handle) {
                vTaskDelete(zork_task_handle);
                zork_task_handle = nullptr;
            }
            /* Drain stale input so the next player starts clean. */
            char discard[ZORK_INPUT_MAX];
            while (xQueueReceive(zork_input_q, discard, 0) == pdTRUE) {}
            zork_client_id = 0;
        }

    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo* info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len
                && info->opcode == WS_TEXT
                && client->id() == zork_client_id) {
            char line[ZORK_INPUT_MAX];
            size_t n = (len < ZORK_INPUT_MAX - 1) ? len : ZORK_INPUT_MAX - 1;
            memcpy(line, data, n);
            line[n] = '\0';
            xQueueSend(zork_input_q, line, 0);
        }
    }
}

// ── Send callback (registered with zork_os.cpp) ──────────────────────────────

static void send_to_client(uint32_t id, const char* text, size_t /*len*/) {
    ws.text(id, text);
}

// ── setup / loop ─────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\nESP32 Zork starting...");

    Serial.printf("Connecting to %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) {
        delay(250);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi failed. Check secrets.h. Halting.");
        while (true) delay(1000);
    }
    Serial.printf("Connected. IP: %s\n", WiFi.localIP().toString().c_str());

    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed — re-run: flash.bat fs");
    }

    if (MDNS.begin("zork")) {
        Serial.println("mDNS started: http://zork.local/");
    }

    /* Wire up the frotz ↔ WebSocket bridge. */
    zork_story_path = STORY_FILE;
    zork_input_q    = xQueueCreate(4, ZORK_INPUT_MAX);
    zork_send_fn    = send_to_client;

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
    server.begin();

    Serial.printf("Open http://%s/ or http://zork.local/\n",
                  WiFi.localIP().toString().c_str());
}

void loop() {
    ws.cleanupClients();

    static uint32_t lastIp = 0;
    if (millis() - lastIp >= 5000) {
        Serial.printf("http://%s/ | http://zork.local/\n",
                      WiFi.localIP().toString().c_str());
        lastIp = millis();
    }
}
