#include <string.h>
#include <stdio.h>

#include <api_os.h>
#include <api_info.h>

#include <api_gps.h>
#include <api_debug.h>
#include <api_mqtt.h>

#include <api_hal_watchdog.h>
#include <api_hal_gpio.h>

#include <gps.h>
#include <gps_parse.h>

#include "mqtt_task.h"
#include "adc_task.h"
#include "secret.h"


static HANDLE mqttTaskHandle = NULL;
HANDLE semMqttStart = NULL;


MQTT_Connect_Info_t ci;
MQTT_Client_t *mqttClient = NULL;

char imei[16] = "";
char buffer[1024] = "";
char mqttLocationTopic[64] = "";
char mqttStateTopic[64] = "";
char mqttBatteryTopic[64] = "";
char mqttLiionTopic[64] = "";
char mqttBuffer[1024] = "";

MQTT_Status_t mqttStatus = MQTT_STATUS_DISCONNECTED;


MQTT_Status_t getMqttState(void) {
    return mqttStatus;
}

void OnMqttConnect(MQTT_Client_t *client, void *arg, MQTT_Connection_Status_t status) {
    Trace(1,"MQTT connection status: %d", status);

    if(status == MQTT_CONNECTION_ACCEPTED) {
        Trace(1,"MQTT succeed connect to broker");
        mqttStatus = MQTT_STATUS_CONNECTED;
    } else {
        mqttStatus = MQTT_STATUS_DISCONNECTED;
    }

    WatchDog_KeepAlive();
}

void MqttConnect(void) {
    Trace(1, "MqttConnect");

    MQTT_Error_t err = MQTT_Connect(mqttClient, BROKER_IP, BROKER_PORT, OnMqttConnect, NULL, &ci);

    if(err != MQTT_ERROR_NONE) {
        Trace(1, "MQTT connect fail, error code: %d", err);
    }

    mqttStatus = MQTT_STATUS_CONNECTING;
}


void OnPublishStateOnline(void* arg, MQTT_Error_t err) {
    if(err == MQTT_ERROR_NONE) {
        Trace(1,"MQTT publish state online success");
        mqttStatus = MQTT_STATUS_ONLINE;
    } else {
        Trace(1,"MQTT publish state online error, error code: %d", err);
    }

    WatchDog_KeepAlive();
}

void MqttPublishStateOnline(void) {
    Trace(1, "MqttPublishStateOnline");

    MQTT_Error_t err = MQTT_Publish(mqttClient, mqttStateTopic, MQTT_PAYLOAD_STATE_ONLINE, strlen(MQTT_PAYLOAD_STATE_ONLINE), 1, 2, 1, OnPublishStateOnline, NULL);

    if(err != MQTT_ERROR_NONE)
        Trace(1,"MQTT publish state online error, error code: %d", err);
}

void OnPublishStateOffline(void* arg, MQTT_Error_t err) {
    if(err == MQTT_ERROR_NONE) {
        Trace(1,"MQTT publish state offline success");
        mqttStatus = MQTT_STATUS_OFFLINE;
    } else {
        Trace(1,"MQTT publish state offline error, error code: %d", err);
    }

    WatchDog_KeepAlive();
}

void MqttPublishStateOffline(void) {
    Trace(1, "MqttPublishStateOffline");

    MQTT_Error_t err = MQTT_Publish(mqttClient, mqttStateTopic, MQTT_PAYLOAD_STATE_OFFLINE, strlen(MQTT_PAYLOAD_STATE_OFFLINE), 1, 2, 1, OnPublishStateOffline, NULL);

    if(err != MQTT_ERROR_NONE)
        Trace(1,"MQTT publish state offline error, error code: %d", err);
}


void OnPublishBattery(void* arg, MQTT_Error_t err) {
    if(err == MQTT_ERROR_NONE) {
        Trace(1,"MQTT publish battery success");
    } else {
        Trace(1,"MQTT publish battery error, error code: %d", err);
    }

    WatchDog_KeepAlive();
}

