#ifndef __GPS_H_
#define __GPS_H_

#define GPS_TASK_STACK_SIZE    (2048 * 4)
#define GPS_TASK_PRIORITY      1
#define GPS_TASK_NAME          "GPS Task"

extern void GpsTaskInit(void);

#endif /* __GPS_H_ */
