/**
 * @file smartConfig.cpp
 * @author Eddy Beaupré (https://github.com/EddyBeaupre)
 * @brief Handle SmartConfig configuration
 * @version 1.1.0
 * @date 2021-10-03
 * 
 * @copyright Copyright 2021 Eddy Beaupré <eddy@beaupre.biz>
 *            Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
 *            following conditions are met:
 * 
 *            1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
 *               disclaimer.
 * 
 *            2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
 *               following disclaimer in the documentation and/or other materials provided with the distribution.
 * 
 *            THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS  "AS IS"  AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *            INCLUDING,   BUT NOT LIMITED TO,  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *            DISCLAIMED.   IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *            SPECIAL,  EXEMPLARY,  OR CONSEQUENTIAL DAMAGES  (INCLUDING,  BUT NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE GOODS OR
 *            SERVICES;  LOSS OF USE, DATA, OR PROFITS;  OR BUSINESS INTERRUPTION)  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *            WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *            OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"

#include "smartConfig.hpp"

/**
 * @brief Smart Config Event Handler
 * 
 * Handle all Smart Config Events.
 * 
 * @param event_handler_arg pointer to smartConfig instance
 * @param event_base        Event base type
 * @param event_id          Event ID
 * @param event_data        Event data if any
 */
void smartConfig::smartConfigEventHandler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    smartConfig *Instance = (smartConfig *)event_handler_arg;
    smartConfigData_t scEventData;

    if (event_base == WIFI_EVENT)
    {
        smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            if (Instance->verboseMode)
            {
                ESP_LOGI("smartConfig::smartConfigEventHandler", "Starting SmartConfig session");
            }
            esp_event_post(SC_EVENT, SC_EVENT_SCAN_START, NULL, 0, portMAX_DELAY);
            ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
            ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            if (Instance->verboseMode)
            {
                ESP_LOGI("smartConfig::smartConfigEventHandler", "Disconnected from AP");
            }
            esp_wifi_connect();
            break;
        default:
            break;
        }
    }
    else if (event_base == IP_EVENT)
    {
        switch (event_id)
        {
        case IP_EVENT_STA_GOT_IP:
            if (Instance->verboseMode)
            {
                ESP_LOGI("smartConfig::smartConfigEventHandler", "Connected to AP");
            }
            break;
        default:
            break;
        }
    }
    else if (event_base == SC_EVENT && !(event_id & 0x80))
    {
        switch (event_id)
        {
        case SC_EVENT_SCAN_DONE:
            if (Instance->verboseMode)
            {
                ESP_LOGI("smartConfig::smartConfigEventHandler", "AP Scan done");
            }
            break;
        case SC_EVENT_FOUND_CHANNEL:
            if (Instance->verboseMode)
            {
                ESP_LOGI("smartConfig::smartConfigEventHandler", "Found target AP channel");
            }
            break;
        case SC_EVENT_GOT_SSID_PSWD:
            if (Instance->verboseMode)
            {
                ESP_LOGI("smartConfig::smartConfigEventHandler", "Got SSID and Password");
            }

            memset(&Instance->wifi_config, 0, sizeof(wifi_config_t));
            memcpy(Instance->wifi_config.sta.ssid, ((smartconfig_event_got_ssid_pswd_t *)event_data)->ssid, sizeof(Instance->wifi_config.sta.ssid));
            memcpy(Instance->wifi_config.sta.password, ((smartconfig_event_got_ssid_pswd_t *)event_data)->password, sizeof(Instance->wifi_config.sta.password));
            Instance->wifi_config.sta.bssid_set = ((smartconfig_event_got_ssid_pswd_t *)event_data)->bssid_set;
            if (Instance->wifi_config.sta.bssid_set == true)
            {
                memcpy(Instance->wifi_config.sta.bssid, ((smartconfig_event_got_ssid_pswd_t *)event_data)->bssid, sizeof(Instance->wifi_config.sta.bssid));
                if (Instance->verboseMode)
                {
                    ESP_LOGI("smartConfig::smartConfigEventHandler", "   BSSID: %02x:%02x:%02x:%02x:%02x:%02x",
                             Instance->wifi_config.sta.bssid[0],
                             Instance->wifi_config.sta.bssid[1],
                             Instance->wifi_config.sta.bssid[2],
                             Instance->wifi_config.sta.bssid[3],
                             Instance->wifi_config.sta.bssid[4],
                             Instance->wifi_config.sta.bssid[5]);
                }
            }

            if (Instance->verboseMode)
            {
                ESP_LOGI("smartConfig::smartConfigEventHandler", "    SSID: %s", Instance->wifi_config.sta.ssid);
                ESP_LOGI("smartConfig::smartConfigEventHandler", "PASSWORD: %s", Instance->wifi_config.sta.password);
            }
            if (((smartconfig_event_got_ssid_pswd_t *)event_data)->type == SC_TYPE_ESPTOUCH_V2)
            {
                Instance->validRvdData = true;
                memset(Instance->rvdData, 0, sizeof(Instance->rvdData));
                ESP_ERROR_CHECK(esp_smartconfig_get_rvd_data(Instance->rvdData, sizeof(Instance->rvdData)));
                if (Instance->verboseMode)
                {
                    ESP_LOGI("smartConfig::smartConfigEventHandler", "RVD_DATA: ");
                    for (int i = 0; i < 33; i++)
                    {
                        printf("%02x ", Instance->rvdData[i]);
                    }
                    printf("\n");
                }
            }

            ESP_ERROR_CHECK(esp_wifi_disconnect());
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &Instance->wifi_config));
            esp_wifi_connect();
            break;
        case SC_EVENT_SEND_ACK_DONE:
            if (Instance->verboseMode)
            {
                ESP_LOGI("smartConfig::smartConfigEventHandler", "Ending SmartConfig session");
            }
            scEventData.ssid = Instance->wifi_config.sta.ssid;
            scEventData.password = Instance->wifi_config.sta.password;
            scEventData.bssid = Instance->wifi_config.sta.bssid_set ? Instance->wifi_config.sta.bssid : NULL;
            scEventData.rvd_data = Instance->validRvdData ? Instance->rvdData : NULL;
            esp_event_post(SC_EVENT, SC_EVENT_SCAN_STOP, &scEventData, sizeof(smartConfigData_t), portMAX_DELAY);
            esp_smartconfig_stop();
            break;
        default:
            break;
        }
    }
}

