#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include "secrets.h"
#include "MorseCmd.h"
#include "MorseOutput.h"
#include "MorseEngine.h"
#include "KochTrainer.h"
#include "WebUI.h"

QueueHandle_t cmdQueue;

static LedOutput   ledOutput;
static MorseEngine engine(ledOutput);
static KochTrainer trainer(engine);
static uint32_t    lastWsPush  = 0;
static uint32_t    lastIpPrint = 0;

static void applyCommand(const MorseCmd& cmd) {
    switch (cmd.type) {
        case MorseCmd::START:
            Serial.println("[CMD] START");
            trainer.start();
            break;
        case MorseCmd::STOP:
            Serial.println("[CMD] STOP");
            trainer.stop();
            break;
        case MorseCmd::SET_PARAM:
            Serial.printf("[CMD] SET_PARAM \"%s\"=\"%s\"\n", cmd.key, cmd.val);
            if      (strcmp(cmd.key, "charWpm")      == 0) trainer.setCharWpm(atoi(cmd.val));
            else if (strcmp(cmd.key, "effectiveWpm") == 0) trainer.setEffectiveWpm(atoi(cmd.val));
            else if (strcmp(cmd.key, "kochLevel")    == 0) trainer.setLevel(atoi(cmd.val));
            else if (strcmp(cmd.key, "answer")       == 0) trainer.submitAnswer(cmd.val);
            else if (strcmp(cmd.key, "timingLog")    == 0) engine.enableTimingLog(strcmp(cmd.val, "true") == 0);
            break;
    }
    webui_push(trainer.statusJson());
}

static void blinkTest() {
    for (int i = 0; i < 3; i++) {
        ledOutput.mark();
        delay(150);
        ledOutput.space();
        delay(150);
    }
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\nESP32 Morse Trainer starting...");

    cmdQueue = xQueueCreate(16, sizeof(MorseCmd));

    ledOutput.begin();
    blinkTest();
    Serial.println("LED self-test complete.");

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
    Serial.printf("Connected. Open http://%s/ in your browser.\n",
                  WiFi.localIP().toString().c_str());

    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed — run: flash.bat fs");
    }

    webui_begin();
    webui_push(trainer.statusJson());
    Serial.println("Web server started.");
}

void loop() {
    MorseCmd cmd;
    while (xQueueReceive(cmdQueue, &cmd, 0) == pdTRUE)
        applyCommand(cmd);

    uint32_t now = millis();

    engine.poll(now);
    trainer.poll(now);

    if (now - lastIpPrint >= 5000) {
        Serial.printf("http://%s/\n", WiFi.localIP().toString().c_str());
        lastIpPrint = now;
    }

    if (now - lastWsPush >= 200) {
        webui_push(trainer.statusJson());
        lastWsPush = now;
    }

    webui_cleanup();
}
