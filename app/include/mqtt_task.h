#ifndef __MQTT_TASK_H_
#define __MQTT_TASK_H_

#define MQTT_TASK_STACK_SIZE       (2048 * 4)
#define MQTT_TASK_PRIORITY         1
#define MQTT_TASK_NAME             "MQTT Task"

#define MQTT_INTERVAL              5000

#define MQTT_PAYLOAD_STATE_ONLINE  "online"
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
    MQTT_STATUS_CONNECTING,
    MQTT_STATUS_CONNECTED,
    MQTT_STATUS_ONLINE,
    MQTT_STATUS_LOCATION_PUBLISHED,
    MQTT_STATUS_OFFLINE
    MQTT_STATUS_MAX
} MQTT_Status_t;


extern HANDLE semMqttStart;

void GpsInit(void);
void MqttPublishState(char *mqttStatePayload);
MQTT_Status_t getMqttState(void);
void MqttTaskInit(void);


#endif /* __MQTT_TASK_H_ */
