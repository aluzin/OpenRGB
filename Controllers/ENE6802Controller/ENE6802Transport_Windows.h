/* ENE6802Transport_Windows.h - PawnIO sequence transport
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "ENE6802Controller.h"
#include "i2c_smbus.h"
#include "SharedSMBusMutex.h"

class ENE6802Transport_Windows final : public ENE6802Transport
{
public:
    explicit ENE6802Transport_Windows(i2c_smbus_interface* bus);
    ~ENE6802Transport_Windows();
    bool Begin() override;
    void End() override;
    int WriteWord(unsigned char address, unsigned char command, unsigned short value) override;
    int ReadWord(unsigned char address, unsigned char command) override;
    int ReadByte(unsigned char address, unsigned char command) override;
    int WriteByte(unsigned char address, unsigned char command, unsigned char value) override;
    int WriteBlock(unsigned char address, unsigned char command, const unsigned char* data, unsigned char size) override;
private:
    int Transfer(unsigned char address, char direction, unsigned char command, int size, i2c_smbus_data* data);
    i2c_smbus_interface* bus;
    HANDLE mutex;
    DWORD owner_thread;
};
