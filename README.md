# GitHub Health Usermod for WLED

A WLED usermod that polls the [GitHub Status API](https://www.githubstatus.com/api/v2/status.json) and blinks all LEDs red when GitHub reports any incident.

When GitHub recovers, the strip restores its previous state.

## Installation

Copy `usermod_github_health.h` into your WLED `usermods/` directory, then register it in `usermods_list.cpp`:

```cpp
#include "../usermods/usermod_github_health.h"

void registerUsermods() {
  UsermodManager::add(new GithubHealthUsermod());
}
```

Copy `platformio_override.ini` to your WLED root if you need a dedicated build environment.

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