void MqttPublishBattery(void) {
    Trace(1, "MqttPublishBattery");

    snprintf(mqttBuffer, sizeof(mqttBuffer), "%.02f", GetBatteryVoltage());

    MQTT_Error_t err = MQTT_Publish(mqttClient, mqttBatteryTopic, mqttBuffer, strlen(mqttBuffer), 1, 1, 0, OnPublishBattery, NULL);

    if(err != MQTT_ERROR_NONE)
        Trace(1,"MQTT publish location error, error code: %d", err);
}

void MqttPublishLiion(void) {
    Trace(1, "MqttPublishLiion");

    snprintf(mqttBuffer, sizeof(mqttBuffer), "%.02f", GetLiionVoltage());

    MQTT_Error_t err = MQTT_Publish(mqttClient, mqttLiionTopic, mqttBuffer, strlen(mqttBuffer), 1, 1, 0, OnPublishBattery, NULL);

    if(err != MQTT_ERROR_NONE)
        Trace(1,"MQTT publish location error, error code: %d", err);
}

void OnPublishTracker(void* arg, MQTT_Error_t err) {
    if(err == MQTT_ERROR_NONE) {
        Trace(1,"MQTT publish success");
        mqttStatus = MQTT_STATUS_LOCATION_PUBLISHED;
    } else {
        Trace(1,"MQTT publish error, error code: %d", err);
    }

    WatchDog_KeepAlive();
}

void MqttPublishTracker(void) {
    Trace(1, "MqttPublishTracker");

    GPS_Info_t* gpsInfo = Gps_GetInfo();
    uint8_t isFixed = gpsInfo->gsa[0].fix_type > gpsInfo->gsa[1].fix_type ? gpsInfo->gsa[0].fix_type : gpsInfo->gsa[1].fix_type;

    if(isFixed == 2 || isFixed == 3) { /* We have 2D or 3D fix */ 

            char* isFixedStr;            
            if(isFixed == 2)
                isFixedStr = "2D fix";
            else if(isFixed == 3)
            {
                if(gpsInfo->gga.fix_quality == 1)
                    isFixedStr = "3D fix";
                else if(gpsInfo->gga.fix_quality == 2)
                    isFixedStr = "3D/DGPS fix";
            }
            else
                isFixedStr = "no fix";


        //convert unit ddmm.mmmm to degree(°) 
        int temp = (int)(gpsInfo->rmc.latitude.value / gpsInfo->rmc.latitude.scale / 100);
        double latitude = temp + (double)(gpsInfo->rmc.latitude.value - temp * gpsInfo->rmc.latitude.scale * 100) / gpsInfo->rmc.latitude.scale / 60.0;
        temp = (int)(gpsInfo->rmc.longitude.value / gpsInfo->rmc.longitude.scale / 100);
        double longitude = temp + (double)(gpsInfo->rmc.longitude.value - temp * gpsInfo->rmc.longitude.scale * 100) / gpsInfo->rmc.longitude.scale / 60.0;

        snprintf(mqttBuffer, sizeof(mqttBuffer), "{\"longitude\": %f,\"latitude\": %f,\"gps_accuracy\": %.f,\"battery_level\": %.02f}", 
                                                            longitude, latitude, minmea_tofloat(&gpsInfo->gga.hdop) * 25, GetLiionLevel());


        MQTT_Error_t err = MQTT_Publish(mqttClient, mqttLocationTopic, mqttBuffer, strlen(mqttBuffer), 1, 1, 0, OnPublishTracker, NULL);


        /*snprintf(mqttBuffer,sizeof(mqttBuffer),"GPS fix mode:%d, GLONASS fix mode:%d, hdop:%f, satellites tracked:%d, gps sates total:%d, is fixed:%s, coordinate:WGS84, Latitude:%f, Longitude:%f, unit:degree, altitude:%f",gpsInfo->gsa[0].fix_type, gpsInfo->gsa[1].fix_type,
                                                                 minmea_tofloat(&gpsInfo->gga.hdop), gpsInfo->gga.satellites_tracked, gpsInfo->gsv[0].total_sats, isFixedStr, latitude,longitude, minmea_tofloat(&gpsInfo->gga.altitude));
       
        err = MQTT_Publish(mqttClient, mqttLocationTopic, mqttBuffer, strlen(mqttBuffer), 1, 2, 0, OnPublishTracker, NULL);*/

        if(err != MQTT_ERROR_NONE)
            Trace(1,"MQTT publish location error, error code: %d", err);
    }
}


