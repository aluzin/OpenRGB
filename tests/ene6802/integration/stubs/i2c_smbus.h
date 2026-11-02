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
struct i2c_smbus_info
{
    char device_name[512] = {};
    int port_id = 0, pci_device = 0, pci_vendor = 0;
    int pci_subsystem_device = 0, pci_subsystem_vendor = 0, bus_id = 0;
};
class i2c_smbus_interface
{
public:
    i2c_smbus_info info;
    virtual ~i2c_smbus_interface() = default;
    virtual s32 i2c_smbus_xfer(u8, char, u8, int, i2c_smbus_data*) = 0;
};
