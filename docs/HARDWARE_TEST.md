# Hardware regression checklist

Run after flashing `scanner.bin` (see README). Default durations: **30 s** BLE / Classic (`main/scanner_config.h`).

## Flash

```bash
cd ~/scanner-standalone
source ~/esp/esp-idf/export.sh
idf.py -p /dev/ttyUSB0 flash monitor
```

## Tests

| Step | Command | Pass |
|------|---------|------|
| Boot | (monitor) | `NVS OK`, `Supervisor ready`, `NRF24: not fitted` (default) |
| BLE | `bt-ble` | Lines during scan; **one** `scan complete: N device(s)`; idle ~30 s |
| Classic | `bt-classic` | Same; COD in line when present |
| Dual | `bt` | BLE then Classic; **one** footer (not two) |
| Full | `bt-full` | Same as `bt`; phase 3 skip or stub log |
| Radio switch | `wifi` → `stop` → `bt-ble` → `stop` → `wifi` | No crash; heap logs OK |
| Stop mid-scan | `bt-ble` then `stop` within ~5 s | LED off; idle |
| Alias | `sweep_all` | Warns renamed to `bt-full`; same behavior |

## UART → CrowPanel (Phase 1)

Wire scanner UART2 TX (GPIO 17) → panel UART1 RX (GPIO 18), cross TX/RX, common GND.

1. Prepare SD: `/sdcard/oui/oui.csv` (see `docs/oui_sample.csv`).
2. Flash both boards; panel shows empty table.
3. Scanner: `wifi` — table fills; CSV under `/sdcard/logs/wifi/`.

## Build verification (no hardware)

```bash
rm -f sdkconfig && idf.py set-target esp32 && idf.py build
```

Expect: `CONFIG_BT_ENABLED=y` in `sdkconfig`, app fits large partition.
