#include "ble_connection_manager.h"
#include <settings/settings.h>
#include <functional>
#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>

namespace {
    bt_conn *connection;
    const struct bt_data advertising_data[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
        BT_DATA_BYTES(BT_DATA_UUID16_ALL,
                0x12, 0x18, /* HID Service */
                0x0f, 0x18), /* Battery Service */
    };
    const bt_le_adv_param advertising_parameters {
        .id = BT_ID_DEFAULT,
        .options = BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_USE_NAME,
        .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
        .interval_max = BT_GAP_ADV_FAST_INT_MAX_2
    };
    std::function<void()> init_callback = nullptr;

    void ble_ready(int err) {
        if(err) {
            printk("Bluetooth initialization failed!\n");
            return;
        }

        if(init_callback) {
            init_callback();
        }

        if (IS_ENABLED(CONFIG_SETTINGS)) {
            settings_load();
        }
        
        printk("Attempting to start advertising ...\n");
        int adv_err = bt_le_adv_start(&advertising_parameters, advertising_data, ARRAY_SIZE(advertising_data), NULL, 0);

        if(adv_err) {
            printk("Bluetooth adv failed to start\n");
            return;
        } else {
            printk("Advertising succeeded!\n");
        }
    }

    void connected(struct bt_conn *conn, u8_t err) {
        char addr[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

        if (err) {
            printk("Failed to connect to %s (%u)\n", addr, err);
            return;
        }

        printk("Connected %s\n", addr);
        connection = bt_conn_ref(conn);

        if (bt_conn_set_security(conn, BT_SECURITY_L2)) {
            printk("Failed to set security\n");
        }
    }

    void disconnected(struct bt_conn *conn, u8_t reason) {
        char addr[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

        printk("Disconnected from %s (reason 0x%02x)\n", addr, reason);

        bt_conn_unref(connection);
        connection = nullptr;
    }

    void security_changed(struct bt_conn *conn, bt_security_t level, enum bt_security_err err) {
        char addr[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

        if(err) {
             printk("An error occurred during security level change for %s to level %d (error 0x%02x)\n", addr, level, err);
        } else {
            printk("Security level for %s changed: level %d\n", addr, level);
        }
    }

    void le_param_updated(struct bt_conn *conn, u16_t interval, u16_t latency, u16_t timeout) {
        char addr[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

        printk("Connection parameters for %s changed: interval %d, latency %d, timeout %d\n", addr, interval, latency, timeout);
    }

    void pairing_complete(struct bt_conn *conn, bool bonded) {
        char addr[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

        printk("Pairing for %s completed (bonded: %s)\n", addr, bonded ? "true" : "false");
    }

    void pairing_failed(struct bt_conn *conn, enum bt_security_err reason) {
        char addr[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

        printk("Pairing for %s failed (error code: %d)\n", addr, reason);
    }

    struct bt_conn_cb conn_callbacks {
        .connected = connected,
        .disconnected = disconnected,
        .le_param_updated = le_param_updated,
        .security_changed = security_changed
    };

    struct bt_conn_auth_cb auth_callbacks {
        .passkey_display = NULL,
        .passkey_entry = NULL,
        .passkey_confirm = NULL,
        .cancel = NULL,
        .pairing_confirm = NULL,
        .pairing_complete = pairing_complete,
        .pairing_failed = pairing_failed
    };
}

void ble_init(std::function<void()> callback) {
    if(!init_callback) {
        init_callback = callback;
    } else {
        printk("Bluetooth initialisation called multiple times. Skipping ... \n");
        return;
    }

    if(!bt_enable(ble_ready)) {
		bt_conn_cb_register(&conn_callbacks);
        bt_conn_auth_cb_register(&auth_callbacks);
	} else {
        printk("Bluetooth initialisation failed\n");
    }
}

bt_conn * ble_get_connection() {
    return connection;
}