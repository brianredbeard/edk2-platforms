#!/usr/bin/env python3
"""Verify and reproduce the pinned OS hardware descriptions (host build tool)."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

def build(source, output):
    manifest = json.loads((source/'source-manifest.json').read_text())
    tree = source/'Sources'
    for name, expected in manifest['files'].items():
        if hashlib.sha256((tree/name).read_bytes()).hexdigest() != expected:
            raise ValueError('source hash mismatch: '+name)
    output.mkdir(parents=True, exist_ok=True)
    for name in ['k3-pico-itx', 'k3-com260-ifx']:
        preprocessed = subprocess.check_output([
            'gcc', '-E', '-nostdinc', '-undef', '-D__DTS__', '-x', 'assembler-with-cpp',
            '-I'+str(tree/'include'), str(source/(name+'.os.dts'))])
        blob = output/(name+'.os.dtb')
        subprocess.run(['dtc','-I','dts','-O','dtb','-o',str(blob)], input=preprocessed,check=True)
        print(hashlib.sha256(blob.read_bytes()).hexdigest(), blob.name)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root',type=Path,default=Path(__file__).resolve().parents[1]/'Platform/Spacemit/K3/DeviceTree/Os')
    parser.add_argument('--output',type=Path,required=True)
    args = parser.parse_args()
    try:
        build(args.source_root, args.output)
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        sys.exit(str(error))
