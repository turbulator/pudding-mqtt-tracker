#include <string.h>
#include <stdio.h>
#include <api_os.h>
#include <api_gps.h>
#include <api_event.h>
#include <api_hal_uart.h>
#include <api_debug.h>
#include "buffer.h"
#include "gps_parse.h"
#include "math.h"
#include "gps.h"
#include "api_hal_pm.h"
#include "api_mqtt.h"
#include "api_key.h"
#include "time.h"
#include "api_info.h"
#include "assert.h"
#include "api_socket.h"
#include "api_network.h"
#include "api_hal_gpio.h"
#include "api_hal_adc.h"
#include "api_hal_pm.h"
#include "api_hal_watchdog.h"

#include "secret.h"
#include "adc_task.h"

#define MAIN_TASK_STACK_SIZE    (2048 * 4)
#define MAIN_TASK_PRIORITY      0
#define MAIN_TASK_NAME          "Main Task"

#define MQTT_TASK_STACK_SIZE    (2048 * 4)
#define MQTT_TASK_PRIORITY      1
#define MQTT_TASK_NAME          "MQTT Task"


#define STATUS_LED_INTERVAL 100
#define STATUS_LED GPIO_PIN27
#define DATA_LED GPIO_PIN28

#define MQTT_RECONNECT_INTERVAL 10000
#define MQTT_PUBLISH_INTERVAL 10000


static HANDLE mainTaskHandle = NULL;
static HANDLE mqttTaskHandle = NULL;
static HANDLE semMqttStart = NULL;

uint8_t imei[16] = "";
uint8_t buffer[1024] = "";

MQTT_Connect_Info_t ci;
char mqttLocationTopic[64] = "";
char mqttStateTopic[64] = "";
char mqttBatteryTopic[64] = "";
char mqttLiionTopic[64] = "";
char mqttBuffer[1024] = "";


#define MQTT_PAYLOAD_STATE_ONLINE "online"
#define MQTT_PAYLOAD_STATE_OFFLINE "offline"



typedef enum {
    MQTT_EVENT_CONNECTED = 0,
    MQTT_EVENT_DISCONNECTED,
    MQTT_EVENT_MAX
} MQTT_Event_ID_t;

typedef struct {
    MQTT_Event_ID_t id;
    MQTT_Client_t* client;
} MQTT_Event_t;

typedef enum {
    MQTT_STATUS_DISCONNECTED = 0,
    MQTT_STATUS_CONNECTED,
    MQTT_STATUS_MAX
} MQTT_Status_t;

MQTT_Status_t mqttStatus = MQTT_STATUS_DISCONNECTED;


void StartTimerConnect(MQTT_Client_t* client);
void StartTimerPublish(MQTT_Client_t* client);

    GPIO_config_t stateLed = {
        .mode         = GPIO_MODE_OUTPUT,
        .pin          = GPIO_PIN28,
        .defaultLevel = GPIO_LEVEL_HIGH
    };

    GPIO_config_t powerLed = {
        .mode         = GPIO_MODE_OUTPUT,
        .pin          = GPIO_PIN27,
        .defaultLevel = GPIO_LEVEL_HIGH
    };
    
    GPIO_config_t powerSupport = {
        .mode         = GPIO_MODE_OUTPUT,
        .pin          = GPIO_PIN29,
        .defaultLevel = GPIO_LEVEL_HIGH
    };


void CheckExternalPower()
{
	static int countdown = 6;
    uint16_t value = 0, mV = 0;
    
    uint8_t percent;
    uint16_t v = PM_Voltage(&percent);
    Trace(1,"voltage: %dmV, %d percent", v, percent);
	
	if(ADC_Read(ADC_CHANNEL_1, &value, &mV)) {
        Trace(1,"Countdown: %d, ADC value:%d, %dV", countdown, value, mV * 11 / 1000);
        if(value > 500) {
			countdown = 6;
		} else {
			if (--countdown < 0) {
			    GPS_Close();
                GPIO_SetLevel(powerLed, GPIO_LEVEL_LOW);
                GPIO_SetLevel(powerSupport, GPIO_LEVEL_LOW);
                OS_Sleep(10000);
			}
		}	
	}
}


