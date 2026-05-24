# UART telemetry to CrowPanel (Phase 1)

## Wiring

| Scanner ESP32 | CrowPanel UART1-OUT |
|---------------|---------------------|
| GPIO 17 (TX)  | GPIO 18 (RX)        |
| GPIO 16 (RX)  | GPIO 17 (TX)        |
| GND           | GND                 |

Baud: **460800** (both sides, `board_config.h`).

**USB serial (this bench):** CrowPanel `/dev/ttyACM0`, scanner `/dev/ttyUSB0`. Each board keeps its own USB link for `idf.py monitor` and scanner commands (`wifi`, `bt-ble`, …).

## Modes

| Scanner command | UART stream | Panel table |
|-----------------|-------------|-------------|
| `wifi` | `WIFI` lines per AP | Typ=WiFi, Name=SSID |
| `bt-ble` | `BLE` per device | Typ=BLE |
| `bt-classic` | `CLASSIC` per device | Typ=BR |
| `bt` / `bt-full` | BLE then Classic | Mixed rows |

1. Flash both firmwares, wire UART, insert SD with `oui/oui.csv`.
2. Run a scan on the scanner; the panel updates live.
3. CSV: `/sdcard/logs/wifi/session_NNN_wifi.csv` and `/sdcard/logs/bluetooth/session_NNN_bt.csv`

## NDJSON line format

```json
{"type":"WIFI","mac":"aa:bb:cc:11:22:33","rssi":-65,"ts":12345,"field1":"MyNet","field2":"WPA2_PSK","field3":"CH_6"}
{"type":"BLE","mac":"dd:ee:ff:44:55:66","rssi":-82,"ts":12346,"field1":"Watch","field2":"0x03c0","field3":"MFG_004c"}
{"type":"CLASSIC","mac":"11:22:33:44:55:66","rssi":-70,"ts":12347,"field1":"Phone","field2":"0x5a020c","field3":"INQUIRY"}
```

## CrowPanel SD card

Copy OUI database to:

```text
/sdcard/oui/oui.csv
```

Format (one per line):

```text
AA:BB:CC,Espressif Inc.
```

See [`oui_sample.csv`](oui_sample.csv) for examples.
