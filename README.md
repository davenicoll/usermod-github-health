# GitHub Health Usermod for WLED

Built for the [Octolamp](https://github.com/martinwoodward/octolamp).

A WLED usermod that polls the [GitHub Status API](https://www.githubstatus.com/api/v2/status.json) and blinks all LEDs red when GitHub reports any incident.

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
git clone --depth 1 --branch v0.15.4 https://github.com/wled/WLED.git
cd WLED
```

### Add the usermod

Copy the usermod header into WLED's usermods directory:

```bash
mkdir -p usermods/github_health
cp /path/to/usermod_github_health.h usermods/github_health/
```

Register it in `wled00/usermods_list.cpp`. Add the include near the other usermod includes:

```cpp
#ifdef USERMOD_GITHUB_HEALTH
#include "../usermods/github_health/usermod_github_health.h"
#endif
```

And add the registration inside `registerUsermods()`:

```cpp
#ifdef USERMOD_GITHUB_HEALTH
UsermodManager::add(new GithubHealthUsermod());
#endif
```

### Configure the build

Copy `platformio_override.ini` to the WLED root directory. To set your upload port, add these lines to the `[env:github_health_esp32]` section:

```ini
upload_port = /dev/cu.wchusbserialXXXXX
```

Replace the port with your device's serial port (check `ls /dev/cu.*` after connecting).

### Build and flash

Put your ESP32 into bootloader mode: hold **BOOT**, press **RESET**, release **BOOT**.

```bash
cd WLED
pio run -e github_health_esp32 -t upload
```

## Configuration

Settings are exposed in the WLED Config UI under "GitHub Health":

| Setting | Default | Description |
|---------|---------|-------------|
| enabled | true | Enable/disable the health check |
| checkInterval | 60 | Seconds between status checks |
| blinkInterval | 500 | Milliseconds between blink toggles |

## How it works

1. Every `checkInterval` seconds, makes an HTTPS GET to `githubstatus.com`
2. Parses the `status.indicator` field — `"none"` means healthy
3. If unhealthy (or the status page is unreachable), saves the current strip state and blinks red
4. When GitHub recovers, restores the previous strip state
