#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Linux/CI integration test: production controller code, no real device backend."""
import hashlib
import json
from pathlib import Path
import subprocess

here = Path(__file__).resolve().parent
root = here.parents[2]
source = root / 'Controllers/ENE6802Controller'
build = here / 'build'
build.mkdir(exist_ok=True)
flags = ['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-g', '-O1',
         '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie',
         '-I' + str(here / 'stubs'), '-I' + str(here.parent / 'stubs'),
         '-I' + str(source), '-I' + str(root / 'i2c_smbus/Windows'),
         '-isystem', str(root / 'dependencies/json'),
         '-isystem', str(root / 'RGBController')]
names = ['ENE6802Controller.cpp', 'ENE6802Transport_Windows.cpp',
         'RGBController_ENE6802_Windows.cpp', 'ENE6802Detect_Windows.cpp']
objects = []
# Compile the actual SPD gate verbatim, without the rest of DetectionManager
# (which contains real discovery). Never replace it with a Python reimplementation.
manager = (root / 'DetectionManager.cpp').read_text()
start = manager.index('bool DetectionManager::IsAnyDimmDetectorEnabled(')
opening = manager.index('{', start)
depth = 1
end = opening + 1
while depth:
    if manager[end] == '{': depth += 1
    if manager[end] == '}': depth -= 1
    end += 1
gate = build / 'spd_gate.cpp'
gate.write_text('#include "DetectionManager.h"\n' + manager[start:end] + '\n')
assert 'bus < i2c_buses.size() && IsAnyDimmDetectorEnabled(detector_settings)' in manager
for path in [gate, here / 'test.cpp'] + [source / name for name in names]:
    obj = build / (path.stem + '.o')
    subprocess.run(flags + ['-c', str(path), '-o', str(obj)], check=True)
    objects.append(str(obj))

def execute(label, inputs, must_pass):
    exe = build / label
    subprocess.run(['g++', '-fsanitize=address,undefined', '-no-pie'] + inputs + ['-o', str(exe)], check=True)
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
    if must_pass:
        assert result.returncode == 0, result.stdout + result.stderr
        print(result.stdout, end='')
    else:
        assert result.returncode != 0 and 'FAIL:' in result.stderr, label + ': mutation survived or failed unexpectedly'
    return result.stdout.strip()

report = {'scope': 'Actual detector, protocol, adapter, RGB wrapper; fake OS/bus/base UI services',
          'sanitizers': ['address', 'undefined'], 'result': execute('integration', objects, True),
          'sources': {name: hashlib.sha256((source / name).read_bytes()).hexdigest() for name in names}}
original = (source / 'ENE6802Detect_Windows.cpp').read_text()
mutations = {
    'default_enabled': ('REGISTER_I2C_DETECTOR_DISABLED(', 'REGISTER_I2C_DETECTOR('),
    'accept_ambiguous_bus': ('if(matches != 1)', 'if(matches == 0)'),
    'ignore_subsystem': ('bus->info.pci_subsystem_device == entry["pci_subsystem_device"].get<int>() &&', ''),
    'allow_legacy_detector': ('detectors["detectors"]["ENE SMBus DRAM"] != false)', 'false)'),
    'allow_unlocked': ('drivers["shared_smbus_access"] != true)', 'false)'),
    'allow_duplicate': ('selected_buses[previous] == selected && selected_addresses[previous] == address', 'selected_buses[previous] == nullptr && selected_addresses[previous] == address'),
    'wrong_backend_allowed': ('dynamic_cast<i2c_smbus_pawnio*>(bus) != nullptr', 'bus != nullptr'),
}
report['mutations_rejected'] = []
for label, (old, new) in mutations.items():
    assert original.count(old) == 1, label
    mutant = build / (label + '.cpp')
    mutant.write_text(original.replace(old, new))
    obj = build / (label + '.o')
    subprocess.run(flags + ['-c', str(mutant), '-o', str(obj)], check=True)
    execute(label, objects[:-1] + [str(obj)], False)
    report['mutations_rejected'].append(label)
(build / 'result.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
