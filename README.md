# GitHub Health Usermod for WLED

Built for the [Octolamp](https://github.com/martinwoodward/octolamp).

A WLED usermod that polls the [GitHub Status API](https://www.githubstatus.com/api/v2/status.json) and makes all LEDs twinkle red and pink when GitHub reports an incident.

When GitHub recovers, the strip restores its previous state.

![Octolamp](https://raw.githubusercontent.com/martinwoodward/octolamp/main/images/IMG_9122.jpg)

## Installation

### Prerequisites

Install PlatformIO and esptool:

```bash
pip3 install platformio esptool
```

### Clone WLED

```bash
git clone --depth 1 --branch v16.0.1 https://github.com/wled/WLED.git
cd WLED
```

### Add the usermod

Copy the usermod into WLED's usermods directory:

```bash
mkdir -p usermods/github_health
cp /path/to/usermod-github-health/{github_health.cpp,github_health_fetch.cpp,usermod_github_health.h,library.json} usermods/github_health/
```

WLED v16 picks up usermods as PlatformIO libraries, so there's no need to edit `usermods_list.cpp`. PlatformIO installs the one dependency, [ESP_SSLClient](https://github.com/mobizt/ESP_SSLClient), on the first build.

This version targets WLED v16. For WLED 0.15.x, use commit `3bddf12` of this repo, which registers the usermod through `usermods_list.cpp` instead.

### Configure the build

Copy `platformio_override.ini` to the WLED root directory. It enables the usermod with `custom_usermods`. To set your upload port, add this line to the `[env:github_health_esp32]` section:

```ini
upload_port = /dev/cu.wchusbserialXXXXX
```

Replace the port with your device's serial port (check `ls /dev/cu.*` after connecting).

### Build and flash

```bash
cd WLED
pio run -e github_health_esp32 -t upload
```

Most boards enter the bootloader automatically. If the upload can't connect, hold **BOOT**, press **RESET**, release **BOOT**, and run it again.

Flashing over USB replaces the firmware but leaves the filesystem alone, so WiFi, presets, and timers survive. Before flashing, it's still worth taking a backup from **Config → Security & Updates → Backup configuration / presets**.

If PlatformIO keeps using an old copy of the usermod after you change `library.json`, delete `.pio/libdeps/github_health_esp32/github_health.pio-link` and rebuild.

## Configuration

Settings are exposed in the WLED Config UI under "GitHub Health":

| Setting | Default | Description |
|---------|---------|-------------|
| enabled | true | Enable/disable the health check |
| checkInterval | 60 | Seconds between status checks |
| alertOnMinor | true | Alert on `minor` status indicator |
| alertOnMajor | true | Alert on `major` status indicator |
| alertOnCritical | true | Alert on `critical` status indicator |

The GitHub Status API returns one of four indicator levels: `none`, `minor`, `major`, or `critical`. By default the lamp alerts on all non-healthy states. Disable individual levels to only react to the severities you care about.

## How it works

1. Every `checkInterval` seconds, makes an HTTPS GET to `githubstatus.com`. If the status page can't be reached, that counts as unhealthy.
2. Parses the `status.indicator` field. `"none"` means healthy.
3. If the indicator matches an enabled alert level, it saves each segment's effect, colors, palette, speed, and intensity. It then turns the lamp on at full brightness and switches every segment to the Twinklefox effect: red and pink twinkles over a faint pink glow.
4. While the alert lasts, each check puts the twinkle back if something else replaced it (a timed preset, for example). Brightness is left alone, so a timer can still switch the lamp off overnight.
5. When GitHub recovers, it restores each segment's saved state. If the lamp was switched off during the alert, it stays off.

The **Info** page shows the usermod's state. When a check fails, it adds the reason: an HTTP status code, or `-1` (TCP connect failed), `-2` (TLS handshake failed), or `-3` (no valid response), followed by the BearSSL error code.

### Why a separate TLS library?

WLED v16 builds on Tasmota's ESP32 Arduino core, which doesn't include `WiFiClientSecure` and strips the TLS layer out of mbedTLS. On ESP32 the usermod therefore makes its HTTPS request with ESP_SSLClient (BearSSL) over a plain `WiFiClient`. That code lives in `github_health_fetch.cpp`, separate from `wled.h`. Certificate verification is off, as before: the status page is public and the response only drives a lamp.
