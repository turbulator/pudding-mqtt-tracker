#include <api_os.h>
#include <api_debug.h>
#include <api_call.h>


#include <api_hal_gpio.h>

#define DIAL_NUMBER "0123456789"

bool isDialSuccess = false;


void OnCallPressed(GPIO_INT_callback_param_t* param)
{
    Trace(1,"OnPinFalling");

    if(!CALL_Dial(DIAL_NUMBER))
    {
        Trace(1,"Make a call failed");
        return;
    }
}

void CallInit(void) {
    GPIO_config_t gpioCall = {
        .mode               = GPIO_MODE_INPUT_INT,
        .pin                = GPIO_PIN1,
        .defaultLevel       = GPIO_LEVEL_LOW,
        .intConfig.debounce = 50,
        .intConfig.type     = GPIO_INT_TYPE_FALLING_EDGE,
        .intConfig.callback = OnCallPressed
    };

    GPIO_Init(gpioCall);
}
