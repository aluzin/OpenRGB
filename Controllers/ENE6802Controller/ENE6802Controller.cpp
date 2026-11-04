/*---------------------------------------------------------*\
| ENE6802Controller.cpp                                    |
|   ENE 6802 static lighting, no scan or address remapping  |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/
#include "ENE6802Controller.h"

class ENE6802Sequence
{
public:
    explicit ENE6802Sequence(ENE6802Transport& transport) : transport(transport), acquired(transport.Begin()) {}
    ~ENE6802Sequence() { if(acquired) transport.End(); }
    bool Ready() const { return acquired; }
    ENE6802Sequence(const ENE6802Sequence&) = delete;
    ENE6802Sequence& operator=(const ENE6802Sequence&) = delete;
private:
    ENE6802Transport& transport;
    bool acquired;
};

ENE6802Controller::ENE6802Controller(ENE6802Transport& transport, unsigned char address) :
    transport(transport), address(address), identified(false)
{
}

bool ENE6802Controller::Reject(const char* reason, unsigned short reg, int index, int actual, int expected)
{
    diagnostic = {reason, reg, index, actual, expected};
    return false;
}

bool ENE6802Controller::Select(unsigned short reg)
{
    unsigned short word = (unsigned short)((reg << 8) | (reg >> 8));
    int result = transport.WriteWord(address, 0x00, word);
    return result == 0 || Reject("selector-io", reg, -1, result, 0);
}

bool ENE6802Controller::Read(unsigned short reg, unsigned char* data, unsigned int size)
{
    if(!Select(reg)) return false;
    for(unsigned int i = 0; i < size; i++)
    {
        int value = transport.ReadByte(address, 0x81);
        if(value < 0 || value > 255) return Reject("read-byte-io", reg, (int)i, value, -1);
        data[i] = (unsigned char)value;
    }
    return true;
}

bool ENE6802Controller::Initialize()
{
    identified = false;
    diagnostic = {"not-initialized", 0, -1, 0, 0};
    // Only explicit 7-bit DRAM RGB endpoints. Never probe the remap address 77.
    if(address < 0x70 || address > 0x76) return Reject("invalid-address", 0, -1, address, -1);
    ENE6802Sequence sequence(transport);
    if(!sequence.Ready()) return Reject("sequence-lock", 0, -1, 0, 1);
    if(!Select(0xF01C)) return false;
    // CTIIO returns DATA0 as high byte; standard SMBus returns it as low byte.
    int chip = transport.ReadWord(address, 0x82);
    if(chip < 0 || chip > 65535) return Reject("read-word-io", 0xF01C, -1, chip, -1);
    if(chip != 0x0268) return Reject("chip-mismatch", 0xF01C, -1, chip, 0x0268);
    unsigned char firmware[5] = {};
    unsigned char config[5] = {};
    const unsigned char expected_firmware[5] = {0xEE, 0xD5, 0x08, 0x03, 0x00};
    const unsigned char expected_config[5] = {1, 8, 0, 0, 0};
    if(!Read(0x87FB, firmware, 5)) return false;
    for(unsigned int i = 0; i < 5; i++)
    {
        if(firmware[i] != expected_firmware[i]) return Reject("firmware-mismatch", 0x87FB, (int)i, firmware[i], expected_firmware[i]);
    }
    if(!Read(0x80A1, config, 5)) return false;
    for(unsigned int i = 0; i < 5; i++)
    {
        if(config[i] != expected_config[i]) return Reject("config-mismatch", 0x80A1, (int)i, config[i], expected_config[i]);
    }
    diagnostic = {"ok", 0, -1, 0, 0};
    identified = true;
    return true;
}

bool ENE6802Controller::Write(unsigned short reg, const unsigned char* data, unsigned char size)
{
    if(!Select(reg)) return false;
    if(size == 1) return transport.WriteByte(address, 0x01, data[0]) == 0;
    // The recovered HAL uses SMBus block command 03, chunks of at most 24.
    // This profile never needs more than one chunk. No fallback after failure.
    return size <= 24 && transport.WriteBlock(address, 0x03, data, size) == 0;
}

bool ENE6802Controller::SetStatic(const unsigned int* colors, unsigned int count)
{
    if(colors == nullptr || count != 8) return false;
    ENE6802Sequence sequence(transport);
    if(!sequence.Ready() || !identified) return false;
    unsigned char payload[24];
    for(unsigned int i = 0; i < 8; i++)
    {
        payload[3 * i]     = (unsigned char)colors[i];
        payload[3 * i + 1] = (unsigned char)(colors[i] >> 16);
        payload[3 * i + 2] = (unsigned char)(colors[i] >> 8);
    }
    const unsigned char control[4] = {0, 1, 0, 0};
    const unsigned char brightness = 9;
    const unsigned char apply = 1;
    bool success = Write(0x8160, payload, 24) &&
                   Write(0x8020, control, 4) &&
                   Write(0x8029, &brightness, 1) &&
                   Write(0x802F, &apply, 1);
    // Do not keep writing after an uncertain partial transaction.
    if(!success) identified = false;
    return success;
}

bool ENE6802Controller::IsIdentified() const
{
    return identified;
}
