#include <api_os.h>
#include <api_debug.h>
#include <api_call.h>

#include <api_hal_gpio.h>

#include "call.h"
#include "main_task.h"
#include "secret.h"


void OnCallInt(GPIO_INT_callback_param_t* param);
void StartCallProcessTimer(void);


static GPIO_config_t gpioCallLed = {
    .mode               = GPIO_MODE_OUTPUT,
    .pin                = CALL_LED_PIN,
    .defaultLevel       = GPIO_LEVEL_LOW,
};

static GPIO_config_t gpioCallInt = {
    .mode               = GPIO_MODE_INPUT_INT,
    .pin                = CALL_INT_PIN,
    .defaultLevel       = GPIO_LEVEL_LOW,
    .intConfig = {
        .debounce       = CALL_INT_DEBOUNCE,
        .type           = GPIO_INT_TYPE_FALLING_EDGE,
        .callback       = OnCallInt
    }
};


void OnCallProcess(void* param) {
    static GPIO_LEVEL gpioCallLedLevel = GPIO_LEVEL_LOW;

    gpioCallLedLevel = (gpioCallLedLevel == GPIO_LEVEL_HIGH) ? GPIO_LEVEL_LOW : GPIO_LEVEL_HIGH;
    GPIO_SetLevel(gpioCallLed, gpioCallLedLevel);
 
    StartCallProcessTimer();
}

void StartCallProcessTimer(void) {
    OS_StartCallbackTimer(mainTaskHandle, CALL_INTERVAL, OnCallProcess, NULL);
}

void OnCallInt(GPIO_INT_callback_param_t* param) {
    Trace(1,"OnPinFalling");

    if(!CALL_Dial(DIAL_NUMBER)) {
        Trace(1,"Make a call failed");
        return;
    }

    StartCallProcessTimer();
}

void CallFinish(void) {
    CALL_HangUp();
    OS_StopCallbackTimer(mainTaskHandle, OnCallProcess, NULL);
    GPIO_SetLevel(gpioCallLed, GPIO_LEVEL_LOW);
}

void CallInit(void) {
    GPIO_Init(gpioCallInt);
    GPIO_Init(gpioCallLed);
}
