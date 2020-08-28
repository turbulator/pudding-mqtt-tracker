#ifndef __CALL_TASK_H_
#define __CALL_TASK_H_

#define CALL_INTERVAL              200
#define CALL_INT_DEBOUNCE          50
#define CALL_INT_PIN               GPIO_PIN1
#define CALL_LED_PIN               GPIO_PIN7


void CallInit(void);
void CallFinish(void);

#endif /* __CALL_TASK_H_ */
