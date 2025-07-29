/*
 * bleprph.c
 *
 *  Created on: 23 июн. 2025 г.
 *      Author: katev
 */

#include "esp_log.h"
#include "esp_log_timestamp.h"
#include "nvs_flash.h"
/* BLE */
#include "bleprph.h"
#include "console/console.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"

/* WIFI */
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "ping/ping_sock.h"
#include <string.h>

#include "lwip/err.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

#include "networkInterface.h"

#include "gatt_svr.h"

/*
You should define in sdkconfig this parameters:

 CONFIG_ESP_WIFI_SSID
 CONFIG_ESP_WIFI_PASSWORD
 CONFIG_ESP_MAXIMUM_RETRY
 CONFIG_ESP_PING_IP
 CONFIG_ESP_PING_COUNT

 */
// #define CONFIG_ESP_WIFI_SSID_ "GTI - Guest"
//  #define CONFIG_ESP_WIFI_PASSWORD_  "Staffordguest"

//  #define CONFIG_ESP_WIFI_SSID 		"Verizon-RC400L-EA",
//  #define CONFIG_ESP_WIFI_PASSWORD   "44424f4f",

//#define CONFIG_ESP_WIFI_SSID		"WiFi-DOM.ru-8698"
//#define CONFIG_ESP_WIFI_PASSWORD   "cjbavXX72K"

#define EXAMPLE_PING_INTERVAL 1

extern TaskHandle_t internet_task_handle;
// extern bool send_wifi;
extern bool telemetry_notify_enabled;
static int bleprph_gap_event(struct ble_gap_event *event, void *arg);
static uint8_t own_addr_type;
uint8_t mac[6];

// wifi new
esp_err_t connect_to_wifi(const char *ssid, const char *password);
void wifi_scan(wifi_ap_record_t *wifi_list, uint16_t *ap_count);
esp_err_t get_password_by_ssid(const char *ssid, char *password_out);
void tets_nvs(void);

/* FreeRTOS event group to signal when we are connected*/
EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about
 * two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#include "networkInterface.h"

static const char *TAG = "wifi_prph_coex";

static int s_retry_num = 0;

static void event_handler(void *arg, esp_event_base_t event_base,
						  int32_t event_id, void *event_data) {
	ESP_LOGW(TAG, "event_handler called");
	ESP_LOGD(TAG, "event_base: %s, event_id: %ld", event_base, event_id);

	if (event_base == WIFI_EVENT) {
		switch (event_id) {
		case WIFI_EVENT_STA_START:
			ESP_LOGI(TAG, "Wi-Fi started, connecting to AP...");
			esp_wifi_connect();
			break;

		case WIFI_EVENT_STA_DISCONNECTED:
			ESP_LOGW(TAG, "Wi-Fi disconnected");

			// Очистить бит соединения
			xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

			if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY) {
				esp_wifi_connect();
				s_retry_num++;
				ESP_LOGI(TAG, "Retrying to connect to the AP (attempt %d)",
						 s_retry_num);
			} else {
				ESP_LOGE(TAG, "Max retries reached. Setting FAIL bit.");
				xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
			}
			break;

		default:
			ESP_LOGW(TAG, "Unhandled WIFI_EVENT id: %ld", event_id);
			break;
		}
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

		ESP_LOGI(TAG, "Got IP Address: " IPSTR, IP2STR(&event->ip_info.ip));

		s_retry_num = 0;

		// Очистить бит ошибки, установить бит подключения
		xEventGroupClearBits(s_wifi_event_group, WIFI_FAIL_BIT);
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

void wifi_init_sta(void) {
	s_wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_netif_init());

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(
		WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(
		IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

	wifi_config_t wifi_config = {
		.sta =
			{
				.ssid = CONFIG_ESP_WIFI_SSID,
				.password = CONFIG_ESP_WIFI_PASSWORD,
				/* Setting a password implies station will connect to all
				 * security modes including WEP/WPA. However these modes are
				 * deprecated and not advisable to be used. In case your Access
				 * point doesn't support WPA2, these mode can be enabled by
				 * commenting below line */
				.threshold.authmode = WIFI_AUTH_WPA2_PSK,
			},
	};
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	esp_wifi_get_mac(WIFI_MODE_STA, mac);
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI(TAG, "wifi_init_sta finished.");

	/* Waiting until either the connection is established (WIFI_CONNECTED_BIT)
	 * or connection failed for the maximum number of re-tries (WIFI_FAIL_BIT).
	 * The bits are set by event_handler() (see above) */
	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
										   WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
										   pdFALSE, pdFALSE, portMAX_DELAY);

	/* xEventGroupWaitBits() returns the bits before the call returned, hence we
	 * can test which event actually happened. */
	if (bits & WIFI_CONNECTED_BIT) {
		ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
				 CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
	} else if (bits & WIFI_FAIL_BIT) {
		ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
				 CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
	} else {
		ESP_LOGE(TAG, "UNEXPECTED EVENT");
	}

	/* The event will not be processed after unregister */
	/*   ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT,
	   IP_EVENT_STA_GOT_IP, instance_got_ip));
	   ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT,
	   ESP_EVENT_ANY_ID, instance_any_id));
	   vEventGroupDelete(s_wifi_event_group);*/
}
#ifdef ping_test
static void cmd_ping_on_ping_success(esp_ping_handle_t hdl, void *args) {
	uint8_t ttl;
	uint16_t seqno;
	uint32_t elapsed_time, recv_len;
	ip_addr_t target_addr;
	esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
	esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
	esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr,
						 sizeof(target_addr));
	esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
	esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time,
						 sizeof(elapsed_time));
	printf("%" PRIu32 "bytes from %s icmp_seq=%d ttl=%d time=%" PRIu32 "ms\n",
		   recv_len, inet_ntoa(target_addr.u_addr.ip4), seqno, ttl,
		   elapsed_time);
}

