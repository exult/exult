#!/usr/bin/env python3
"""Compile embedded Metal sources for an Apple SDK without requiring a GPU."""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sdk', choices=('macosx', 'iphoneos', 'iphonesimulator'), required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
ios = args.sdk != 'macosx'
header = root / 'imagewin/shaders' / 'scalers_metal.h'
modules = []
for name, initializer in re.findall(r'inline constexpr char (\w+)\[\]=\{(.*?)\};', header.read_text(), re.S):
    data = bytes(int(value) for value in re.findall(r'\d+', initializer))
    if not data or data[-1] != 0:
        raise SystemExit('Missing Metal source terminator')
    modules.append((name, data[:-1].decode('ascii')))
if len(modules) != 10:
    raise SystemExit('Missing embedded Metal shader modules')
with tempfile.TemporaryDirectory() as temporary:
    for name, source in modules:
        path = Path(temporary) / (name + '.metal')
        path.write_text(source)
        result = subprocess.run(['xcrun', '-sdk', args.sdk, 'metal',
                                 '-std=' + ('ios-metal2.1' if ios else 'macos-metal2.1'),
                                 '-c', str(path), '-o', str(path.with_suffix('.air'))],
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if result.returncode:
            print(result.stdout)
            detail = next((line for line in result.stdout.splitlines() if 'error:' in line), result.stdout[:500])
            detail = detail.replace('%', '%25').replace('\r', '%0D').replace('\n', '%0A')
            print(f'::error title={args.sdk} {name}::{detail}')
            raise SystemExit(result.returncode)
print(f'{args.sdk}: all {len(modules)} Metal shaders compiled')
