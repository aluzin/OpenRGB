// SPDX-License-Identifier: GPL-2.0-or-later
// Entire production protocol implementation, fake transport only; no OS imports.
#include "../../Controllers/ENE6802Controller/ENE6802Controller.h"
struct Event { int type, address, command, value, size; unsigned char data[24]; };
static int failures = 0;
extern "C" int _purecall() { failures++; return 0; }
static void Check(bool condition) { if(!condition) failures++; }
static bool Equal(const char* a, const char* b)
{
    while(*a && *a == *b) { a++; b++; }
    return *a == *b;
}
static void Diagnostic(const ENE6802Controller& device, const char* reason, int reg, int index, int actual, int expected)
{
    const ENE6802Diagnostic& d = device.GetDiagnostic();
    Check(Equal(d.reason, reason) && d.reg == reg && d.index == index && d.actual == actual && d.expected == expected);
}
class Fake : public ENE6802Transport
{
public:
    Event events[64] = {};
    int count = 0, fail_at = -1, begins = 0, ends = 0, next_byte = 0;
    bool held = false, blocked = false;
    int chip = 0x0268;
    unsigned char bytes[10] = {0xEE, 0xD5, 8, 3, 0, 1, 8, 0, 0, 0};
    bool Begin() override { begins++; if(blocked) return false; Check(!held); held = true; return true; }
    void End() override { Check(held); held = false; ends++; }
    int Record(int type, int address, int command, int value, const unsigned char* data = nullptr, int size = 0)
    {
        Check(held);
        if(count >= 64) { Check(false); return -1; }
        Event& event = events[count++];
        event.type = type; event.address = address; event.command = command;
        event.value = value; event.size = size;
        for(int i = 0; i < size; i++) event.data[i] = data[i];
        return count - 1 == fail_at ? -1 : 0;
    }
    int WriteWord(unsigned char a, unsigned char c, unsigned short v) override { return Record(0,a,c,v); }
    int ReadWord(unsigned char a, unsigned char c) override { return Record(1,a,c,0) == 0 ? chip : -1; }
    int ReadByte(unsigned char a, unsigned char c) override
    {
        if(Record(2,a,c,0) != 0) return -1;
        Check(next_byte < 10);
        return next_byte < 10 ? bytes[next_byte++] : -1;
    }
    int WriteByte(unsigned char a, unsigned char c, unsigned char v) override { return Record(3,a,c,v); }
    int WriteBlock(unsigned char a, unsigned char c, const unsigned char* d, unsigned char n) override { return Record(4,a,c,0,d,n); }
    void ResetTrace() { count = 0; fail_at = -1; }
};

static void SuccessfulTrace(unsigned char address)
{
    Fake bus;
    ENE6802Controller device(bus,address);
    Check(device.Initialize());
    Diagnostic(device, "ok", 0, -1, 0, 0);
    Check(bus.count == 14 && bus.begins == 1 && bus.ends == 1 && !bus.held);
    // Golden identification selector bytes taken from vendor branches.
    Check(bus.events[0].value == 0x1CF0 && bus.events[1].command == 0x82);
    Check(bus.events[2].value == 0xFB87 && bus.events[8].value == 0xA180);
    for(int i = 0; i < 14; i++) Check(bus.events[i].address == address);
    bus.ResetTrace();
    unsigned int colors[8] = {0xFF,0xFF00,0xFF0000,0x123456,0xAABBCC,0,0xFFFFFF,0x80112233};
    const unsigned char expected_colors[24] = {
        255,0,0, 0,0,255, 0,255,0, 0x56,0x12,0x34,
        0xCC,0xAA,0xBB, 0,0,0, 255,255,255, 0x33,0x11,0x22};
    Check(device.SetStatic(colors,8));
    Check(bus.count == 8 && bus.begins == 2 && bus.ends == 2 && !bus.held);
    const int types[8] = {0,4,0,4,0,3,0,3};
    const int commands[8] = {0,3,0,3,0,1,0,1};
    const int values[8] = {0x6081,0,0x2080,0,0x2980,9,0x2F80,1};
    for(int i = 0; i < 8; i++)
    {
        Check(bus.events[i].type == types[i]);
        Check(bus.events[i].command == commands[i]);
        Check(bus.events[i].value == values[i]);
        Check(bus.events[i].address == address);
    }
    Check(bus.events[1].size == 24 && bus.events[3].size == 4);
    for(int i = 0; i < 24; i++) Check(bus.events[1].data[i] == expected_colors[i]);
    for(int i = 0; i < 4; i++) Check(bus.events[3].data[i] == (i == 1 ? 1 : 0));
    // A later full frame is allowed after a successful update, no stale cache.
    bus.ResetTrace(); colors[0] = 0;
    Check(device.SetStatic(colors,8) && bus.events[1].data[0] == 0);
}

