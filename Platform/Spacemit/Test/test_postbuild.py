#!/usr/bin/env python3
"""Exercise the real post-build CLI with competing baseline/candidate outputs."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

PLATFORM = Path(__file__).resolve().parents[1]

class PostBuildTests(unittest.TestCase):
    def build(self, workspace, current=True, missing_payload=False):
        for prefix, payload in [('baseline/Build', b'baseline firmware'), ('Build', b'candidate firmware')]:
            if prefix == 'Build' and not current:
                continue
            out = workspace / prefix / 'MUSE-Pico/RELEASE_GCC5/FV'
            out.mkdir(parents=True)
            if not (missing_payload and prefix == 'Build'):
                (out / 'MUSE-PICO.fd').write_bytes(payload)
            for name in ['k3-pico-itx', 'k3-com260-ifx', 'k3-deepcomputing-fml13v05']:
                subprocess.run(['dtc', '-I', 'dts', '-O', 'dtb', '-o', str(out / (name + '.dtb'))],
                               input='/dts-v1/; / { compatible="test"; };', text=True, check=True)
        (workspace / 'platform.dsc').write_text('[Defines]\n')
        (workspace / 'MUSE-Pico.its').write_bytes((PLATFORM / 'K3/MUSE-Pico/MUSE-Pico.its').read_bytes())
        (workspace / 'PostBuild.cfg').write_text('''[CONFIG]
BOARD = MUSE-Pico
BUILD_OUTPUT_DIRECTORY = Build/MUSE-Pico
FitImageBuild = TRUE
FIT_ITS_FILE = MUSE-Pico.its
FIT_Image_FILE = MUSE-Pico.itb
FIT_BUILD_FLAGS = -E -B 0x1000
''')
        env = dict(os.environ, WORKSPACE=str(workspace), PACKAGES_PATH=str(workspace))
        return subprocess.run(['python3', str(PLATFORM / 'PostBuild.py'), '-a', 'RISCV64',
                               '-p', 'platform.dsc', '--post', 'PostBuild.cfg', '-t', 'GCC5', '-b', 'RELEASE'],
                              env=env, text=True, capture_output=True)

    def test_candidate_selected_over_nested_baseline(self):
        with tempfile.TemporaryDirectory() as directory:
            w = Path(directory)
            r = self.build(w)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            extracted = w / 'payload.bin'
            subprocess.run(['dumpimage', '-T', 'flat_dt', '-p', '0', '-o', str(extracted),
                            str(w / 'fitimage/MUSE-Pico/MUSE-Pico.itb')], check=True, capture_output=True)
            self.assertEqual(extracted.read_bytes(), b'candidate firmware')

    def test_missing_candidate_does_not_package_baseline(self):
        with tempfile.TemporaryDirectory() as directory:
            r = self.build(Path(directory), current=False)
            self.assertNotEqual(r.returncode, 0, 'must reject missing configured build output')

    def test_missing_payload_does_not_package_baseline(self):
        with tempfile.TemporaryDirectory() as directory:
            r = self.build(Path(directory), missing_payload=True)
            self.assertNotEqual(r.returncode, 0, 'must reject missing firmware payload')

if __name__ == '__main__':
    unittest.main()
