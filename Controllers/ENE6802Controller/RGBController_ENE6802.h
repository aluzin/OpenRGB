/* RGBController_ENE6802.h - Static-only ENE 6802 profile
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "RGBController.h"
#include "ENE6802Controller.h"
class ENE6802Transport_Windows;

class RGBController_ENE6802 : public RGBController
{
public:
    RGBController_ENE6802(ENE6802Controller* controller, ENE6802Transport_Windows* transport, const std::string& location);
    ~RGBController_ENE6802();
    void SetupZones();
    void DeviceUpdateLEDs() override;
    void DeviceUpdateZoneLEDs(int zone) override;
    void DeviceUpdateSingleLED(int led) override;
    void DeviceUpdateMode() override;
private:
    ENE6802Controller* controller;
    ENE6802Transport_Windows* transport;
};
