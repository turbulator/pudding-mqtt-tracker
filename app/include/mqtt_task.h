#ifndef __MQTT_TASK_H_
#define __MQTT_TASK_H_

#define MQTT_TASK_STACK_SIZE       (2048 * 4)
#define MQTT_TASK_PRIORITY         1
#define MQTT_TASK_NAME             "MQTT Task"

#define MQTT_INTERVAL              1000
#define MQTT_WATCHDOG_INTERVAL     2 * 60

#define MQTT_TRACKER_TOPIC_FORMAT  "vehicle/%s/tracker"
#define MQTT_STATE_TOPIC_FORMAT    "vehicle/%s/state"
#define MQTT_BATTERY_TOPIC_FORMAT  "vehicle/%s/battery"
#define MQTT_LIION_TOPIC_FORMAT    "vehicle/%s/liion"
#define MQTT_SPEED_TOPIC_FORMAT    "vehicle/%s/speed"

#define MQTT_PAYLOAD_STATE_ONLINE  "online"
#define MQTT_PAYLOAD_STATE_OFFLINE "offline"


typedef enum {
    MQTT_STATUS_DISCONNECTED = 0,
    MQTT_STATUS_CONNECTING,
    MQTT_STATUS_CONNECTED,
    MQTT_STATUS_ONLINE,
    MQTT_STATUS_LOCATION_PUBLISHED,
    MQTT_STATUS_OFFLINE,
    MQTT_STATUS_MAX
} MQTT_Status_t;


extern HANDLE semMqttStart;

void MqttPublishState(char *mqttStatePayload);
MQTT_Status_t getMqttState(void);
void MqttTaskInit(void);


#endif /* __MQTT_TASK_H_ */
