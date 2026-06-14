#include "WebUI.h"
#include "ClickerCmd.h"
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");
static String         lastStatus;

static void onWsEvent(AsyncWebSocket* srv, AsyncWebSocketClient* client,
                      AwsEventType type, void* arg, uint8_t* data, size_t len)
{
    if (type == WS_EVT_CONNECT) {
        // Send cached status so the UI is consistent on reconnect.
        if (lastStatus.length()) client->text(lastStatus);
        return;
    }

    if (type != WS_EVT_DATA) return;

    // Only handle single-frame, complete text messages.
    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    if (!info->final || info->index != 0 || info->len != len
        || info->opcode != WS_TEXT) return;

    JsonDocument doc;
    if (deserializeJson(doc, (const char*)data, len) != DeserializationError::Ok) return;

    const char* cmd = doc["cmd"] | "";
    ClickerCmd c{};

    if (strcmp(cmd, "getStatus") == 0) {
        if (lastStatus.length()) client->text(lastStatus);
        return;

    } else if (strcmp(cmd, "start") == 0) {
        c.type = ClickerCmd::START;

    } else if (strcmp(cmd, "stop") == 0) {
        c.type = ClickerCmd::STOP;

    } else if (strcmp(cmd, "setMode") == 0) {
        c.type = ClickerCmd::SET_MODE;
        strlcpy(c.paramVal, doc["mode"] | "", sizeof(c.paramVal));

    } else if (strcmp(cmd, "setParam") == 0) {
        c.type = ClickerCmd::SET_PARAM;
        strlcpy(c.paramKey, doc["key"] | "", sizeof(c.paramKey));
        // Serialize the value to a string regardless of its JSON type.
        JsonVariant val = doc["value"];
        if (val.is<bool>())
            strlcpy(c.paramVal, val.as<bool>() ? "true" : "false", sizeof(c.paramVal));
        else if (val.is<const char*>())
            strlcpy(c.paramVal, val.as<const char*>(), sizeof(c.paramVal));
        else
            snprintf(c.paramVal, sizeof(c.paramVal), "%ld", val.as<long>());

    } else {
        return; // Unknown command — ignore.
    }

    xQueueSend(cmdQueue, &c, 0);
}

void webui_begin(ClickerMode** /*modes*/, int /*modeCount*/) {
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(LittleFS, "/index.html", "text/html");
    });

    // Phase 5: will return built-in macro names. Empty list for now.
    server.on("/macros", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "application/json", "[]");
    });

    server.onNotFound([](AsyncWebServerRequest* req) {
        req->send(404, "text/plain", "Not found");
    });

    server.begin();
}

void webui_push(const String& json) {
    lastStatus = json;
    ws.textAll(json);
}

void webui_cleanup() {
    ws.cleanupClients();
}