bool AttachActivate()
{
    uint8_t status;
    bool ret = Network_GetAttachStatus(&status);

    if(!ret) {
        Trace(2, "get attach status fail");
        return false;
    }

    Trace(2, "attach status: %d", status);

    if(!status) {
        ret = Network_StartAttach();

        if(!ret) {
            Trace(2, "network attach fail");
            return false;
        }
    } else {
        ret = Network_GetActiveStatus(&status);

        if(!ret) {
            Trace(2, "get activate staus fail");
            return false;
        }
        
        Trace(2,"activate status:%d",status);
        
        if(!status) {
            Network_PDP_Context_t context = {
                .apn = PDP_CONTEXT_APN,
                .userName = PDP_CONTEXT_USERNAME,
                .userPasswd = PDP_CONTEXT_PASSWD
            };
            Network_StartActive(context);
        }
    }

    return true;
}


void EventDispatch(API_Event_t* pEvent)
{
    switch(pEvent->id) {
        case API_EVENT_ID_NO_SIMCARD:
            Trace(10,"!!NO SIM CARD%d!!!!",pEvent->param1);
            break;

        case API_EVENT_ID_NETWORK_REGISTER_DENIED:
            Trace(2,"network register denied");

        case API_EVENT_ID_NETWORK_REGISTER_NO:
            Trace(2,"network register no");
            break;

        case API_EVENT_ID_NETWORK_REGISTERED_HOME:
        case API_EVENT_ID_NETWORK_REGISTERED_ROAMING:
        case API_EVENT_ID_NETWORK_DETACHED:
        case API_EVENT_ID_NETWORK_ATTACH_FAILED:
        case API_EVENT_ID_NETWORK_ATTACHED:
        case API_EVENT_ID_NETWORK_DEACTIVED:
        case API_EVENT_ID_NETWORK_ACTIVATE_FAILED:
            AttachActivate();
            break;

        case API_EVENT_ID_NETWORK_ACTIVATED:
            Trace(2,"network activate success");
            if(semMqttStart)
                OS_ReleaseSemaphore(semMqttStart);
            break;

        case API_EVENT_ID_GPS_UART_RECEIVED:
            // Trace(1, "received GPS data, length:%d, data:%s, flag:%d", pEvent->param1, pEvent->pParam1, flag);
            GPS_Update(pEvent->pParam1, pEvent->param1);
            break;

        default:
            break;
    }
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


void OnMqttConnection(MQTT_Client_t *client, void *arg, MQTT_Connection_Status_t status)
{
    Trace(1,"MQTT connection status: %d", status);
    MQTT_Event_t* event = (MQTT_Event_t*)OS_Malloc(sizeof(MQTT_Event_t));

    if(!event) {
        Trace(1,"MQTT no memory");
        return;
    }

    if(status == MQTT_CONNECTION_ACCEPTED) {
        Trace(1,"MQTT succeed connect to broker");
        //!!! DO NOT suscribe here(interrupt function), do MQTT suscribe in task, or it will not excute
        event->id = MQTT_EVENT_CONNECTED;
        event->client = client;
        OS_SendEvent(mqttTaskHandle, event, OS_TIME_OUT_WAIT_FOREVER, OS_EVENT_PRI_NORMAL);
    } else {
        event->id = MQTT_EVENT_DISCONNECTED;
        event->client = client;
        OS_SendEvent(mqttTaskHandle, event, OS_TIME_OUT_WAIT_FOREVER, OS_EVENT_PRI_NORMAL);
        Trace(1,"MQTT connect to broker fail, error code: %d", status);
    }

    Trace(1,"MQTT OnMqttConnection() end");
}


void OnTimerStartConnect(void* param)
{
    MQTT_Error_t err;
    MQTT_Client_t* client = (MQTT_Client_t*)param;

    if(mqttStatus == MQTT_STATUS_CONNECTED) {
        Trace(1,"already connected!");
        return;
    }
    
    CheckExternalPower();

    err = MQTT_Connect(client, BROKER_IP, BROKER_PORT, OnMqttConnection, NULL, &ci);

    if(err != MQTT_ERROR_NONE) {
        Trace(1, "MQTT connect fail, error code: %d", err);
        StartTimerConnect(client);
    }

    WatchDog_KeepAlive();
}


void StartTimerConnect(MQTT_Client_t* client)
{
	char ip[16];

    if(Network_GetIp(ip, sizeof(ip))) {
        Trace(1, "network local ip: %s", ip);
    }

    OS_StartCallbackTimer(mainTaskHandle, MQTT_RECONNECT_INTERVAL, OnTimerStartConnect, (void*)client);
}


void OnPublishState(void* arg, MQTT_Error_t err)
{
    if(err == MQTT_ERROR_NONE)
        Trace(1,"MQTT publish state success");
    else
        Trace(1,"MQTT publish state error, error code: %d",err);
}


void MqttPublishState(MQTT_Client_t* client)
{
        MQTT_Error_t err = MQTT_Publish(client, mqttStateTopic, MQTT_PAYLOAD_STATE_ONLINE, strlen(MQTT_PAYLOAD_STATE_ONLINE), 1, 2, 0, OnPublishState, NULL);

        if(err != MQTT_ERROR_NONE)
            Trace(1,"MQTT publish state error, error code: %d", err);
}


void OnPublishBattery(void* arg, MQTT_Error_t err)
{
    if(err == MQTT_ERROR_NONE)
        Trace(1,"MQTT publish battery success");
    else
        Trace(1,"MQTT publish battery error, error code: %d", err);
}


void MqttPublishBattery(MQTT_Client_t* client)
{
    Trace(1, "MqttPublishBattery");

    snprintf(mqttBuffer, sizeof(mqttBuffer), "%.02f", GetBatteryVoltage());

    MQTT_Error_t err = MQTT_Publish(client, mqttBatteryTopic, mqttBuffer, strlen(mqttBuffer), 1, 2, 0, OnPublishBattery, NULL);

    if(err != MQTT_ERROR_NONE)
        Trace(1,"MQTT publish location error, error code: %d", err);
}


void OnPublishLiion(void* arg, MQTT_Error_t err)
{
    if(err == MQTT_ERROR_NONE)
        Trace(1,"MQTT publish liion success");
    else
        Trace(1,"MQTT publish liion error, error code: %d", err);
}


void MqttPublishLiion(MQTT_Client_t* client)
{
    Trace(1, "MqttPublishLiion");

    snprintf(mqttBuffer, sizeof(mqttBuffer), "%.02f", GetLiionVoltage());

    MQTT_Error_t err = MQTT_Publish(client, mqttLiionTopic, mqttBuffer, strlen(mqttBuffer), 1, 2, 0, OnPublishLiion, NULL);

    if(err != MQTT_ERROR_NONE)
        Trace(1,"MQTT publish location error, error code: %d", err);
}


void OnPublishLocation(void* arg, MQTT_Error_t err)
{
    if(err == MQTT_ERROR_NONE)
        Trace(1,"MQTT publish location success");
    else
        Trace(1,"MQTT publish location error, error code: %d", err);
}


void MqttPublishLocation(MQTT_Client_t* client)
{
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

        snprintf(mqttBuffer, sizeof(mqttBuffer), "{\"longitude\": %f,\"latitude\": %f}", 
                                                            longitude, latitude);


        MQTT_Error_t err = MQTT_Publish(client, mqttLocationTopic, mqttBuffer, strlen(mqttBuffer), 1, 2, 0, OnPublishLocation, NULL);


        snprintf(mqttBuffer,sizeof(mqttBuffer),"GPS fix mode:%d, BDS fix mode:%d, fix quality:%d, satellites tracked:%d, gps sates total:%d, is fixed:%s, coordinate:WGS84, Latitude:%f, Longitude:%f, unit:degree, altitude:%f",gpsInfo->gsa[0].fix_type, gpsInfo->gsa[1].fix_type,
                                                                 gpsInfo->gga.fix_quality,gpsInfo->gga.satellites_tracked, gpsInfo->gsv[0].total_sats, isFixedStr, latitude,longitude,gpsInfo->gga.altitude);
       
        err = MQTT_Publish(client, mqttLocationTopic, mqttBuffer, strlen(mqttBuffer), 1, 2, 0, OnPublishLocation, NULL);


        if(err != MQTT_ERROR_NONE)
            Trace(1,"MQTT publish location error, error code: %d", err);
    }
}

