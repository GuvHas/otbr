# ESP32-C6 Standalone OpenThread Border Router

A standalone Thread Border Router for the **Seeed Studio XIAO ESP32-C6** that
integrates with **Home Assistant**. Each device is powered by USB-C only — no
host computer or data connection required after flashing.

## Hardware

- **Seeed Studio XIAO ESP32-C6** (or any ESP32-C6 development board)
- USB-C cable for flashing & power
- Any USB power source (phone charger, power bank, etc.)

## Features

- **Standalone operation** — USB power only, no host needed
- **Wi-Fi backbone** — connects to your home network wirelessly
- **Native 802.15.4** — hardware Thread radio, no external module
- **Configurable device names** — run multiple OTBRs with unique identities
- **Home Assistant discovery** — auto-discovered via mDNS
- **OpenThread CLI** — serial console for provisioning & diagnostics
- **NVS persistence** — Thread credentials survive reboots
- **NAT64 / DNS64** — Thread devices can reach IPv4 services
- **SRP Server** — Thread device service registration

## Quick Start

### 1. Prerequisites

- [VS Code](https://code.visualstudio.com/) with the
  [PlatformIO extension](https://platformio.org/install/ide?install=vscode)
- USB-C cable

### 2. Configure

Edit **`src/config.h`** before flashing each device:

```c
#define DEVICE_NAME     "otbr-01"           // Unique per device
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"
```

### 3. Build & Flash

1. Open this folder in VS Code
2. PlatformIO should auto-detect the project
3. Plug in the ESP32-C6 via USB-C
4. Click the **Upload** button (→ arrow) in the PlatformIO toolbar
   — or run in the terminal:
   ```bash
   pio run -t upload
   ```

### 4. Monitor (optional)

To see logs and use the OpenThread CLI:

```bash
pio device monitor
```

### 5. Join Your Thread Network

**Option A: Via Home Assistant (recommended)**

1. Go to **Settings → Devices & Services → Thread**
2. Your new OTBR should appear (named as per `DEVICE_NAME`)
3. HA can push your existing Thread network credentials to the new OTBR

**Option B: Via Serial Console**

1. Open the serial monitor: `pio device monitor`
2. Get the Thread dataset from HA (Settings → Thread → your network → copy TLV)
3. Enter these commands:
   ```
   > dataset set active <paste-hex-TLV-here>
   Done
   > ifconfig up
   Done
   > thread start
   Done
   ```
4. After a few seconds, check status:
   ```
   > state
   router
   ```

**Option C: Auto-start a new network**

In `src/config.h`, set `THREAD_AUTO_START` to `1`. The device will create its
own Thread network on first boot (you can then share credentials to HA).

## Running Multiple OTBRs

To deploy additional border routers:

1. Edit `src/config.h` — change only `DEVICE_NAME`:
   ```c
   #define DEVICE_NAME  "otbr-02"
   ```
2. Flash to the next ESP32-C6
3. Repeat for `otbr-03`, etc.

All devices should use the **same Wi-Fi credentials** and the **same Thread
dataset** (HA manages this automatically once they're all discovered).

## Useful OpenThread CLI Commands

| Command | Description |
|---------|-------------|
| `state` | Current role (disabled/detached/child/router/leader) |
| `dataset active -x` | Show current active dataset as hex TLV |
| `dataset set active <hex>` | Set active dataset |
| `ifconfig up` | Enable the Thread interface |
| `thread start` | Start Thread |
| `thread stop` | Stop Thread |
| `ipaddr` | Show all IPv6 addresses |
| `neighbor table` | Show connected Thread neighbors |
| `netdata show` | Show Thread network data |
| `br state` | Border router state |
| `factoryreset` | Erase all settings and restart |

## Troubleshooting

### Device not appearing in Home Assistant
- Verify the ESP32-C6 is connected to Wi-Fi (check serial logs for IP address)
- Ensure HA and the ESP32-C6 are on the same network/subnet
- Check that mDNS is not blocked by your router
- Restart the Thread integration in HA

### Wi-Fi keeps disconnecting
- Move the device closer to your router
- Check for interference on your Wi-Fi channel
- The serial log will show reconnection attempts

### Thread stuck in "detached" state
- Verify the dataset matches your existing Thread network exactly
- Check that the Thread channel isn't congested
- Try `factoryreset` and re-provision

### Build errors
- Ensure PlatformIO has the latest `espressif32` platform:
  ```bash
  pio pkg update
  ```
- ESP-IDF v5.3+ is required for full C6 + OTBR support
- If you see OpenThread-related errors, verify `sdkconfig.defaults` was picked up

## Project Structure

```
esp32c6-otbr/
├── platformio.ini          # PlatformIO project config
├── CMakeLists.txt          # Top-level ESP-IDF cmake
├── partitions.csv          # Custom partition table
├── sdkconfig.defaults      # ESP-IDF Kconfig defaults (OpenThread, Wi-Fi, etc.)
├── README.md               # This file
└── src/
    ├── CMakeLists.txt      # Main component cmake
    ├── config.h            # ★ USER CONFIG — edit this per device ★
    └── main.c              # Application entry point
```

## License

MIT — use freely for your home automation projects.
