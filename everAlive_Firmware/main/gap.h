
#pragma once

#include "common.h"
#include "services/gap/ble_svc_gap.h"

#define BLE_GAP_APPEARANCE_GENERIC_TAG 0x0200
#define BLE_GAP_URI_PREFIX_HTTPS 0x17
#define BLE_GAP_LE_ROLE_PERIPHERAL 0x00

void gap_update_payload(const sensor_payload_t *payload);

int  gap_init(void);
void adv_init(void);