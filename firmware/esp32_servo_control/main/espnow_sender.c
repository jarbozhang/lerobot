/**
 * @file espnow_sender.c
 * @brief ESP-NOW 发送端，精简自 coffee_machine_controller/espnow_controller.c
 *
 * 保留完整配对协议（espnow_data_t / CRC16 / magic），配对后发送裸 ASCII 命令。
 * 硬编码 PMK/LMK/Channel 与咖啡机端 Kconfig 默认值一致。
 */

#include "espnow_sender.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_crc.h"
#include "esp_random.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"

static const char *TAG = "espnow_tx";

// ---------- 与咖啡机端一致的协议定义 ----------

#define ESPNOW_PMK          "pmk1234567890123"
#define ESPNOW_LMK          "lmk1234567890123"
#define ESPNOW_CHANNEL      1
#define ESPNOW_SEND_DELAY   1000   // ms
#define ESPNOW_SEND_LEN     200    // bytes
#define ESPNOW_QUEUE_SIZE   6
#define ESPNOW_MAXDELAY     512

#define ESPNOW_WIFI_MODE    WIFI_MODE_STA
#define ESPNOW_WIFI_IF      ESP_IF_WIFI_STA

enum {
    ESPNOW_DATA_BROADCAST,
    ESPNOW_DATA_UNICAST,
    ESPNOW_DATA_MAX,
};

typedef struct {
    uint8_t  type;
    uint8_t  state;
    uint16_t seq_num;
    uint16_t crc;
    uint32_t magic;
    uint8_t  payload[0];
} __attribute__((packed)) espnow_data_t;

typedef enum {
    ESPNOW_EVENT_SEND_CB,
    ESPNOW_EVENT_RECV_CB,
} espnow_event_id_t;

typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    esp_now_send_status_t status;
} espnow_event_send_cb_t;

typedef struct {
    uint8_t  mac_addr[ESP_NOW_ETH_ALEN];
    uint8_t *data;
    int      data_len;
} espnow_event_recv_cb_t;

typedef union {
    espnow_event_send_cb_t send_cb;
    espnow_event_recv_cb_t recv_cb;
} espnow_event_info_t;

typedef struct {
    espnow_event_id_t   id;
    espnow_event_info_t info;
} espnow_event_t;

typedef struct {
    bool     broadcast;
    uint8_t  state;
    uint32_t magic;
    int      len;
    uint8_t *buffer;
    uint8_t  dest_mac[ESP_NOW_ETH_ALEN];
} send_param_t;

// ---------- 全局状态 ----------

static QueueHandle_t s_queue = NULL;
static uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static uint16_t s_seq[ESPNOW_DATA_MAX] = {0, 0};
static uint8_t s_peer_mac[ESP_NOW_ETH_ALEN] = {0};
static bool s_paired = false;

#define IS_BROADCAST_ADDR(addr) (memcmp(addr, s_broadcast_mac, ESP_NOW_ETH_ALEN) == 0)

// ---------- 公共接口 ----------

bool espnow_is_paired(void)
{
    return s_paired;
}

esp_err_t espnow_send_command(const char *cmd)
{
    if (!s_paired || cmd == NULL) {
        ESP_LOGW(TAG, "Cannot send: not paired or cmd NULL");
        return ESP_FAIL;
    }
    int len = strlen(cmd);
    if (len == 0 || len > 250) {
        ESP_LOGW(TAG, "Command length invalid: %d", len);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Sending to "MACSTR": %s", MAC2STR(s_peer_mac), cmd);
    return esp_now_send(s_peer_mac, (const uint8_t *)cmd, len);
}

// ---------- WiFi ----------

void espnow_sender_wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(ESPNOW_WIFI_MODE));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));
}

// ---------- 回调 ----------

static void send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    if (tx_info == NULL) return;
    espnow_event_t evt = {
        .id = ESPNOW_EVENT_SEND_CB,
    };
    memcpy(evt.info.send_cb.mac_addr, tx_info->des_addr, ESP_NOW_ETH_ALEN);
    evt.info.send_cb.status = status;
    xQueueSend(s_queue, &evt, ESPNOW_MAXDELAY);
}

static void recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    if (recv_info == NULL || data == NULL || len <= 0) return;
    espnow_event_t evt = {
        .id = ESPNOW_EVENT_RECV_CB,
    };
    memcpy(evt.info.recv_cb.mac_addr, recv_info->src_addr, ESP_NOW_ETH_ALEN);
    evt.info.recv_cb.data = malloc(len);
    if (evt.info.recv_cb.data == NULL) return;
    memcpy(evt.info.recv_cb.data, data, len);
    evt.info.recv_cb.data_len = len;
    if (xQueueSend(s_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE) {
        free(evt.info.recv_cb.data);
    }
}

// ---------- 数据处理 ----------

static int data_parse(uint8_t *data, uint16_t data_len,
                      uint8_t *state, uint16_t *seq, uint32_t *magic)
{
    if (data_len < sizeof(espnow_data_t)) return -1;
    espnow_data_t *buf = (espnow_data_t *)data;
    *state = buf->state;
    *seq   = buf->seq_num;
    *magic = buf->magic;
    uint16_t crc = buf->crc;
    buf->crc = 0;
    uint16_t crc_cal = esp_crc16_le(UINT16_MAX, (const uint8_t *)buf, data_len);
    return (crc_cal == crc) ? buf->type : -1;
}