void OnTimerPublish(void* param)
{
    MQTT_Client_t* client = (MQTT_Client_t*)param;

    CheckExternalPower();

    //MqttPublishBattery(client);
    //MqttPublishLiion(client);

    MqttPublishLocation(client); 

    OS_StartCallbackTimer(mainTaskHandle, MQTT_PUBLISH_INTERVAL, OnTimerPublish, (void*)client);
    WatchDog_KeepAlive();
}


void StartTimerPublish(MQTT_Client_t* client)
{
    OS_StartCallbackTimer(mainTaskHandle, MQTT_PUBLISH_INTERVAL, OnTimerPublish, (void*)client);
}


void MqttInit()
{
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

    err = MQTT_Connect(client, BROKER_IP, BROKER_PORT, OnMqttConnection, NULL, &ci);

    if(err != MQTT_ERROR_NONE)
        Trace(1,"MQTT connect fail, error code: %d", err);
}




/**
 * The proc for handling MQTT events
 *
 * @param  pEvent The pointer to MQTT_Event_t structure
 */
void MqttTaskEventDispatch(MQTT_Event_t* pEvent)
{
	
    switch(pEvent->id) {
        case MQTT_EVENT_CONNECTED:
            mqttStatus = MQTT_STATUS_CONNECTED;
            StartTimerPublish(pEvent->client);
            MqttPublishState(pEvent->client);
            MqttPublishBattery(pEvent->client);
            MqttPublishLiion(pEvent->client);
            break;

        case MQTT_EVENT_DISCONNECTED:
            mqttStatus = MQTT_STATUS_DISCONNECTED;
            // TODO: We should stop publishing while we reconnecting to broker?
            // StopTimerPublish()
            StartTimerConnect(pEvent->client);
            break;

        default:
            break;
    }
}

