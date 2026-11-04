"""Compile a review-only PawnIO module. Never load it or open hardware."""
import argparse
import difflib
import hashlib
import json
import shutil
import subprocess
from pathlib import Path

REVISION = '52a7e536dff3e53c96917a28caac5e0fa6510696'  # signed tag 0.2.11
HERE = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--upstream', required=True, type=Path)
    parser.add_argument('--compiler', default='pawncc')
    args = parser.parse_args()
    upstream = args.upstream.resolve()
    revision = subprocess.check_output(
        ['git', '-C', str(upstream), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != REVISION:
        raise RuntimeError('Unexpected upstream revision: ' + revision)
    # No compiler input comes from an unreviewed dirty checkout.
    if subprocess.check_output(['git', '-C', str(upstream), 'status', '--porcelain']):
        raise RuntimeError('Upstream checkout must be clean')
    original = (upstream / 'SmbusI801.p').read_text()
    anchor = 'NTSTATUS:main() {'
    if original.count(anchor) != 1:
        raise RuntimeError('Unexpected upstream entry point')
    modified = original.replace(anchor, '#include "startup_recovery.inc"\n\n' + anchor)
    build = HERE / 'build' / 'pawn'
    build.mkdir(parents=True, exist_ok=True)
    shutil.copytree(upstream / 'include', build / 'include', dirs_exist_ok=True)
    shutil.copyfile(upstream / 'sleepmode.inc', build / 'sleepmode.inc')
    shutil.copyfile(HERE / 'startup_recovery.inc', build / 'startup_recovery.inc')
    (build / 'SmbusI801.p').write_text(modified)
    diff = ''.join(difflib.unified_diff(original.splitlines(True), modified.splitlines(True),
                                      fromfile='a/SmbusI801.p', tofile='b/SmbusI801.p'))
    helper = (HERE / 'startup_recovery.inc').read_text()
    diff += ''.join(difflib.unified_diff([], helper.splitlines(True),
                                       fromfile='/dev/null', tofile='b/startup_recovery.inc'))
    (build / 'candidate.patch').write_text(diff)
    command = [args.compiler, 'SmbusI801.p', '-iinclude', '-C64', '-;+', '-(+', '-p']
    compiled = subprocess.run(command, cwd=build, text=True, capture_output=True)
    log = compiled.stdout + compiled.stderr
    (build / 'compiler.txt').write_text(log)
    print(log)
    if compiled.returncode:
        raise RuntimeError('Pawn compiler failed: ' + str(compiled.returncode))
    output = build / 'SmbusI801.amx'
    data = output.read_bytes()
    if not data:
        raise RuntimeError('Empty compiler output')
    result = dict(upstream_revision=revision, compiler_command=command,
                  sha256=hashlib.sha256(data).hexdigest(), bytes=len(data),
                  hardware_loaded=False, signed=False,
                  automatic_recovery=False)
    (build / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
