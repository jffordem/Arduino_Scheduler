#include "WebUI.h"
#include "MorseCmd.h"
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");
static String lastStatus;

static void onWsEvent(AsyncWebSocket* /*srv*/, AsyncWebSocketClient* client,
                      AwsEventType type, void* arg, uint8_t* data, size_t len)
{
    if (type == WS_EVT_CONNECT) {
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
    MorseCmd c{};

    if (strcmp(cmd, "getStatus") == 0) {
        if (lastStatus.length()) client->text(lastStatus);
        return;
    } else if (strcmp(cmd, "start") == 0) {
        c.type = MorseCmd::START;
    } else if (strcmp(cmd, "stop") == 0) {
        c.type = MorseCmd::STOP;
    } else if (strcmp(cmd, "setParam") == 0) {
        c.type = MorseCmd::SET_PARAM;
        strlcpy(c.key, doc["key"] | "", sizeof(c.key));
        JsonVariant val = doc["value"];
        if (val.is<bool>())
            strlcpy(c.val, val.as<bool>() ? "true" : "false", sizeof(c.val));
        else if (val.is<const char*>())
            strlcpy(c.val, val.as<const char*>(), sizeof(c.val));
        else
            snprintf(c.val, sizeof(c.val), "%ld", val.as<long>());
    } else {
        return;
    }

    xQueueSend(cmdQueue, &c, 0);
}

void webui_begin() {
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(LittleFS, "/index.html", "text/html");
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
