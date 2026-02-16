# ESP32-C6 Standalone OpenThread Border Router

A standalone Thread Border Router for the **Seeed Studio XIAO ESP32-C6** that
integrates with **Home Assistant**. Each device is powered by USB-C only — no
host computer or data connection required after flashing.

The primary use case is **joining an existing Thread network** so the ESP32-C6
extends your mesh as an additional border router.

## Hardware

- **Seeed Studio XIAO ESP32-C6** (or any ESP32-C6 development board)
- USB-C cable for flashing & power
- Any USB power source (phone charger, power bank, etc.)

## Features

- **Standalone operation** — USB power only, no host needed
- **Wi-Fi backbone** — connects to your home network wirelessly
- **Native 802.15.4** — hardware Thread radio, no external module
- **Pre-provisioned joining** — paste your Thread dataset TLV and flash
- **Configurable device names** — run multiple OTBRs with unique identities
- **Home Assistant discovery** — auto-discovered via mDNS
- **OpenThread CLI** — serial console for provisioning & diagnostics
- **NVS persistence** — Thread credentials survive reboots
- **NAT64 / DNS64** — Thread devices can reach IPv4 services
- **SRP Server** — Thread device service registration

## Quick Start

### 1. Prerequisites

Install **one** of the following:

| Option | Install |
|--------|---------|
| **VS Code ESP-IDF Extension** (recommended) | [VS Code](https://code.visualstudio.com/) + [ESP-IDF extension](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension) |
| **ESP-IDF command line** | [ESP-IDF v5.3+](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/get-started/index.html) |

### 2. Configure

Edit **`main/config.h`** before flashing each device:

```c
// Give each border router a unique name
#define DEVICE_NAME     "otbr-01"

// Your home Wi-Fi
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"
```

#### Joining an existing Thread network (recommended)

Get your Thread network's active dataset TLV hex string, then paste it into
`main/config.h`:

```c
#define THREAD_DATASET_TLVS  "0e080000000000010000000300001935060004001fffe0..."
```

**Where to find the TLV hex string:**

- **Home Assistant:** Settings → Devices & Services → Thread → your network
  → "Active Operational Dataset" (copy the hex)
- **Another OTBR's CLI:** `dataset active -x`
- **Apple Home:** Settings → Thread Network → scroll down → copy credential

The device will automatically join the network on boot. The dataset is saved
to NVS, so it persists across reboots.

#### Waiting for provisioning (alternative)

If you leave `THREAD_DATASET_TLVS` empty (`""`), the device starts with no
Thread dataset and waits for credentials via Home Assistant discovery or
serial CLI.

### 3. Build & Flash

#### Using VS Code ESP-IDF Extension

1. Install the [ESP-IDF extension](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension)
   — when prompted, let it install ESP-IDF v5.5 (handles Python, toolchains, everything)
2. Open this folder in VS Code
3. `Ctrl+Shift+P` → **"ESP-IDF: Set Espressif Device Target"** → select `esp32c6`
4. `Ctrl+Shift+P` → **"ESP-IDF: Build your project"**
5. Plug in the ESP32-C6 via USB-C
6. `Ctrl+Shift+P` → **"ESP-IDF: Flash your project"**
7. `Ctrl+Shift+P` → **"ESP-IDF: Monitor your device"**

#### Using idf.py (command line)

```bash
# 1. Set the target chip
idf.py set-target esp32c6

# 2. (Optional) Fine-tune config — sdkconfig.defaults is applied automatically
idf.py menuconfig

# 3. Build
idf.py build

# 4. Flash (adjust port if needed)
idf.py -p /dev/ttyACM0 flash      # Linux
idf.py -p COM3 flash               # Windows

# 5. Monitor serial output
idf.py -p /dev/ttyACM0 monitor    # Linux
idf.py -p COM3 monitor             # Windows
```

### 4. Monitor

To see logs and use the OpenThread CLI:

```bash
idf.py -p COM3 monitor
```

### 5. Verify It Joined

Once the device boots, the serial log should show:

```
Thread dataset loaded from config (XX bytes)
Thread interface up — joining network...
Thread role changed: child
Thread role changed: router
```

If you see `router` or `leader`, the device has successfully joined the
Thread network and is providing border routing services.

You can also check from the CLI:

```
> state
router
> ipaddr
fd12:3456:789a:1::abcd
...
```

### 6. Home Assistant Discovery

1. Go to **Settings → Devices & Services → Thread**
2. Your new OTBR should appear (named as per `DEVICE_NAME`)
3. If you used `THREAD_DATASET_TLVS`, it's already on the correct network
4. If you left it empty, HA can push your Thread credentials to the device

## Running Multiple OTBRs

To deploy additional border routers:

1. Edit `main/config.h` — change only `DEVICE_NAME`:
   ```c
   #define DEVICE_NAME  "otbr-02"
   ```
2. Keep the same `THREAD_DATASET_TLVS` (all OTBRs share one Thread network)
3. Flash to the next ESP32-C6
4. Repeat for `otbr-03`, etc.

All devices should use the **same Wi-Fi credentials** and the **same Thread
dataset**.

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

## RF Coexistence Note

The ESP32-C6 has a **single 2.4 GHz radio** shared between Wi-Fi and
802.15.4 (Thread) via time-division multiplexing. This means:

- Wi-Fi and Thread **cannot receive simultaneously**
- Higher Wi-Fi traffic may cause some Thread packet loss
- This is adequate for **home use** with small-to-medium Thread networks

For production or high-reliability deployments, Espressif recommends a
dual-SoC design (e.g., ESP32-S3 + ESP32-H2 with separate radios). For most
Home Assistant setups, the single-chip ESP32-C6 works well.

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
- Make sure `THREAD_DATASET_TLVS` was copied correctly (no extra spaces)
- Check that the Thread channel isn't congested
- Try `factoryreset` and re-provision

### Build errors
- ESP-IDF v5.3+ is required for full C6 + OTBR support
- Run `idf.py fullclean` and rebuild if sdkconfig gets out of sync
- Verify `sdkconfig.defaults` is in the project root

### "No Thread dataset configured" on boot
- You left `THREAD_DATASET_TLVS` empty and `THREAD_AUTO_START` is 0
- Either paste your dataset TLV hex into `main/config.h` and reflash,
  or provision via serial CLI or Home Assistant

## Project Structure

```
esp32c6-otbr/
├── CMakeLists.txt          # Top-level ESP-IDF cmake
├── partitions.csv          # Custom partition table
├── sdkconfig.defaults      # ESP-IDF Kconfig defaults (OpenThread, Wi-Fi, etc.)
├── README.md               # This file
└── main/
    ├── CMakeLists.txt      # Main component cmake
    ├── idf_component.yml   # Managed component dependencies (mdns)
    ├── config.h            # ★ USER CONFIG — edit this per device ★
    └── main.c              # Application entry point
```

## License

MIT — use freely for your home automation projects.
