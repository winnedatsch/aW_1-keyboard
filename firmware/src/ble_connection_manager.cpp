#include "ble_connection_manager.h"

#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <bluetooth/gap.h>
#include <logging/log.h>
#include <settings/settings.h>

#include <algorithm>
#include <functional>
#include <memory>
LOG_MODULE_REGISTER(ble_conn_mgr);

namespace {
bt_conn *connection;
std::unique_ptr<bt_addr_le_t> paired_addr = nullptr;
const struct bt_data advertising_data[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0x12, 0x18, /* HID Service */
                  0x0f, 0x18),                    /* Battery Service */
};
const bt_le_adv_param advertising_parameters{
    .id = BT_ID_DEFAULT,
    .options = BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_USE_NAME,
    .interval_min = BT_GAP_ADV_FAST_INT_MIN_1,
    .interval_max = BT_GAP_ADV_FAST_INT_MAX_1};
std::function<void()> init_callback = nullptr;
static struct k_work advertise_work;

static int paired_settings_set(const char *name, size_t len, settings_read_cb read_cb,
                               void *cb_arg) {
    const char *next;

    if (settings_name_steq(name, "one", &next) && !next) {
        if (len != sizeof(paired_addr)) {
            return -EINVAL;
        }

        LOG_DBG("Reading paired device");

        bt_addr_le_t saved_addr;
        int err = read_cb(cb_arg, &saved_addr, sizeof(saved_addr));
        if (err >= 0) {
            paired_addr = std::make_unique<bt_addr_le_t>();
            paired_addr->a = saved_addr.a;
            paired_addr->type = saved_addr.type;
            return 0;
        } else {
            return err;
        }

        return err;
    }

    return -ENOENT;
}

struct settings_handler paired_conf = {.name = "paired", .h_set = paired_settings_set};

void start_advertising(struct k_work *work) {
    bt_le_adv_stop();
    LOG_INF("Attempting to start advertising ...");
    int adv_err = 0;

    if (paired_addr != nullptr) {
        char addr[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(paired_addr.get(), addr, sizeof(addr));
        LOG_INF("Paired device stored (%s), advertising with whitelist", log_strdup(addr));

        bt_le_adv_param directed_params = advertising_parameters;
        directed_params.options = directed_params.options | BT_LE_ADV_OPT_FILTER_CONN;

        if(bt_le_whitelist_clear()) {
            LOG_ERR("Error while clearing whitelist");
        }

        if(bt_le_whitelist_add(paired_addr.get())) {
            LOG_ERR("Error while adding paired device to whitelist");
        }

        adv_err = bt_le_adv_start(&advertising_parameters, advertising_data,
                                  ARRAY_SIZE(advertising_data), nullptr, 0);
    } else {
        LOG_INF("No paired device stored, advertising globally");
        adv_err = bt_le_adv_start(&advertising_parameters, advertising_data,
                                  ARRAY_SIZE(advertising_data), nullptr, 0);
    }

    if (adv_err) {
        LOG_ERR("Bluetooth adv failed to start");
        return;
    } else {
        LOG_INF("Advertising succeeded!");
    }
}

void ble_ready(int err) {
    if (err) {
        LOG_ERR("Bluetooth initialization failed!");
        return;
    }

    if (init_callback) {
        init_callback();
    }

    if (IS_ENABLED(CONFIG_SETTINGS)) {
        settings_load();
    }

    k_work_submit(&advertise_work);
}

void connected(struct bt_conn *conn, u8_t err) {
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (err) {
        LOG_ERR("Failed to connect to %s (%d)", log_strdup(addr), err);
        return;
    }

    LOG_INF("Connected %s", log_strdup(addr));
    connection = bt_conn_ref(conn);

    if (bt_conn_set_security(conn, BT_SECURITY_L2)) {
        LOG_ERR("Failed to set security");
    }
}

void disconnected(struct bt_conn *conn, u8_t reason) {
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Disconnected from %s (reason 0x%02x)", log_strdup(addr), reason);

    bt_conn_unref(connection);
    connection = nullptr;

    k_work_submit(&advertise_work);
}

void security_changed(struct bt_conn *conn, bt_security_t level, enum bt_security_err err) {
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (err) {
        LOG_ERR("An error occurred during security level change for %s to level %d (error 0x%02x)",
                log_strdup(addr), level, err);
    } else {
        LOG_INF("Security level for %s changed: level %d", log_strdup(addr), level);
    }
}

void le_param_updated(struct bt_conn *conn, u16_t interval, u16_t latency, u16_t timeout) {
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Connection parameters for %s changed: interval %d, latency %d, timeout %d",
            log_strdup(addr), interval, latency, timeout);
}

void pairing_complete(struct bt_conn *conn, bool bonded) {
    char addr[BT_ADDR_LE_STR_LEN];
    const bt_addr_le_t *central_addr = bt_conn_get_dst(conn);
    bt_addr_le_to_str(central_addr, addr, sizeof(addr));

    if (paired_addr != nullptr) {
        bt_unpair(BT_ID_DEFAULT, paired_addr.get());
    }
    settings_save_one("paired/one", central_addr, sizeof(central_addr));
    paired_addr = std::make_unique<bt_addr_le_t>();
    paired_addr->a = central_addr->a;
    paired_addr->type = central_addr->type;

    LOG_INF("Pairing for %s completed (bonded: %s)", log_strdup(addr), bonded ? "true" : "false");
}

void pairing_failed(struct bt_conn *conn, enum bt_security_err reason) {
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_ERR("Pairing for %s failed (error code: %d)", log_strdup(addr), reason);
}

struct bt_conn_cb conn_callbacks {
    .connected = connected, .disconnected = disconnected, .le_param_updated = le_param_updated,
    .security_changed = security_changed
};

struct bt_conn_auth_cb auth_callbacks {
    .passkey_display = NULL, .passkey_entry = NULL, .passkey_confirm = NULL, .cancel = NULL,
    .pairing_confirm = NULL, .pairing_complete = pairing_complete, .pairing_failed = pairing_failed
};
}  // namespace

void ble_init(std::function<void()> callback) {
    if (!init_callback) {
        k_work_init(&advertise_work, start_advertising);
        init_callback = callback;
    } else {
        LOG_INF("Bluetooth initialisation called multiple times. Skipping ... ");
        return;
    }

    if (!bt_enable(ble_ready)) {
        bt_conn_cb_register(&conn_callbacks);
        bt_conn_auth_cb_register(&auth_callbacks);
    } else {
        LOG_ERR("Bluetooth initialisation failed");
    }
}

struct settings_handler *get_paired_conf() {
    return &paired_conf;
}

bt_conn *ble_get_connection() { return connection; }

void reset_paired_device() {
    LOG_INF("Disconnecting and unpairing stored paired device");

    bt_unpair(BT_ID_DEFAULT, paired_addr.get());
    settings_delete("paired/one");
    paired_addr = nullptr;

    if (connection) {
        LOG_INF("Active connection was found. Disconnecting ...");
        bt_conn_disconnect(connection, 0x13);
    } else {
        k_work_submit(&advertise_work);
    }
}