void MqttPublishTelemetry(void) {
    static int cnt = 0;
    
    if (cnt++ % 2 == 0) {
        MqttPublishBattery(); 
        MqttPublishLiion(); 
        // TODO: MqttPublishSpeed(); 
        MqttPublishTracker();
    }
}


void MqttInit(void) {
    INFO_GetIMEI(imei);
    Trace(1,"IMEI: %s", imei);
    snprintf(mqttLocationTopic, sizeof(mqttLocationTopic), "location/%s", imei);
    snprintf(mqttStateTopic, sizeof(mqttStateTopic), "vehicle/%s/state", imei);
    snprintf(mqttBatteryTopic, sizeof(mqttBatteryTopic), "vehicle/%s/battery", imei);
    snprintf(mqttLiionTopic, sizeof(mqttBatteryTopic), "vehicle/%s/liion", imei);

    mqttClient = MQTT_ClientNew();

    memset(&ci, 0, sizeof(MQTT_Connect_Info_t));
    ci.client_id = imei;
    ci.client_user = CLIENT_USER;
    ci.client_pass = CLIENT_PASS;
    ci.keep_alive = 60;
    ci.clean_session = 1;
    ci.use_ssl = false;
#if 1  // Set connection status offline
    ci.will_qos = 2;
    ci.will_topic = mqttStateTopic;
    ci.will_retain = 1;
    ci.will_msg = MQTT_PAYLOAD_STATE_OFFLINE;
#endif
#if 1  // TLS
    ci.use_ssl = true;
    ci.ssl_verify_mode = MQTT_SSL_VERIFY_MODE_OPTIONAL;
    ci.ca_cert = ca_crt;
    ci.ca_crl = NULL;
    ci.client_cert = client_crt;
    ci.client_key  = client_key;
    ci.client_key_passwd = NULL;
    ci.broker_hostname = BROKER_HOSTNAME;
    ci.ssl_min_version = MQTT_SSL_VERSION_TLSv1_2;
    ci.ssl_max_version = MQTT_SSL_VERSION_TLSv1_2;
    ci.entropy_custom = "GPRS_A9";
#endif

    Trace(1, "MQTT init");
}


void MqttTask(void *pData) {
    WatchDog_Open(WATCHDOG_SECOND_TO_TICK(60));

    semMqttStart = OS_CreateSemaphore(0);
    OS_WaitForSemaphore(semMqttStart, OS_WAIT_FOREVER);
    OS_DeleteSemaphore(semMqttStart);
    semMqttStart = NULL;

    WatchDog_KeepAlive();

    MqttInit();
  
    while(1) {
        switch(mqttStatus) {
            case MQTT_STATUS_DISCONNECTED:
                MqttConnect();
                break;

             case MQTT_STATUS_CONNECTING:
                Trace(1,"MQTT connecting...");
                break;

            case MQTT_STATUS_CONNECTED:
                MqttPublishStateOnline();
                break;

            case MQTT_STATUS_ONLINE:
            case MQTT_STATUS_LOCATION_PUBLISHED:            
                MqttPublishTelemetry();
                break;

            case MQTT_STATUS_OFFLINE:
                break;

            default:
                break;
        }

        OS_Sleep(MQTT_INTERVAL);
    }
}


void MqttTaskInit(void) {
    mqttTaskHandle = OS_CreateTask(MqttTask,
                                   NULL, NULL, MQTT_TASK_STACK_SIZE, MQTT_TASK_PRIORITY, 0, 0, MQTT_TASK_NAME);
}