static void cmd_ping_on_ping_timeout(esp_ping_handle_t hdl, void *args) {
	uint16_t seqno;
	ip_addr_t target_addr;
	esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
	esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr,
						 sizeof(target_addr));
	printf("From %s icmp_seq=%d timeout\n", inet_ntoa(target_addr.u_addr.ip4),
		   seqno);
}

static void cmd_ping_on_ping_end(esp_ping_handle_t hdl, void *args) {
	ip_addr_t target_addr;
	uint32_t transmitted;
	uint32_t received;
	uint32_t total_time_ms;
	esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted,
						 sizeof(transmitted));
	esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
	esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr,
						 sizeof(target_addr));
	esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms,
						 sizeof(total_time_ms));
	uint32_t loss = (uint32_t)((1 - ((float)received) / transmitted) * 100);
	if (IP_IS_V4(&target_addr)) {
		printf("\n--- %s ping statistics ---\n",
			   inet_ntoa(*ip_2_ip4(&target_addr)));
	} else {
		printf("\n--- %s ping statistics ---\n",
			   inet6_ntoa(*ip_2_ip6(&target_addr)));
	}
	printf("%" PRIu32 "packets transmitted, %" PRIu32 " received, %" PRIu32
		   "%% packet loss, time %" PRIu32 "ms\n",
		   transmitted, received, loss, total_time_ms);
	// delete the ping sessions, so that we clean up all resources and can
	// create a new ping session we don't have to call delete function in the
	// callback, instead we can call delete function from other tasks
	esp_ping_delete_session(hdl);
}

static int do_ping_cmd(void) {
	esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
	static esp_ping_handle_t ping;

	config.interval_ms = (uint32_t)(EXAMPLE_PING_INTERVAL * 1000);
	config.count = (uint32_t)(CONFIG_ESP_PING_COUNT);

	// parse IP address
	ip_addr_t target_addr;
	struct addrinfo hint;
	struct addrinfo *res = NULL;
	memset(&hint, 0, sizeof(hint));
	memset(&target_addr, 0, sizeof(target_addr));

	/* convert domain name to IP address */
	if (getaddrinfo(CONFIG_ESP_PING_IP, NULL, &hint, &res) != 0) {
		printf("ping: unknown host %s\n", CONFIG_ESP_PING_IP);
		return 1;
	}
	if (res->ai_family == AF_INET) {
		struct in_addr addr4 = ((struct sockaddr_in *)(res->ai_addr))->sin_addr;
		inet_addr_to_ip4addr(ip_2_ip4(&target_addr), &addr4);
	} else {
		struct in6_addr addr6 =
			((struct sockaddr_in6 *)(res->ai_addr))->sin6_addr;
		inet6_addr_to_ip6addr(ip_2_ip6(&target_addr), &addr6);
	}
	freeaddrinfo(res);
	config.target_addr = target_addr;

	/* set callback functions */
	esp_ping_callbacks_t cbs = {.on_ping_success = cmd_ping_on_ping_success,
								.on_ping_timeout = cmd_ping_on_ping_timeout,
								.on_ping_end = cmd_ping_on_ping_end,
								.cb_args = NULL};

	esp_ping_new_session(&config, &cbs, &ping);
	esp_ping_start(ping);

	return 0;
}
#endif
void ble_store_config_init(void);

