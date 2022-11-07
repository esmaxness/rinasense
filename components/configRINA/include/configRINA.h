#ifndef CONFIG_RINA_H
#define CONFIG_RINA_H

/************* SHIM WIFI CONFIGURATION ***********/
#define SHIM_WIFI_MODULE (1) // Zero if not shim WiFi modules is required.

#define SHIM_PROCESS_NAME "wlan0.ue"
#define SHIM_PROCESS_INSTANCE "1"
#define SHIM_ENTITY_NAME ""
#define SHIM_ENTITY_INSTANCE ""

#define SHIM_DIF_NAME "WiFiTerminet" //"Irati"

#define SHIM_INTERFACE "ESP_WIFI_MODE_STA"

/************ SHIM DIF CONFIGURATION **************/
#define ESP_WIFI_SSID "irati"     //"WiFiTerminet"     // //"WS02"
#define ESP_WIFI_PASS "irati2017" //"20TrmnT22"    //"Esdla2025"

/*********** NORMAL CONFIGURATION ****************/

#define NORMAL_PROCESS_NAME "edge1.mobile" //"st2.slice1" // "ue1.mobile"
#define NORMAL_PROCESS_INSTANCE "1"
#define NORMAL_ENTITY_NAME ""
#define NORMAL_ENTITY_INSTANCE ""

#define NORMAL_DIF_NAME "mobile.DIF" //"slice1.DIF" //" mobile.DIF" //

/*********** NORMAL IPCP CONFIGURATION ****************/
/**** Known IPCProcess Address *****/
#define LOCAL_ADDRESS (1)
#define LOCAL_ADDRESS_AP_INSTANCE "1"
#define LOCAL_ADDRESS_AP_NAME "edge1.mobile" //"st2.slice1" //"ue1.mobile" //"st1.slice1"

#define REMOTE_ADDRESS (3) // 11
#define REMOTE_ADDRESS_AP_INSTANCE "1"
#define REMOTE_ADDRESS_AP_NAME "ar1.mobile" // "gw1.slice1" // ar1.mobile //edge1.slice1

/**** QoS CUBES ****/
#define QoS_CUBE_NAME "unreliable"
#define QoS_CUBE_ID (3)
#define QoS_CUBE_PARTIAL_DELIVERY pdFALSE
#define QoS_CUBE_ORDERED_DELIVERY pdTRUE

#define SIZE_SDU_QUEUE (16)

/**** EFCP POLICIES ****/
/* DTP POLICY SET */
#define DTP_POLICY_SET_NAME "default"
#define DTP_POLICY_SET_VERSION "0"

#define DTP_INITIAL_A_TIMER (300)
#define DTP_DTCP_PRESENT pdFALSE

#endif
