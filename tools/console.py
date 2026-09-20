#!/usr/bin/env python3
"""Interactive VVVC serial console.

Opens the ESP32 USB console port and lets you type VVVC commands
(English text, -phonemes, -play, -learn, ...) directly. A background
thread continuously prints the board's output, so you can keep typing
the next command as soon as you want - the prompt is never blocked
waiting for a reply.

Usage:
    python console.py [PORT]          (default: COM3)
    python console.py --list          (list available serial ports)

Type 'exit' or Ctrl+C to quit.
"""
import argparse
import sys
import threading
import time

import serial
import serial.tools.list_ports


class Console:
    def __init__(self, port):
        self.port = port
        self._stop = threading.Event()
        self._reader = threading.Thread(target=self._read_loop, daemon=True)

    def _read_loop(self):
        # Continuously print whatever the board sends, so the main
        # thread can stay at input() and keep echoing your typing.
        while not self._stop.is_set():
            data = self.port.read(256)
            if data:
                sys.stdout.write(data.decode("ascii", "replace"))
                sys.stdout.flush()

    def start(self):
        self._reader.start()

    def send(self, line):
        self.port.write((line + "\n").encode("ascii"))
        self.port.flush()

    def stop(self):
        self._stop.set()
        self._reader.join(timeout=1.0)


def wait_for_boot(port, max_wait=12.0):
    """The board resets when the port opens and plays a startup test.
    Drain output until it goes quiet so the first command isn't lost."""
    print("Waiting for the board to finish booting ...")
    deadline = time.monotonic() + max_wait
    last_data = time.monotonic()
    seen_data = False
    while time.monotonic() < deadline:
        if port.in_waiting:
            port.read_all()  # discard boot noise
            seen_data = True
            last_data = time.monotonic()
        elif seen_data and time.monotonic() - last_data > 1.5:
            break  # board is idle and ready
        else:
            time.sleep(0.05)
    port.reset_input_buffer()
    print("Ready.\n")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", nargs="?", default="COM3",
                        help="serial port, e.g. COM3 (default: COM3)")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--list", action="store_true",
                        help="list available serial ports and exit")
    args = parser.parse_args(argv)

    if args.list:
        for p in serial.tools.list_ports.comports():
            print(f"{p.device}: {p.description}")
        return 0

    try:
        port = serial.Serial(args.port, args.baud, timeout=0.1)
    except serial.SerialException as exc:
        print(f"Cannot open {args.port}: {exc}")
        print("Run with --list to see available ports.")
        return 1

    wait_for_boot(port)

    console = Console(port)
    console.start()

    print(f"Connected to {args.port} at {args.baud} baud.")
    print("Type English text or VVVC commands (-help for the list).")
    print("'exit' or Ctrl+C to quit.\n")

    try:
        while True:
            try:
                line = input("vvvc> ")
            except (EOFError, KeyboardInterrupt):
                print()
                break
            line = line.strip()
            if not line:
                continue
            if line.lower() in ("exit", "quit"):
                break
            console.send(line)
    finally:
        console.stop()
        port.close()
        print("Disconnected.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