/**
 * Logs information about a connection to the console.
 */
static void bleprph_print_conn_desc(struct ble_gap_conn_desc *desc) {
	ESP_LOGI(TAG,
			 "handle=%d our_ota_addr_type=%d "
			 "our_ota_addr=%02x:%02x:%02x:%02x:%02x:%02x",
			 desc->conn_handle, desc->our_ota_addr.type,
			 desc->our_ota_addr.val[5], desc->our_ota_addr.val[4],
			 desc->our_ota_addr.val[3], desc->our_ota_addr.val[2],
			 desc->our_ota_addr.val[1], desc->our_ota_addr.val[0]);

	ESP_LOGI(TAG,
			 "our_id_addr_type=%d our_id_addr=%02x:%02x:%02x:%02x:%02x:%02x",
			 desc->our_id_addr.type, desc->our_id_addr.val[5],
			 desc->our_id_addr.val[4], desc->our_id_addr.val[3],
			 desc->our_id_addr.val[2], desc->our_id_addr.val[1],
			 desc->our_id_addr.val[0]);

	ESP_LOGI(
		TAG,
		"peer_ota_addr_type=%d peer_ota_addr=%02x:%02x:%02x:%02x:%02x:%02x",
		desc->peer_ota_addr.type, desc->peer_ota_addr.val[5],
		desc->peer_ota_addr.val[4], desc->peer_ota_addr.val[3],
		desc->peer_ota_addr.val[2], desc->peer_ota_addr.val[1],
		desc->peer_ota_addr.val[0]);

	ESP_LOGI(TAG,
			 "peer_id_addr_type=%d peer_id_addr=%02x:%02x:%02x:%02x:%02x:%02x",
			 desc->peer_id_addr.type, desc->peer_id_addr.val[5],
			 desc->peer_id_addr.val[4], desc->peer_id_addr.val[3],
			 desc->peer_id_addr.val[2], desc->peer_id_addr.val[1],
			 desc->peer_id_addr.val[0]);

	ESP_LOGI(TAG,
			 "conn_itvl=%d conn_latency=%d supervision_timeout=%d "
			 "encrypted=%d authenticated=%d bonded=%d",
			 desc->conn_itvl, desc->conn_latency, desc->supervision_timeout,
			 desc->sec_state.encrypted, desc->sec_state.authenticated,
			 desc->sec_state.bonded);
}

/**
 * Enables advertising with the following parameters:
 *     o General discoverable mode.
 *     o Undirected connectable mode.
 */
static void bleprph_advertise(void) {
	struct ble_gap_adv_params adv_params;
	struct ble_hs_adv_fields fields;
	const char *name;
	int rc;

	/**
	 *  Set the advertisement data included in our advertisements:
	 *     o Flags (indicates advertisement type and other general info).
	 *     o Advertising tx power.
	 *     o Device name.
	 *     o 16-bit service UUIDs (alert notifications).
	 */

	memset(&fields, 0, sizeof fields);

	/* Advertise two flags:
	 *     o Discoverability in forthcoming advertisement (general)
	 *     o BLE-only (BR/EDR unsupported).
	 */
	fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

	/* Indicate that the TX power level field should be included; have the
	 * stack fill this value automatically.  This is done by assigning the
	 * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
	 */
	fields.tx_pwr_lvl_is_present = 1;
	fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

	name = ble_svc_gap_device_name();
	fields.name = (uint8_t *)name;
	fields.name_len = strlen(name);
	fields.name_is_complete = 1;

	fields.uuids16 = (ble_uuid16_t[]){BLE_UUID16_INIT(GATT_SVR_SVC_ALERT_UUID)};
	fields.num_uuids16 = 1;
	fields.uuids16_is_complete = 1;

	rc = ble_gap_adv_set_fields(&fields);
	if (rc != 0) {
		ESP_LOGE(TAG, "error setting advertisement data; rc=%d", rc);
		return;
	}

	/* Begin advertising. */
	memset(&adv_params, 0, sizeof adv_params);
	adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
	adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
	rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
						   bleprph_gap_event, NULL);
	if (rc != 0) {
		ESP_LOGE(TAG, "error enabling advertisement; rc=%d", rc);
		return;
	}
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that forms.
 * bleprph uses the same callback for all connections.
 *
 * @param event                 The type of event being signalled.
 * @param ctxt                  Various information pertaining to the event.
 * @param arg                   Application-specified argument; unused by
 *                                  bleprph.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */

