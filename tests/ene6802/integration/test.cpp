// SPDX-License-Identifier: GPL-2.0-or-later
// All four production ENE6802 files, real JSON/types, fake OS and SMBus only.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include "DetectionManager.h"
#include "ResourceManager.h"
#include "i2c_smbus_pawnio.h"
#include "ENE6802Transport_Windows.h"
#include "RGBController_ENE6802.h"
DetectedControllers DetectENE6802Controllers(std::vector<i2c_smbus_interface*>&);

static unsigned int checks = 0, cases = 0, creates = 0, closes = 0, depth = 0;
static DWORD wait_result = WAIT_OBJECT_0;
static bool no_mutex = false;
static const char* scenario = "startup";
static void Check(bool value)
{
    checks++;
    if(!value) { std::fprintf(stderr, "FAIL: %s (check %u)\n", scenario, checks); std::abort(); }
}
BOOL ConvertStringSecurityDescriptorToSecurityDescriptorA(const char*, DWORD, void** out, DWORD*) { *out=(void*)2; return 1; }
HANDLE CreateMutexA(SECURITY_ATTRIBUTES*, BOOL, const char*) { creates++; return no_mutex ? nullptr : (HANDLE)1; }
HANDLE OpenMutexA(DWORD, BOOL, const char*) { Check(false); return nullptr; }
DWORD GetLastError() { return 0; }
void SetLastError(DWORD) {}
void* LocalFree(void*) { return nullptr; }
DWORD GetCurrentThreadId() { return 1; }
DWORD WaitForSingleObject(HANDLE, DWORD ms)
{
    Check(ms == 3000);
    if(wait_result == WAIT_OBJECT_0 || wait_result == WAIT_ABANDONED) depth++;
    return wait_result;
}
BOOL ReleaseMutex(HANDLE) { Check(depth > 0); depth--; return 1; }
BOOL CloseHandle(HANDLE) { Check(depth == 0); closes++; return 1; }

