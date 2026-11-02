#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Prepare/verify isolated trial JSON from source and a saved detector inventory.

File operations only. Never launches OpenRGB, changes installed settings,
loads a DLL, or accesses a device. Existing output files are not overwritten.
"""
import argparse
import ast
import hashlib
import json
from pathlib import Path
import re

TARGET = 'ENE 6802 DDR5 (configured endpoints)'
root = Path(__file__).resolve().parents[2]

def without_comments(text):
    return re.sub(r'"(?:\\.|[^"\\])*"|//[^\n]*|/\*[\s\S]*?\*/',
                  lambda m: m.group() if m.group().startswith('"') else ' ', text)

def registry_names():
    names, dimm, unresolved = set(), set(), []
    for path in (root / 'Controllers').rglob('*.cpp'):
        text = without_comments(path.read_text(encoding='utf-8-sig'))
        # Resolve file-local string defines, including sibling headers.
        definitions = text + '\n' + '\n'.join(without_comments(p.read_text(encoding='utf-8-sig')) for p in path.parent.glob('*.h'))
        constants = dict(re.findall(r'^\s*#define\s+(\w+)\s+("(?:\\.|[^"\\])*")', definitions, re.M))
        # Macro declarations are not registrations; their invocations are handled below.
        text = re.sub(r'^\s*#define[^\n]*(?:\\\n[^\n]*)*', '', text, flags=re.M)
        for match in re.finditer(r'\b(REGISTER_[A-Z0-9_]*DETECTOR[A-Z0-9_]*)\s*\(\s*("(?:\\.|[^"\\])*"|[^,\n]+)\s*,', text):
            macro, first = match.groups()
            if macro == 'REGISTER_I2C_BUS_DETECTOR':
                continue
            first = constants.get(first.strip(), first.strip())
            if not first.startswith('"'):
                # These children can only be registered by their disabled parent detectors.
                if first == 'name' and 'QMKController' in str(path) and macro.startswith('REGISTER_DYNAMIC_HID_DETECTOR'):
                    continue
                unresolved.append((str(path.relative_to(root)), first))
                continue
            name = ast.literal_eval(first)
            names.add(name)
            if 'I2C_DRAM_DETECTOR' in macro:
                dimm.add(name)
    if unresolved:
        raise ValueError('Unresolved detector registrations: ' + repr(unresolved))
    assert TARGET in names and dimm
    return names, dimm

def config(names, addresses):
    disabled = {name: False for name in sorted(names)}
    disabled[TARGET] = True
    return {
        'Detectors': {'detectors': disabled, 'hid_safe_mode': True, 'initial_detection_delay_ms': 0},
        'Drivers': {'shared_smbus_access': True, 'smbus_sleep_mode': 2},
        'ENE6802Devices': {'enabled': True, 'devices': [
            {'bus_name': 'PawnIO SMBus i801 0', 'address': a,
             'pci_subsystem_vendor': 0x1043, 'pci_subsystem_device': 0x88EF}
            for a in addresses]},
        'ProfileManager': {key: {'enabled': False, 'name': ''} for key in
                           ['startup_profile', 'exit_profile', 'resume_profile', 'suspend_profile']},
        'AutoStart': {'enabled': False},
        'Server': {'enabled': False},
        'QMKOpenRGBDevices': [], 'QMKVialRGBDevices': [],
    }

def verify(document, names, dimm):
    detectors = document['Detectors']['detectors']
    assert set(names).issubset(detectors)
    assert [name for name, flag in detectors.items() if flag is True] == [TARGET]
    assert all(detectors[name] is False for name in dimm)
    assert detectors['ENE SMBus DRAM'] is False
    assert detectors['QMK OpenRGB Devices'] is False and detectors['QMK VialRGB Devices'] is False
    assert document['Drivers']['shared_smbus_access'] is True
    assert document['ENE6802Devices']['enabled'] is True
    endpoints=document['ENE6802Devices']['devices']
    addresses=[e['address'] for e in endpoints]
    assert addresses in ([0x71],[0x73],[0x71,0x73])
    assert endpoints == config(names,addresses)['ENE6802Devices']['devices']
    assert document['QMKOpenRGBDevices'] == [] and document['QMKVialRGBDevices'] == []
    assert document['AutoStart']['enabled'] is False and document['Server']['enabled'] is False
    assert all(x['enabled'] is False for x in document['ProfileManager'].values())

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--inventory', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--verify-only', action='store_true')
    args=parser.parse_args()
    names,dimm=registry_names()
    saved=json.loads(args.inventory.read_text(encoding='utf-8-sig'))['Detectors']['detectors']
    names.update(saved)  # Retain platform-specific names from the prior Windows inventory.
    source_hashes={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest()
                   for p in sorted((root/'Controllers/ENE6802Controller').glob('*')) if p.is_file()}
    manifest={'source_hashes':source_hashes,'detector_count':len(names),'dimm_detectors':sorted(dimm),
              'enabled_detector':TARGET,'hardware_executed':False,'configs':{}}
    for label,addresses in [('identify-both',[0x71,0x73]),('module-71',[0x71]),('module-73',[0x73])]:
        directory=args.output/label
        target=directory/'OpenRGB.json'
        if args.verify_only:
            data=target.read_bytes(); document=json.loads(data)
        else:
            document=config(names,addresses)
            data=(json.dumps(document,indent=2,ensure_ascii=False)+'\n').encode('utf-8')
            directory.mkdir(parents=True,exist_ok=True)
            with target.open('xb') as f: f.write(data)
        verify(document,names,dimm)
        assert [e['address'] for e in document['ENE6802Devices']['devices']] == addresses
        manifest['configs'][label]={'sha256':hashlib.sha256(data).hexdigest(),'addresses':addresses}
    if not args.verify_only:
        with (args.output/'manifest.json').open('x',encoding='utf-8') as f: json.dump(manifest,f,indent=2)
    print(json.dumps(manifest,indent=2))

if __name__ == '__main__': main()
