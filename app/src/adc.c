#include <api_os.h>
#include <api_hal_gpio.h>
#include <api_hal_adc.h>

#include <adc.h>

#define ADC_STEPS  100


HANDLE adcTaskHandle = NULL;
HANDLE adcSema = NULL;


float battery = 0.0;
float liion = 0.0;


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

    adcSema = OS_CreateSemaphore(0);
}


void updateBattery(void){
    uint16_t value = 0, mV = 0;

    if(ADC_Read(ADC_CHANNEL_1, &value, &mV)) {
        OS_WaitForSemaphore(adcSema, OS_WAIT_FOREVER);
            if (battery == 0.0) {
                battery = ((float)mV) * 11 / 1000;
            } else {
                battery += ((((float)mV) * 11 / 1000) - battery) / ADC_STEPS;
            }
        OS_ReleaseSemaphore(adcSema);
    }
}


void updateLiion(void){
    uint16_t value = 0, mV = 0;

    if(ADC_Read(ADC_CHANNEL_0, &value, &mV)) {
        OS_WaitForSemaphore(adcSema, OS_WAIT_FOREVER);
            if (liion == 0.0) {
                liion = ((float)mV) * 3 / 1000;
            } else {
                liion += ((((float)mV) * 3 / 1000) - liion) / ADC_STEPS;
            }
        OS_ReleaseSemaphore(adcSema);
    }
}


void adcTask(void* pData){
    adcInit();
    
    while(1){
        updateBattery();
        updateLiion();
        OS_Sleep(100);
    }
}


float getBattery(void){
    float value;

    OS_WaitForSemaphore(adcSema, OS_WAIT_FOREVER);
        value = battery;
    OS_ReleaseSemaphore(adcSema);

    return value;
}


float getLiion(void){
    float value;

    OS_WaitForSemaphore(adcSema, OS_WAIT_FOREVER);
        value = liion;
    OS_ReleaseSemaphore(adcSema);

    return value;
}
