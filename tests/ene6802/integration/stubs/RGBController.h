// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "RGBControllerInterface.h"
#include <cstdio>
#include <cstdlib>
#include <functional>
// Real OpenRGB mode/zone/LED/color types; only the UI/base-class services are fake.
class RGBController
{
public:
    std::string name, vendor, description, version, location;
    device_type type = DEVICE_TYPE_UNKNOWN;
    int active_mode = 0;
    std::vector<mode> modes;
    std::vector<zone> zones;
    std::vector<led> leds;
    std::vector<RGBColor> colors;
    inline static std::function<void(RGBController*)> shutdown_hook;
    bool shutdown_called = false;
    virtual ~RGBController()
    {
        if(!shutdown_called) { std::fprintf(stderr, "FAIL: controller destroyed without Shutdown\n"); std::abort(); }
    }
    void Shutdown()
    {
        if(shutdown_called) { std::fprintf(stderr, "FAIL: duplicate Shutdown\n"); std::abort(); }
        // Model an in-flight callback completing while Shutdown joins it.
        if(shutdown_hook) shutdown_hook(this);
        shutdown_called = true;
    }
    virtual void DeviceUpdateLEDs() = 0;
    virtual void DeviceUpdateZoneLEDs(int) = 0;
    virtual void DeviceUpdateSingleLED(int) = 0;
    virtual void DeviceUpdateMode() = 0;
    void SetupColors()
    {
        unsigned int count = 0;
        for(const zone& z : zones) count += z.leds_count;
        colors.resize(count);
        unsigned int offset = 0;
        for(zone& z : zones)
        {
            z.start_idx = offset;
            z.colors = colors.data() + offset;
            z.leds = leds.data() + offset;
            offset += z.leds_count;
        }
    }
};
