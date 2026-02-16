/*
 * ESP32-C6 Standalone OpenThread Border Router
 *
 * This firmware turns a Seeed Studio XIAO ESP32-C6 into a standalone
 * Thread Border Router that integrates with Home Assistant.
 *
 * Hardware:  Seeed Studio XIAO ESP32-C6 (or any ESP32-C6 board)
 * Power:    USB-C (no data connection needed after flashing)
 * Backbone: Wi-Fi (connects to your home network)
 * Thread:   Native IEEE 802.15.4 radio
 *
 * Configuration is in config.h — edit before flashing each device.
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "mdns.h"
#include "driver/uart.h"

#include "esp_openthread.h"
#include "esp_openthread_border_router.h"
#include "esp_openthread_cli.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_netif_glue.h"
#include "esp_openthread_types.h"
#include "esp_vfs_eventfd.h"

#include "openthread/border_router.h"
#include "openthread/cli.h"
#include "openthread/dataset.h"
#include "openthread/dataset_ftd.h"
#include "openthread/instance.h"
#include "openthread/logging.h"
#include "openthread/platform/logging.h"
#include "openthread/tasklet.h"
#include "openthread/thread.h"

#include "config.h"

/* ------------------------------------------------------------------ */
/*  Constants & tags                                                    */
/* ------------------------------------------------------------------ */

static const char *TAG = DEVICE_NAME;

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

/* ------------------------------------------------------------------ */
/*  Globals                                                            */
/* ------------------------------------------------------------------ */

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_count = 0;

/* ------------------------------------------------------------------ */
/*  Wi-Fi event handler                                                */
/* ------------------------------------------------------------------ */

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (WIFI_MAX_RETRY == 0 || s_retry_count < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_count++;
            ESP_LOGW(TAG, "Wi-Fi disconnected, retrying... (%d)", s_retry_count);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "Wi-Fi connection failed after %d retries", s_retry_count);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/* ------------------------------------------------------------------ */
/*  Wi-Fi initialization (STA mode)                                    */
/* ------------------------------------------------------------------ */

static esp_netif_t *init_wifi(void)
{
    s_wifi_event_group = xEventGroupCreate();

    esp_netif_t *wifi_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Register event handlers */
    esp_event_handler_instance_t any_wifi_event;
    esp_event_handler_instance_t got_ip_event;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &any_wifi_event));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &got_ip_event));

    /* Configure Wi-Fi */
    wifi_config_t wifi_config = {
        .sta = {
            .ssid     = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to Wi-Fi SSID: %s ...", WIFI_SSID);

    /* Wait for connection */
    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE, pdFALSE,
        portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Wi-Fi connected successfully");
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Wi-Fi connection failed — rebooting in 5 s");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }

    return wifi_netif;
}

/* ------------------------------------------------------------------ */
/*  mDNS setup (for Home Assistant discovery)                          */
/* ------------------------------------------------------------------ */

static void init_mdns(void)
{
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(DEVICE_NAME));
    ESP_ERROR_CHECK(mdns_instance_name_set(MDNS_INSTANCE_NAME));

    /* Advertise the OTBR meshcop service — this is what HA looks for */
    mdns_txt_item_t meshcop_txt[] = {
        { "rv", "1" },
        { "dd", DEVICE_NAME },
        { "vn", "Espressif" },
        { "mn", "ESP32-C6 OTBR" },
    };

    ESP_ERROR_CHECK(mdns_service_add(
        DEVICE_NAME,         /* instance name  */
        "_meshcop",          /* service type   */
        "_udp",              /* protocol       */
        49191,               /* port           */
        meshcop_txt,         /* TXT records    */
        sizeof(meshcop_txt) / sizeof(meshcop_txt[0])));

    ESP_LOGI(TAG, "mDNS started: %s._meshcop._udp.local", DEVICE_NAME);
}