extern uint16_t telemetry_conn_handle;
static int bleprph_gap_event(struct ble_gap_event *event, void *arg) {
	struct ble_gap_conn_desc desc;
	int rc;

	switch (event->type) {
	case BLE_GAP_EVENT_LINK_ESTAB:
		/* A new connection was established or a connection attempt failed. */
		ESP_LOGI(TAG, "connection %s; status=%d ",
				 event->connect.status == 0 ? "established" : "failed",
				 event->connect.status);
		if (event->connect.status == 0) {
			rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
			assert(rc == 0);
			bleprph_print_conn_desc(&desc);
			telemetry_conn_handle = event->connect.conn_handle;
		}

		if (event->connect.status != 0) {
			/* Connection failed; resume advertising. */
			bleprph_advertise();
			telemetry_conn_handle = BLE_HS_CONN_HANDLE_NONE;
		}
		return 0;

	case BLE_GAP_EVENT_DISCONNECT:
		ESP_LOGI(TAG, "disconnect; reason=%d ", event->disconnect.reason);
		bleprph_print_conn_desc(&event->disconnect.conn);
		telemetry_conn_handle = BLE_HS_CONN_HANDLE_NONE;

		/* Connection terminated; resume advertising. */
		bleprph_advertise();
		return 0;

	case BLE_GAP_EVENT_CONN_UPDATE:
		/* The central has updated the connection parameters. */
		ESP_LOGI(TAG, "connection updated; status=%d ",
				 event->conn_update.status);
		rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
		assert(rc == 0);
		bleprph_print_conn_desc(&desc);
		return 0;

	case BLE_GAP_EVENT_ADV_COMPLETE:
		ESP_LOGI(TAG, "advertise complete; reason=%d",
				 event->adv_complete.reason);
		bleprph_advertise();
		return 0;

	case BLE_GAP_EVENT_ENC_CHANGE:
		/* Encryption has been enabled or disabled for this connection. */
		ESP_LOGI(TAG, "encryption change event; status=%d ",
				 event->enc_change.status);
		rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
		assert(rc == 0);
		bleprph_print_conn_desc(&desc);
		return 0;

	case BLE_GAP_EVENT_SUBSCRIBE:
		ESP_LOGI(TAG,
				 "subscribe event; conn_handle=%d attr_handle=%d "
				 "reason=%d prevn=%d curn=%d previ=%d curi=%d",
				 event->subscribe.conn_handle, event->subscribe.attr_handle,
				 event->subscribe.reason, event->subscribe.prev_notify,
				 event->subscribe.cur_notify, event->subscribe.prev_indicate,
				 event->subscribe.cur_indicate);

		if (event->subscribe.cur_notify) {
			telemetry_notify_enabled = true;
		} else {
			telemetry_notify_enabled = false;
		}

		return 0;

	case BLE_GAP_EVENT_MTU:
		ESP_LOGI(TAG, "mtu update event; conn_handle=%d cid=%d mtu=%d",
				 event->mtu.conn_handle, event->mtu.channel_id,
				 event->mtu.value);
		return 0;

	case BLE_GAP_EVENT_REPEAT_PAIRING:
		/* We already have a bond with the peer, but it is attempting to
		 * establish a new secure link.  This app sacrifices security for
		 * convenience: just throw away the old bond and accept the new link.
		 */

		/* Delete the old bond. */
		rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
		assert(rc == 0);
		ble_store_util_delete_peer(&desc.peer_id_addr);

		/* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
		 * continue with the pairing operation.
		 */
		return BLE_GAP_REPEAT_PAIRING_RETRY;
	}

	return 0;
}