static void RejectIdentity()
{
    unsigned int colors[8] = {};
    for(int chip = 0; chip < 4; chip++)
    {
        Fake bus;
        const int others[4] = {0x7035,0x3077,0x4277,0xFFFF};
        bus.chip = others[chip];
        ENE6802Controller device(bus,0x71);
        Check(!device.Initialize() && bus.count == 2 && bus.ends == 1);
        Diagnostic(device, "chip-mismatch", 0xF01C, -1, others[chip], 0x0268);
        Check(!device.SetStatic(colors,8) && bus.count == 2);
    }
    for(int byte = 0; byte < 10; byte++)
    {
        Fake bus; bus.bytes[byte] ^= 1;
        ENE6802Controller device(bus,0x71);
        Check(!device.Initialize() && !device.IsIdentified() && !bus.held);
        Diagnostic(device, byte < 5 ? "firmware-mismatch" : "config-mismatch",
                   byte < 5 ? 0x87FB : 0x80A1, byte % 5, bus.bytes[byte], bus.bytes[byte] ^ 1);
        int count = bus.count;
        Check(!device.SetStatic(colors,8) && bus.count == count);
    }
    for(int address = 0; address < 256; address++)
    {
        if(address >= 0x70 && address <= 0x76) continue;
        Fake bus;
        ENE6802Controller device(bus,(unsigned char)address);
        Check(!device.Initialize() && bus.count == 0 && bus.begins == 0);
        Diagnostic(device, "invalid-address", 0, -1, address, -1);
    }
}

static void InjectFailures()
{
    unsigned int colors[8] = {};
    for(int failure = 0; failure < 14; failure++)
    {
        Fake bus; bus.fail_at = failure;
        ENE6802Controller device(bus,0x71);
        Check(!device.Initialize() && bus.count == failure + 1 && bus.ends == 1 && !bus.held);
        if(failure == 0 || failure == 2 || failure == 8)
            Diagnostic(device, "selector-io", failure == 0 ? 0xF01C : failure == 2 ? 0x87FB : 0x80A1, -1, -1, 0);
        else if(failure == 1) Diagnostic(device, "read-word-io", 0xF01C, -1, -1, -1);
        else Diagnostic(device, "read-byte-io", failure < 8 ? 0x87FB : 0x80A1, failure < 8 ? failure - 3 : failure - 9, -1, -1);
        int count = bus.count;
        Check(!device.SetStatic(colors,8) && bus.count == count);
    }
    for(int failure = 0; failure < 8; failure++)
    {
        Fake bus; ENE6802Controller device(bus,0x73);
        Check(device.Initialize()); bus.ResetTrace(); bus.fail_at = failure;
        Check(!device.SetStatic(colors,8));
        Check(bus.count == failure + 1 && bus.ends == 2 && !bus.held && !device.IsIdentified());
        int count = bus.count;
        Check(!device.SetStatic(colors,8) && bus.count == count);
    }
    Fake bus; ENE6802Controller device(bus,0x71);
    bus.blocked = true;
    Check(!device.Initialize() && bus.count == 0 && bus.ends == 0);
    Diagnostic(device, "sequence-lock", 0, -1, 0, 1);
    bus.blocked = false; Check(device.Initialize()); bus.ResetTrace();
    bus.blocked = true;
    Check(!device.SetStatic(colors,8) && bus.count == 0 && bus.ends == 1);
    bus.blocked = false;
    Check(!device.SetStatic(nullptr,8) && bus.count == 0);
    for(unsigned int size = 0; size < 33; size++)
        if(size != 8) Check(!device.SetStatic(colors,size) && bus.count == 0);
    Check(device.SetStatic(colors,8));
}

extern "C" int main()
{
    SuccessfulTrace(0x71);
    SuccessfulTrace(0x73);
    RejectIdentity();
    InjectFailures();
    return failures ? 1 : 0;
}
