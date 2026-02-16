/*
 * ESP32-C6 OpenThread Border Router — User Configuration
 *
 * Edit the values below for each device you flash.
 * Each OTBR on your network should have a UNIQUE device name.
 */

#ifndef CONFIG_H
#define CONFIG_H

/* ------------------------------------------------------------------ */
/*  DEVICE IDENTITY                                                    */
/* ------------------------------------------------------------------ */

/* Unique name for this border router (used in mDNS / HA discovery).
 * Examples: "otbr-living-room", "otbr-garage", "otbr-upstairs"       */
#define DEVICE_NAME             "otbr-01"

/* ------------------------------------------------------------------ */
/*  WI-FI CREDENTIALS                                                  */
/* ------------------------------------------------------------------ */

#define WIFI_SSID               "YOUR_WIFI_SSID"
#define WIFI_PASSWORD           "YOUR_WIFI_PASSWORD"

/* Maximum connection retries before reboot (0 = retry forever)        */
#define WIFI_MAX_RETRY          0

/* ------------------------------------------------------------------ */
/*  JOIN AN EXISTING THREAD NETWORK                                    */
/* ------------------------------------------------------------------ */

/* To join an existing Thread network, paste the dataset TLV hex here.
 *
 * How to get this string:
 *   Home Assistant → Settings → Devices & Services → Thread
 *     → your network → copy the "Active Operational Dataset" hex.
 *   Or from another OTBR's CLI:  dataset active -x
 *
 * Example (yours will be different):
 *   #define THREAD_DATASET_TLVS  "0e080000000000010000..."
 *
 * Leave empty ("") to provision later via CLI or Home Assistant.      */
#define THREAD_DATASET_TLVS     ""

/* ------------------------------------------------------------------ */
/*  CREATE A NEW THREAD NETWORK (only if not joining an existing one)  */
/* ------------------------------------------------------------------ */

/* If THREAD_DATASET_TLVS is empty AND no saved dataset exists in NVS,
 * setting this to 1 will auto-create a brand new Thread network.
 * Usually you want this set to 0 so the device waits for credentials. */
#define THREAD_AUTO_START       0

/* Default Thread channel (only used when THREAD_AUTO_START == 1)      */
#define THREAD_CHANNEL          15

/* Default Thread network name (only used when THREAD_AUTO_START == 1) */
#define THREAD_NETWORK_NAME     "OpenThread-HA"

/* ------------------------------------------------------------------ */
/*  ADVANCED / OPTIONAL                                                */
/* ------------------------------------------------------------------ */

/* OpenThread CLI over USB serial — handy for provisioning             */
#define OT_CLI_UART_ENABLE      1

/* mDNS instance name (used for HA discovery, derived from DEVICE_NAME)*/
#define MDNS_INSTANCE_NAME      DEVICE_NAME

#endif /* CONFIG_H */
