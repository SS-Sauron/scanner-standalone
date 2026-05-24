# Scanner Standalone (ESP32) — Learning Guide

This folder contains firmware (software that runs **on** the microcontroller) for an **ESP32** board. The chip listens to commands you type over USB serial and can scan for nearby Wi‑Fi networks. Bluetooth scanning is planned for a later step; today it is only a placeholder.

This README explains the **whole project** in plain language. The `.c` and `.h` files in `main/` have shorter comments that point back here when something needs more context.

---

## What you need to know first

| Word | Meaning |
|------|---------|
| **ESP32** | A small computer-on-a-chip with Wi‑Fi and Bluetooth. |
| **Firmware** | The program flashed into the chip; it survives power loss. |
| **ESP-IDF** | Espressif’s official C toolkit to build firmware for ESP32. |
| **FreeRTOS** | A “task manager” inside ESP-IDF: many mini-programs (tasks) run at once. |
| **Serial monitor** | A window on your PC that shows `printf` logs and lets you type commands. |
| **GPIO** | A pin you can set HIGH or LOW — here we use it for the blue LED. |

---

## What this project does today (Stage 2)

1. **Boot** — Sets up memory, USB serial, Wi‑Fi networking layer, and a status LED.
2. **Wait for you** — You type commands at 115200 baud in the serial monitor.
3. **Commands:**
   - `wifi` — Start scanning for Wi‑Fi access points (routers) every few seconds.
   - `bt` — Placeholder (“Bluetooth coming later”); LED still shows “active”.
   - `stop` — Go idle; turn off scanning and the LED.
4. **Blue LED** — On while `wifi` or `bt` mode is active; off after `stop` or at boot.

Later (Grand Plan Phase 1), this will become a full **Bluetooth device scanner** that sends results to other hardware over ESP‑NOW. This repo is the **foundation** for that.

---

## Folder map

```
scanner-standalone/
├── CMakeLists.txt          # Top-level “project name” for ESP-IDF
├── sdkconfig               # Chip options (Wi‑Fi on, Bluetooth off for now, etc.)
├── README.md               # You are here
├── main/
│   ├── CMakeLists.txt      # Lists source files and libraries to link
│   ├── main.c              # Brain: setup, commands, supervisor, LED, modes
│   ├── scanner_types.h     # Shared “mode” enum and mode_get / mode_set API
│   ├── wifi_scanner.c      # Wi‑Fi scan loop (start/stop driver, count APs)
│   └── wifi_scanner.h      # Public functions for the Wi‑Fi module
└── build/                  # Created by `idf.py build` — not committed to git
```

---

## How the program is structured (big picture)

Think of the firmware as **three workers** plus **one boss**:

```text
  YOU (serial monitor)
       │
       ▼
  ┌─────────────┐     sets mode      ┌──────────────────┐
  │ command_task│ ────────────────► │  mode (IDLE/WIFI/ │
  │  reads wifi, │                   │       BT)        │
  │  bt, stop    │                   └────────┬─────────┘
  └─────────────┘                            │
                                             ▼
                                    ┌──────────────────┐
                                    │ supervisor_task  │
                                    │  if WIFI → run   │
                                    │   wifi_scanner   │
                                    │  if BT   → stub  │
                                    │  if IDLE → sleep │
                                    └──────────────────┘
```

- **`app_main`** runs once at boot, does setup, starts the two tasks, then exits.
- **`command_task`** only reads the keyboard (`fgets`) and calls `mode_set(...)`.
- **`supervisor_task`** looks at the mode and runs the right scanner until the mode changes.

That split keeps **slow human typing** separate from **long-running Wi‑Fi work**, so one does not block the other.

---

## Walkthrough: boot sequence (`app_main`)

Order matters. In [`main/main.c`](main/main.c), `app_main` roughly does:

1. **NVS** — Non-volatile storage (flash section for settings). Required by Wi‑Fi stack.
2. **Event group** — A FreeRTOS object used to wake tasks when the mode changes.
3. **LED** — Configure GPIO 2 as output; start with LED off.
4. **USB serial** — Make `stdin` **blocking** so `fgets` waits for you instead of returning instantly.
5. **TCP/IP + Wi‑Fi interface** — Start the network stack once for the whole program lifetime (needed now and for future ESP‑NOW).
6. **`wifi_scanner_init()`** — Register a callback for “scan finished” events (does not scan yet).
7. **Create tasks** — `command_task` and `supervisor_task`.

