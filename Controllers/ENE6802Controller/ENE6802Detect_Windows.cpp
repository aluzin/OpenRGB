/* ENE6802Detect_Windows.cpp - explicitly configured endpoints only
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <cstring>
#include "DetectionManager.h"
#include "ResourceManager.h"
#include "SettingsManager.h"
#include "LogManager.h"
#include "i2c_smbus_pawnio.h"
#include "ENE6802Transport_Windows.h"
#include "RGBController_ENE6802.h"

DetectedControllers DetectENE6802Controllers(std::vector<i2c_smbus_interface*>& buses)
{
    DetectedControllers detected;
    json settings = ResourceManager::get()->GetSettingsManager()->GetSettings("ENE6802Devices");
    if(!settings.is_object() || !settings.contains("enabled") || settings["enabled"] != true ||
       !settings.contains("devices") || !settings["devices"].is_array()) return detected;

    // The existing generic detector remaps RAM. Never enable both paths.
    json detectors = ResourceManager::get()->GetSettingsManager()->GetSettings("Detectors");
    if(!detectors.contains("detectors") || !detectors["detectors"].is_object() ||
       !detectors["detectors"].contains("ENE SMBus DRAM") ||
       detectors["detectors"]["ENE SMBus DRAM"] != false)
    {
        LOG_ERROR("[ENE 6802] Explicitly disable ENE SMBus DRAM before using manual endpoints");
        return detected;
    }
    json drivers = ResourceManager::get()->GetSettingsManager()->GetSettings("Drivers");
    if(drivers.contains("shared_smbus_access") && drivers["shared_smbus_access"] != true) return detected;

    // Validate the complete configuration before touching any endpoint.
    // Require exact bus name AND PCI identity; bus numbering alone is not stable.
    if(settings["devices"].empty() || settings["devices"].size() > 4) return detected;
    std::vector<i2c_smbus_interface*> selected_buses;
    std::vector<unsigned char> selected_addresses;
    for(unsigned int i = 0; i < settings["devices"].size(); i++)
    {
        json entry = settings["devices"][i];
        if(!entry.is_object() || !entry.contains("bus_name") || !entry["bus_name"].is_string() ||
           !entry.contains("address") || !entry["address"].is_number_integer() ||
           !entry.contains("pci_subsystem_vendor") || !entry["pci_subsystem_vendor"].is_number_integer() ||
           !entry.contains("pci_subsystem_device") || !entry["pci_subsystem_device"].is_number_integer()) return detected;
        if(entry["address"] < 0x70 || entry["address"] > 0x76 ||
           entry["pci_subsystem_vendor"] < 0 || entry["pci_subsystem_vendor"] > 65535 ||
           entry["pci_subsystem_device"] < 0 || entry["pci_subsystem_device"] > 65535) return detected;
        std::string bus_name = entry["bus_name"].get<std::string>();
        i2c_smbus_interface* selected = nullptr;
        unsigned int matches = 0;
        for(unsigned int b = 0; b < buses.size(); b++)
        {
            i2c_smbus_interface* bus = buses[b];
            if(dynamic_cast<i2c_smbus_pawnio*>(bus) != nullptr &&
               bus->info.pci_vendor == 0x8086 && bus->info.pci_device == 0x7F23 &&
               bus->info.pci_subsystem_vendor == entry["pci_subsystem_vendor"].get<int>() &&
               bus->info.pci_subsystem_device == entry["pci_subsystem_device"].get<int>() &&
               std::strncmp(bus->info.device_name, "PawnIO SMBus i801 ", 18) == 0 &&
               bus_name == bus->info.device_name)
            {
                matches++;
                selected = bus;
            }
        }
        if(matches != 1) return detected;
        unsigned char address = entry["address"].get<unsigned char>();
        for(unsigned int previous = 0; previous < selected_buses.size(); previous++)
        {
            if(selected_buses[previous] == selected && selected_addresses[previous] == address) return detected;
        }
        selected_buses.push_back(selected);
        selected_addresses.push_back(address);
    }

    for(unsigned int i = 0; i < selected_buses.size(); i++)
    {
        ENE6802Transport_Windows* transport = new ENE6802Transport_Windows(selected_buses[i]);
        ENE6802Controller* controller = new ENE6802Controller(*transport, selected_addresses[i]);
        if(controller->Initialize())
        {
            std::string location = std::string("I2C: ") + selected_buses[i]->info.device_name +
                                   ", configured address " + std::to_string(selected_addresses[i]);
            detected.push_back(new RGBController_ENE6802(controller, transport, location));
        }
        else
        {
            LOG_ERROR("[ENE 6802] Configured endpoint rejected: identity, firmware, configuration or I/O mismatch");
            delete controller;
            delete transport;
        }
    }
    return detected;
}

REGISTER_I2C_DETECTOR_DISABLED("ENE 6802 DDR5 (configured endpoints)", DetectENE6802Controllers);