static void bleprph_on_reset(int reason) {
	ESP_LOGE(TAG, "Resetting state; reason=%d", reason);
}

static void bleprph_on_sync(void) {
	int rc;

	rc = ble_hs_util_ensure_addr(0);
	assert(rc == 0);

	/* Figure out address to use while advertising (no privacy for now) */
	rc = ble_hs_id_infer_auto(0, &own_addr_type);
	if (rc != 0) {
		ESP_LOGE(TAG, "error determining address type; rc=%d", rc);
		return;
	}

	/* Printing ADDR */
	uint8_t addr_val[6] = {0};
	rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);

	ESP_LOGI(TAG, "Device Address:%02x:%02x:%02x:%02x:%02x:%02x", addr_val[5],
			 addr_val[4], addr_val[3], addr_val[2], addr_val[1], addr_val[0]);
	/* Begin advertising. */
	bleprph_advertise();
}

void bleprph_host_task(void *param) {
	ESP_LOGI(TAG, "BLE Host Task Started");
	/* This function will return only when nimble_port_stop() is executed */
	nimble_port_run();

	nimble_port_freertos_deinit();
}

TaskHandle_t ble_task_handle = NULL;
void send_ble_task(void *arg) {
	telemetry_notify();
	ble_task_handle = NULL;
	vTaskDelete(NULL);
}

void run_ble_task(void) {
	if (ble_task_handle == NULL) {
		BaseType_t xReturned = xTaskCreate(&send_ble_task, "ble_sender_task",
										   4096, NULL, 5, &ble_task_handle);
		if (xReturned != pdPASS) {
			ESP_LOGE(TAG, "Failed to create BLE send_ble_task");
		}
	}
}

TaskHandle_t check_wifi_task_handle = NULL;

void check_wifi_task(void *arg) {
	wifi_ap_record_t ap_info;
	uint16_t ap_count = 0;

	while (1) {
		EventBits_t bits =
			xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE,
								pdTRUE, pdMS_TO_TICKS(3000));

		if (!(bits & WIFI_CONNECTED_BIT)) {
			ESP_LOGW(TAG, "WiFi disconnected, trying to reconnect...");

			// Настроим точечное сканирование по SSID
			wifi_scan_config_t scan_config = {
				.ssid = (const uint8_t *)CONFIG_ESP_WIFI_SSID,
				.bssid = NULL,
				.channel = 0,
				.show_hidden = true,
				.scan_type = WIFI_SCAN_TYPE_ACTIVE,
				.scan_time = {.active = {.min = 100, .max = 300}}};

			if (esp_wifi_scan_start(&scan_config, true) == ESP_OK &&
				esp_wifi_scan_get_ap_num(&ap_count) == ESP_OK && ap_count > 0) {

				ESP_LOGI(TAG, "Target network '%s' found. Reconnecting...",
						 CONFIG_ESP_WIFI_SSID);

				wifi_config_t wifi_config = {0};
				strcpy((char *)wifi_config.sta.ssid, CONFIG_ESP_WIFI_SSID);
				strcpy((char *)wifi_config.sta.password,
					   CONFIG_ESP_WIFI_PASSWORD);

				esp_wifi_disconnect(); // отключим текущую попытку (если есть)
				esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
				esp_wifi_connect();

			} else {
				ESP_LOGE(TAG, "Target SSID '%s' not found",
						 CONFIG_ESP_WIFI_SSID);
			}
		} else {
			ESP_LOGD(TAG, "WiFi still connected");
		}

		vTaskDelay(pdMS_TO_TICKS(5000)); // ждать перед следующей проверкой
	}
}

void check_wifi() {
	if (check_wifi_task_handle == NULL) {
		BaseType_t xReturned = xTaskCreate(&check_wifi_task, "check_wifi_task",
										   4096, NULL, 5, &ble_task_handle);
		if (xReturned != pdPASS) {
			ESP_LOGE(TAG, "Failed to create BLE send_ble_task");
		}
	}
}

