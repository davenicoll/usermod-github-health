// HTTPS fetch for ESP32.
// WLED v16 builds against Tasmota's ESP32 core, which ships without WiFiClientSecure
// and with mbedTLS's TLS layer stripped, so TLS comes from ESP_SSLClient (BearSSL)
// layered over a plain WiFiClient. Kept out of wled.h's translation unit to avoid
// pulling BearSSL headers into the rest of the build.
#ifdef ESP32
#include <Arduino.h>
#include <WiFiClient.h>
// WLED's common build flags set BEARSSL_SSL_BASIC (an ESP8266 option for a reduced,
// RSA-only cipher set). It trips an unguarded debug call in ESP_SSLClient and drops
// the EC ciphers modern CDNs use, so build this client with the full cipher set.
#undef BEARSSL_SSL_BASIC
#include <ESP_SSLClient.h>

// Last BearSSL error code from githubHealthFetch (0 = none), for diagnostics.
int githubHealthLastSSLError = 0;

// Performs GET https://<host><path>. On return, out holds the response body
// (NUL-terminated, truncated to outSize-1). Returns the HTTP status code, or a
// negative stage code: -1 TCP connect, -2 TLS handshake, -3 no/garbled response.
int githubHealthFetch(const char* host, const char* path, char* out, size_t outSize) {
  githubHealthLastSSLError = 0;
  if (outSize == 0) return -3;
  out[0] = '\0';

  WiFiClient    tcp;
  ESP_SSLClient ssl;
  ssl.setClient(&tcp);
  ssl.setInsecure();               // status page is public and not sensitive
  ssl.setBufferSizes(16384, 512);  // CDN doesn't negotiate small TLS fragments
  ssl.setSessionTimeout(10);

  if (!ssl.connect(host, 443)) {
    githubHealthLastSSLError = ssl.getLastSSLError();
    return tcp.connected() ? -2 : -1;
  }

  ssl.print(F("GET "));
  ssl.print(path);
  ssl.print(F(" HTTP/1.0\r\nHost: "));
  ssl.print(host);
  ssl.print(F("\r\nUser-Agent: WLED-GitHubHealth\r\nAccept: application/json\r\nConnection: close\r\n\r\n"));
  ssl.flush(); // BearSSL buffers writes until its record buffer fills; send the request now

  size_t len = 0;
  uint32_t start = millis();
  while ((ssl.connected() || ssl.available()) && len < outSize - 1 && millis() - start < 5000) {
    int avail = ssl.available();
    if (avail <= 0) { delay(5); continue; }
    int n = ssl.read((uint8_t*)out + len, min((size_t)avail, outSize - 1 - len));
    if (n > 0) len += n;
  }
  out[len] = '\0';
  githubHealthLastSSLError = ssl.getLastSSLError();
  ssl.stop();

  int code = -3;
  if (sscanf(out, "HTTP/%*d.%*d %d", &code) != 1) return -3;

  char* body = strstr(out, "\r\n\r\n");
  if (body) memmove(out, body + 4, strlen(body + 4) + 1);
  else out[0] = '\0';
  return code;
}
#endif