/* ------------------------------------------------------------------ */
/*  Hex string → byte array helper                                     */
/* ------------------------------------------------------------------ */

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int hex_to_bytes(const char *hex, uint8_t *out, size_t max_len)
{
    size_t hex_len = strlen(hex);
    if (hex_len == 0 || hex_len % 2 != 0) return -1;

    size_t byte_len = hex_len / 2;
    if (byte_len > max_len) return -1;

    for (size_t i = 0; i < byte_len; i++) {
        int hi = hex_nibble(hex[i * 2]);
        int lo = hex_nibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return -1;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return (int)byte_len;
}

/* ------------------------------------------------------------------ */
/*  OpenThread dataset helpers                                         */
/* ------------------------------------------------------------------ */

/**
 * Load a Thread active dataset from the hex TLV string in config.h.
 * Returns true if the dataset was successfully applied.
 */
static bool load_dataset_from_tlvs(otInstance *instance)
{
    const char *hex = THREAD_DATASET_TLVS;
    if (strlen(hex) == 0) return false;

    otOperationalDatasetTlvs tlvs;
    int len = hex_to_bytes(hex, tlvs.mTlvs, sizeof(tlvs.mTlvs));
    if (len < 0) {
        ESP_LOGE(TAG, "THREAD_DATASET_TLVS: invalid hex string");
        return false;
    }
    tlvs.mLength = (uint8_t)len;

    otError error = otDatasetSetActiveTlvs(instance, &tlvs);
    if (error != OT_ERROR_NONE) {
        ESP_LOGE(TAG, "Failed to set active dataset from TLVs: %d", error);
        return false;
    }

    ESP_LOGI(TAG, "Thread dataset loaded from config (%d bytes)", len);
    return true;
}

#if THREAD_AUTO_START
/**
 * Create a brand new Thread network (only used when no existing
 * dataset is available and THREAD_AUTO_START == 1).
 */
static void create_default_dataset(otInstance *instance)
{
    otOperationalDataset dataset;

    ESP_LOGI(TAG, "Creating new Thread network");

    memset(&dataset, 0, sizeof(dataset));

    otError error = otDatasetCreateNewNetwork(instance, &dataset);
    if (error != OT_ERROR_NONE) {
        ESP_LOGE(TAG, "Failed to create new network dataset: %d", error);
        return;
    }

    /* Override channel and network name with our config */
    dataset.mChannel = THREAD_CHANNEL;
    dataset.mComponents.mIsChannelPresent = true;

    size_t name_len = strlen(THREAD_NETWORK_NAME);
    if (name_len > OT_NETWORK_NAME_MAX_SIZE) {
        name_len = OT_NETWORK_NAME_MAX_SIZE;
    }
    memcpy(dataset.mNetworkName.m8, THREAD_NETWORK_NAME, name_len);
    dataset.mNetworkName.m8[name_len] = '\0';
    dataset.mComponents.mIsNetworkNamePresent = true;

    error = otDatasetSetActive(instance, &dataset);
    if (error != OT_ERROR_NONE) {
        ESP_LOGE(TAG, "Failed to set active dataset: %d", error);
    } else {
        ESP_LOGI(TAG, "New network created: ch=%d, name=%s",
                 THREAD_CHANNEL, THREAD_NETWORK_NAME);
    }
}
#endif /* THREAD_AUTO_START */

/* ------------------------------------------------------------------ */
/*  OpenThread state-change callback                                   */
/* ------------------------------------------------------------------ */

static void ot_state_change_callback(otChangedFlags flags, void *context)
{
    otInstance *instance = (otInstance *)context;

    if (flags & OT_CHANGED_THREAD_ROLE) {
        otDeviceRole role = otThreadGetDeviceRole(instance);
        const char *role_str;

        switch (role) {
        case OT_DEVICE_ROLE_DISABLED: role_str = "disabled"; break;
        case OT_DEVICE_ROLE_DETACHED: role_str = "detached"; break;
        case OT_DEVICE_ROLE_CHILD:    role_str = "child";    break;
        case OT_DEVICE_ROLE_ROUTER:   role_str = "router";   break;
        case OT_DEVICE_ROLE_LEADER:   role_str = "leader";   break;
        default:                      role_str = "unknown";  break;
        }

        ESP_LOGI(TAG, "Thread role changed: %s", role_str);
    }

    if (flags & OT_CHANGED_THREAD_NETDATA) {
        ESP_LOGI(TAG, "Thread network data updated");
    }
}

/* ------------------------------------------------------------------ */
/*  OpenThread main task                                               */
/* ------------------------------------------------------------------ */

static void ot_task(void *arg)
{
    esp_netif_t *wifi_netif = (esp_netif_t *)arg;

    /* OpenThread platform configuration for the native 802.15.4 radio */
    esp_openthread_platform_config_t ot_config = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config  = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config  = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };

    /* Initialize the OpenThread stack */
    ESP_ERROR_CHECK(esp_openthread_init(&ot_config));

    /* Get the OpenThread instance */
    otInstance *instance = esp_openthread_get_instance();

    /* Register state-change callback for logging */
    otSetStateChangedCallback(instance, ot_state_change_callback, instance);

#if OT_CLI_UART_ENABLE
    /* Enable the OpenThread CLI over USB serial for provisioning.
     * You can connect via serial monitor and type OT CLI commands
     * like:  dataset set active <hex>
     *        ifconfig up
     *        thread start                                          */
    esp_openthread_cli_init();
#endif

    /* Initialize the border router backbone (Wi-Fi interface) */
    ESP_ERROR_CHECK(esp_openthread_border_router_init(wifi_netif));

    ESP_LOGI(TAG, "OpenThread Border Router initialized");

    /* ----- Acquire the dataset and start Thread ----- */
    esp_openthread_lock_acquire(portMAX_DELAY);

    bool dataset_ready = false;
    otOperationalDataset dataset;

    /* Priority 1: Saved dataset in NVS (from a previous boot or CLI) */
    if (otDatasetGetActive(instance, &dataset) == OT_ERROR_NONE) {
        ESP_LOGI(TAG, "Using saved Thread dataset from NVS");
        dataset_ready = true;
    }
    /* Priority 2: Pre-provisioned TLV hex from config.h */
    else if (load_dataset_from_tlvs(instance)) {
        dataset_ready = true;
    }
#if THREAD_AUTO_START
    /* Priority 3: Create a brand new Thread network */
    if (!dataset_ready) {
        create_default_dataset(instance);
        dataset_ready = true;
    }
#endif

    if (dataset_ready) {
        otIp6SetEnabled(instance, true);
        otThreadSetEnabled(instance, true);
        ESP_LOGI(TAG, "Thread interface up — joining network...");
    } else {
        ESP_LOGI(TAG, "No Thread dataset configured");
        ESP_LOGI(TAG, "Provision via Home Assistant or serial CLI:");
        ESP_LOGI(TAG, "  > dataset set active <hex-TLV>");
        ESP_LOGI(TAG, "  > ifconfig up");
        ESP_LOGI(TAG, "  > thread start");
    }

    esp_openthread_lock_release();

    /* Main OpenThread run loop — this never returns */
    esp_openthread_cli_create_task();
    esp_openthread_launch_mainloop();

    /* Should never get here */
    esp_openthread_netif_glue_deinit();
    esp_vfs_eventfd_unregister();
    vTaskDelete(NULL);
}

/* ------------------------------------------------------------------ */
/*  Application entry point                                            */
/* ------------------------------------------------------------------ */

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  ESP32-C6 OpenThread Border Router");
    ESP_LOGI(TAG, "  Device: %s", DEVICE_NAME);
    ESP_LOGI(TAG, "========================================");

    /* --- NVS (required for Wi-Fi and OT dataset storage) --- */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* --- Event-fd (required by OpenThread platform layer) --- */
    esp_vfs_eventfd_config_t eventfd_config = {
        .max_fds = 4,
    };
    ESP_ERROR_CHECK(esp_vfs_eventfd_register(&eventfd_config));

    /* --- TCP/IP and event loop --- */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* --- Wi-Fi (backbone network) --- */
    esp_netif_t *wifi_netif = init_wifi();

    /* --- mDNS (Home Assistant discovery) --- */
    init_mdns();

    /* --- Launch the OpenThread task --- */
    xTaskCreate(ot_task, "ot_main", 20480, wifi_netif, 5, NULL);

    ESP_LOGI(TAG, "OTBR startup complete — %s is online", DEVICE_NAME);
}