#define WIFI_NAMESPACE "wifi_nvs"
#define WIFI_KEY_SSID "ssid"
#define WIFI_KEY_PASS "pass"
#define PASSWORD_MAXLEN 32

esp_err_t save_wifi_credentials(const char *ssid, const char *password) {
	nvs_handle_t nvs;
	esp_err_t err = nvs_open(WIFI_NAMESPACE, NVS_READWRITE, &nvs);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Ошибка открытия NVS: %s", esp_err_to_name(err));
		return err;
	}

	err = nvs_set_str(nvs, WIFI_KEY_SSID, ssid);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Ошибка записи SSID: %s", esp_err_to_name(err));
		nvs_close(nvs);
		return err;
	}

	err = nvs_set_str(nvs, WIFI_KEY_PASS, password);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Ошибка записи пароля: %s", esp_err_to_name(err));
		nvs_close(nvs);
		return err;
	}

	err = nvs_commit(nvs);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Ошибка коммита NVS: %s", esp_err_to_name(err));
	} else {
		ESP_LOGI(TAG, "Wi-Fi данные успешно сохранены");
	}

	nvs_close(nvs);
	return err;
}

// Загрузка SSID и пароля из NVS
esp_err_t load_wifi_credentials(char *ssid_out, size_t ssid_size,
								char *pass_out, size_t pass_size) {
	nvs_handle_t nvs;
	esp_err_t err = nvs_open(WIFI_NAMESPACE, NVS_READONLY, &nvs);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Ошибка открытия NVS: %s", esp_err_to_name(err));
		return err;
	}

	err = nvs_get_str(nvs, WIFI_KEY_SSID, ssid_out, &ssid_size);
	if (err != ESP_OK) {
		ESP_LOGW(TAG, "SSID не найден или ошибка чтения: %s",
				 esp_err_to_name(err));
		nvs_close(nvs);
		return err;
	}

	err = nvs_get_str(nvs, WIFI_KEY_PASS, pass_out, &pass_size);
	if (err != ESP_OK) {
		ESP_LOGW(TAG, "Пароль не найден или ошибка чтения: %s",
				 esp_err_to_name(err));
		nvs_close(nvs);
		return err;
	}

	nvs_close(nvs);
	ESP_LOGI(TAG, "Wi-Fi данные успешно загружены");
	return ESP_OK;
}

void reset_all_nvs() {
	nvs_flash_erase();
	nvs_flash_init();
}

esp_err_t get_password_by_ssid(const char *ssid, char *password_out) {
	nvs_handle_t nvs;
	esp_err_t err = nvs_open(WIFI_NAMESPACE, NVS_READONLY, &nvs);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Ошибка открытия NVS: %s", esp_err_to_name(err));
		return err;
	}

	char stored_ssid[32] = {0};
	size_t ssid_size = sizeof(stored_ssid);
	err = nvs_get_str(nvs, WIFI_KEY_SSID, stored_ssid, &ssid_size);
	if (err != ESP_OK) {
		ESP_LOGW(TAG, "SSID не найден: %s", esp_err_to_name(err));
		nvs_close(nvs);
		return err;
	}

	if (strcmp(ssid, stored_ssid) != 0) {
		ESP_LOGW(TAG, "Запрошенный SSID не совпадает с сохранённым");
		nvs_close(nvs);
		return ESP_ERR_NOT_FOUND;
	}

	size_t pass_size = PASSWORD_MAXLEN;
	err = nvs_get_str(nvs, WIFI_KEY_PASS, password_out, &pass_size);
	if (err != ESP_OK) {
		ESP_LOGW(TAG, "Пароль не найден: %s", esp_err_to_name(err));
	} else {
		ESP_LOGI(TAG, "Пароль для SSID %s успешно загружен", ssid);
	}

	nvs_close(nvs);
	return err;
}

#define MAX_WIFI_APs 20

void wifi_scan(wifi_ap_record_t *wifi_list, uint16_t *ap_count) {
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI("wifi_scan", "Сканирование Wi-Fi сетей...");
	ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, true));

	uint16_t found = 0;
	ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&found));
	if (found > MAX_WIFI_APs)
		found = MAX_WIFI_APs;

	ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&found, wifi_list));
	*ap_count = found;

	ESP_ERROR_CHECK(esp_wifi_stop());
}

