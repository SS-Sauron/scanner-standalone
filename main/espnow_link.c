/*
 * espnow_link.c — ESP-NOW transmitter to CrowPanel (Phase 5)
 */

#include "espnow_link.h"

#include <string.h>

#include "board_config.h"
#include "device_record.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "espnow_proto.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "radio_guard.h"

static const char *TAG = "espnow";

static bool s_ready;
static bool s_wifi_up;
static SemaphoreHandle_t s_wifi_mux;

static void panel_mac(uint8_t out[6])
{
    const uint8_t mac[6] = ESPNOW_PEER_PANEL_MAC;
    memcpy(out, mac, 6);
}

static bool peer_mac_configured(void)
{
    uint8_t mac[6];
    panel_mac(mac);
    for (int i = 0; i < 6; i++) {
        if (mac[i] != 0x00 && mac[i] != 0xFF) {
            return true;
        }
    }
    return false;
}

static void log_own_mac(void)
{
    uint8_t mac[6];
    if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK) {
        ESP_LOGI(TAG, "Scanner Wi-Fi STA MAC %02x:%02x:%02x:%02x:%02x:%02x "
                     "(set ESPNOW_PEER_PANEL_MAC on panel to CrowPanel MAC)",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
}

static void send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    if (status != ESP_NOW_SEND_SUCCESS && tx_info != NULL) {
        ESP_LOGW(TAG, "send fail -> " MACSTR " status=%d",
                 MAC2STR(tx_info->des_addr), (int)status);
    }
}

static esp_err_t wifi_ensure_started(void)
{
    if (s_wifi_mux == NULL) {
        s_wifi_mux = xSemaphoreCreateMutex();
    }
    if (xSemaphoreTake(s_wifi_mux, pdMS_TO_TICKS(2000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = ESP_OK;
    if (!s_wifi_up) {
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ret = esp_wifi_init(&cfg);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            xSemaphoreGive(s_wifi_mux);
            return ret;
        }
        ret = esp_wifi_set_mode(WIFI_MODE_STA);
        if (ret != ESP_OK) {
            xSemaphoreGive(s_wifi_mux);
            return ret;
        }
        ret = esp_wifi_start();
        if (ret != ESP_OK) {
            xSemaphoreGive(s_wifi_mux);
            return ret;
        }
        ret = esp_wifi_set_channel(ESPNOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "set_channel: %s", esp_err_to_name(ret));
        }
        s_wifi_up = true;
        radio_guard_wifi_mark_up();
        ESP_LOGI(TAG, "Wi-Fi STA up (ESP-NOW channel %d)", ESPNOW_WIFI_CHANNEL);
    }

    xSemaphoreGive(s_wifi_mux);
    return ESP_OK;
}

static esp_err_t add_panel_peer(void)
{
    if (!peer_mac_configured()) {
        ESP_LOGW(TAG, "ESPNOW_PEER_PANEL_MAC not set — edit board_config.h");
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t panel[6];
    panel_mac(panel);

    if (esp_now_is_peer_exist(panel)) {
        return ESP_OK;
    }

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, panel, 6);
    peer.channel = ESPNOW_WIFI_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;

    esp_err_t ret = esp_now_add_peer(&peer);
    if (ret == ESP_ERR_ESPNOW_EXIST) {
        return ESP_OK;
    }
    return ret;
}

esp_err_t espnow_link_init(void)
{
    if (s_ready) {
        return ESP_OK;
    }

    esp_err_t ret = wifi_ensure_started();
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_now_init();
    if (ret != ESP_OK && ret != ESP_ERR_ESPNOW_EXIST) {
        ESP_LOGE(TAG, "esp_now_init: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_now_register_send_cb(send_cb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "register_send_cb: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = add_panel_peer();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "add_peer panel: %s", esp_err_to_name(ret));
        return ret;
    }

    s_ready = true;
    log_own_mac();
    uint8_t panel[6];
    panel_mac(panel);
    ESP_LOGI(TAG, "ESP-NOW ready → panel " MACSTR, MAC2STR(panel));
    return ESP_OK;
}

bool espnow_link_ready(void)
{
    return s_ready;
}

bool espnow_link_wifi_held(void)
{
    return s_ready && s_wifi_up;
}

static esp_err_t send_command(const command_t *cmd)
{
    if (!s_ready || cmd == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!peer_mac_configured()) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = wifi_ensure_started();
    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t panel[6];
    panel_mac(panel);
    return esp_now_send(panel, (const uint8_t *)cmd, sizeof(command_t));
}

static void fill_from_record(const device_record_t *rec, device_info_t *dev)
{
    memset(dev, 0, sizeof(*dev));
    memcpy(dev->bda, rec->addr, 6);
    dev->rssi = rec->rssi;
    dev->cod = rec->cod;
    dev->company_id = rec->mfg_company_id;
    dev->tx_power = 0;

    if (rec->name_len > 0) {
        strncpy(dev->name, rec->name, sizeof(dev->name) - 1);
    } else {
        strncpy(dev->name, "(no name)", sizeof(dev->name) - 1);
    }

    switch (rec->radio) {
    case SCAN_RADIO_BLE:
        dev->type = DEVICE_TYPE_BLE;
        strncpy(dev->vendor, "BLE", sizeof(dev->vendor) - 1);
        break;
    case SCAN_RADIO_CLASSIC:
        dev->type = DEVICE_TYPE_CLASSIC;
        strncpy(dev->vendor, "Classic", sizeof(dev->vendor) - 1);
        break;
    case SCAN_RADIO_NRF24:
        dev->type = DEVICE_TYPE_BLE;
        strncpy(dev->name, "nRF24", sizeof(dev->name) - 1);
        strncpy(dev->vendor, "NRF", sizeof(dev->vendor) - 1);
        break;
    default:
        dev->type = DEVICE_TYPE_BLE;
        break;
    }
}

esp_err_t espnow_link_send_device(const device_record_t *rec)
{
    if (rec == NULL || !rec->valid) {
        return ESP_ERR_INVALID_ARG;
    }

    command_t cmd = {0};
    cmd.version = ESPNOW_PROTO_VERSION;
    cmd.cmd_id = CMD_SEND_DEVICE;
    fill_from_record(rec, &cmd.payload.device);
    return send_command(&cmd);
}

esp_err_t espnow_link_send_wifi_ap(const wifi_ap_record_t *ap, uint32_t ts_ms)
{
    if (ap == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    command_t cmd = {0};
    cmd.version = ESPNOW_PROTO_VERSION;
    cmd.cmd_id = CMD_SEND_DEVICE;

    device_info_t *dev = &cmd.payload.device;
    memcpy(dev->bda, ap->bssid, 6);
    dev->rssi = ap->rssi;
    dev->type = DEVICE_TYPE_WIFI;
    dev->cod = ((uint32_t)ap->primary << 16) | (uint32_t)ap->authmode;
    dev->tx_power = 0;

    if (ap->ssid[0] == '\0') {
        strncpy(dev->name, "(hidden)", sizeof(dev->name) - 1);
    } else {
        strncpy(dev->name, (const char *)ap->ssid, sizeof(dev->name) - 1);
    }
    snprintf(dev->vendor, sizeof(dev->vendor), "ts=%lu", (unsigned long)ts_ms);

    return send_command(&cmd);
}
