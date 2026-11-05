#include "platform.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <string.h>

int8_t VL53L8CX_WriteMulti(
        VL53L8CX_Platform *p_platform,
        uint16_t reg,
        uint8_t *pdata,
        uint32_t count)
{

    const uint16_t max_chunk = 256;
    uint32_t offset = 0;

    while(offset < count)
    {

        uint16_t chunk = count - offset;

        if(chunk > max_chunk)
            chunk = max_chunk;

        uint8_t buffer[chunk + 2];

        buffer[0] = (reg + offset) >> 8;
        buffer[1] = (reg + offset) & 0xFF;

        memcpy(&buffer[2], &pdata[offset], chunk);

        struct i2c_rdwr_ioctl_data packets;
        struct i2c_msg message;

        message.addr  = p_platform->address;
        message.flags = 0;
        message.len   = chunk + 2;
        message.buf   = buffer;

        packets.msgs  = &message;
        packets.nmsgs = 1;

        if(ioctl(p_platform->fd, I2C_RDWR, &packets) < 0)
        {
            perror("I2C WriteMulti");
            return -1;
        }

        offset += chunk;
    }

    return 0;
}

int8_t VL53L8CX_ReadMulti(
        VL53L8CX_Platform *p_platform,
        uint16_t reg,
        uint8_t *pdata,
        uint32_t count)
{

    uint8_t addr[2];

    addr[0] = reg >> 8;
    addr[1] = reg & 0xFF;

    struct i2c_rdwr_ioctl_data packets;
    struct i2c_msg messages[2];

    messages[0].addr  = p_platform->address;
    messages[0].flags = 0;
    messages[0].len   = 2;
    messages[0].buf   = addr;

    messages[1].addr  = p_platform->address;
    messages[1].flags = I2C_M_RD;
    messages[1].len   = count;
    messages[1].buf   = pdata;

    packets.msgs  = messages;
    packets.nmsgs = 2;

    if(ioctl(p_platform->fd, I2C_RDWR, &packets) < 0)
    {
        perror("I2C ReadMulti");
        return -1;
    }

    return 0;
}

int8_t VL53L8CX_WrByte(
    VL53L8CX_Platform *p_platform,
    uint16_t register_address,
    uint8_t value)
{
    return VL53L8CX_WriteMulti(
        p_platform,
        register_address,
        &value,
        1);
}

int8_t VL53L8CX_RdByte(
    VL53L8CX_Platform *p_platform,
    uint16_t register_address,
    uint8_t *value)
{
    return VL53L8CX_ReadMulti(
        p_platform,
        register_address,
        value,
        1);
}

uint8_t VL53L8CX_WaitMs(
    VL53L8CX_Platform *p_platform,
    uint32_t TimeMs)
{
    usleep(TimeMs * 1000);
    return 0;
}

void VL53L8CX_SwapBuffer(uint8_t *buffer, uint16_t size)
{
    uint32_t i;
    uint8_t tmp;

    for(i = 0; i < size; i += 4)
    {
        tmp = buffer[i];
        buffer[i] = buffer[i+3];
        buffer[i+3] = tmp;

        tmp = buffer[i+1];
        buffer[i+1] = buffer[i+2];
        buffer[i+2] = tmp;
    }
}