esp_err_t connect_to_wifi(const char *ssid, const char *password) {
	wifi_config_t wifi_config = {0};

	strncpy((char *)wifi_config.sta.ssid, ssid,
			sizeof(wifi_config.sta.ssid) - 1);
	strncpy((char *)wifi_config.sta.password, password,
			sizeof(wifi_config.sta.password) - 1);
	wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

	ESP_LOGI(TAG, "Установка конфигурации Wi-Fi: SSID='%s'", ssid);
	esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_wifi_set_config failed: %s", esp_err_to_name(err));
		return err;
	}

	ESP_LOGI(TAG, "Запуск подключения к Wi-Fi...");
	err = esp_wifi_connect();
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_wifi_connect failed: %s", esp_err_to_name(err));
		return err;
	}

	// Ждём подключения (максимум 10 сек)
	EventBits_t bits =
		xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE,
							pdTRUE, pdMS_TO_TICKS(10000));
	if (!(bits & WIFI_CONNECTED_BIT)) {
		ESP_LOGE(TAG, "Таймаут подключения к Wi-Fi %s", ssid);
		return ESP_FAIL;
	}

	ESP_LOGI(TAG, "Успешно подключено к Wi-Fi %s", ssid);

	// Проверяем, сохранены ли данные
	char saved_ssid[32] = {0};
	char saved_pass[64] = {0};
	bool already_saved = false;
	if (load_wifi_credentials(saved_ssid, sizeof(saved_ssid), saved_pass,
							  sizeof(saved_pass)) == ESP_OK) {
		if (strcmp(saved_ssid, ssid) == 0 &&
			strcmp(saved_pass, password) == 0) {
			already_saved = true;
		}
	}

	if (!already_saved) {
		ESP_LOGI(TAG, "Сохраняем Wi-Fi данные в NVS");
		save_wifi_credentials(ssid, password);
	} else {
		ESP_LOGI(TAG, "Wi-Fi данные уже сохранены");
	}

	return ESP_OK;
}

void test_nvs(void *arg) {
	char *ssid = "KATE";
	char *password = "123";
	save_wifi_credentials(ssid, password);
	char password_out[PASSWORD_MAXLEN];

	while (1) {

		esp_err_t err = get_password_by_ssid(ssid, password_out);
		if (err == ESP_OK) {
			ESP_LOGI(TAG, "get_password_by_ssid: %s\n", password_out);
		} else {
			ESP_LOGE(TAG, "Failed to get password. err=0x%x\n", err);
		}
		
		vTaskDelay(pdMS_TO_TICKS(5000)); // ждать перед следующей проверкой
	}
}

TaskHandle_t check_nvs_task_handle = NULL;
void test_nvs_write_ONLY_FOR_TEST(void) {
	ESP_LOGI(TAG, " START test_nvs_write_ONLY_FOR_TEST" );
	if (check_nvs_task_handle == NULL) {
		BaseType_t xReturned = xTaskCreate(&test_nvs, "check_wifi_task", 4096,
										   NULL, 5, &check_nvs_task_handle);
		if (xReturned != pdPASS) {
			ESP_LOGE(TAG, "Failed to create check_nvs_task_handle");
		}
	}
}

void ble_wifi_coex_init() {
	int rc;

	/* Initialize NVS — it is used to store PHY calibration data */
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
		ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
	wifi_init_sta();

	check_wifi();

	 
	// run_internet_task();
	ret = nimble_port_init();
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to init nimble %d ", ret);
		return;
	}
	/* Initialize the NimBLE host configuration. */
	ble_hs_cfg.reset_cb = bleprph_on_reset;
	ble_hs_cfg.sync_cb = bleprph_on_sync;
	ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
	ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

	rc = gatt_svr_init();
	assert(rc == 0);

	/* Set the default device name. */
	rc = ble_svc_gap_device_name_set("nimble-bleprph");
	assert(rc == 0);

	/* XXX Need to have template for store */
	ble_store_config_init();
	nimble_port_freertos_init(bleprph_host_task);
	
	//test_nvs_write_ONLY_FOR_TEST();
}
