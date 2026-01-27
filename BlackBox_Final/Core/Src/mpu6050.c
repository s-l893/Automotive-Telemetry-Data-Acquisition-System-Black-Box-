#include "mpu6050.h"

#define MPU6050_ADDR 0x68 << 1
#define PWR_MGMT_1   0x6B
#define ACCEL_XOUT_H 0x3B

void MPU6050_Init(I2C_HandleTypeDef *hi2c) {
    uint8_t check;
    uint8_t data = 0;
    // Wake up the sensor (Power Management 1)
    HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, PWR_MGMT_1, 1, &data, 1, 100);
}

void MPU6050_Read_Accel(I2C_HandleTypeDef *hi2c, MPU6050_t *DataStruct) {
    uint8_t Rec_Data[6];
    // Read 6 bytes of data starting from ACCEL_XOUT_H
    HAL_I2C_Mem_Read(hi2c, MPU6050_ADDR, ACCEL_XOUT_H, 1, Rec_Data, 6, 100);

    // Convert raw bits to G-force (Assuming +/- 2g range)
    DataStruct->Ax = (int16_t)(Rec_Data[0] << 8 | Rec_Data[1]) / 16384.0;
    DataStruct->Ay = (int16_t)(Rec_Data[2] << 8 | Rec_Data[3]) / 16384.0;
    DataStruct->Az = (int16_t)(Rec_Data[4] << 8 | Rec_Data[5]) / 16384.0;
}
