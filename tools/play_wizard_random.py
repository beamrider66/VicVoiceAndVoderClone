#!/usr/bin/env python3
"""Load the original Wizard of Wor SC-01 phrases, then play them at random.

Reuses the proven serial/teach logic from learn_wizard.py. After all phrases
are learned and verified, it speaks a random phrase, waits for the board to
finish (the "OK N" reply arrives after speech completes), then picks the next
one. Stops after --count phrases, or runs until Ctrl+C when --loop is given.
"""
import argparse
import random
import sys
import time

from learn_wizard import Device, load_catalog, teach


def play_random(device, phrases, count=None, loop=False, report=print):
    """Speak random learned phrases. Returns the number of phrases played."""
    by_id = {phrase["id"]: phrase for phrase in phrases}
    pool = list(by_id)
    played = 0
    last = None
    try:
        while loop or (count is None) or (played < count):
            # Avoid repeating the same phrase back-to-back when the pool allows it.
            if len(pool) > 1:
                choice = random.choice([p for p in pool if p != last])
            else:
                choice = pool[0]
            last = choice
            report(f"Playing slot {choice}: {by_id[choice]['text']}")
            # The "OK N" reply is sent only after the phrase has finished speaking.
            device.expect(f"-play {choice}", f"OK {choice}", timeout=120)
            played += 1
            if count is not None and played >= count:
                break
    except KeyboardInterrupt:
        report(f"\nStopped after {played} phrases.")
    return played


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="ESP32 USB serial port, e.g. COM3 or /dev/ttyUSB0")
    parser.add_argument("--phrases", type=int, nargs="+", metavar="N",
                        help="load and play only these original phrase numbers (default: all 75)")
    parser.add_argument("--count", type=int, metavar="N",
                        help="play this many random phrases, then stop (default: 10)")
    parser.add_argument("--loop", action="store_true",
                        help="keep playing random phrases until interrupted (Ctrl+C)")
    parser.add_argument("--seed", type=int, metavar="N",
                        help="seed the random generator for reproducible order")
    args = parser.parse_args(argv)

    if not args.port:
        parser.error("--port is required; this script always connects to the board")

    try:
        import serial
    except ImportError:
        raise RuntimeError("Install pyserial: python -m pip install -r tools/requirements.txt") from None

    if args.seed is not None:
        random.seed(args.seed)

    phrases = load_catalog()
    if args.phrases:
        if any(number < 1 or number > len(phrases) for number in args.phrases):
            parser.error("--phrases numbers must be 1..75")
        phrases = [phrase for phrase in phrases if phrase["id"] in args.phrases]

    count = None if args.loop else (args.count if args.count is not None else 10)

    try:
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
            print(f"Verified {len(phrases)} phrases. Playing at random ...")
            played = play_random(device, phrases, count=count, loop=args.loop)
            print(f"Done. Played {played} phrase(s).")
        return 0
    except (OSError, ValueError, RuntimeError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("Interrupted; slots already acknowledged remain learned.", file=sys.stderr)
        return 130


if __name__ == "__main__":
    sys.exit(main())
