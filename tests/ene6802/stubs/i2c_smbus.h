// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
using u8 = unsigned char;
using s32 = int;
union i2c_smbus_data { u8 byte; unsigned short word; u8 block[34]; };
#define I2C_SMBUS_WRITE 0
#define I2C_SMBUS_READ 1
#define I2C_SMBUS_BYTE_DATA 2
#define I2C_SMBUS_WORD_DATA 3
#define I2C_SMBUS_BLOCK_DATA 5
class i2c_smbus_interface
{
public:
    virtual s32 i2c_smbus_xfer(u8, char, u8, int, i2c_smbus_data*) = 0;
};
