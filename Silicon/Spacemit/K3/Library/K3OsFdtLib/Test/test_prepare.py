"""Run real DTB preparation code against libfdt; no firmware services mocked."""
import ctypes as C
import pathlib
import subprocess
import tempfile
import unittest

HERE = pathlib.Path(__file__).resolve().parent
SRC = HERE.parent

class PrepareTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.root = pathlib.Path(cls.tmp.name)
        library = cls.root / 'prepare.so'
        subprocess.run(['cc', '-shared', '-fPIC', '-Wall', '-Wextra', '-Werror',
                        '-I'+str(HERE/'Include'), '-I'+str(SRC.parents[2]/'Include'),
                        str(SRC/'PrepareFdt.c'), '-lfdt', '-o', str(library)], check=True)
        cls.lib = C.CDLL(str(library))
        cls.fn = cls.lib.K3PrepareOsFdt
        cls.fn.argtypes = [C.c_void_p, C.c_size_t, C.c_void_p, C.c_size_t,
                          C.c_void_p, C.c_size_t, C.c_uint32]
        cls.fn.restype = C.c_int

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def dtb(self, compatible='spacemit,k3-pico-itx', extra=''):
        text = '/dts-v1/; / { compatible="'+compatible+'"; #address-cells=<2>; #size-cells=<2>; '+extra+' };'
        return subprocess.run(['dtc', '-q', '-I', 'dts', '-O', 'dtb'],
                              input=text.encode(), capture_output=True, check=True).stdout

    def prepare(self, fw, os, size=32768):
        a, b, out = C.create_string_buffer(fw), C.create_string_buffer(os), C.create_string_buffer(size)
        result = self.fn(a, len(fw), b, len(os), out, size, 3)
        self.assertEqual(a.raw[:len(fw)], fw)
        self.assertEqual(b.raw[:len(os)], os)
        return result, out.raw

    def prop(self, blob, path, name, kind='s'):
        f = self.root/'out.dtb'; f.write_bytes(blob)
        return subprocess.check_output(['fdtget', '-t', kind, str(f), path, name]).decode().strip()

    def test_boot_identity_and_os_uart_survive(self):
        fw = self.dtb(extra='chosen { stdout-path="bad"; };')
        os = self.dtb(extra='chosen { stdout-path="serial0:115200n8"; };')
        result, out = self.prepare(fw, os)
        self.assertEqual(result, 0)
        self.assertEqual(self.prop(out, '/chosen', 'boot-hartid', 'u'), '3')
        self.assertEqual(self.prop(out, '/chosen', 'stdout-path'), 'serial0:115200n8')

    def test_wrong_board_rejected(self):
        pico, com = self.dtb(), self.dtb('spacemit,k3-com260-ifx')
        self.assertLess(self.prepare(com, pico)[0], 0)
        self.assertLess(self.prepare(pico, com)[0], 0)

    def test_com260_ifx_supported_without_cross_board_fallback(self):
        com = self.dtb('spacemit,k3-com260-ifx')
        self.assertEqual(self.prepare(com, com)[0], 0)
        tq = self.dtb('spacemit,k3-com260-tq')
        self.assertLess(self.prepare(tq, com)[0], 0)

    def test_ambiguous_board_list_rejected(self):
        mixed = self.dtb('spacemit,k3-pico-itx","spacemit,k3-com260-ifx')
        self.assertLess(self.prepare(mixed, self.dtb())[0], 0)

    def test_product_identity_is_bounded_and_exact(self):
        self.assertTrue(hasattr(self.lib, 'K3ValidateProduct'), 'bounded EEPROM identity API required')
        fn = self.lib.K3ValidateProduct
        fn.argtypes = [C.c_void_p,C.c_size_t,C.c_void_p,C.c_size_t]
        fn.restype = C.c_int
        fw = self.dtb()
        for product, expected in [(b'k3-pico-itx\0',0), (b'k3_com260_ifx\0',-1),
                                  (b'k3-pico-itx',-1),(b'k3-pico-itx-unknown\0',-1)]:
            result = fn(C.create_string_buffer(fw),len(fw),C.create_string_buffer(product),len(product))
            self.assertEqual(result == 0, expected == 0)

    def test_truncated_inputs_and_short_output_rejected(self):
        good = self.dtb()
        for bad in (b'', good[:16], good[:-1], b'X'*64):
            self.assertLess(self.prepare(bad, good)[0], 0)
            self.assertLess(self.prepare(good, bad)[0], 0)
        self.assertLess(self.prepare(good, good, 32)[0], 0)

    def test_firmware_reservation_preserved_without_changing_source(self):
        fw = self.dtb(extra='reserved-memory { #address-cells=<2>; #size-cells=<2>; ranges; '
                           'secure@100000000 { reg=<1 0 0 0x100000>; no-map; }; };')
        result, out = self.prepare(fw, self.dtb())
        self.assertEqual(result, 0)
        self.assertEqual(self.prop(out, '/reserved-memory/firmware-secure@100000000', 'reg', 'x'), '1 0 0 100000')
        self.assertEqual(self.prop(out, '/reserved-memory/firmware-secure@100000000', 'no-map'), '')

    def test_existing_os_nomap_cover_is_not_duplicated(self):
        fw = self.dtb(extra='reserved-memory { #address-cells=<2>; #size-cells=<2>; ranges; '
                           'secure@100000000 { reg=<1 0 0 0x100000>; no-map; }; };')
        os = self.dtb(extra='reserved-memory { #address-cells=<2>; #size-cells=<2>; ranges; '
                           'os@100000000 { reg=<1 0 0 0x2000000>; no-map; }; };')
        result, out = self.prepare(fw, os)
        self.assertEqual(result, 0)
        f=self.root/'out.dtb'; f.write_bytes(out)
        nodes=subprocess.check_output(['fdtget','-l',str(f),'/reserved-memory']).decode().split()
        self.assertEqual(nodes, ['os@100000000'])

    def test_missing_chosen_created(self):
        result, out = self.prepare(self.dtb(), self.dtb())
        self.assertEqual(result, 0)
        self.assertEqual(self.prop(out, '/chosen', 'boot-hartid', 'u'), '3')

    def test_reserved_memory_encoding_must_be_explicit(self):
        fw=self.dtb(extra='reserved-memory { #address-cells=<1>; #size-cells=<1>; ranges; secure@1000 { reg=<0x1000 0x1000>; no-map; }; };')
        self.assertLess(self.prepare(fw, self.dtb())[0], 0)

    def test_reservation_block_survives(self):
        text='/dts-v1/; /memreserve/ 0x100000000 0x1000; / { compatible="spacemit,k3-pico-itx"; };'
        fw=subprocess.run(['dtc','-q','-O','dtb'],input=text.encode(),capture_output=True,check=True).stdout
        result,out=self.prepare(fw,self.dtb())
        self.assertEqual(result,0)
        rendered=subprocess.run(['dtc','-q','-I','dtb','-O','dts'],input=out,capture_output=True,check=True).stdout.decode()
        self.assertIn('/memreserve/\t0x0000000100000000 0x0000000000001000;',rendered)

    def test_corrupt_header_bounds_rejected(self):
        import struct
        good=self.dtb()
        for offset in (4,8,12,16,32,36):
            bad=bytearray(good); struct.pack_into('>I',bad,offset,0xfffffff0)
            self.assertLess(self.prepare(bytes(bad),good)[0],0)

    def test_os_reusable_region_does_not_cover_firmware_nomap(self):
        fw=self.dtb(extra='reserved-memory { #address-cells=<2>; #size-cells=<2>; ranges; secure@100000000 { reg=<1 0 0 0x1000>; no-map; }; };')
        os=self.dtb(extra='reserved-memory { #address-cells=<2>; #size-cells=<2>; ranges; pool@100000000 { reg=<1 0 0 0x10000>; reusable; }; };')
        result,out=self.prepare(fw,os)
        self.assertLess(result,0)

    def test_disabled_os_reservation_does_not_cover_firmware(self):
        fw=self.dtb(extra='reserved-memory { #address-cells=<2>; #size-cells=<2>; ranges; secure@100000000 { reg=<1 0 0 0x1000>; no-map; }; };')
        os=self.dtb(extra='reserved-memory { #address-cells=<2>; #size-cells=<2>; ranges; disabled@100000000 { reg=<1 0 0 0x1000>; no-map; status="disabled"; }; };')
        result,out=self.prepare(fw,os)
        self.assertEqual(result,0)
        self.assertEqual(self.prop(out,'/reserved-memory/firmware-secure@100000000','no-map'),'')

if __name__ == '__main__':
    unittest.main()
