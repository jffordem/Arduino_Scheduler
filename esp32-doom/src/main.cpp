#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include "secrets.h"

static AsyncWebServer server(80);

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\nESP32 Doom starting...");

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

    if (MDNS.begin("doom")) {
        Serial.println("mDNS started: http://doom.local/");
    }

    // .wasm files must be served as application/wasm or browsers refuse to
    // compile them via WebAssembly.instantiateStreaming()
    server.on("/assets/doom.wasm", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(LittleFS, "/assets/doom.wasm", "application/wasm");
    });
    server.serveStatic("/", LittleFS, "/").setDefaultFile("doom.html");
    server.begin();

    Serial.printf("Open http://%s/ or http://doom.local/\n",
                  WiFi.localIP().toString().c_str());
}

void loop() {
    static uint32_t lastIp = 0;
    if (millis() - lastIp >= 5000) {
        Serial.printf("http://%s/ | http://doom.local/\n",
                      WiFi.localIP().toString().c_str());
        lastIp = millis();
    }
}
