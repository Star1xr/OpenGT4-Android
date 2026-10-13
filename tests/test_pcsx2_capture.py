"""Synthetic PINE capture regressions; no emulator or game data needed."""
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools import pcsx2_capture as cap


class WirePine(cap.Pine):
    def __init__(self):
        self.commands = []

    def call(self, commands):
        self.commands.extend(struct.iter_unpack('<BI', commands))
        result = bytearray()
        for opcode, address in struct.iter_unpack('<BI', commands):
            if not cap.RAM_ALIAS <= address < cap.RAM_ALIAS + cap.RAM_SIZE:
                raise AssertionError('PINE read used a low virtual address')
            size = 8 if opcode == cap.MSG_READ64 else 4
            result.extend((address + i) & 255 for i in range(size))
        return bytes(result)


class SnapshotPine:
    def __init__(self, statuses=None, kernel=True):
        self.statuses = iter(statuses or ['paused'] * 5)
        self.kernel = kernel
        self.reads = []

    def status(self):
        return next(self.statuses)

    def string(self, opcode):
        return 'synthetic'

    def read_ram(self, start, end):
        self.reads.append((start, end))
        data = bytearray(end - start)
        if start == 0 and self.kernel:
            data[0] = 1
        return bytes(data)

    def read32(self, addr):
        return 0


class CaptureTests(unittest.TestCase):
    def test_ram_alias_and_exact_word_tail(self):
        p = WirePine()
        self.assertEqual(p.read_ram(0x75000, 0x75014), bytes(range(20)))
        self.assertEqual(p.commands, [(3, 0x80075000), (3, 0x80075008), (2, 0x80075010)])

    def test_batch_boundary_and_ram_end(self):
        p = WirePine()
        with patch.object(cap, 'BATCH', 2):
            self.assertEqual(len(p.read_ram(0x1ffffe8, 0x2000000)), 24)
        self.assertEqual(p.commands[-1], (3, 0x81fffff8))
        with self.assertRaises(ValueError):
            p.read_ram(0x1fffff8, 0x2000004)

    def test_short_response_is_rejected(self):
        p = WirePine()
        with patch.object(p, 'call', return_value=b'\x00'):
            with self.assertRaisesRegex(RuntimeError, 'incomplete'):
                p.read_ram(0, 8)

    def test_running_vm_is_rejected_before_reads(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = SnapshotPine(['running'])
            with self.assertRaisesRegex(RuntimeError, 'Pause PCSX2'):
                cap.capture(p, Path(tmp) / 'capture')
            self.assertEqual(p.reads, [])
            self.assertEqual(list(Path(tmp).iterdir()), [])

    def test_resume_and_zero_kernel_do_not_publish(self):
        for p in [SnapshotPine(['paused'] * 4 + ['running']), SnapshotPine(kernel=False)]:
            with tempfile.TemporaryDirectory() as tmp:
                out = Path(tmp) / 'capture'
                with self.assertRaises(RuntimeError):
                    cap.capture(p, out)
                self.assertFalse(out.exists())

    def test_capture_metadata_and_no_overwrite(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / 'capture'
            cap.capture(SnapshotPine(), out)
            info = json.loads((out / 'info.json').read_text())
            self.assertEqual(info['capture_format'], 2)
            self.assertEqual(info['ranges'][0]['pine_start'], '0x80000000')
            for start, end in cap.RANGES:
                self.assertEqual((out / f'ram_{start:08x}_{end:08x}.bin').stat().st_size, end-start)
            with self.assertRaisesRegex(RuntimeError, 'existing captures'):
                cap.capture(SnapshotPine(), out)


if __name__ == '__main__':
    unittest.main()
