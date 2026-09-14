#include "ble_service.h"
#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "dashboard.h"
#include <stdio.h>
#include <string.h>

#define BLE_DEVICE_NAME "KriSHE-Carbon"
#define BLE_SVC_UUID_16  0xFFE0
#define BLE_CHR_UUID_16  0xFFE1

static const char *TAG = "BLE_SVC";
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_char_val_handle = 0;
static bool s_notify_enabled = false;
static uint8_t s_own_addr_type;

static int ble_gap_event(struct ble_gap_event *event, void *arg);

static int ble_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        device_state_t st = data_model_get_snapshot();
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "{\"state\":\"%s\",\"top\":%.2f,\"mid\":%.2f,\"bot\":%.2f,\"top_v\":%d,\"mid_v\":%d,\"bot_v\":%d,\"lat\":%.6f,\"lon\":%.6f,\"sat\":%u,\"utc\":%lld,\"rate\":%.2f,\"batch_id\":\"%s\",\"duration\":%lu,\"up\":%lu}",
                 kiln_state_to_str(st.kiln_state),
                 st.top_c, st.middle_c, st.bottom_c,
                 st.top_valid ? 1 : 0, st.middle_valid ? 1 : 0, st.bottom_valid ? 1 : 0,
                 st.latitude, st.longitude,
                 (unsigned int)st.satellites,
                 (long long)st.utc_epoch,
                 st.top_rate,
                 st.batch_id,
                 (unsigned long)st.session_duration_s,
                 (unsigned long)st.uptime_s);
        os_mbuf_append(ctxt->om, buf, strlen(buf));
        return 0;
    }
    return 0;
}

static const struct ble_gatt_svc_def s_gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_SVC_UUID_16),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = BLE_UUID16_DECLARE(BLE_CHR_UUID_16),
                .access_cb = ble_chr_access,
                .val_handle = &s_char_val_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
            },
            {
                0, /* End of characteristics */
            }
        },
    },
    {
        0, /* End of services */
    },
};

static void ble_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    const char *name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    ble_uuid16_t uuids16[] = { BLE_UUID16_INIT(BLE_SVC_UUID_16) };
    fields.uuids16 = uuids16;
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error setting advertisement fields; rc=%d", rc);
        return;
    }

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error starting advertisement; rc=%d", rc);
        return;
    }
    ESP_LOGI(TAG, "BLE advertising started: %s (Service: 0x%04X)", BLE_DEVICE_NAME, BLE_SVC_UUID_16);
}

static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "BLE Connect: status=%d", event->connect.status);
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            /* Turn off Wi-Fi SoftAP and web server to reduce heat and save power while app is connected */
            wifi_stop_softap();
        } else {
            ble_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "BLE Disconnect; reason=%d", event->disconnect.reason);
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        s_notify_enabled = false;
        /* Turn Wi-Fi back on when app disconnects so web dashboard remains accessible */
        wifi_resume_softap();
        ble_advertise();
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "BLE Subscribe: handle=%d, cur_notify=%d",
                 event->subscribe.attr_handle, event->subscribe.cur_notify);
        if (event->subscribe.attr_handle == s_char_val_handle) {
            s_notify_enabled = event->subscribe.cur_notify;
        }
        return 0;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "BLE MTU update: conn_handle=%d, mtu=%d",
                 event->mtu.conn_handle, event->mtu.value);
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "BLE Adv complete; restarting");
        ble_advertise();
        return 0;
    }
    return 0;
}

static void on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to ensure address; rc=%d", rc);
        return;
    }
    rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to infer own addr type; rc=%d", rc);
        return;
    }
    ble_advertise();
}

static void ble_host_task(void *param)
{
    ESP_LOGI(TAG, "NimBLE host task started on core %d", xPortGetCoreID());
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t ble_service_init(void)
{
    ESP_LOGI(TAG, "Initializing NimBLE peripheral subsystem...");

    int rc = nimble_port_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to initialize NimBLE port: %d", rc);
        return ESP_FAIL;
    }

    ble_hs_cfg.sync_cb = on_sync;

    rc = ble_svc_gap_device_name_set(BLE_DEVICE_NAME);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set device name: %d", rc);
        return ESP_FAIL;
    }

    ble_svc_gap_init();
    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(s_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed: %d", rc);
        return ESP_FAIL;
    }

    rc = ble_gatts_add_svcs(s_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed: %d", rc);
        return ESP_FAIL;
    }

    nimble_port_freertos_init(ble_host_task);
    ESP_LOGI(TAG, "NimBLE peripheral initialized successfully");
    return ESP_OK;
}

void ble_service_notify_state(const device_state_t *st)
{
    if (!st || s_conn_handle == BLE_HS_CONN_HANDLE_NONE || !s_notify_enabled) {
        return;
    }

    char buf[256];
    int len = snprintf(buf, sizeof(buf),
             "{\"state\":\"%s\",\"top\":%.2f,\"mid\":%.2f,\"bot\":%.2f,\"top_v\":%d,\"mid_v\":%d,\"bot_v\":%d,\"lat\":%.6f,\"lon\":%.6f,\"sat\":%u,\"utc\":%lld,\"rate\":%.2f,\"batch_id\":\"%s\",\"duration\":%lu,\"up\":%lu}",
             kiln_state_to_str(st->kiln_state),
             st->top_c, st->middle_c, st->bottom_c,
             st->top_valid ? 1 : 0, st->middle_valid ? 1 : 0, st->bottom_valid ? 1 : 0,
             st->latitude, st->longitude,
             (unsigned int)st->satellites,
             (long long)st->utc_epoch,
             st->top_rate,
             st->batch_id,
             (unsigned long)st->session_duration_s,
             (unsigned long)st->uptime_s);

    if (len > 0) {
        struct os_mbuf *om = ble_hs_mbuf_from_flat(buf, len);
        if (om) {
            ble_gatts_notify_custom(s_conn_handle, s_char_val_handle, om);
        }
    }
}

bool ble_service_is_connected(void)
{
    return (s_conn_handle != BLE_HS_CONN_HANDLE_NONE);
}
