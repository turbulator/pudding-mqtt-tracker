#include <api_os.h>
#include <api_hal_gpio.h>
#include <api_gps.h>

#include "mqtt_task.h"


static HANDLE mqttTaskHandle = NULL;

#define MQTT_TASK_STACK_SIZE    (2048 * 4)
#define MQTT_TASK_PRIORITY      1
#define MQTT_TASK_NAME          "MQTT Task"


static HANDLE mqttTaskHandle = NULL;
static HANDLE semMqttStart = NULL;


MQTT_Connect_Info_t ci;
MQTT_Client_t *client = NULL;

char imei[16] = "";
char buffer[1024] = "";
char mqttLocationTopic[64] = "";
char mqttStateTopic[64] = "";
char mqttBatteryTopic[64] = "";
char mqttLiionTopic[64] = "";
char mqttBuffer[1024] = "";

MQTT_Status_t mqttStatus = MQTT_STATUS_DISCONNECTED;


void OnMqttConnect(MQTT_Client_t *client, void *arg, MQTT_Connection_Status_t status) {
    Trace(1,"MQTT connection status: %d", status);

    if(status == MQTT_CONNECTION_ACCEPTED) {
        Trace(1,"MQTT succeed connect to broker");
        mqttStatus = MQTT_STATUS_CONNECTED;
        MqttPublishState();
    } 
}

void MqttConnect(voic) {
    MQTT_Error_t err = MQTT_Connect(client, BROKER_IP, BROKER_PORT, OnMqttConnect, NULL, &ci);

    if(err != MQTT_ERROR_NONE) {
        Trace(1, "MQTT connect fail, error code: %d", err);
    }

    WatchDog_KeepAlive();
}


void OnPublish(void* arg, MQTT_Error_t err) {
    if(err == MQTT_ERROR_NONE)
        Trace(1,"MQTT publish success");
    else
        Trace(1,"MQTT publish error, error code: %d",err);
}

void MqttPublishState(void) {
    MQTT_Error_t err = MQTT_Publish(client, mqttStateTopic, MQTT_PAYLOAD_STATE_ONLINE, strlen(MQTT_PAYLOAD_STATE_ONLINE), 1, 2, 0, OnPublish, NULL);

    if(err != MQTT_ERROR_NONE)
        Trace(1,"MQTT publish state error, error code: %d", err);
}

void MqttPublishBattery(void) {
    Trace(1, "MqttPublishBattery");

    snprintf(mqttBuffer, sizeof(mqttBuffer), "%.02f", GetBatteryVoltage());

    MQTT_Error_t err = MQTT_Publish(client, mqttBatteryTopic, mqttBuffer, strlen(mqttBuffer), 1, 2, 0, OnPublish, NULL);

    if(err != MQTT_ERROR_NONE)
        Trace(1,"MQTT publish location error, error code: %d", err);
}

void MqttPublishLiion(void) {
    Trace(1, "MqttPublishLiion");

    snprintf(mqttBuffer, sizeof(mqttBuffer), "%.02f", GetLiionVoltage());

    MQTT_Error_t err = MQTT_Publish(client, mqttLiionTopic, mqttBuffer, strlen(mqttBuffer), 1, 2, 0, OnPublish, NULL);

    if(err != MQTT_ERROR_NONE)
        Trace(1,"MQTT publish location error, error code: %d", err);
}

void MqttPublishLocation(void) {
    Trace(1, "MqttPublishLocation");

    GPS_Info_t* gpsInfo = Gps_GetInfo();
    uint8_t isFixed = gpsInfo->gsa[0].fix_type > gpsInfo->gsa[1].fix_type ? gpsInfo->gsa[0].fix_type : gpsInfo->gsa[1].fix_type;

    if(mqttStatus != MQTT_STATUS_CONNECTED) {
        Trace(1,"MQTT not connected to broker! can not publish");
        return;
    }

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


        MQTT_Error_t err = MQTT_Publish(client, mqttLocationTopic, mqttBuffer, strlen(mqttBuffer), 1, 2, 0, OnPublish, NULL);


        snprintf(mqttBuffer,sizeof(mqttBuffer),"GPS fix mode:%d, GLONASS fix mode:%d, hdop:%f, satellites tracked:%d, gps sates total:%d, is fixed:%s, coordinate:WGS84, Latitude:%f, Longitude:%f, unit:degree, altitude:%f",gpsInfo->gsa[0].fix_type, gpsInfo->gsa[1].fix_type,
                                                                 minmea_tofloat(&gpsInfo->gga.hdop), gpsInfo->gga.satellites_tracked, gpsInfo->gsv[0].total_sats, isFixedStr, latitude,longitude, minmea_tofloat(&gpsInfo->gga.altitude));
       
        err = MQTT_Publish(client, mqttLocationTopic, mqttBuffer, strlen(mqttBuffer), 1, 2, 0, OnPublish, NULL);


        if(err != MQTT_ERROR_NONE)
            Trace(1,"MQTT publish location error, error code: %d", err);
    }
}


