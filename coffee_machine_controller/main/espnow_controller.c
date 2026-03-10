/**
 * @file espnow_controller.c
 * @brief ESP-NOW 通信控制模块实现
 */

#include "espnow_controller.h"
#include "command_handler.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "esp_random.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_crc.h"

static const char *TAG = "espnow";

// 全局状态
static QueueHandle_t s_espnow_queue = NULL;
static uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static uint16_t s_espnow_seq[ESPNOW_DATA_MAX] = { 0, 0 };
static uint8_t s_peer_mac[ESP_NOW_ETH_ALEN] = { 0 };
static bool s_peer_matched = false;
static espnow_send_param_t *s_send_param = NULL;

#define IS_BROADCAST_ADDR(addr) (memcmp(addr, s_broadcast_mac, ESP_NOW_ETH_ALEN) == 0)

// ==================== 公共接口 ====================

bool espnow_is_peer_matched(void)
{
    return s_peer_matched;
}

void espnow_get_peer_mac(uint8_t *mac)
{
    if (mac) {
        memcpy(mac, s_peer_mac, ESP_NOW_ETH_ALEN);
    }
}

esp_err_t espnow_send_command(const char *cmd)
{
    if (!s_peer_matched || cmd == NULL) {
        ESP_LOGW(TAG, "Cannot send: peer not matched or cmd is NULL");
        return ESP_FAIL;
    }
    
    int len = strlen(cmd);
    if (len == 0 || len > 250) {
        ESP_LOGW(TAG, "Command length invalid: %d", len);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Sending command to "MACSTR": %s", MAC2STR(s_peer_mac), cmd);
    return esp_now_send(s_peer_mac, (const uint8_t *)cmd, len);
}

// ==================== WiFi 初始化 ====================

void espnow_wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(ESPNOW_WIFI_MODE));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(CONFIG_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

#if CONFIG_ESPNOW_ENABLE_LONG_RANGE
    ESP_ERROR_CHECK(esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR));
#endif
}

// ==================== 回调函数 ====================

static void espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    espnow_event_t evt;
    espnow_event_send_cb_t *send_cb = &evt.info.send_cb;

    if (tx_info == NULL) {
        ESP_LOGE(TAG, "Send cb arg error");
        return;
    }

    evt.id = ESPNOW_EVENT_SEND_CB;
    memcpy(send_cb->mac_addr, tx_info->des_addr, ESP_NOW_ETH_ALEN);
    send_cb->status = status;
    if (xQueueSend(s_espnow_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE) {
        ESP_LOGW(TAG, "Send queue fail");
    }
}

