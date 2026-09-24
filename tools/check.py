#!/usr/bin/env python3
"""Run speech, serial and DAC-output checks using a host GCC/Clang toolchain."""
import os
from pathlib import Path
import shutil
import subprocess
import sys

from learn_wizard import load_catalog

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build" / "test"
CORE = ROOT / "lib" / "sc01" / "src"


def run(*args):
    subprocess.run([str(arg) for arg in args], cwd=ROOT, check=True)


def main():
    cc = shutil.which(os.environ.get("CC", "gcc"))
    cxx = shutil.which(os.environ.get("CXX", "g++"))
    if not cc or not cxx:
        raise SystemExit("Install GCC/Clang (or set CC/CXX) to run the host checks.")
    BUILD.mkdir(parents=True, exist_ok=True)
    obj = BUILD / "english_reciter.o"
    exe = BUILD / ("test_core.exe" if os.name == "nt" else "test_core")
    run(cc, "-std=c11", "-O2", "-Wall", "-Wextra", "-c", CORE / "english_reciter.c", "-o", obj)
    run(cxx, "-std=c++11", "-O2", "-Wall", "-Wextra", "-I", CORE, "-I", ROOT / "include",
        ROOT / "test/test_core.cpp", CORE / "votrax_reciter.cpp", CORE / "votrax_synth.cpp",
        ROOT / "src/phrase_bank.cpp", obj, "-o", exe)
    subprocess.run([str(exe)], cwd=ROOT, check=True, timeout=30)
    firmware = BUILD / ("test_firmware.exe" if os.name == "nt" else "test_firmware")
    run(cxx, "-std=c++11", "-O2", "-Wall", "-Wextra", "-I", ROOT / "test/support",
        "-I", ROOT / "include", "-I", CORE, ROOT / "test/test_firmware.cpp",
        ROOT / "src/main.cpp", ROOT / "src/speech.cpp", ROOT / "src/vic_serial.cpp",
        ROOT / "src/phrase_bank.cpp", ROOT / "src/phrase_commands.cpp", ROOT / "src/serial_commands.cpp",
        CORE / "votrax_reciter.cpp", CORE / "votrax_synth.cpp", obj, "-o", firmware)
    catalog = BUILD / "wizard_phrases.tsv"
    catalog.write_text("".join(f"{p['id']}\t{p['phonemes']}\n" for p in load_catalog()),
                       encoding="ascii")
    subprocess.run([str(firmware), str(catalog)], cwd=ROOT, check=True, timeout=30)
    audio = BUILD / ("test_audio.exe" if os.name == "nt" else "test_audio")
    run(cxx, "-std=c++11", "-O2", "-Wall", "-Wextra", "-I", ROOT / "test/support/audio",
        "-I", ROOT / "test/support", "-I", ROOT / "include",
        ROOT / "test/test_audio.cpp", ROOT / "src/audio_codec.cpp", "-o", audio)
    subprocess.run([str(audio)], cwd=ROOT, check=True, timeout=30)
    fanout = BUILD / ("test_audio_fanout.exe" if os.name == "nt" else "test_audio_fanout")
    run(cxx, "-std=c++11", "-O2", "-Wall", "-Wextra", "-I", ROOT / "test/support",
        "-I", ROOT / "include", ROOT / "test/test_audio_fanout.cpp", ROOT / "src/audio.cpp",
        "-o", fanout)
    for mode in ("none", "pwm", "codec", "both", "fail-codec", "fail-pwm"):
        subprocess.run([str(fanout), mode], cwd=ROOT, check=True, timeout=30)
    pwm = BUILD / ("test_audio_pwm.exe" if os.name == "nt" else "test_audio_pwm")
    run(cxx, "-std=c++11", "-O2", "-Wall", "-Wextra", "-DVVVC_PWM_HOST_TEST",
        "-I", ROOT / "test/support/pwm", "-I", ROOT / "include",
        ROOT / "test/test_audio_pwm.cpp", ROOT / "src/audio_pwm.cpp", "-o", pwm)
    subprocess.run([str(pwm)], cwd=ROOT, check=True, timeout=30)
    run(sys.executable, "-m", "unittest", "discover", "-s", "test", "-p", "test_loader.py")


if __name__ == "__main__":
    main()