/**
 * The MQTT task
 *
 * Init GPS receiver and MQTT client. 
 * Handle events from MQTT callbacks.
 *
 * @param  pData Parameter passed in when this function is called
 */
void app_MqttTask(void *pData)
{
    MQTT_Event_t* event = NULL;

    WatchDog_Open(WATCHDOG_SECOND_TO_TICK(60));

    semMqttStart = OS_CreateSemaphore(0);
    OS_WaitForSemaphore(semMqttStart, OS_WAIT_FOREVER);
    OS_DeleteSemaphore(semMqttStart);
    semMqttStart = NULL;

    WatchDog_KeepAlive();

    MqttInit();

    WatchDog_KeepAlive();

    while(1) {
        if(OS_WaitEvent(mqttTaskHandle, (void**)&event, OS_TIME_OUT_WAIT_FOREVER)) {
            MqttTaskEventDispatch(event);
            OS_Free(event);
        }
    }
}


/**
 * The Main task
 *
 * Init HW and run MQTT task. 
 *
 * @param  pData Parameter passed in when this function is called
 */
void app_MainTask(void *pData)
{
    API_Event_t* event=NULL;

    Trace(1, "Main Task started");

    TIME_SetIsAutoUpdateRtcTime(true); // Local time will be synced with GPS

    PM_PowerEnable(POWER_TYPE_VPAD, true); // GPIO0  ~ GPIO7  and GPIO25 ~ GPIO36    2.8V
    PM_SetSysMinFreq(PM_SYS_FREQ_312M);
    
    GPIO_Init(stateLed);
    GPIO_Init(powerLed);
    GPIO_Init(powerSupport);
    
    // Create GPS task
    GpsTaskInit();

    // Create ADC task
    AdcTaskInit();
    
    // Create MQTT task
    mqttTaskHandle = OS_CreateTask(app_MqttTask,
                                    NULL, NULL, MQTT_TASK_STACK_SIZE, MQTT_TASK_PRIORITY, 0, 0, MQTT_TASK_NAME);

    // Wait and process system events
    while(1) {
        if(OS_WaitEvent(mainTaskHandle, (void**)&event, OS_TIME_OUT_WAIT_FOREVER)) {
            EventDispatch(event);
            OS_Free(event->pParam1);
            OS_Free(event->pParam2);
            OS_Free(event);
        }
    }
}

/**
 * The entry point of application. Just create the main task.
 */
void app_Main(void)
{
    mainTaskHandle = OS_CreateTask(app_MainTask,
        NULL, NULL, MAIN_TASK_STACK_SIZE, MAIN_TASK_PRIORITY, 0, 0, MAIN_TASK_NAME);
    OS_SetUserMainHandle(&mainTaskHandle);
}

