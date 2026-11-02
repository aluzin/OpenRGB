# SPDX-License-Identifier: GPL-2.0-or-later
"""Compile/run actual protocol code with a fake transport and no OS imports."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

here = Path(__file__).resolve().parent
root = here.parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--cl', default='cl.exe')
parser.add_argument('--mutations', action='store_true')
args = parser.parse_args()
build = here / 'build'
build.mkdir(exist_ok=True)
source = root / 'Controllers/ENE6802Controller/ENE6802Controller.cpp'
exe = build / 'test.exe'
command = [args.cl, '/nologo', '/std:c++17', '/W4', '/WX', '/GS-', '/Gs65536', '/GR-', '/Zl', '/Od',
           str(here / 'test.cpp'), str(source), '/Fe:' + str(exe),
           '/link', '/nodefaultlib', '/entry:main', '/subsystem:console', '/stack:1048576,65536']
subprocess.run(command, cwd=build, check=True)
data = exe.read_bytes()
pe = struct.unpack_from('<I', data, 0x3c)[0]
assert data[pe:pe + 4] == b'PE\0\0'
optional = pe + 24
assert struct.unpack_from('<H', data, optional)[0] == 0x20b
for index in (1, 13):
    assert struct.unpack_from('<II', data, optional + 112 + 8 * index) == (0, 0), 'Unexpected OS import'
result = subprocess.run([str(exe)], timeout=15)
report = {'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
          'exe_sha256': hashlib.sha256(data).hexdigest(), 'no_os_imports': True,
          'exit_code': result.returncode,
          'scope': 'Actual ENE6802Controller; fake transport; no hardware or vendor code execution'}
(build / 'result.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
assert result.returncode == 0, 'Protocol tests failed'

adapter_exe = build / 'adapter.exe'
adapter_command = command[:]
adapter_command[adapter_command.index(str(here / 'test.cpp'))] = str(here / 'adapter.cpp')
adapter_command[adapter_command.index(str(source))] = str(root / 'Controllers/ENE6802Controller/ENE6802Transport_Windows.cpp')
adapter_command[adapter_command.index('/Fe:' + str(exe))] = '/Fe:' + str(adapter_exe)
adapter_command[1:1] = ['/I' + str(here / 'stubs'), '/I' + str(root / 'i2c_smbus/Windows')]
subprocess.run(adapter_command, cwd=build, check=True)
adapter_bytes = adapter_exe.read_bytes()
adapter_pe = struct.unpack_from('<I', adapter_bytes, 0x3c)[0] + 24
for index in (1, 13):
    assert struct.unpack_from('<II', adapter_bytes, adapter_pe + 112 + 8 * index) == (0, 0)
assert subprocess.run([str(adapter_exe)], timeout=15).returncode == 0
report['adapter_no_os_imports'] = True
report['adapter_sha256'] = hashlib.sha256(adapter_bytes).hexdigest()

if args.mutations:
    original = source.read_text()
    mutations = {
        'legacy_apply': ('Write(0x802F, &apply, 1)', 'Write(0x80A0, &apply, 1)'),
        'wrong_color_bank': ('Write(0x8160, payload, 24)', 'Write(0xE300, payload, 24)'),
        'wrong_brightness': ('brightness = 9', 'brightness = 0'),
        'wrong_chip': ('!= 0x0268', '!= 0x6802'),
        'firmware_gate_removed': ('if(firmware[i] != expected_firmware[i])', 'if(false && firmware[i] != expected_firmware[i])'),
        'ignore_commit_error': ('Write(0x802F, &apply, 1);', '(Write(0x802F, &apply, 1), true);'),
        'no_failure_latch': ('if(!success) identified = false;', 'if(!success) identified = true;'),
        'green_blue_swap': ('(colors[i] >> 16)', '(colors[i] >> 8)'),
    }
    rejected = []
    for name, (old, new) in mutations.items():
        assert original.count(old) == 1, name
        mutant = build / (name + '.cpp')
        mutant.write_text(original.replace(old, new))
        mutant_exe = build / (name + '.exe')
        mutant_command = command[:]
        mutant_command[mutant_command.index(str(source))] = str(mutant)
        mutant_command[mutant_command.index('/Fe:' + str(exe))] = '/Fe:' + str(mutant_exe)
        mutant_command[1:1] = ['/I' + str(source.parent)]
        compiled = subprocess.run(mutant_command, cwd=build, capture_output=True, text=True)
        assert compiled.returncode == 0, compiled.stdout + compiled.stderr
        assert subprocess.run([str(mutant_exe)], timeout=15).returncode != 0, name
        rejected.append(name)
    report['mutations_rejected'] = rejected
(build / 'result.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
