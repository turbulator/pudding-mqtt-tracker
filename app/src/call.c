#include <api_os.h>
#include <api_debug.h>
#include <api_call.h>


#include <api_hal_gpio.h>

#define DIAL_NUMBER "0123456789"


bool isCallInProgress = false;

static GPIO_config_t gpioCallState = {
    .mode               = GPIO_MODE_OUTPUT,
    .pin                = GPIO_PIN7,
    .defaultLevel       = GPIO_LEVEL_LOW,
};

void CallFinish(void) {
    CALL_HangUp();
    isCallInProgress = false;
}

void CallProcess(void) {
    static GPIO_LEVEL gpioCallStateLevel = GPIO_LEVEL_LOW;

    if (isCallInProgress) {
        gpioCallStateLevel = (gpioCallStateLevel == GPIO_LEVEL_HIGH) ? GPIO_LEVEL_LOW : GPIO_LEVEL_HIGH;
    } else {
        gpioCallStateLevel = GPIO_LEVEL_LOW;
    }

    GPIO_SetLevel(gpioCallState, gpioCallStateLevel);
}

void OnCallInt(GPIO_INT_callback_param_t* param)
{
    Trace(1,"OnPinFalling");

    if(!CALL_Dial(DIAL_NUMBER)) {
        Trace(1,"Make a call failed");
        return;
    }

    isCallInProgress = true;
}

void CallInit(void) {
    GPIO_config_t gpioCallInt = {
        .mode               = GPIO_MODE_INPUT_INT,
        .pin                = GPIO_PIN1,
        .defaultLevel       = GPIO_LEVEL_LOW,
        .intConfig.debounce = 50,
        .intConfig.type     = GPIO_INT_TYPE_FALLING_EDGE,
            .intConfig.callback = OnCallInt
    };

    GPIO_Init(gpioCallInt);
    GPIO_Init(gpioCallState);
}
