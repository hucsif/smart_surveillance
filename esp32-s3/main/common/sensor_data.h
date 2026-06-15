#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

//蜂鸣器告警阈值
#define LIGHT_ALARM_THRESHOLD       3000    //光照强度
#define HUMIDITY_ALARM_THRESHOLD    80      //湿度

typedef struct {
    int temperature;
    int humidity;
    int light_intensity;
    int beep_status;        //蜂鸣器状态
} sensor_data_t;


#endif