struct Event
{
    int address, direction, command, kind, reg;
    i2c_smbus_data data;
};
class Bus : public i2c_smbus_pawnio
{
public:
    std::vector<Event> trace;
    unsigned short reg[128] = {};
    int bad_address = -1, fail_at = -1;
    Bus()
    {
        std::strcpy(info.device_name, "PawnIO SMBus i801 0");
        info.pci_vendor=0x8086; info.pci_device=0x7F23;
        info.pci_subsystem_vendor=0x1043; info.pci_subsystem_device=0x88EF;
    }
    s32 i2c_smbus_xfer(u8 a, char d, u8 c, int kind, i2c_smbus_data* data) override
    {
        Check(depth == 1 && (a == 0x71 || a == 0x73));
        // Model the production PawnIO backend's recursive use of the shared lock.
        SharedSMBusLock lock((HANDLE)1, true);
        Check(lock.Ready() && depth == 2);
        if(d == I2C_SMBUS_WRITE && c == 0 && kind == I2C_SMBUS_WORD_DATA)
            reg[a] = (unsigned short)((data->word >> 8) | (data->word << 8));
        trace.push_back({a,d,c,kind,reg[a],*data});
        if((int)trace.size() - 1 == fail_at) return -1;
        if(d == I2C_SMBUS_READ)
        {
            if(c == 0x82 && kind == I2C_SMBUS_WORD_DATA && reg[a] == 0xF01C)
                data->word = a == bad_address ? 0xFFFF : 0x0268;
            else if(c == 0x81 && kind == I2C_SMBUS_BYTE_DATA)
            {
                const unsigned char fw[5] = {0xEE,0xD5,8,3,0};
                const unsigned char cfg[5] = {1,8,0,0,0};
                if(reg[a] >= 0x87FB && reg[a] <= 0x87FF) data->byte=fw[reg[a]++ - 0x87FB];
                else if(reg[a] >= 0x80A1 && reg[a] <= 0x80A5) data->byte=cfg[reg[a]++ - 0x80A1];
                else Check(false);
            }
            else Check(false);
        }
        else
        {
            Check(c == 0 || reg[a] == 0x8160 || reg[a] == 0x8020 || reg[a] == 0x8029 || reg[a] == 0x802F);
        }
        return 0;
    }
    void Clear() { trace.clear(); fail_at=-1; }
};
class OtherBackend : public i2c_smbus_interface
{
public:
    s32 i2c_smbus_xfer(u8, char, u8, int, i2c_smbus_data*) override { Check(false); return -1; }
};
static json Entry(int address)
{
    return {{"bus_name","PawnIO SMBus i801 0"},{"address",address},
            {"pci_subsystem_vendor",0x1043},{"pci_subsystem_device",0x88EF}};
}
static json Valid()
{
    return {{"ENE6802Devices",{{"enabled",true},{"devices",json::array({Entry(0x71),Entry(0x73)})}}},
            {"Detectors",{{"detectors",{{"ENE SMBus DRAM",false}}}}},
            {"Drivers",{{"shared_smbus_access",true}}}};
}
static void Dispose(DetectedControllers& devices)
{
    for(RGBController* device : devices) delete device;
    devices.clear();
    Check(depth == 0);
}
static void Reject(const char* label, const json& settings, std::vector<i2c_smbus_interface*> buses)
{
    scenario=label; cases++;
    ResourceManager::get()->settings.sections=settings;
    unsigned int before=creates;
    DetectedControllers devices=DetectENE6802Controllers(buses);
    Check(devices.empty() && creates == before && depth == 0);
    // No transport object may be constructed for any invalid complete configuration.
    Dispose(devices);
}
static void ConfigurationGuards()
{
    Bus bus, duplicate; OtherBackend other; other.info=bus.info;
    std::vector<i2c_smbus_interface*> buses={&bus};
    Check(!registered_default_enabled);
    Reject("missing config",json::object(),buses);
    const json invalid_flags[] = {false,0,1,"true",nullptr,json::array()};
    for(const json& flag : invalid_flags)
    {
        json s=Valid(); s["ENE6802Devices"]["enabled"]=flag;
        Reject("strict opt-in",s,buses);
    }
    for(const char* key : {"enabled","devices"})
    {
        json s=Valid(); s["ENE6802Devices"].erase(key); Reject("missing top field",s,buses);
    }
    for(const json& invalid : {json(),json(false),json(1),json("x"),json::object(),json::array()})
    {
        json s=Valid(); s["ENE6802Devices"]["devices"]=invalid; Reject("invalid device list",s,buses);
    }
    json s=Valid();
    for(int i=0;i<3;i++) s["ENE6802Devices"]["devices"].push_back(Entry(0x71));
    Reject("too many endpoints",s,buses);
    for(const char* key : {"bus_name","address","pci_subsystem_vendor","pci_subsystem_device"})
    {
        s=Valid(); s["ENE6802Devices"]["devices"][1].erase(key);
        Reject("late missing field before ALL I/O",s,buses);
        for(const json& invalid : {json(),json(false),json::object(),json::array(),json(1.5)})
        {
            s=Valid(); s["ENE6802Devices"]["devices"][1][key]=invalid;
            Reject("wrong field type",s,buses);
        }
    }
    for(const json& invalid : {json(-1),json(0x50),json(0x77),json(0x100),json(std::numeric_limits<unsigned long long>::max())})
    {
        s=Valid(); s["ENE6802Devices"]["devices"][1]["address"]=invalid;
        Reject("out of range address",s,buses);
    }
    for(const char* key : {"pci_subsystem_vendor","pci_subsystem_device"})
        for(const json& invalid : {json(-1),json(65536),json(std::numeric_limits<unsigned long long>::max())})
        {
            s=Valid(); s["ENE6802Devices"]["devices"][1][key]=invalid;
            Reject("out of range PCI",s,buses);
        }
    for(const json& invalid : {json(),json(false),json::array(),json::object()})
    {
        s=Valid(); s["Detectors"]=invalid; Reject("legacy gate missing",s,buses);
    }
    for(const json& flag : {json(true),json(0),json("false"),json()})
    {
        s=Valid(); s["Detectors"]["detectors"]["ENE SMBus DRAM"]=flag;
        Reject("legacy must be explicitly false",s,buses);
    }
    for(const json& flag : invalid_flags)
    {
        s=Valid(); s["Drivers"]["shared_smbus_access"]=flag;
        Reject("shared mutex required",s,buses);
    }
    s=Valid(); s["ENE6802Devices"]["devices"][1]=Entry(0x71);
    Reject("duplicate endpoint",s,buses);
    Reject("ambiguous buses",Valid(),{&bus,&duplicate});
    Reject("wrong backend",Valid(),{&other});
    Reject("no bus",Valid(),{}); Reject("null bus",Valid(),{nullptr});
    for(int* field : {&bus.info.pci_vendor,&bus.info.pci_device,&bus.info.pci_subsystem_vendor,&bus.info.pci_subsystem_device})
    {
        int saved=*field; *field^=1; Reject("PCI mismatch",Valid(),buses); *field=saved;
    }
    std::strcpy(bus.info.device_name,"PawnIO SMBus piix4_0 0");
    s=Valid(); for(json& e : s["ENE6802Devices"]["devices"]) e["bus_name"]=bus.info.device_name;
    Reject("wrong PawnIO module",s,buses);
    std::strcpy(bus.info.device_name,"PawnIO SMBus i801 1");
    Reject("bus number changed",Valid(),buses);
    Check(bus.trace.empty() && duplicate.trace.empty());
}
static void AssertFrame(const Bus& bus, int address, unsigned char r, unsigned char g, unsigned char b)
{
    Check(bus.trace.size() == 8);
    for(const Event& e : bus.trace) Check(e.address == address);
    const Event& color=bus.trace[1];
    Check(color.reg == 0x8160 && color.kind == 5 && color.command == 3 && color.data.block[0] == 24);
    for(int i=0;i<8;i++)
    {
        Check(color.data.block[1+3*i] == r);
        Check(color.data.block[2+3*i] == b);
        Check(color.data.block[3+3*i] == g);
    }
    Check(bus.trace[7].reg == 0x802F && bus.trace[7].data.byte == 1 && depth == 0);
}
static void DeviceIntegration()
{
    scenario="two-module construction"; cases++;
    Bus bus; std::vector<i2c_smbus_interface*> buses={&bus};
    ResourceManager::get()->settings.sections=Valid();
    DetectedControllers devices=DetectENE6802Controllers(buses);
    Check(devices.size() == 2 && bus.trace.size() == 28);
    for(const Event& e : bus.trace) Check(e.direction == I2C_SMBUS_READ || (e.command == 0 && e.kind == 3));
    for(RGBController* device : devices)
    {
        Check(device->modes.size() == 1 && device->modes[0].name == "Static");
        Check(device->modes[0].flags == MODE_FLAG_HAS_PER_LED_COLOR);
        Check(device->zones.size() == 1 && device->zones[0].leds_count == 8 && device->leds.size() == 8 && device->colors.size() == 8);
    }
    scenario="targeted RGB commands";
    const unsigned char rgb[3][3]={{255,0,0},{0,255,0},{0,0,255}};
    for(int device=0;device<2;device++)
        for(const auto& c : rgb)
        {
            bus.Clear(); for(RGBColor& color : devices[device]->colors) color=ToRGBColor(c[0],c[1],c[2]);
            devices[device]->DeviceUpdateLEDs(); AssertFrame(bus,0x71+2*device,c[0],c[1],c[2]);
        }
    scenario="invalid UI selections"; bus.Clear();
    for(int invalid : {-2,-1,1,2,100}) devices[0]->DeviceUpdateZoneLEDs(invalid);
    for(int invalid : {-2,-1,8,9,100}) devices[0]->DeviceUpdateSingleLED(invalid);
    devices[0]->active_mode=1; devices[0]->DeviceUpdateMode(); devices[0]->active_mode=0;
    devices[0]->colors.resize(7); devices[0]->DeviceUpdateLEDs(); devices[0]->colors.resize(8);
    Check(bus.trace.empty());
    scenario="valid UI callbacks";
    for(RGBColor& color : devices[0]->colors) color=ToRGBColor(255,0,0);
    devices[0]->DeviceUpdateZoneLEDs(0); AssertFrame(bus,0x71,255,0,0); bus.Clear();
    devices[0]->DeviceUpdateSingleLED(7); AssertFrame(bus,0x71,255,0,0); bus.Clear();
    devices[0]->DeviceUpdateMode(); AssertFrame(bus,0x71,255,0,0); bus.Clear();
    scenario="partial frame failure is latched"; bus.fail_at=3;
    devices[0]->DeviceUpdateLEDs(); Check(bus.trace.size() == 4);
    bus.Clear(); devices[0]->DeviceUpdateLEDs(); Check(bus.trace.empty());
    scenario="other module remains independent";
    devices[1]->DeviceUpdateLEDs(); AssertFrame(bus,0x73,0,0,255);
    unsigned int before=(unsigned int)bus.trace.size(); Dispose(devices);
    Check(bus.trace.size() == before); // Destruction must not send off/reset/save.
}
static void FailureCleanup()
{
    for(int bad : {0x71,0x73})
    {
        scenario="identity mismatch cleanup"; cases++;
        Bus bus; bus.bad_address=bad; std::vector<i2c_smbus_interface*> buses={&bus};
        ResourceManager::get()->settings.sections=Valid();
        unsigned int opened=creates, closed=closes;
        DetectedControllers devices=DetectENE6802Controllers(buses);
        Check(devices.size() == 1 && bus.trace.size() == 16);
        for(const Event& e : bus.trace) Check(e.direction == 1 || e.command == 0);
        Dispose(devices); Check(creates-opened == 2 && closes-closed == 2);
    }
    for(DWORD failure : {WAIT_TIMEOUT,WAIT_FAILED,WAIT_ABANDONED})
    {
        scenario="mutex failure during detection"; cases++;
        Bus bus; std::vector<i2c_smbus_interface*> buses={&bus};
        ResourceManager::get()->settings.sections=Valid(); wait_result=failure;
        DetectedControllers devices=DetectENE6802Controllers(buses);
        Check(devices.empty() && bus.trace.empty() && depth == 0);
    }
    wait_result=WAIT_OBJECT_0;
    scenario="no mutex handle"; Bus bus; std::vector<i2c_smbus_interface*> buses={&bus};
    no_mutex=true; DetectedControllers devices=DetectENE6802Controllers(buses); no_mutex=false;
    Check(devices.empty() && bus.trace.empty());
}
static void SpdGate()
{
    scenario="actual DetectionManager SPD gate"; cases++;
    DetectionManager manager;
    manager.i2c_dram_device_detectors={{"DIMM one"},{"DIMM two"}};
    json settings={{"detectors",{{"DIMM one",false},{"DIMM two",false},
                                {"ENE 6802 DDR5 (configured endpoints)",true}}}};
    Check(!manager.IsAnyDimmDetectorEnabled(settings));
    settings["detectors"]["DIMM one"]=true;
    Check(manager.IsAnyDimmDetectorEnabled(settings));
    settings["detectors"]["DIMM one"]=false;
    settings["detectors"]["DIMM two"]=true;
    Check(manager.IsAnyDimmDetectorEnabled(settings));
    settings["detectors"]["DIMM two"]=false;
    Check(!manager.IsAnyDimmDetectorEnabled(settings));
}
int main()
{
    ConfigurationGuards(); DeviceIntegration(); FailureCleanup(); SpdGate();
    std::printf("PASS: %u configuration/scenario cases, %u assertions; fake OS/bus only\n",cases,checks);
}
