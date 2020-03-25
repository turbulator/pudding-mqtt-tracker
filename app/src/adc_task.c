#include <api_os.h>
#include <api_hal_gpio.h>
#include <api_hal_adc.h>

#include "adc_task.h"

#define ADC_STEPS  100
#define LIION_VOLTAGE_EMPTY  3.2
#define LIION_VOLTAGE_SCALE  100


static HANDLE adcTaskHandle = NULL;
static HANDLE adcSema = NULL;


static float battery = 0.0;
static float liion = 0.0;


static void AdcInit() {
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

    adcSema = OS_CreateSemaphore(1);
}


static void UpdateBattery(void){
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


static void UpdateLiion(void){
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


static void AdcTask(void* pData){
    AdcInit();
    
    while(1){
        UpdateBattery();
        UpdateLiion();
        OS_Sleep(100);
    }
}


float GetBatteryVoltage(void){
    float value;

    OS_WaitForSemaphore(adcSema, OS_WAIT_FOREVER);
        value = battery > 10.0 ? battery : 0.0;
    OS_ReleaseSemaphore(adcSema);

    return value;
}


float GetLiionVoltage(void){
    float value;

    OS_WaitForSemaphore(adcSema, OS_WAIT_FOREVER);
        value = liion;
    OS_ReleaseSemaphore(adcSema);

    return value;
}

float GetLiionLevel(void){
    float value;

    OS_WaitForSemaphore(adcSema, OS_WAIT_FOREVER);
        value = (liion - LIION_VOLTAGE_EMPTY) * LIION_VOLTAGE_SCALE;
    OS_ReleaseSemaphore(adcSema);

    return value;
}



void AdcTaskInit(void){
    adcTaskHandle = OS_CreateTask(AdcTask,
                                  NULL, NULL, ADC_TASK_STACK_SIZE, ADC_TASK_PRIORITY, 0, 0, ADC_TASK_NAME);
}
