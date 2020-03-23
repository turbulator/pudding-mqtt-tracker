#include <api_os.h>
#include <api_hal_gpio.h>
#include <api_hal_adc.h>

#include <adc.h>

#define ADC_STEPS  100

HANDLE adcTaskHandle = NULL;

float battery = 0.0;


void adcInit() {
    ADC_Config_t config0 = {
        .channel = ADC_CHANNEL_0,
        .samplePeriod = ADC_SAMPLE_PERIOD_1MS
    };

    ADC_Config_t config1 = {
        .channel = ADC_CHANNEL_1,
        .samplePeriod = ADC_SAMPLE_PERIOD_1MS
    };

    ADC_Init(config0);
    ADC_Init(config1);
}


void updateBatteryVoltage(void){
    uint16_t value = 0, mV = 0;

    if(ADC_Read(ADC_CHANNEL_0, &value, &mV)) {
        if (battery == 0.0) {
            battery = ((float)mV) * 3 / 1000;
        } else {
            battery += ((((float)mV) * 3 / 1000) - battery) / ADC_STEPS;
        }
    }
}


void adcTask(void* pData){
    adcInit();
    
    while(1){
        updateBatteryVoltage();
        OS_Sleep(100);
    }
}
