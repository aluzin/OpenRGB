# SPDX-License-Identifier: GPL-2.0-or-later
"""Offline MSVC tests. No Windows SDK, real mutex, PawnIO or hardware access.

Compile production headers and verbatim constructor, destructor, transfer and
port-selection function bodies with fake dependencies. Not a full OpenRGB build.
"""
import argparse
from pathlib import Path
import shutil
import struct
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
CPP = Path('i2c_smbus/Windows/i2c_smbus_pawnio.cpp')
HEADER = Path('i2c_smbus/Windows/SharedSMBusMutex.h')


def function(source, signature):
    if source.count(signature) != 1:
        raise RuntimeError(f'Expected exactly one {signature}')
    start = source.index(signature)
    # These two functions have no braces in string literals/comments. Stop at
    # their top-level closing brace; never replace or reimplement their logic.
    brace = source.index('{', start)
    depth = 0
    for pos in range(brace, len(source)):
        depth += (source[pos] == '{') - (source[pos] == '}')
        if depth == 0:
            return source[start:pos + 1]
    raise RuntimeError('Unterminated production function')


def assert_no_imports(exe):
    data = exe.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    assert data[pe:pe+4] == b'PE\0\0'
    optional = pe + 24
    assert struct.unpack_from('<H', data, optional)[0] == 0x20b, 'Require x64 PE'
    # PE32+ data directories: import table and delay-import table.
    for index in (1, 13):
        assert struct.unpack_from('<II', data, optional + 112 + 8*index) == (0, 0), 'Unexpected DLL imports'


def run(cl, root, integration=True):
    testdir = root / 'tests/smbus_mutex'
    build = testdir / 'build'
    build.mkdir(exist_ok=True)
    if integration:
        source = (root / CPP).read_text()
        code = (testdir / 'integration_prefix.inc').read_text()
        for signature in ('s32 piix4_port_sel(', 'i2c_smbus_pawnio::i2c_smbus_pawnio(',
                          'i2c_smbus_pawnio::~i2c_smbus_pawnio(', 's32 i2c_smbus_pawnio::i2c_smbus_xfer('):
            code += '\n' + function(source, signature) + '\n'
        code += (testdir / 'integration_suffix.inc').read_text()
        src = build / 'integration.cpp'
        src.write_text(code)
    else:
        src = testdir / 'test.cpp'
    exe = build / (src.stem + '.exe')
    args = [str(cl), '/nologo', '/std:c++17', '/W4', '/WX', '/GS-', '/Zl', '/Od',
            '/I' + str(testdir / 'stubs'), '/I' + str(testdir), str(src),
            '/Fo' + str(build / (src.stem + '.obj')), '/Fe' + str(exe),
            '/link', '/nodefaultlib', '/entry:main', '/subsystem:console']
    compiled = subprocess.run(args, capture_output=True, text=True)
    if compiled.returncode:
        raise RuntimeError(compiled.stdout + compiled.stderr)
    assert_no_imports(exe)
    return subprocess.run([str(exe)]).returncode


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--cl', required=True, type=Path, help='x64 MSVC cl.exe')
    parser.add_argument('--mutations', action='store_true')
    args = parser.parse_args()
    for integration in (False, True):
        result = run(args.cl, ROOT, integration)
        assert result == 0, f'{"integration" if integration else "unit"} failure count: {result}'
        print(f'PASS: {"production constructor/destructor/transfer/port bodies" if integration else "mutex helper"}; no DLL imports')

    if args.mutations:
        mutations = [
            ('default_acl', HEADER, '&attributes, FALSE', 'nullptr, FALSE'),
            ('no_fallback', HEADER, 'error == ERROR_ACCESS_DENIED', 'error == ERROR_INVALID_HANDLE'),
            ('accept_failed_wait', HEADER, 'result == WAIT_ABANDONED', 'result == WAIT_FAILED'),
            ('reject_abandoned', HEADER, ' || result == WAIT_ABANDONED', ''),
            ('release_without_ownership', HEADER, 'if(acquired)', 'if(handle != nullptr)'),
            ('ignore_transfer_lock', CPP, 'if(!lock.Ready())', 'if(false)', 2),
            ('ignore_port_lock', CPP, 'if(!lock.Ready())', 'if(false)', 1),
            ('bypass_null_handle', CPP,
             'SharedSMBusLock lock(global_smbus_access_handle, shared_smbus_access);',
             'SharedSMBusLock lock(global_smbus_access_handle, global_smbus_access_handle != nullptr);'),
            ('invalid_disabled_handle', Path('i2c_smbus/Windows/i2c_smbus_pawnio.h'),
             'global_smbus_access_handle = nullptr;',
             'global_smbus_access_handle = reinterpret_cast<HANDLE>(1);'),
        ]
        for item in mutations:
            name, relative, before, after, *nth = item
            target = HERE / 'build' / 'mutations' / name
            for path in (CPP, HEADER, Path('i2c_smbus/Windows/i2c_smbus_pawnio.h'), Path('tests/smbus_mutex/test.cpp'),
                         Path('tests/smbus_mutex/integration_prefix.inc'),
                         Path('tests/smbus_mutex/integration_suffix.inc'),
                         Path('tests/smbus_mutex/stubs/windows.h'),
                         Path('tests/smbus_mutex/stubs/sddl.h'),
                         Path('tests/smbus_mutex/stubs/string'),
                         Path('tests/smbus_mutex/stubs/unordered_map'),
                         Path('tests/smbus_mutex/stubs/i2c_smbus.h')):
                dest = target / path
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / path, dest)
            file = target / relative
            original = file.read_text()
            occurrence = nth[0] if nth else 1
            positions = []
            pos = original.find(before)
            while pos != -1:
                positions.append(pos)
                pos = original.find(before, pos + len(before))
            assert len(positions) >= occurrence
            pos = positions[occurrence - 1]
            file.write_text(original[:pos] + after + original[pos + len(before):])
            # Compilation failure does not count as detecting a broken behavior.
            result = run(args.cl, target)
            assert result != 0, f'Mutation escaped: {name}'
            print(f'PASS: detected regression {name} (failure count {result})')


if __name__ == '__main__':
    main()
