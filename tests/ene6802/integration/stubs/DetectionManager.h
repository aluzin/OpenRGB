// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <vector>
#include "RGBController.h"
#include "i2c_smbus.h"
#include "SettingsManager.h"
using DetectedControllers = std::vector<RGBController*>;
inline bool registered_default_enabled = true;
struct TestRegistration
{
    TestRegistration(bool enabled) { registered_default_enabled = enabled; }
};
#define REGISTER_I2C_DETECTOR_DISABLED(name, function) static TestRegistration registration(false)
#define REGISTER_I2C_DETECTOR(name, function) static TestRegistration registration(true)
class DetectionManager
{
public:
    struct DimmDetector { std::string name; };
    std::vector<DimmDetector> i2c_dram_device_detectors;
    bool IsAnyDimmDetectorEnabled(json& settings);
};
