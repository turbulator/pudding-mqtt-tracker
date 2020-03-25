#include <api_os.h>
#include <api_hal_gpio.h>
#include <api_gps.h>

#include "gps_task.h"
#include "gps_parse.h"
#include "gps.h"


HANDLE gpsTaskHandle = NULL;


void GpsInit(void){
    GPS_Info_t* gpsInfo = Gps_GetInfo();

    // Open GPS hardware(UART2 open either)
    GPS_Init();
    GPS_Open(NULL);

    // Wait for gps start up, or gps will not response command
    while(gpsInfo->rmc.latitude.value == 0)
        OS_Sleep(1000);

    // Set gps nmea output interval
    for(uint8_t i = 0; i < 5; i++){
        bool ret = GPS_SetOutputInterval(10000);
        if(ret)
            break;
        OS_Sleep(1000);
    }

    // Set fix mode
    if(!GPS_SetFixMode(GPS_FIX_MODE_NORMAL))
        Trace(1,"set fix mode fail");

    // Set GPS + GLONASS
    if(!GPS_SetSearchMode(true, true, false, false))
        Trace(1,"set search mode fail");

    // Enable SBAS
    if(!GPS_SetSBASEnable(true))
        Trace(1,"enable sbas fail");

    // Set power safe mode
    if(!GPS_SetLpMode(GPS_LP_MODE_SUPPER_LP))
        Trace(1, "set gps lp mode fail");

    // Set output interval
    if(!GPS_SetOutputInterval(1000))
        Trace(1, "set nmea output interval fail");
}


void GpsTask(void* pData){
    // Initialize the HW
    GpsInit();

    while(1){
        // Just sleep
        OS_Sleep(1000);
    }
}


void GpsTaskInit(void) {
    gpsTaskHandle = OS_CreateTask(GpsTask,
                                  NULL, NULL, GPS_TASK_STACK_SIZE, GPS_TASK_PRIORITY, 0, 0, GPS_TASK_NAME);
}
