#include <api_os.h>
#include <api_hal_gpio.h>
#include <api_gps.h>

#include "pm_task.h"


static HANDLE pmTaskHandle = NULL;
static GPIO_LEVEL ign = 0;
static PM_State_t state = PM_STATE_STANDBY;
static int shutdownCounter = PM_SHUTDOWN_COUNTER_MAX;

static GPIO_config_t gpioIgn = {
    .mode               = GPIO_MODE_INPUT,
    .pin                = GPIO_PIN2,
    .defaultLevel       = GPIO_LEVEL_HIGH,
    .intConfig.debounce = 0,
    .intConfig.type     = GPIO_INT_TYPE_MAX,
    .intConfig.callback = NULL
};

static GPIO_config_t gpioPower = {
    .mode               = GPIO_MODE_OUTPUT,
    .pin                = GPIO_PIN30,
    .defaultLevel       = GPIO_LEVEL_HIGH
};


void PmShutdown(void) {
    // TODO: send offline mqtt message
    GPIO_SetLevel(gpioPower, GPIO_LEVEL_LOW);
    OS_Sleep(10000);
}


void UpdateIgn(void) {
    // Read GPIO state
    GPIO_GetLevel(gpioIgn, &ign);

    if (ign == GPIO_LEVEL_LOW) {
        // Reset countdown counter if IGN is ON
        shutdownCounter = PM_SHUTDOWN_COUNTER_MAX;
    }

    switch (state) {
        case PM_STATE_STANDBY:
            if (ign == GPIO_LEVEL_LOW) {
                // Once we've got IGN we should go to ON state
                state = PM_STATE_ACTIVE;
            } else if (getMqttState() == 2 /*MQTT_STATUS_LOCATION_PUBLISHED*/) {
                // We are in STANDBY state and we've already published own lacation
                // Where are no reason to be active - just do shutdown
                PmShutdown();
            }
            break;

        case PM_STATE_ACTIVE:
            break;

        defaut:
            break;
    }

    if (--shutdownCounter == 0) {
        // The IGN has lost for a long time
        // Save battery - just do shutdown
        PmShutdown();
    }
}


void PmInit(void) {
    GPIO_Init(gpioIgn);
    GPIO_Init(gpioPower);
}


static void PmTask(void* pData) {
    PmInit();

    while(1) {
        UpdateIgn();
        OS_Sleep(PM_UPDATE_IGN_INTERVAL);
    }
}


void PmTaskInit(void) {
    pmTaskHandle = OS_CreateTask(PmTask,
                                 NULL, NULL, PM_TASK_STACK_SIZE, PM_TASK_PRIORITY, 0, 0, PM_TASK_NAME);
}
