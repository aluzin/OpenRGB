// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <nlohmann/json.hpp>
using json = nlohmann::json;
class SettingsManager
{
public:
    json sections = json::object();
    json GetSettings(const char* name) const
    {
        return sections.contains(name) ? sections[name] : json();
    }
};
