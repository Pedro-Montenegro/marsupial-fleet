#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>

#define VL53L8CX_RdMulti VL53L8CX_ReadMulti
#define VL53L8CX_WrMulti VL53L8CX_WriteMulti

typedef struct {
    int fd;
    uint8_t address;
} VL53L8CX_Platform;

int8_t VL53L8CX_WriteMulti(
        VL53L8CX_Platform *p_platform,
        uint16_t reg,
        uint8_t *pdata,
        uint32_t count);

int8_t VL53L8CX_ReadMulti(
        VL53L8CX_Platform *p_platform,
        uint16_t reg,
        uint8_t *pdata,
        uint32_t count);

int8_t VL53L8CX_WrByte(
    VL53L8CX_Platform *p_platform,
    uint16_t register_address,
    uint8_t value);

int8_t VL53L8CX_RdByte(
    VL53L8CX_Platform *p_platform,
    uint16_t register_address,
    uint8_t *value);

uint8_t VL53L8CX_WaitMs(
    VL53L8CX_Platform *p_platform,
    uint32_t TimeMs);

void VL53L8CX_SwapBuffer(uint8_t *buffer, uint16_t size);

#endif