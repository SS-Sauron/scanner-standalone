# Scanner Standalone (ESP32) — Learning Guide

Firmware for an **ESP32** (DOIT DevKit V1) that scans **Wi‑Fi**, **BLE**, or **Classic Bluetooth** from the serial monitor. Only **one radio activity** runs at a time: you never Wi‑Fi scan and Bluetooth scan together.

Results are stored in a structured **device cache** (for future ESP‑NOW to the CrowPanel). The serial monitor prints a compact summary, not an ASCII table.

---

## What this project does today (Stage 3)

| Command | Action |
|---------|--------|
| `wifi` | Wi‑Fi AP scan every ~5 s (driver up only during this mode) |
| `bt-ble` | BLE active scan ~10 s, then stack off |
| `bt-classic` | Classic inquiry ~11 s, then stack off |
| `bt` | BLE phase → pause → Classic phase (full BT teardown between) |
| `sweep_all` | Same as `bt`, then optional nRF24 phase if hardware detected |
| `stop` | Idle; all radios off |

**LED (GPIO 2):** on during any scan mode; off when idle.

**Rule:** `radio_guard` ensures Wi‑Fi driver and Bluedroid are not both active.

---

## Folder map

```
scanner-standalone/
├── CMakeLists.txt
├── sdkconfig.defaults      # Bluetooth + large app partition defaults
├── sdkconfig               # Generated; commit after menuconfig changes
├── README.md
├── main/
│   ├── main.c              # Commands, supervisor, LED, modes
│   ├── scanner_types.h     # MODE_IDLE / WIFI / BT / BT_CLASSIC / SWEEP
│   ├── board_config.h      # LED + optional nRF24 SPI pins
│   ├── wifi_scanner.c      # Wi‑Fi AP scan
│   ├── radio_guard.c       # One-radio-active policy
│   ├── bt_stack.c          # Bluedroid BTDM init/shutdown
│   ├── ble_scanner.c       # BLE GAP scan → device_cache
│   ├── classic_scanner.c   # Classic inquiry → device_cache
│   ├── bt_scanner.c        # Sequential BLE + Classic macro
│   ├── sweep_runner.c      # sweep_all orchestration
│   ├── device_record.h     # Structured discovery record
│   ├── device_cache.c      # Dedup table (64 entries)
│   └── nrf24_scanner.c     # nRF24 probe stub (scan TBD)
└── build/
```

---

## Modes

| Mode | Supervisor runs |
|------|-----------------|
| `MODE_IDLE` | Wait |
| `MODE_WIFI` | `wifi_scanner_run()` |
| `MODE_BT` | BLE only |
| `MODE_BT_CLASSIC` | Classic only |
| `MODE_SWEEP` | `sweep_runner_run()` (`bt` or `sweep_all`) |

---

## Bluetooth architecture

```text
  bt_stack_init()  [BTDM — do NOT mem_release BLE or Classic]
        │
        ├── ble_scanner_run()     esp_ble_gap_*  (no GATT)
        │
        └── classic_scanner_run() esp_bt_gap_start_discovery (no SDP in v1)

  bt_stack_shutdown()  between phases in `bt` / `sweep_all`
```

**device_record** fields include: radio type, MAC, RSSI, name, Classic COD, BLE appearance/manufacturer data, optional raw adv bytes.

---

## Boot sequence

1. NVS  
2. LED + `nrf24_probe_on_boot()`  
3. `device_cache_init()`  
4. USB serial (blocking stdin)  
5. `esp_netif` + event loop (no `esp_wifi_init` yet)  
6. `wifi_scanner_init()`  
7. `command_task` + `supervisor_task` (16 KB stack for BT)

---

## Build and flash

Requires **ESP-IDF 5.2+** (tested with 6.x), target **esp32**.

```bash
cd scanner-standalone
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

First build applies [`sdkconfig.defaults`](sdkconfig.defaults): Bluedroid dual-mode, large app partition, GATT/SMP off to save IRAM.

---

## Grand Plan alignment

| Phase 1 goal | This repo |
|--------------|-----------|
| Classic + BLE discovery | **Done** (GAP/inquiry) |
| Device table for panel | **device_cache** (serial summary today) |
| ESP‑NOW sender | Not yet |
| nRF24 sweep | Probe only; scan stub |

---

## Troubleshooting

| Problem | Check |
|---------|--------|
| Build: IRAM overflow | Use `sdkconfig.defaults`; disable GATTS/GATTC/SMP |
| Build: app partition too small | `CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE` in defaults |
| No BLE devices | Active scan needs nearby advertisers; try `bt-ble` |
| No Classic devices | Inquiry ~11 s; phones may need to be discoverable |
| Wi‑Fi after BT fails | Type `stop`; guard should deinit BT — check logs for `heap free` |
| Wrong LED polarity | Swap levels in `board_config.h` |

---

## License / ethics

For **security research and learning** on hardware you own. Do not monitor networks or devices without permission.
