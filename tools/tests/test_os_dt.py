"""Compile the actual firmware-owned sources and check their handoff identity."""
import hashlib
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'Platform/Spacemit/K3/DeviceTree/Os'

class OsDtTests(unittest.TestCase):
    def build(self, output, source=SOURCE):
        return subprocess.run(['python3', str(ROOT/'tools/build-k3-os-dt.py'),
                               '--source-root', str(source), '--output', str(output)],
                              capture_output=True, text=True)

    def test_source_build_reproduces_pico_and_identifies_both_boards(self):
        with tempfile.TemporaryDirectory() as d:
            out = Path(d)
            result = self.build(out)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            pico = out/'k3-pico-itx.os.dtb'
            self.assertEqual(hashlib.sha256(pico.read_bytes()).hexdigest(),
                             '46609974206c59b2ebcc37ddfc6740c39f5f65a404c404a86322bc6b564ea55e')
            for name in ['k3-pico-itx', 'k3-com260-ifx']:
                blob = out/(name+'.os.dtb')
                compatible = subprocess.check_output(['fdtget', str(blob), '/', 'compatible'], text=True).split()
                self.assertEqual(compatible[0], 'spacemit,'+name)
                self.assertEqual(subprocess.check_output(['fdtget', str(blob), '/chosen', 'stdout-path'],text=True).strip(), 'serial0:115200n8')
                cpus = subprocess.check_output(['fdtget','-l',str(blob),'/cpus'],text=True).split()
                self.assertEqual(len([n for n in cpus if n.startswith('cpu@')]),8)

    def test_changed_source_is_rejected_before_build(self):
        with tempfile.TemporaryDirectory() as d:
            source = Path(d)/'source'
            shutil.copytree(SOURCE, source)
            file = source/'Sources/arch/riscv/boot/dts/spacemit/k3.dtsi'
            file.write_bytes(file.read_bytes()+b'\n// unintended source change\n')
            result = self.build(Path(d)/'output', source)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('source hash mismatch', result.stderr)

if __name__ == '__main__':
    unittest.main()
