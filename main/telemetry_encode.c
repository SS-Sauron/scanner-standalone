/*
 * telemetry_encode.c — NDJSON line builder (scanner)
 */

#include "telemetry_encode.h"

#include <stdio.h>
#include <string.h>

#include "telemetry_proto.h"

static const char *wifi_auth_to_str(wifi_auth_mode_t mode)
{
    switch (mode) {
    case WIFI_AUTH_OPEN:                 return "OPEN";
    case WIFI_AUTH_WEP:                  return "WEP";
    case WIFI_AUTH_WPA_PSK:              return "WPA_PSK";
    case WIFI_AUTH_WPA2_PSK:             return "WPA2_PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:         return "WPA_WPA2_PSK";
    case WIFI_AUTH_WPA2_ENTERPRISE:      return "WPA2_ENTERPRISE";
    case WIFI_AUTH_WPA3_PSK:             return "WPA3_PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK:        return "WPA2_WPA3_PSK";
    case WIFI_AUTH_WAPI_PSK:             return "WAPI_PSK";
    case WIFI_AUTH_OWE:                  return "OWE";
    case WIFI_AUTH_WPA3_ENT_192:         return "WPA3_ENT_192";
    case WIFI_AUTH_DPP:                  return "DPP";
    case WIFI_AUTH_WPA3_ENTERPRISE:      return "WPA3_ENTERPRISE";
    case WIFI_AUTH_WPA2_WPA3_ENTERPRISE: return "WPA2_WPA3_ENTERPRISE";
    default:                             return "OTHER";
    }
}

static size_t json_escape(const char *in, char *out, size_t out_len)
{
    size_t j = 0;
    if (out_len == 0) {
        return 0;
    }
    for (size_t i = 0; in[i] != '\0' && j + 2 < out_len; i++) {
        char c = in[i];
        if (c == '"' || c == '\\') {
            out[j++] = '\\';
            out[j++] = c;
        } else if (c >= 32 && c < 127) {
            out[j++] = c;
        } else {
            out[j++] = '?';
        }
    }
    out[j] = '\0';
    return j;
}

esp_err_t telemetry_encode_wifi(const wifi_ap_record_t *ap,
                                uint32_t ts_ms,
                                char *out,
                                size_t out_len)
{
    if (ap == NULL || out == NULL || out_len < 32) {
        return ESP_ERR_INVALID_ARG;
    }

    char mac[18];
    snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
             ap->bssid[0], ap->bssid[1], ap->bssid[2],
             ap->bssid[3], ap->bssid[4], ap->bssid[5]);

    char ssid_esc[TELEMETRY_FIELD_MAX * 2];
    if (ap->ssid[0] == '\0') {
        strncpy(ssid_esc, "(hidden)", sizeof(ssid_esc) - 1);
    } else {
        json_escape((const char *)ap->ssid, ssid_esc, sizeof(ssid_esc));
    }

    char ch[16];
    snprintf(ch, sizeof(ch), "CH_%d", ap->primary);

    int n = snprintf(out, out_len,
                     "{\"type\":\"WIFI\",\"mac\":\"%s\",\"rssi\":%d,\"ts\":%lu,"
                     "\"field1\":\"%s\",\"field2\":\"%s\",\"field3\":\"%s\"}\n",
                     mac,
                     (int)ap->rssi,
                     (unsigned long)ts_ms,
                     ssid_esc,
                     wifi_auth_to_str(ap->authmode),
                     ch);
    if (n < 0 || (size_t)n >= out_len) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static void format_mac(const uint8_t addr[6], char *mac, size_t mac_len)
{
    snprintf(mac, mac_len, "%02x:%02x:%02x:%02x:%02x:%02x",
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

static esp_err_t encode_line(const char *type,
                             const char *mac,
                             int8_t rssi,
                             uint32_t ts_ms,
                             const char *f1,
                             const char *f2,
                             const char *f3,
                             char *out,
                             size_t out_len)
{
    char e1[TELEMETRY_FIELD_MAX * 2];
    char e2[TELEMETRY_FIELD_MAX * 2];
    char e3[TELEMETRY_FIELD_MAX * 2];
    json_escape(f1 ? f1 : "", e1, sizeof(e1));
    json_escape(f2 ? f2 : "", e2, sizeof(e2));
    json_escape(f3 ? f3 : "", e3, sizeof(e3));

    int n = snprintf(out, out_len,
                     "{\"type\":\"%s\",\"mac\":\"%s\",\"rssi\":%d,\"ts\":%lu,"
                     "\"field1\":\"%s\",\"field2\":\"%s\",\"field3\":\"%s\"}\n",
                     type, mac, (int)rssi, (unsigned long)ts_ms, e1, e2, e3);
    if (n < 0 || (size_t)n >= out_len) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t telemetry_encode_record(const device_record_t *rec,
                                  char *out,
                                  size_t out_len)
{
    if (rec == NULL || !rec->valid || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    char mac[18];
    format_mac(rec->addr, mac, sizeof(mac));

    char f1[TELEMETRY_FIELD_MAX];
    char f2[TELEMETRY_FIELD_MAX];
    char f3[TELEMETRY_FIELD_MAX];
    f1[0] = f2[0] = f3[0] = '\0';

    const char *type = NULL;
    uint32_t ts = rec->last_seen_ms;

    switch (rec->radio) {
    case SCAN_RADIO_BLE:
        type = "BLE";
        if (rec->name_len > 0) {
            strncpy(f1, rec->name, sizeof(f1) - 1);
        } else {
            strncpy(f1, "(no name)", sizeof(f1) - 1);
        }
        snprintf(f2, sizeof(f2), "0x%04x", (unsigned)rec->appearance);
        if (rec->mfg_data_len > 0) {
            snprintf(f3, sizeof(f3), "MFG_%04x", (unsigned)rec->mfg_company_id);
        } else {
            strncpy(f3, "ADV", sizeof(f3) - 1);
        }
        break;
    case SCAN_RADIO_CLASSIC:
        type = "CLASSIC";
        if (rec->name_len > 0) {
            strncpy(f1, rec->name, sizeof(f1) - 1);
        } else {
            strncpy(f1, "(no name)", sizeof(f1) - 1);
        }
        snprintf(f2, sizeof(f2), "0x%06lx", (unsigned long)rec->cod);
        strncpy(f3, "INQUIRY", sizeof(f3) - 1);
        break;
    case SCAN_RADIO_NRF24:
        type = "NRF";
        strncpy(f1, "nRF24", sizeof(f1) - 1);
        strncpy(f2, "-", sizeof(f2) - 1);
        strncpy(f3, "STUB", sizeof(f3) - 1);
        break;
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }

    return encode_line(type, mac, rec->rssi, ts, f1, f2, f3, out, out_len);
}
