#!/usr/bin/env python3
"""Load the original Wizard of Wor SC-01 phrases into VVVC's volatile RAM slots."""
import argparse
import json
from pathlib import Path
import re
import sys
import time

CATALOG = Path(__file__).resolve().parent / "data" / "wizard_of_wor.json"
USB_LINE_LIMIT = 253


def load_catalog(path=CATALOG):
    phrases = json.loads(path.read_text(encoding="utf-8"))["phrases"]
    if [phrase["id"] for phrase in phrases] != list(range(1, 76)):
        raise ValueError("The Wizard of Wor catalog must contain original IDs 1..75.")
    for phrase in phrases:
        phones = phrase["phonemes"]
        if not re.fullmatch(r"[A-Z0-9_]+(?: [A-Z0-9_]+)*", phones):
            raise ValueError(f"Invalid phoneme data in phrase {phrase['id']}.")
        if len(phones.split()) > 256:
            raise ValueError(f"Phrase {phrase['id']} exceeds 256 phonemes.")
        command_for(phrase)
    return phrases


def command_for(phrase):
    command = f"-learn {phrase['id']} phonemes {phrase['phonemes']}"
    if len(command.encode("ascii")) > USB_LINE_LIMIT:
        raise ValueError(f"Phrase {phrase['id']} exceeds the USB command limit.")
    return command


class Device:
    def __init__(self, port):
        self.port = port
        self.pending = bytearray()

    def send(self, command):
        data = (command + "\n").encode("ascii")
        if self.port.write(data) != len(data):
            raise RuntimeError("Incomplete USB serial write.")
        self.port.flush()

    def line(self, deadline):
        while time.monotonic() < deadline:
            byte = self.port.read(1)
            if byte == b"\n":
                line = self.pending.decode("ascii", errors="replace").rstrip("\r")
                self.pending.clear()
                if line.startswith("ERROR") or line.startswith("Audio unavailable"):
                    raise RuntimeError(f"Board replied: {line}")
                return line
            self.pending.extend(byte)
            if len(self.pending) > 4096:
                raise RuntimeError("Invalid serial response; check the port and baud rate.")
        raise TimeoutError("No complete reply from VVVC; close the serial monitor and check the port.")

    def expect(self, command, expected, timeout=5):
        self.send(command)
        deadline = time.monotonic() + timeout
        while True:
            line = self.line(deadline)
            if line == expected:
                return
            if line.startswith(("OK ", "VVVC -")):
                raise RuntimeError(f"Unexpected reply or board reset during {command.split()[0]}: {line}")

    def capacity(self):
        self.send("-help")
        deadline = time.monotonic() + 5
        while True:
            match = re.fullmatch(r"RAM phrase bank: (\d+) slots, (\d+) phones each; cleared on reset\.",
                                self.line(deadline))
            if match:
                return tuple(map(int, match.groups()))

    def slots(self):
        self.send("-slots")
        deadline = time.monotonic() + 5
        while True:
            line = self.line(deadline)
            if line.startswith("VVVC -"):
                raise RuntimeError("Board reset while verifying learned phrases.")
            header = re.fullmatch(r"SLOTS (\d+)", line)
            if header:
                break
        slots = {}
        while True:
            line = self.line(deadline)
            if line == "END":
                if len(slots) != int(header[1]):
                    raise RuntimeError("Incomplete slot listing from VVVC.")
                return slots
            row = re.fullmatch(r"SLOT (\d+) (\d+)", line)
            if not row or int(row[1]) in slots:
                raise RuntimeError(f"Invalid slot listing: {line}")
            slots[int(row[1])] = int(row[2])


def teach(device, phrases, report=print):
    slots, limit = device.capacity()
    if max(phrase["id"] for phrase in phrases) > slots:
        raise RuntimeError(f"Firmware has only {slots} slots. Upload the current 80-slot VVVC build first.")
    if any(len(phrase["phonemes"].split()) > limit for phrase in phrases):
        raise RuntimeError("The firmware's phrase size limit is too small.")
    for phrase in phrases:
        device.expect(command_for(phrase), f"OK {phrase['id']}")
        report(f"Slot {phrase['id']:2}: {phrase['text']}")
    learned = device.slots()
    for phrase in phrases:
        if learned.get(phrase["id"]) != len(phrase["phonemes"].split()):
            raise RuntimeError(f"Slot {phrase['id']} failed verification; the board may have reset.")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="ESP32 USB serial port, e.g. COM5 or /dev/ttyUSB0")
    parser.add_argument("--list", action="store_true", help="list original phrase/slot numbers and exit")
    parser.add_argument("--dry-run", action="store_true", help="print learn commands without connecting")
    parser.add_argument("--phrases", type=int, nargs="+", metavar="N",
                        help="load only these original phrase numbers (default: all 75)")
    parser.add_argument("--play", type=int, metavar="N", help="play this loaded slot once after verification")
    args = parser.parse_args(argv)
    try:
        phrases = load_catalog()
        if args.phrases:
            if any(number < 1 or number > len(phrases) for number in args.phrases):
                parser.error("--phrases numbers must be 1..75")
            phrases = [phrase for phrase in phrases if phrase["id"] in args.phrases]
        if args.play is not None and args.play not in {phrase["id"] for phrase in phrases}:
            parser.error("--play must refer to one of the phrases being loaded")
        if args.list:
            for phrase in phrases:
                print(f"{phrase['id']:2}: {phrase['text']}")
            return 0
        if args.dry_run:
            for phrase in phrases:
                print(command_for(phrase))
            return 0
        if not args.port:
            parser.error("--port is required to load the ESP32; use --list or --dry-run offline")
        try:
            import serial
        except ImportError:
            raise RuntimeError("Install pyserial: python -m pip install -r tools/requirements.txt") from None

        # Set control lines before opening to avoid deliberately resetting the RAM bank.
        # Some USB drivers still cause a reset on open, so allow boot tones to finish.
        with serial.Serial(port=None, baudrate=115200, timeout=0.1, write_timeout=2) as port:
            port.dtr = False
            port.rts = False
            port.port = args.port
            port.open()
            time.sleep(3)
            port.reset_input_buffer()
            device = Device(port)
            teach(device, phrases)
            print(f"Verified {len(phrases)} phrases. Recall with -play N from either port.")
            print("Slots are RAM-only: run this loader again after an ESP32 reset/reboot.")
            if args.play is not None:
                device.expect(f"-play {args.play}", f"OK {args.play}", timeout=120)
        return 0
    except (OSError, ValueError, RuntimeError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("Loading interrupted; slots already acknowledged remain learned.", file=sys.stderr)
        return 130


if __name__ == "__main__":
    sys.exit(main())
