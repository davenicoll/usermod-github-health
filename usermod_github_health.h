#pragma once
#include "wled.h"

#ifndef WLED_DISABLE_MQTT
  // MQTT not required but we don't want to break builds
#endif

#ifdef ESP32
  // Implemented in github_health_fetch.cpp (TLS via ESP_SSLClient; see note there).
  int githubHealthFetch(const char* host, const char* path, char* out, size_t outSize);
  extern int githubHealthLastSSLError;
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

    // which indicator levels trigger the alert
    bool     alertOnMinor     = true;
    bool     alertOnMajor     = true;
    bool     alertOnCritical  = true;

    uint32_t checkIntervalMs  = 60000;   // check every 60s
    uint32_t lastCheckTime    = 0;
    int      lastHttpCode     = 0;       // result of the last check, shown in Info
    int      lastSSLError     = 0;

    // saved state per segment to restore when GitHub recovers
    static const uint8_t MAX_SEGMENTS = 16;
    struct SegState {
      uint8_t  mode;
      uint8_t  palette;
      uint8_t  speed;
      uint8_t  intensity;
      uint32_t colors[NUM_COLORS];
    };

    // alert look: red and pink twinkles over a faint pink glow
    static const uint8_t  ALERT_FX        = FX_MODE_TWINKLEFOX;
    static const uint8_t  ALERT_PALETTE   = 5;     // "* Colors Only": color 1/2/3 bands
    static const uint8_t  ALERT_SPEED     = 128;
    static const uint8_t  ALERT_INTENSITY = 200;   // twinkle rate
    static const uint32_t ALERT_COLORS[NUM_COLORS]; // red, pink (also bg glow), red
    byte     savedBrightness           = 0;
    SegState savedSegments[MAX_SEGMENTS];
    uint8_t  savedSegCount             = 0;
    bool     stateSaved                = false;

    static constexpr const char* STATUS_HOST = "www.githubstatus.com";
    static constexpr const char* STATUS_PATH = "/api/v2/status.json";
    static const size_t MAX_PAYLOAD = 4096; // headers + ~300 byte body

    // Fetches the status page into payload; returns the HTTP status code (or <= 0 on failure).
    int fetchStatus(String& payload) {
      #ifdef ESP32
        static char buf[MAX_PAYLOAD]; // static: keep it off the loop task stack
        int httpCode = githubHealthFetch(STATUS_HOST, STATUS_PATH, buf, sizeof(buf));
        payload = buf;
        return httpCode;
      #elif defined(ESP8266)
        BearSSL::WiFiClientSecure client;
        client.setInsecure();

        HTTPClient http;
        http.begin(client, String(F("https://")) + STATUS_HOST + STATUS_PATH);
        http.setTimeout(5000);
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) payload = http.getString();
        http.end();
        return httpCode;
      #endif
    }

    void checkGithubHealth() {
      if (WiFi.status() != WL_CONNECTED) return;

      String payload;
      int httpCode = fetchStatus(payload);
      lastHttpCode = httpCode;
      #ifdef ESP32
        lastSSLError = githubHealthLastSSLError;
      #endif

      if (httpCode == 200) {
        bool shouldAlert = false;

        if (payload.indexOf("\"indicator\":\"none\"") < 0) {
          // not healthy — check which level
          if (alertOnMinor    && payload.indexOf("\"indicator\":\"minor\"")    >= 0) shouldAlert = true;
          if (alertOnMajor    && payload.indexOf("\"indicator\":\"major\"")    >= 0) shouldAlert = true;
          if (alertOnCritical && payload.indexOf("\"indicator\":\"critical\"") >= 0) shouldAlert = true;
        }

        setHealthState(shouldAlert);
      } else {
        // can't reach status page — treat as unhealthy
        setHealthState(true);
      }

      // re-assert the alert look if something (e.g. a timed preset) replaced it
      if (githubUnhealthy && applyAlertLook()) stateUpdated(CALL_MODE_DIRECT_CHANGE);
    }

    // Sets the twinkle alert on every segment; returns true if anything changed.
    bool applyAlertLook() {
      bool changed = false;
      for (unsigned i = 0; i < strip.getSegmentsNum(); i++) {
        Segment& seg = strip.getSegment(i);
        if (seg.mode != ALERT_FX)           { seg.setMode(ALERT_FX); changed = true; }
        if (seg.palette != ALERT_PALETTE)   { seg.setPalette(ALERT_PALETTE); changed = true; }
        if (seg.speed != ALERT_SPEED)       { seg.speed = ALERT_SPEED; changed = true; }
        if (seg.intensity != ALERT_INTENSITY) { seg.intensity = ALERT_INTENSITY; changed = true; }
        for (uint8_t c = 0; c < NUM_COLORS; c++) {
          if (seg.colors[c] != ALERT_COLORS[c]) { seg.setColor(c, ALERT_COLORS[c]); changed = true; }
        }
      }
      return changed;
    }

    void setHealthState(bool unhealthy) {
      if (unhealthy == githubUnhealthy) return; // no change

      githubUnhealthy = unhealthy;

      if (githubUnhealthy) {
        // save current state for all segments
        savedBrightness = bri;
        savedSegCount = min((uint8_t)strip.getSegmentsNum(), MAX_SEGMENTS);
        for (uint8_t i = 0; i < savedSegCount; i++) {
          Segment& seg = strip.getSegment(i);
          SegState& st = savedSegments[i];
          st.mode      = seg.mode;
          st.palette   = seg.palette;
          st.speed     = seg.speed;
          st.intensity = seg.intensity;
          for (uint8_t c = 0; c < NUM_COLORS; c++) st.colors[c] = seg.colors[c];
        }
        stateSaved = true;

        bri = 255; // light up to show the alert (a timer may still switch it off later)
        applyAlertLook();
        stateUpdated(CALL_MODE_DIRECT_CHANGE);
      } else if (stateSaved) {
        // GitHub recovered — restore previous state for all segments.
        // If the lamp was switched off during the alert (e.g. by a timer), leave it off.
        if (bri) bri = savedBrightness;
        uint8_t numSegs = min((uint8_t)strip.getSegmentsNum(), savedSegCount);
        for (uint8_t i = 0; i < numSegs; i++) {
          Segment& seg = strip.getSegment(i);
          SegState& st = savedSegments[i];
          seg.setMode(st.mode);
          seg.setPalette(st.palette);
          seg.speed     = st.speed;
          seg.intensity = st.intensity;
          for (uint8_t c = 0; c < NUM_COLORS; c++) seg.setColor(c, st.colors[c]);
        }
        stateUpdated(CALL_MODE_DIRECT_CHANGE);
        stateSaved = false;
      }
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
    }

    void connected() override {
      wifiConnected = true;
    }

    void addToJsonInfo(JsonObject& root) override {
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");
      JsonArray gh = user.createNestedArray(FPSTR(_name));
      gh.add(githubUnhealthy ? F("unhealthy") : F("healthy"));
      if (lastHttpCode == 200 || lastHttpCode == 0) {
        gh.add(F(" "));
      } else {
        // status page unreachable: show why (HTTP status, or -1 TCP / -2 TLS / -3 response)
        char detail[40];
        snprintf(detail, sizeof(detail), " (check failed: %d, tls %d)", lastHttpCode, lastSSLError);
        gh.add(detail);
      }
    }

    void addToConfig(JsonObject& root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top["enabled"]        = enabled;
      top["checkInterval"]  = checkIntervalMs / 1000; // store as seconds
      top["alertOnMinor"]   = alertOnMinor;
      top["alertOnMajor"]   = alertOnMajor;
      top["alertOnCritical"]= alertOnCritical;
    }

    bool readFromConfig(JsonObject& root) override {
      JsonObject top = root[FPSTR(_name)];
      if (top.isNull()) return false;

      enabled         = top["enabled"]        | enabled;
      checkIntervalMs = (top["checkInterval"] | (checkIntervalMs / 1000)) * 1000;
      alertOnMinor    = top["alertOnMinor"]    | alertOnMinor;
      alertOnMajor    = top["alertOnMajor"]    | alertOnMajor;
      alertOnCritical = top["alertOnCritical"] | alertOnCritical;

      return !top.isNull();
    }

    uint16_t getId() override {
      return USERMOD_ID_UNSPECIFIED;
    }
};

const char GithubHealthUsermod::_name[] PROGMEM = "GitHub Health";
const uint32_t GithubHealthUsermod::ALERT_COLORS[NUM_COLORS] = { RED, PINK, RED };
