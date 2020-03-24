#ifndef __ADC_H_
#define __ADC_H_

#define ADC_TASK_STACK_SIZE    (2048 * 4)
#define ADC_TASK_PRIORITY      1
#define ADC_TASK_NAME          "ADC Task"

extern HANDLE adcTaskHandle;

extern void adcTask(void* pData);
extern float getBattery(void);
extern float getLiion(void);

#endif /* __ADC_H_ */
