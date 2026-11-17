#!/usr/bin/env python3
"""Validate the embedded Vulkan modules without requiring a GPU."""
from pathlib import Path
import re
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
text = (root / 'imagewin/shaders/scalers_spirv.h').read_text()
modules = re.findall(r'inline constexpr uint32_t (\w+)\[\]\s*=\s*\{(.*?)\};', text, re.S)
if len(modules) != 10:
    raise SystemExit('Missing embedded Vulkan shader modules')
with tempfile.TemporaryDirectory() as temporary:
    for name, words in modules:
        values = [int(value, 16) for value in re.findall(r'0x[0-9a-fA-F]+', words)]
        path = Path(temporary) / (name + '.spv')
        path.write_bytes(struct.pack('<' + 'I' * len(values), *values))
        subprocess.run(['spirv-val', '--target-env', 'vulkan1.0', str(path)], check=True)
print(f'Vulkan 1.0: all {len(modules)} embedded shaders validated')
