/*---------------------------------------------------------*\
| ENE6802Controller.h                                      |
|   Narrow ENE 6802 / EE D5 08 03 00 DDR5 protocol          |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/
#pragma once

// Transport owns sequence synchronization. Word arguments use the standard
// SMBus API byte order, not the vendor HAL's reversed word convention.
class ENE6802Transport
{
public:
    virtual bool Begin() = 0;
    virtual void End() = 0;
    virtual int WriteWord(unsigned char address, unsigned char command, unsigned short value) = 0;
    virtual int ReadWord(unsigned char address, unsigned char command) = 0;
    virtual int ReadByte(unsigned char address, unsigned char command) = 0;
    virtual int WriteByte(unsigned char address, unsigned char command, unsigned char value) = 0;
    virtual int WriteBlock(unsigned char address, unsigned char command, const unsigned char* data, unsigned char size) = 0;
protected:
    ~ENE6802Transport() = default;
};

struct ENE6802Diagnostic
{
    const char* reason;
    unsigned short reg;
    int index;
    int actual;
    int expected;
};

class ENE6802Controller
{
public:
    ENE6802Controller(ENE6802Transport& transport, unsigned char address);
    bool Initialize();
    // Eight colors in OpenRGB's 0x00BBGGRR convention. No flash save or effects.
    bool SetStatic(const unsigned int* colors, unsigned int count);
    bool IsIdentified() const;
    const ENE6802Diagnostic& GetDiagnostic() const { return diagnostic; }
private:
    bool Reject(const char* reason, unsigned short reg, int index, int actual, int expected);
    ENE6802Diagnostic diagnostic = {"not-initialized", 0, -1, 0, 0};
    bool Select(unsigned short reg);
    bool Read(unsigned short reg, unsigned char* data, unsigned int size);
    bool Write(unsigned short reg, const unsigned char* data, unsigned char size);
    ENE6802Transport& transport;
    unsigned char address;
    bool identified;
};
