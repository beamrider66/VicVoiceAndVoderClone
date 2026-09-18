"""Exercise the real loader against a serial peer, without an ESP32 attached."""
from contextlib import redirect_stdout
import io
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from tools import learn_wizard as loader


class SerialPeer:
    def __init__(self, slots=80):
        self.slot_limit = slots
        self.bank = {80: 2}  # Loading WOW must preserve unrelated slots.
        self.commands = []
        self.rx = bytearray()
        self.fail_id = None
        self.corrupt_id = None
        self.reset_id = None
        self.dtr = self.rts = True
        self.port = None
        self.closed = False

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.closed = True

    def open(self):
        assert self.port == "TEST" and not self.dtr and not self.rts

    def reset_input_buffer(self):
        self.rx.clear()

    def flush(self):
        pass

    def read(self, size):
        result = bytes(self.rx[:size])
        del self.rx[:size]
        return result

    def write(self, data):
        command = data.decode("ascii").rstrip("\n")
        self.commands.append(command)
        if command == "-help":
            reply = (f"VVVC - Vic Voice and Voder Clone\r\nRAM phrase bank: {self.slot_limit} "
                     "slots, 256 phones each; cleared on reset.\r\nVIC input: text...\r\n")
        elif command == "-slots":
            reply = f"SLOTS {len(self.bank)}\r\n"
            for number, count in sorted(self.bank.items()):
                reply += f"SLOT {number} {count + (number == self.corrupt_id)}\r\n"
            reply += "END\r\n"
        else:
            parts = command.split()
            number = int(parts[1])
            if number == self.fail_id:
                reply = f"ERROR {number} INVALID PHONEME\r\n"
            elif number == self.reset_id:
                reply = "VVVC - Vic Voice and Voder Clone\r\n"
            else:
                if parts[0] == "-learn":
                    assert parts[2] == "phonemes"
                    self.bank[number] = len(parts[3:])
                else:
                    assert parts[0] == "-play" and number in self.bank
                reply = f"OK {number}\r\n"
        self.rx.extend(reply.encode("ascii"))
        return len(data)


class LoaderTests(unittest.TestCase):
    def setUp(self):
        self.phrases = loader.load_catalog()
        self.serial = SerialPeer()
        self.device = loader.Device(self.serial)

    def teach(self):
        loader.teach(self.device, self.phrases, report=lambda _: None)

    def test_all_phrases_and_unrelated_slot(self):
        self.teach()
        self.assertEqual(len(self.serial.bank), 76)
        self.assertEqual(self.serial.bank[80], 2)
        self.assertEqual(self.serial.commands[1:76], [loader.command_for(p) for p in self.phrases])
        self.assertEqual(max(len(command) for command in self.serial.commands), 218)
        self.assertIn("I1_IU", self.serial.commands[2])

    def test_old_firmware_rejected_before_learning(self):
        self.serial.slot_limit = 32
        with self.assertRaisesRegex(RuntimeError, "only 32 slots"):
            self.teach()
        self.assertEqual(self.serial.commands, ["-help"])

    def test_device_error_stops_next_command(self):
        self.serial.fail_id = 3
        with self.assertRaisesRegex(RuntimeError, "ERROR 3"):
            self.teach()
        self.assertNotIn(3, self.serial.bank)
        self.assertNotIn(4, self.serial.bank)

    def test_reset_during_loading(self):
        self.serial.reset_id = 3
        with self.assertRaisesRegex(RuntimeError, "board reset"):
            self.teach()

    def test_verification_detects_wrong_count(self):
        self.serial.corrupt_id = 21
        with self.assertRaisesRegex(RuntimeError, "Slot 21 failed verification"):
            self.teach()

    def test_timeout_without_reply(self):
        with patch.object(loader.time, "monotonic", return_value=10):
            with self.assertRaises(TimeoutError):
                self.device.line(deadline=5)

    def test_offline_selection(self):
        output = io.StringIO()
        with redirect_stdout(output):
            self.assertEqual(loader.main(["--dry-run", "--phrases", "21", "75"]), 0)
        self.assertEqual(output.getvalue().splitlines(),
                         [loader.command_for(self.phrases[i - 1]) for i in (21, 75)])

    def test_load_and_play_cli(self):
        module = SimpleNamespace(Serial=lambda **_: self.serial)
        with patch.dict("sys.modules", {"serial": module}), patch.object(loader.time, "sleep"), \
                redirect_stdout(io.StringIO()):
            self.assertEqual(loader.main(["--port", "TEST", "--phrases", "21", "26", "--play", "21"]), 0)
        self.assertEqual(self.serial.commands[-1], "-play 21")
        self.assertEqual(set(self.serial.bank), {21, 26, 80})
        self.assertTrue(self.serial.closed)


if __name__ == "__main__":
    unittest.main()
