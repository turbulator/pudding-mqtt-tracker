#ifndef __ADC_H_
#define __ADC_H_

#define ADC_TASK_STACK_SIZE    (2048 * 4)
#define ADC_TASK_PRIORITY      1
#define ADC_TASK_NAME          "ADC Task"

extern void AdcTaskInit(void);
extern float GetBatteryVoltage(void);
extern float GetLiionVoltage(void);
extern float GetLiionLevel(void);

#endif /* __ADC_H_ */