static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    espnow_event_t evt;
    espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;
    uint8_t *mac_addr = recv_info->src_addr;

    if (mac_addr == NULL || data == NULL || len <= 0) {
        ESP_LOGE(TAG, "Receive cb arg error");
        return;
    }

    evt.id = ESPNOW_EVENT_RECV_CB;
    memcpy(recv_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    recv_cb->data = malloc(len);
    if (recv_cb->data == NULL) {
        ESP_LOGE(TAG, "Malloc receive data fail");
        return;
    }
    memcpy(recv_cb->data, data, len);
    recv_cb->data_len = len;
    if (xQueueSend(s_espnow_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE) {
        ESP_LOGW(TAG, "Receive queue fail");
        free(recv_cb->data);
    }
}

// ==================== 数据处理 ====================

static int espnow_data_parse(uint8_t *data, uint16_t data_len, uint8_t *state, uint16_t *seq, uint32_t *magic)
{
    espnow_data_t *buf = (espnow_data_t *)data;
    uint16_t crc, crc_cal = 0;

    if (data_len < sizeof(espnow_data_t)) {
        ESP_LOGE(TAG, "Receive data too short, len:%d", data_len);
        return -1;
    }

    *state = buf->state;
    *seq = buf->seq_num;
    *magic = buf->magic;
    crc = buf->crc;
    buf->crc = 0;
    crc_cal = esp_crc16_le(UINT16_MAX, (uint8_t const *)buf, data_len);

    if (crc_cal == crc) {
        return buf->type;
    }

    return -1;
}

static void espnow_data_prepare(espnow_send_param_t *send_param)
{
    espnow_data_t *buf = (espnow_data_t *)send_param->buffer;

    assert(send_param->len >= sizeof(espnow_data_t));

    buf->type = IS_BROADCAST_ADDR(send_param->dest_mac) ? ESPNOW_DATA_BROADCAST : ESPNOW_DATA_UNICAST;
    buf->state = send_param->state;
    buf->seq_num = s_espnow_seq[buf->type]++;
    buf->crc = 0;
    buf->magic = send_param->magic;
    esp_fill_random(buf->payload, send_param->len - sizeof(espnow_data_t));
    buf->crc = esp_crc16_le(UINT16_MAX, (uint8_t const *)buf, send_param->len);
}

// ==================== 主任务 ====================

static void espnow_task(void *pvParameter)
{
    espnow_event_t evt;
    uint8_t recv_state = 0;
    uint16_t recv_seq = 0;
    uint32_t recv_magic = 0;
    bool is_broadcast = false;
    int ret;

    vTaskDelay(5000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Start sending broadcast data");

    espnow_send_param_t *send_param = (espnow_send_param_t *)pvParameter;
    if (esp_now_send(send_param->dest_mac, send_param->buffer, send_param->len) != ESP_OK) {
        ESP_LOGE(TAG, "Send error");
        vTaskDelete(NULL);
    }

    while (xQueueReceive(s_espnow_queue, &evt, portMAX_DELAY) == pdTRUE) {
        switch (evt.id) {
            case ESPNOW_EVENT_SEND_CB:
            {
                espnow_event_send_cb_t *send_cb = &evt.info.send_cb;
                is_broadcast = IS_BROADCAST_ADDR(send_cb->mac_addr);

                // 如果已经匹配成功，不再继续发送
                if (s_peer_matched) {
                    break;
                }

                // 如果广播已停止，跳过
                if (is_broadcast && !send_param->broadcast) {
                    break;
                }

                // 发送延迟
                if (send_param->delay > 0) {
                    vTaskDelay(send_param->delay / portTICK_PERIOD_MS);
                }

                ESP_LOGI(TAG, "send broadcast to "MACSTR"", MAC2STR(send_cb->mac_addr));

                memcpy(send_param->dest_mac, send_cb->mac_addr, ESP_NOW_ETH_ALEN);
                espnow_data_prepare(send_param);

                if (esp_now_send(send_param->dest_mac, send_param->buffer, send_param->len) != ESP_OK) {
                    ESP_LOGE(TAG, "Send error");
                }
                break;
            }
            case ESPNOW_EVENT_RECV_CB:
            {
                espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;

                // 匹配成功后，处理命令
                if (s_peer_matched) {
                    // 检查是否为有效文本命令（ASCII >= 0x20）
                    if (recv_cb->data_len > 0 && recv_cb->data[0] >= 0x20) {
                        char *cmd = malloc(recv_cb->data_len + 1);
                        if (cmd != NULL) {
                            memcpy(cmd, recv_cb->data, recv_cb->data_len);
                            cmd[recv_cb->data_len] = '\0';
                            str_rstrip(cmd);
                            
                            if (strlen(cmd) > 0) {
                                ESP_LOGI(TAG, "Received command from "MACSTR": %s (len=%d)", 
                                         MAC2STR(recv_cb->mac_addr), cmd, recv_cb->data_len);
                                command_process(cmd);
                            }
                            free(cmd);
                        }
                    }
                    free(recv_cb->data);
                    break;
                }

                // 匹配阶段
                ret = espnow_data_parse(recv_cb->data, recv_cb->data_len, &recv_state, &recv_seq, &recv_magic);
                free(recv_cb->data);
                
                if (ret == ESPNOW_DATA_BROADCAST) {
                    ESP_LOGI(TAG, "Receive %dth broadcast from: "MACSTR", len: %d", 
                             recv_seq, MAC2STR(recv_cb->mac_addr), recv_cb->data_len);

                    // 添加 peer
                    if (!esp_now_is_peer_exist(recv_cb->mac_addr)) {
                        esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
                        if (peer != NULL) {
                            memset(peer, 0, sizeof(esp_now_peer_info_t));
                            peer->channel = CONFIG_ESPNOW_CHANNEL;
                            peer->ifidx = ESPNOW_WIFI_IF;
                            peer->encrypt = true;
                            memcpy(peer->lmk, CONFIG_ESPNOW_LMK, ESP_NOW_KEY_LEN);
                            memcpy(peer->peer_addr, recv_cb->mac_addr, ESP_NOW_ETH_ALEN);
                            ESP_ERROR_CHECK(esp_now_add_peer(peer));
                            free(peer);
                        }
                    }

                    if (send_param->state == 0) {
                        send_param->state = 1;
                    }

                    // 双方都收到广播后，完成匹配
                    if (recv_state == 1) {
                        ESP_LOGI(TAG, "=== Peer matching completed! ===");
                        ESP_LOGI(TAG, "Peer MAC: "MACSTR"", MAC2STR(recv_cb->mac_addr));
                        
                        send_param->broadcast = false;
                        s_peer_matched = true;
                        memcpy(s_peer_mac, recv_cb->mac_addr, ESP_NOW_ETH_ALEN);
                        
                        ESP_LOGI(TAG, "Now waiting for ESP-NOW commands...");
                    }
                }
                else if (ret == ESPNOW_DATA_UNICAST) {
                    ESP_LOGI(TAG, "Receive %dth unicast from: "MACSTR", len: %d", 
                             recv_seq, MAC2STR(recv_cb->mac_addr), recv_cb->data_len);
                    
                    send_param->broadcast = false;
                    s_peer_matched = true;
                    memcpy(s_peer_mac, recv_cb->mac_addr, ESP_NOW_ETH_ALEN);
                    ESP_LOGI(TAG, "=== Peer matched (via unicast)! ===");
                }
                else {
                    ESP_LOGD(TAG, "Receive invalid data from: "MACSTR"", MAC2STR(recv_cb->mac_addr));
                }
                break;
            }
            default:
                ESP_LOGE(TAG, "Callback type error: %d", evt.id);
                break;
        }
    }
}

// ==================== 初始化/反初始化 ====================

esp_err_t espnow_controller_init(void)
{
    s_espnow_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(espnow_event_t));
    if (s_espnow_queue == NULL) {
        ESP_LOGE(TAG, "Create queue fail");
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
    
#if CONFIG_ESPNOW_ENABLE_POWER_SAVE
    ESP_ERROR_CHECK(esp_now_set_wake_window(CONFIG_ESPNOW_WAKE_WINDOW));
    ESP_ERROR_CHECK(esp_wifi_connectionless_module_set_wake_interval(CONFIG_ESPNOW_WAKE_INTERVAL));
#endif

    ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK));

    // 添加广播 peer
    esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL) {
        ESP_LOGE(TAG, "Malloc peer fail");
        vQueueDelete(s_espnow_queue);
        s_espnow_queue = NULL;
        esp_now_deinit();
        return ESP_FAIL;
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = CONFIG_ESPNOW_CHANNEL;
    peer->ifidx = ESPNOW_WIFI_IF;
    peer->encrypt = false;
    memcpy(peer->peer_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_add_peer(peer));
    free(peer);

    // 初始化发送参数
    s_send_param = malloc(sizeof(espnow_send_param_t));
    if (s_send_param == NULL) {
        ESP_LOGE(TAG, "Malloc send param fail");
        vQueueDelete(s_espnow_queue);
        s_espnow_queue = NULL;
        esp_now_deinit();
        return ESP_FAIL;
    }
    memset(s_send_param, 0, sizeof(espnow_send_param_t));
    s_send_param->unicast = false;
    s_send_param->broadcast = true;
    s_send_param->state = 0;
    s_send_param->magic = esp_random();
    s_send_param->count = CONFIG_ESPNOW_SEND_COUNT;
    s_send_param->delay = CONFIG_ESPNOW_SEND_DELAY;
    s_send_param->len = CONFIG_ESPNOW_SEND_LEN;
    s_send_param->buffer = malloc(CONFIG_ESPNOW_SEND_LEN);
    if (s_send_param->buffer == NULL) {
        ESP_LOGE(TAG, "Malloc send buffer fail");
        free(s_send_param);
        vQueueDelete(s_espnow_queue);
        s_espnow_queue = NULL;
        esp_now_deinit();
        return ESP_FAIL;
    }
    memcpy(s_send_param->dest_mac, s_broadcast_mac, ESP_NOW_ETH_ALEN);
    espnow_data_prepare(s_send_param);

    xTaskCreate(espnow_task, "espnow_task", 4096, s_send_param, 4, NULL);

    return ESP_OK;
}

void espnow_controller_deinit(void)
{
    if (s_send_param) {
        free(s_send_param->buffer);
        free(s_send_param);
        s_send_param = NULL;
    }
    if (s_espnow_queue) {
        vQueueDelete(s_espnow_queue);
        s_espnow_queue = NULL;
    }
    esp_now_deinit();
}