void MqttPublish(void) {

    MqttPublishBattery(); 
    MqttPublishLiion(); 
    MqttPublishSpeed(); 
    MqttPublishLocation();

    WatchDog_KeepAlive();
}


void GpsInit()
{
    Trace(1, "GpsInit 1");
    GPS_Info_t* gpsInfo = Gps_GetInfo();

    Trace(1, "GpsInit 2");
    //open GPS hardware(UART2 open either)
    GPS_Init();
    Trace(3, "GpsInit 3");
    GPS_Open(NULL);

    //wait for gps start up, or gps will not response command
    while(gpsInfo->rmc.latitude.value == 0)
        OS_Sleep(1000);
    

    // set gps nmea output interval
    for(uint8_t i = 0; i < 5; ++i)
    {
        bool ret = GPS_SetOutputInterval(10000);
        Trace(1, "set gps ret:%d", ret);
        if(ret)
            break;
        OS_Sleep(1000);
    }
    
      
    if(!GPS_GetVersion(buffer, 150))
        Trace(1, "get gps firmware version fail");
    else
        Trace(1, "gps firmware version:%s", buffer);

    // if(!GPS_SetFixMode(GPS_FIX_MODE_LOW_SPEED))
        // Trace(1,"set fix mode fail");
    
    if(!GPS_SetSearchMode(true, true, false, false))
        Trace(1,"set search mode fail");
        
    if(!GPS_SetSBASEnable(true))
        Trace(1,"enable sbas fail");

    if(!GPS_SetLpMode(GPS_LP_MODE_SUPPER_LP))
        Trace(1, "set gps lp mode fail");

    if(!GPS_SetOutputInterval(1000))
        Trace(1, "set nmea output interval fail");
}


void MqttInit(void) {
    INFO_GetIMEI(imei);
    Trace(1,"IMEI: %s", imei);
    snprintf(mqttLocationTopic, sizeof(mqttLocationTopic), "location/%s", imei);
    snprintf(mqttStateTopic, sizeof(mqttStateTopic), "vehicle/%s/state", imei);
    snprintf(mqttBatteryTopic, sizeof(mqttBatteryTopic), "vehicle/%s/battery", imei);
    snprintf(mqttLiionTopic, sizeof(mqttBatteryTopic), "vehicle/%s/liion", imei);


    char ip[16];
    if(Network_GetIp(ip, sizeof(ip))) {
        Trace(1, "network local ip: %s", ip);
    }


    MQTT_Client_t* client = MQTT_ClientNew();

    MQTT_Error_t err;
    memset(&ci, 0, sizeof(MQTT_Connect_Info_t));
    ci.client_id = imei;
    ci.client_user = CLIENT_USER;
    ci.client_pass = CLIENT_PASS;
    ci.keep_alive = 60;
    ci.clean_session = 1;
    ci.use_ssl = false;
#if 1  // TODO: set connection status offline
    ci.will_qos = 2;
    ci.will_topic = mqttStateTopic;
    ci.will_retain = 1;
    ci.will_msg = MQTT_PAYLOAD_STATE_OFFLINE;
#endif
#if 1  // TSL
    ci.use_ssl = true;
    ci.ssl_verify_mode = MQTT_SSL_VERIFY_MODE_OPTIONAL;
    ci.ca_cert = ca_crt;
    ci.ca_crl = NULL;
    ci.client_cert = client_crt;
    ci.client_key  = client_key;
    ci.client_key_passwd = NULL;
    ci.broker_hostname = BROKER_HOSTNAME;
    ci.ssl_min_version   = MQTT_SSL_VERSION_SSLv3;
    ci.ssl_max_version   = MQTT_SSL_VERSION_TLSv1_2;
    ci.entropy_custom    = "GPRS_A9";
#endif

    Trace(1, "MQTT init");
}


void MqttTask(void *pData) {
    MQTT_Event_t* event = NULL;

    WatchDog_Open(WATCHDOG_SECOND_TO_TICK(60));

    semMqttStart = OS_CreateSemaphore(0);
    OS_WaitForSemaphore(semMqttStart, OS_WAIT_FOREVER);
    OS_DeleteSemaphore(semMqttStart);
    semMqttStart = NULL;

    WatchDog_KeepAlive();

    GpsInit();
    
    WatchDog_KeepAlive();
    
    MqttInit();

    WatchDog_KeepAlive();

    
    while(1) {
        switch(mqttStatus) {
            case MQTT_STATUS_DISCONNECTED:
                MqttConnect();
                break;

            case MQTT_STATUS_CONNECTED:
                MqttPublish();
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
