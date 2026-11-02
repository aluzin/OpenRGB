/* ENE6802Transport_Windows.cpp - serialize complete ENE register sequences
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "ENE6802Transport_Windows.h"

ENE6802Transport_Windows::ENE6802Transport_Windows(i2c_smbus_interface* bus) :
    bus(bus), mutex(CreateSharedSMBusMutex()), owner_thread(0)
{
}

ENE6802Transport_Windows::~ENE6802Transport_Windows()
{
    if(mutex) CloseHandle(mutex);
}

bool ENE6802Transport_Windows::Begin()
{
    if(!mutex) return false;
    DWORD result = WaitForSingleObject(mutex, 3000);
    if(result == WAIT_ABANDONED)
    {
        // Ownership was acquired, but peripheral state is uncertain.
        ReleaseMutex(mutex);
        return false;
    }
    if(result != WAIT_OBJECT_0) return false;
    owner_thread = GetCurrentThreadId();
    return true;
}

void ENE6802Transport_Windows::End()
{
    if(owner_thread == GetCurrentThreadId())
    {
        owner_thread = 0;
        ReleaseMutex(mutex);
    }
}

int ENE6802Transport_Windows::Transfer(unsigned char address, char direction, unsigned char command, int size, i2c_smbus_data* data)
{
    if(owner_thread != GetCurrentThreadId()) return -1;
    // Detector restricts this adapter to synchronous PawnIO i801. Its raw xfer
    // takes the SAME recursive Windows mutex on this thread. Do not acquire
    // the per-bus wrapper mutex here: other callers acquire it BEFORE the
    // shared mutex, which would invert lock order. All I/O is serialized by
    // the shared mutex, held for the entire selector/data/apply sequence.
    return bus->i2c_smbus_xfer(address, direction, command, size, data);
}

int ENE6802Transport_Windows::WriteWord(unsigned char address, unsigned char command, unsigned short value)
{
    i2c_smbus_data data = {};
    data.word = value;
    return Transfer(address, I2C_SMBUS_WRITE, command, I2C_SMBUS_WORD_DATA, &data);
}

int ENE6802Transport_Windows::ReadWord(unsigned char address, unsigned char command)
{
    i2c_smbus_data data = {};
    return Transfer(address, I2C_SMBUS_READ, command, I2C_SMBUS_WORD_DATA, &data) == 0 ? data.word : -1;
}

int ENE6802Transport_Windows::ReadByte(unsigned char address, unsigned char command)
{
    i2c_smbus_data data = {};
    return Transfer(address, I2C_SMBUS_READ, command, I2C_SMBUS_BYTE_DATA, &data) == 0 ? data.byte : -1;
}

int ENE6802Transport_Windows::WriteByte(unsigned char address, unsigned char command, unsigned char value)
{
    i2c_smbus_data data = {};
    data.byte = value;
    return Transfer(address, I2C_SMBUS_WRITE, command, I2C_SMBUS_BYTE_DATA, &data);
}

int ENE6802Transport_Windows::WriteBlock(unsigned char address, unsigned char command, const unsigned char* bytes, unsigned char size)
{
    if(bytes == nullptr || size == 0 || size > 24) return -1;
    i2c_smbus_data data = {};
    data.block[0] = size;
    for(unsigned int i = 0; i < size; i++) data.block[i + 1] = bytes[i];
    return Transfer(address, I2C_SMBUS_WRITE, command, I2C_SMBUS_BLOCK_DATA, &data);
}