static void data_prepare(send_param_t *p)
{
    espnow_data_t *buf = (espnow_data_t *)p->buffer;
    assert(p->len >= (int)sizeof(espnow_data_t));
    buf->type    = IS_BROADCAST_ADDR(p->dest_mac) ? ESPNOW_DATA_BROADCAST : ESPNOW_DATA_UNICAST;
    buf->state   = p->state;
    buf->seq_num = s_seq[buf->type]++;
    buf->crc     = 0;
    buf->magic   = p->magic;
    esp_fill_random(buf->payload, p->len - sizeof(espnow_data_t));
    buf->crc = esp_crc16_le(UINT16_MAX, (const uint8_t *)buf, p->len);
}

// ---------- 配对任务 ----------

static void pairing_task(void *arg)
{
    send_param_t *sp = (send_param_t *)arg;

    vTaskDelay(pdMS_TO_TICKS(5000));
    ESP_LOGI(TAG, "Start sending broadcast for pairing");

    if (esp_now_send(sp->dest_mac, sp->buffer, sp->len) != ESP_OK) {
        ESP_LOGE(TAG, "Initial send error");
        vTaskDelete(NULL);
    }

    espnow_event_t evt;
    while (xQueueReceive(s_queue, &evt, portMAX_DELAY) == pdTRUE) {
        switch (evt.id) {
        case ESPNOW_EVENT_SEND_CB: {
            if (s_paired || !sp->broadcast) break;
            if (sp->broadcast) {
                vTaskDelay(pdMS_TO_TICKS(ESPNOW_SEND_DELAY));
                memcpy(sp->dest_mac, s_broadcast_mac, ESP_NOW_ETH_ALEN);
                data_prepare(sp);
                esp_now_send(sp->dest_mac, sp->buffer, sp->len);
            }
            break;
        }
        case ESPNOW_EVENT_RECV_CB: {
            espnow_event_recv_cb_t *rc = &evt.info.recv_cb;

            if (s_paired) {
                // 配对后忽略接收（发送端不处理命令）
                free(rc->data);
                break;
            }

            uint8_t  recv_state = 0;
            uint16_t recv_seq   = 0;
            uint32_t recv_magic = 0;
            int ret = data_parse(rc->data, rc->data_len, &recv_state, &recv_seq, &recv_magic);
            free(rc->data);

            if (ret == ESPNOW_DATA_BROADCAST) {
                ESP_LOGI(TAG, "Recv %dth broadcast from "MACSTR, recv_seq, MAC2STR(rc->mac_addr));

                if (!esp_now_is_peer_exist(rc->mac_addr)) {
                    esp_now_peer_info_t peer = {
                        .channel = ESPNOW_CHANNEL,
                        .ifidx   = ESPNOW_WIFI_IF,
                        .encrypt = true,
                    };
                    memcpy(peer.lmk, ESPNOW_LMK, ESP_NOW_KEY_LEN);
                    memcpy(peer.peer_addr, rc->mac_addr, ESP_NOW_ETH_ALEN);
                    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
                }

                if (sp->state == 0) sp->state = 1;

                if (recv_state == 1) {
                    ESP_LOGI(TAG, "=== Pairing completed! Peer: "MACSTR" ===", MAC2STR(rc->mac_addr));
                    sp->broadcast = false;
                    s_paired = true;
                    memcpy(s_peer_mac, rc->mac_addr, ESP_NOW_ETH_ALEN);
                }
            } else if (ret == ESPNOW_DATA_UNICAST) {
                ESP_LOGI(TAG, "=== Paired via unicast! Peer: "MACSTR" ===", MAC2STR(rc->mac_addr));
                sp->broadcast = false;
                s_paired = true;
                memcpy(s_peer_mac, rc->mac_addr, ESP_NOW_ETH_ALEN);
            }
            break;
        }
        default:
            break;
        }
    }

    vTaskDelete(NULL);
}

// ---------- 初始化 ----------

esp_err_t espnow_sender_init(void)
{
    s_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(espnow_event_t));
    if (s_queue == NULL) return ESP_FAIL;

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(recv_cb));
    ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t *)ESPNOW_PMK));

    // 广播 peer（不加密）
    esp_now_peer_info_t bcast_peer = {
        .channel = ESPNOW_CHANNEL,
        .ifidx   = ESPNOW_WIFI_IF,
        .encrypt = false,
    };
    memcpy(bcast_peer.peer_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_add_peer(&bcast_peer));

    // 发送参数
    send_param_t *sp = calloc(1, sizeof(send_param_t));
    if (sp == NULL) {
        esp_now_deinit();
        vQueueDelete(s_queue);
        return ESP_FAIL;
    }
    sp->broadcast = true;
    sp->state     = 0;
    sp->magic     = esp_random();
    sp->len       = ESPNOW_SEND_LEN;
    sp->buffer    = malloc(ESPNOW_SEND_LEN);
    if (sp->buffer == NULL) {
        free(sp);
        esp_now_deinit();
        vQueueDelete(s_queue);
        return ESP_FAIL;
    }
    memcpy(sp->dest_mac, s_broadcast_mac, ESP_NOW_ETH_ALEN);
    data_prepare(sp);

    xTaskCreate(pairing_task, "espnow_pair", 4096, sp, 4, NULL);
    return ESP_OK;
}
