idf_component_register(
    SRCS
        "main.c"
        "wifi_scanner.c"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash          # Flash settings storage (Wi-Fi stack expects this)
        esp_wifi           # Wi-Fi driver; also used later for ESP-NOW
        esp_driver_uart    # USB serial console for commands
        esp_driver_gpio    # On-board scan-active LED
        esp_netif          # TCP/IP layer under Wi-Fi
        esp_event          # Event loop for Wi-Fi "scan done" callbacks
        # espnow_proto     # Phase 1.6: shared packet format with other ESP32s
)

# IDF >= 5.1 required (esp_driver_uart split from legacy "driver" component).
