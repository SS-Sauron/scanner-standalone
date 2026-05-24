#include "telemetry_emit.h"

#include "telemetry_encode.h"
#include "telemetry_proto.h"
#include "telemetry_uart.h"

void telemetry_emit_record(const device_record_t *rec)
{
    if (rec == NULL || !rec->valid) {
        return;
    }

    char line[TELEMETRY_LINE_MAX];
    if (telemetry_encode_record(rec, line, sizeof(line)) != ESP_OK) {
        return;
    }
    telemetry_uart_write_line(line);
}
