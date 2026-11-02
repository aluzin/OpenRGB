// SPDX-License-Identifier: GPL-2.0-or-later
// Compile actual Windows adapter; every OS/bus operation is a test double.
#include "../../Controllers/ENE6802Controller/ENE6802Transport_Windows.h"
static int failures = 0, depth = 0, calls = 0, closes = 0;
static DWORD thread_id = 1, wait_result = WAIT_OBJECT_0;
static bool create_failed = false;
static HANDLE handle = (HANDLE)1;
static void Check(bool b) { if(!b) failures++; }
extern "C" int _purecall() { failures++; return 0; }
BOOL ConvertStringSecurityDescriptorToSecurityDescriptorA(const char*,DWORD,void** out,DWORD*) { *out=(void*)2; return 1; }
HANDLE CreateMutexA(SECURITY_ATTRIBUTES*,BOOL,const char*) { return create_failed ? nullptr : handle; }
HANDLE OpenMutexA(DWORD,BOOL,const char*) { Check(false); return nullptr; }
DWORD GetLastError() { return 0; }
void SetLastError(DWORD) {}
void* LocalFree(void*) { return nullptr; }
DWORD WaitForSingleObject(HANDLE h,DWORD ms)
{
    Check(h==handle && ms==3000);
    if(wait_result==WAIT_OBJECT_0 || wait_result==WAIT_ABANDONED) depth++;
    return wait_result;
}
BOOL ReleaseMutex(HANDLE h) { Check(h==handle && depth>0); depth--; return 1; }
BOOL CloseHandle(HANDLE h) { Check(h==handle && depth==0); closes++; return 1; }
DWORD GetCurrentThreadId() { return thread_id; }
class Bus : public i2c_smbus_interface
{
public:
    int direction=0, command=0, size=0, address=0, error=0;
    i2c_smbus_data data = {};
    s32 i2c_smbus_xfer(u8 a,char d,u8 c,int s,i2c_smbus_data* bytes) override
    {
        Check(depth==1);
        // PawnIO re-enters the same mutex on this calling thread.
        SharedSMBusLock recursive(handle,true);
        Check(recursive.Ready() && depth==2);
        calls++; address=a; direction=d; command=c; size=s; data=*bytes;
        if(d==I2C_SMBUS_READ) bytes->word=0x0268;
        return error;
    }
};
extern "C" int main()
{
    Bus bus;
    {
        ENE6802Transport_Windows transport(&bus);
        Check(transport.WriteWord(0x71,0,0x6081)==-1 && calls==0);
        Check(transport.Begin() && depth==1);
        Check(transport.WriteWord(0x71,0,0x6081)==0);
        Check(bus.address==0x71 && bus.command==0 && bus.size==3 && bus.data.word==0x6081 && depth==1);
        Check(transport.ReadWord(0x73,0x82)==0x0268 && bus.direction==1 && bus.size==3);
        Check(transport.ReadByte(0x73,0x81)==0x68 && bus.direction==1 && bus.size==2);
        Check(transport.WriteByte(0x73,1,9)==0 && bus.direction==0 && bus.size==2 && bus.data.byte==9);
        unsigned char bytes[25]; for(int i=0;i<25;i++) bytes[i]=(unsigned char)i;
        Check(transport.WriteBlock(0x71,3,bytes,24)==0 && bus.size==5 && bus.command==3 && bus.data.block[0]==24);
        for(int i=0;i<24;i++) Check(bus.data.block[i+1]==i);
        int previous=calls;
        Check(transport.WriteBlock(0x71,3,bytes,25)==-1 && transport.WriteBlock(0x71,3,nullptr,1)==-1 && transport.WriteBlock(0x71,3,bytes,0)==-1);
        Check(calls==previous);
        thread_id=2;
        Check(transport.WriteWord(0x71,0,0)==-1 && calls==previous);
        thread_id=1;
        bus.error=-5;
        Check(transport.ReadByte(0x71,0x81)==-1 && transport.ReadWord(0x71,0x82)==-1);
        Check(transport.WriteByte(0x71,1,1)==-5);
        transport.End(); Check(depth==0);
        previous=calls;
        wait_result=WAIT_TIMEOUT; Check(!transport.Begin() && depth==0 && calls==previous);
        wait_result=WAIT_FAILED; Check(!transport.Begin() && depth==0 && calls==previous);
        wait_result=WAIT_ABANDONED; Check(!transport.Begin() && depth==0 && calls==previous);
        wait_result=WAIT_OBJECT_0;
    }
    Check(closes==1);
    create_failed=true;
    { ENE6802Transport_Windows transport(&bus); Check(!transport.Begin() && depth==0); }
    Check(closes==1);
    return failures ? 1 : 0;
}
