// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "SettingsManager.h"
class ResourceManager
{
public:
    SettingsManager settings;
    static ResourceManager* get() { static ResourceManager instance; return &instance; }
    SettingsManager* GetSettingsManager() { return &settings; }
};
