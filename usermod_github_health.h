#pragma once
#include "wled.h"

#ifndef WLED_DISABLE_MQTT
  // MQTT not required but we don't want to break builds
#endif

#ifdef ESP32
  #include <HTTPClient.h>
  #include <WiFiClientSecure.h>
#elif defined(ESP8266)
  #include <ESP8266HTTPClient.h>
  #include <WiFiClientSecureBearSSL.h>
#endif

class GithubHealthUsermod : public Usermod {
  private:
    static const char _name[];

    bool     enabled          = true;
    bool     wifiConnected    = false;
    bool     githubUnhealthy  = false;
    bool     blinkOn          = false;

    uint32_t checkIntervalMs  = 60000;   // check every 60s
    uint32_t blinkIntervalMs  = 500;     // blink toggle every 500ms
    uint32_t lastCheckTime    = 0;
    uint32_t lastBlinkToggle  = 0;

    // saved state to restore when GitHub recovers
    byte     savedBrightness  = 0;
    uint8_t  savedMode        = 0;
    uint32_t savedColor       = 0;
    bool     stateSaved       = false;

    void checkGithubHealth() {
      if (WiFi.status() != WL_CONNECTED) return;

      #ifdef ESP32
        WiFiClientSecure client;
        client.setInsecure(); // skip cert verification (status page, not sensitive)
      #elif defined(ESP8266)
        BearSSL::WiFiClientSecure client;
        client.setInsecure();
      #endif

      HTTPClient http;
      http.begin(client, F("https://www.githubstatus.com/api/v2/status.json"));
      http.setTimeout(5000);

      int httpCode = http.GET();

      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        // look for "indicator":"none" which means all systems operational
        bool healthy = payload.indexOf("\"indicator\":\"none\"") >= 0;
        setHealthState(!healthy);
      } else {
        // can't reach status page — treat as unhealthy
        setHealthState(true);
      }

      http.end();
    }

    void setHealthState(bool unhealthy) {
      if (unhealthy == githubUnhealthy) return; // no change

      githubUnhealthy = unhealthy;

      if (githubUnhealthy) {
        // save current strip state so we can restore later
        Segment& seg = strip.getMainSegment();
        savedBrightness = bri;
        savedMode       = seg.mode;
        savedColor      = seg.colors[0];
        stateSaved      = true;
      } else if (stateSaved) {
        // GitHub recovered — restore previous state
        Segment& seg = strip.getMainSegment();
        bri = savedBrightness;
        seg.setMode(savedMode);
        seg.setColor(0, savedColor);
        stateUpdated(CALL_MODE_DIRECT_CHANGE);
        stateSaved = false;
      }
    }

    void handleBlink() {
      uint32_t now = millis();
      if (now - lastBlinkToggle < blinkIntervalMs) return;
      lastBlinkToggle = now;

      blinkOn = !blinkOn;
      Segment& seg = strip.getMainSegment();

      if (blinkOn) {
        bri = 255;
        seg.setMode(FX_MODE_STATIC);
        seg.setColor(0, RED);
      } else {
        bri = 0;
      }
      stateUpdated(CALL_MODE_DIRECT_CHANGE);
    }

  public:
    void setup() override {
      // nothing special needed at boot
    }

    void loop() override {
      if (!enabled) return;

      uint32_t now = millis();

      // periodic health check
      if (now - lastCheckTime >= checkIntervalMs) {
        lastCheckTime = now;
        checkGithubHealth();
      }

      // blink red while unhealthy
      if (githubUnhealthy) {
        handleBlink();
      }
    }

    void connected() override {
      wifiConnected = true;
    }

    void addToJsonInfo(JsonObject& root) override {
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");
      JsonArray gh = user.createNestedArray(FPSTR(_name));
      gh.add(githubUnhealthy ? F("unhealthy") : F("healthy"));
      gh.add(F(" "));
    }

    void addToConfig(JsonObject& root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top["enabled"]       = enabled;
      top["checkInterval"] = checkIntervalMs / 1000; // store as seconds
      top["blinkInterval"] = blinkIntervalMs;
    }

    bool readFromConfig(JsonObject& root) override {
      JsonObject top = root[FPSTR(_name)];
      if (top.isNull()) return false;

      enabled         = top["enabled"]       | enabled;
      checkIntervalMs = (top["checkInterval"] | (checkIntervalMs / 1000)) * 1000;
      blinkIntervalMs = top["blinkInterval"]  | blinkIntervalMs;

      return !top.isNull();
    }

    uint16_t getId() override {
      return USERMOD_ID_UNSPECIFIED;
    }

    const char* getUiName() override {
      return _name;
    }
};

const char GithubHealthUsermod::_name[] PROGMEM = "GitHub Health";
