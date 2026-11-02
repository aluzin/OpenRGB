/* RGBController_ENE6802_Windows.cpp - opt-in eight-LED static profile
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "RGBController_ENE6802.h"
#include "ENE6802Transport_Windows.h"
#include "LogManager.h"

RGBController_ENE6802::RGBController_ENE6802(ENE6802Controller* controller, ENE6802Transport_Windows* transport, const std::string& device_location) :
    controller(controller), transport(transport)
{
    name = "ENE 6802 DDR5 RGB";
    vendor = "ENE";
    description = "Experimental EE D5 08 03 00 profile (G.Skill Trident Z5 RGB sample)";
    version = "6802 / EE D5 08 03 00";
    location = device_location;
    type = DEVICE_TYPE_DRAM;
    mode static_mode;
    static_mode.name = "Static";
    static_mode.value = 1;
    static_mode.flags = MODE_FLAG_HAS_PER_LED_COLOR;
    static_mode.color_mode = MODE_COLORS_PER_LED;
    modes.push_back(static_mode);
    active_mode = 0;
    SetupZones();
}

RGBController_ENE6802::~RGBController_ENE6802()
{
    // Join queued/in-flight device callbacks before destroying their backend.
    Shutdown();
    delete controller;
    delete transport;
}

void RGBController_ENE6802::SetupZones()
{
    zones.clear();
    leds.clear();
    zone dram_zone;
    dram_zone.name = "DRAM";
    dram_zone.type = ZONE_TYPE_LINEAR;
    dram_zone.leds_min = 8;
    dram_zone.leds_max = 8;
    dram_zone.leds_count = 8;
    zones.push_back(dram_zone);
    for(unsigned int i = 0; i < 8; i++)
    {
        led dram_led;
        dram_led.name = "DRAM LED " + std::to_string(i + 1);
        dram_led.value = i;
        leds.push_back(dram_led);
    }
    SetupColors();
}

void RGBController_ENE6802::DeviceUpdateLEDs()
{
    if(colors.size() != 8 || !controller->SetStatic(colors.data(), 8))
        LOG_ERROR("[ENE 6802] Static update rejected or failed; no automatic retry");
}

void RGBController_ENE6802::DeviceUpdateZoneLEDs(int zone)
{
    if(zone == 0) DeviceUpdateLEDs();
}

void RGBController_ENE6802::DeviceUpdateSingleLED(int led)
{
    if(led >= 0 && led < 8) DeviceUpdateLEDs();
}

void RGBController_ENE6802::DeviceUpdateMode()
{
    if(active_mode == 0) DeviceUpdateLEDs();
}