After that, all real work happens inside those tasks.

---

## Walkthrough: the “mode” system

The chip is always in exactly one of three modes (see [`scanner_types.h`](main/scanner_types.h)):

| Mode | Value | Meaning |
|------|-------|---------|
| `MODE_IDLE` | 0 | Not scanning; LED off. |
| `MODE_WIFI` | 1 | Supervisor runs Wi‑Fi scanner. |
| `MODE_BT` | 2 | Supervisor runs BT placeholder. |

**Rules:**

- Only use `mode_get()` and `mode_set()` to read/write mode (not the variable directly).
- A **spinlock** protects mode on the ESP32’s two CPU cores.
- `mode_set` also updates the LED and wakes anyone waiting in `mode_wait_change()`.

---

## Walkthrough: Wi‑Fi scanning ([`wifi_scanner.c`](main/wifi_scanner.c))

When the supervisor calls `wifi_scanner_run()`:

1. `esp_wifi_init` → set STA mode → `esp_wifi_start`.
2. Loop while mode is still `MODE_WIFI`:
   - **`scan_once`** — Start a non-blocking scan; wait up to 4 seconds for “scan done”; log how many APs were seen; **free** the driver’s AP list (avoids memory leaks).
   - **`mode_wait_change(5000)`** — Pause up to 5 seconds, unless you typed `stop` (then wake early).
3. `esp_wifi_stop` → `esp_wifi_deinit` and return.

Important: typing `stop` sets mode to **IDLE** immediately (LED off), but the supervisor might still be inside one `scan_once()` until that finishes (up to ~4 s). That is normal for this design.

---

## How to build and flash

Install [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/) and open a terminal where `idf.py` works.

```bash
cd scanner-standalone
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Replace `/dev/ttyUSB0` with your port (`/dev/ttyACM0` on some boards).

In the monitor:

- Type `wifi` and press Enter — you should see `Found N access point(s)` every few seconds and the blue LED on.
- Type `stop` — scanning stops, LED off.
- Type `bt` — placeholder message; LED on until `stop`.

Wrong commands like `button` should print **Unknown command** (exact matching, not partial words).

---

## LED wiring note (DOIT DevKit)

This project uses **GPIO 2** for the built-in blue LED. Some boards are **active LOW** (0 = on), others **active HIGH** (1 = on). If the LED is backwards, swap `SCAN_LED_LEVEL_ON` and `SCAN_LED_LEVEL_OFF` in `main.c`.

---

## How this fits the “Grand Plan”

The larger research project needs a **Scanner ESP32** that discovers Bluetooth devices and sends them to a dashboard device over **ESP‑NOW**. This repo is **Stage 2 infrastructure**:

| Grand Plan Phase 1 | This repo |
|--------------------|-----------|
| Bluetooth Classic + BLE discovery | Not yet — `bt` is a stub |
| ASCII device table | Not yet |
| Commands `start` / `stop` / `table on` … | Today: `wifi` / `bt` / `stop` |
| ESP‑NOW sender | Not yet |

You are roughly at **Phase 1.1 done + custom Stage 2 Wi‑Fi bring-up**. See your Master Implementation doc for the next steps.

---

## Learning path through the source files

Read in this order:

1. [`main/scanner_types.h`](main/scanner_types.h) — Modes and function names.
2. [`main/main.c`](main/main.c) — `app_main`, tasks, commands, LED.
3. [`main/wifi_scanner.h`](main/wifi_scanner.h) — What the Wi‑Fi module exposes.
4. [`main/wifi_scanner.c`](main/wifi_scanner.c) — How one scan cycle works.

Experiment: change the log tag, add an `ESP_LOGI` in `command_task`, rebuild, flash, and watch the monitor. That is the fastest way to see cause and effect.

---

## Troubleshooting

| Problem | Things to check |
|---------|------------------|
| Commands ignored | Serial monitor must send newline; boot log should say `stdin blocking OK`. |
| No APs found | Normal in a shielded room; try near your home router. |
| LED always wrong polarity | Swap ON/OFF levels in `main.c`. |
| Build errors | Run `idf.py set-target esp32`; source ESP-IDF install. |

---

## License / ethics

This firmware is for **security research and learning** on hardware you own. Do not use it to monitor networks or devices without permission.