/**
 * @brief Construct a new smartConfig object
 * 
 * This will initiate a smart config session and wait for WiFi Configuration.
 * Status can be track with the system default event loop via the 
 * SC_EVENT.
 * 
 * Two new event id are available, SC_EVENT_SCAN_START and SC_EVENT_SCAN_STOP.
 * 
 * SC_EVENT_SCAN_START:
 *   Indicate that the scan is started and waiting for connection. event_data
 *   will be NULL.
 * 
 * SC_EVENT_SCAN_STOP:
 *   Indicate that the scan has ended and we have successfully connect to the
 *   access point. event_data will contain a pointer to smartConfigData_t with
 *   all the information we got from the APP.
 * 
 * @param verbose Log extra data to serial console
 */
smartConfig::smartConfig(bool verbose)
{
    verboseMode = verbose;
    validRvdData = false;

    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &smartConfigEventHandler, this, &wlEventHandlerInstance));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &smartConfigEventHandler, this, &ipEventHandlerInstance));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(SC_EVENT, ESP_EVENT_ANY_ID, &smartConfigEventHandler, this, &scEventHandlerInstance));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

smartConfig::~smartConfig()
{
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wlEventHandlerInstance));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, ipEventHandlerInstance));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(SC_EVENT, ESP_EVENT_ANY_ID, scEventHandlerInstance));
}