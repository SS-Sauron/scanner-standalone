/*
 * nrf24_scanner.c — SPI probe for optional nRF24L01 module (scan stub)
 */

#include "nrf24_scanner.h"

#include "board_config.h"
#include "esp_log.h"

#if NRF24L01_PRESENT
#include "driver/gpio.h"
#include "driver/spi_master.h"
#endif

static const char *TAG = "nrf24";
static bool s_attached;

void nrf24_probe_on_boot(void)
{
    s_attached = false;

#if NRF24L01_PRESENT
    if (NRF24L01_SCLK_GPIO == GPIO_NUM_NC || NRF24L01_CS_GPIO == GPIO_NUM_NC) {
        ESP_LOGI(TAG, "nRF24 pins not configured — not fitted");
        return;
    }

    spi_bus_config_t buscfg = {
        .mosi_io_num = NRF24L01_MOSI_GPIO,
        .miso_io_num = NRF24L01_MISO_GPIO,
        .sclk_io_num = NRF24L01_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return;
    }

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = NRF24L01_CS_GPIO,
        .queue_size = 1,
    };

    spi_device_handle_t spi;
    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SPI add device failed: %s", esp_err_to_name(ret));
        return;
    }

    uint8_t cmd = NRF24L01_REG_CONFIG | 0x80;
    uint8_t rx = 0;
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
        .rx_buffer = &rx,
    };
    ret = spi_device_transmit(spi, &t);
    spi_bus_remove_device(spi);

    if (ret == ESP_OK && rx != 0 && rx != 0xFF) {
        s_attached = true;
        ESP_LOGI(TAG, "nRF24 attached (CONFIG=0x%02x)", rx);
    } else {
        ESP_LOGI(TAG, "nRF24 not detected");
    }
#else
    ESP_LOGI(TAG, "nRF24 not enabled in board_config (NRF24L01_PRESENT=false)");
#endif
}

bool nrf24_is_attached(void)
{
    return s_attached;
}

esp_err_t nrf24_scanner_run(void)
{
    if (!s_attached) {
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGW(TAG, "nRF24 channel sweep not implemented");
    return ESP_ERR_NOT_SUPPORTED;
}
