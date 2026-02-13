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
/*  THREAD NETWORK (leave defaults to join via HA or CLI later)        */
/* ------------------------------------------------------------------ */

/* If you want the device to auto-form a network on first boot,
 * set this to 1.  If 0 the device waits for dataset provisioning
 * via the OpenThread CLI (serial console) or via HA.                  */
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
