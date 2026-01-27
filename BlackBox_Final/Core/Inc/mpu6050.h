#ifndef MPU6050_H_
#define MPU6050_H_

#include "stm32f4xx_hal.h"

// Struct to hold sensor data
typedef struct {
    float Ax, Ay, Az;
    float Gx, Gy, Gz;
    float Temperature;
} MPU6050_t;

// Function Prototypes
void MPU6050_Init(I2C_HandleTypeDef *hi2c);
void MPU6050_Read_Accel(I2C_HandleTypeDef *hi2c, MPU6050_t *DataStruct);

#endif
