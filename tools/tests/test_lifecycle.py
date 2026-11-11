"""Exercise actual DXE and reservation C code with mocked EFI services."""
import pathlib
import subprocess
import tempfile
import unittest
ROOT = pathlib.Path(__file__).resolve().parents[2]
LIB = ROOT / 'Silicon/Spacemit/K3/Library/K3OsFdtLib'
class LifecycleTests(unittest.TestCase):
    def test_publication_and_reservations(self):
        driver = ROOT / 'Silicon/Spacemit/K3/Drivers/K3OsFdtDxe'
        self.assertTrue((driver / 'K3OsFdtDxe.c').exists(), 'early DXE producer is missing')
        with tempfile.TemporaryDirectory() as tmp:
            exe = str(pathlib.Path(tmp)/'lifecycle')
            subprocess.run(['cc','-Wall','-Wextra','-Werror','-I'+str(ROOT/'Silicon/Spacemit/Drivers/FdtFixupDxe/Test/Include'), '-I'+str(LIB/'Test/Include'),
                '-I'+str(ROOT/'Silicon/Spacemit/Include'),'-I'+str(LIB),
                str(driver/'Test/test_publish.c'),str(ROOT/'Silicon/Spacemit/Drivers/FdtFixupDxe/FdtMemoryFixup.c'),str(LIB/'PrepareFdt.c'),str(LIB/'ReserveFdt.c'),
                '-lfdt','-o',exe],check=True)
            subprocess.run([exe],check=True)
if __name__ == '__main__': unittest.main()
