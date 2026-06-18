#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include "secrets.h"
#include "../lib/frotz/zork_frotz.h"

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");

/* Path buffer for the currently selected story file. */
static char current_story_buf[64];

static void send_to_client(uint32_t id, const char* text, size_t len);

/* Returns true if `name` is a bare filename with a Z-machine extension (.z1-.z9). */
static bool isStoryFile(const char* name) {
    if (!name) return false;
    size_t slen = strlen(name);
    if (slen == 0 || slen > 48) return false;
    if (strchr(name, '/') || strchr(name, '\\') || strstr(name, "..")) return false;
    const char* dot = strrchr(name, '.');
    if (!dot) return false;
    char e1 = dot[1], e2 = dot[2];
    if ((e1 != 'z' && e1 != 'Z') || e2 < '1' || e2 > '9' || dot[3] != '\0') return false;
    return true;
}

// ── /api/games endpoint ───────────────────────────────────────────────────────

static void handleGetGames(AsyncWebServerRequest* req) {
    String json = "[";
    bool first = true;
    File root = LittleFS.open("/");
    if (root) {
        File f = root.openNextFile();
        while (f) {
            const char* raw = f.name();
            /* file.name() may include a leading '/' on some ESP32 core versions. */
            const char* name = (raw && raw[0] == '/') ? raw + 1 : raw;
            if (isStoryFile(name)) {
                if (!first) json += ",";
                json += "\"";
                json += name;
                json += "\"";
                first = false;
            }
            f.close();
            f = root.openNextFile();
        }
        root.close();
    }
    json += "]";
    req->send(200, "application/json", json);
}

// ── WebSocket handler ─────────────────────────────────────────────────────────

static void onWsEvent(AsyncWebSocket*, AsyncWebSocketClient* client,
                      AwsEventType type, void* arg, uint8_t* data, size_t len) {

    if (type == WS_EVT_CONNECT) {
        Serial.printf("[WS] #%u connected\n", client->id());

        if (zork_client_id != 0) {
            client->text("Game in progress. Try again later.");
            client->close();
            return;
        }

        zork_client_id = client->id();
        /* Don't start frotz yet — the first data message selects the game. */

    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("[WS] #%u disconnected\n", client->id());

        if (client->id() == zork_client_id) {
            if (zork_task_handle) {
                vTaskDelete(zork_task_handle);
                zork_task_handle = nullptr;
            }
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

            if (zork_task_handle == nullptr) {
                /* No game running — treat this message as the game selection. */
                if (!isStoryFile(line)) {
                    client->text("[Invalid game selection.]\n");
                    return;
                }
                snprintf(current_story_buf, sizeof(current_story_buf),
                         "/littlefs/%s", line);
                zork_story_path = current_story_buf;

                xTaskCreatePinnedToCore(
                    zork_interpreter_task, "frotz",
                    24 * 1024, nullptr, 1, &zork_task_handle, 1
                );
            } else {
                xQueueSend(zork_input_q, line, 0);
            }
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

    zork_story_path = nullptr;
    zork_input_q    = xQueueCreate(4, ZORK_INPUT_MAX);
    zork_send_fn    = send_to_client;

    server.on("/api/games", HTTP_GET, handleGetGames);
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
