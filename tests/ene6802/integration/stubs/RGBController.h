// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "RGBControllerInterface.h"
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
    virtual ~RGBController() = default;
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
