// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
using u8 = unsigned char;
using s32 = int;
union i2c_smbus_data { u8 byte; unsigned short word; u8 block[34]; };
struct i2c_smbus_interface {
    struct { int pci_vendor, pci_device, pci_subsystem_vendor, pci_subsystem_device; char device_name[512]; } info;
};
