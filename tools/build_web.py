"""Build a self-contained browser installer; optionally publish to a web folder."""
import argparse
import base64
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
PIO_HOME = Path.home() / '.platformio'
TOOL_VERSION = '10.4.0'
INTEGRITY = '3pwkeFFm5Fj7UQo8SJNYK5RXrtNCpq6X9QoI6bMT4GBZWgrJqjn0YvM9ihG74BtMoSFYXfmDtkehuxe50PTMPQ=='


def run(*args):
    subprocess.run([str(a) for a in args], cwd=ROOT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--publish', type=Path, help='Destination folder, e.g. \\\\qnap\\qweb\\vvvc')
    args = parser.parse_args()
    pio = shutil.which('pio') or PIO_HOME / 'penv/Scripts/pio.exe'
    python = PIO_HOME / ('penv/Scripts/python.exe' if __import__('os').name == 'nt' else 'penv/bin/python')
    run(pio, 'run', '-e', 'esp32dev')
    output = ROOT / 'build/web'
    output.mkdir(parents=True, exist_ok=True)
    deps = ROOT / 'build/web-deps'
    deps.mkdir(exist_ok=True)
    archive = deps / f'esp-web-tools-{TOOL_VERSION}.tgz'
    if not archive.exists():
        urllib.request.urlretrieve(f'https://registry.npmjs.org/esp-web-tools/-/esp-web-tools-{TOOL_VERSION}.tgz', archive)
    assert base64.b64encode(hashlib.sha512(archive.read_bytes()).digest()).decode() == INTEGRITY, 'ESP Web Tools integrity mismatch'
    vendor = output / 'vendor/esp-web-tools'
    vendor.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive) as package:
        for member in package.getmembers():
            if member.isfile() and (member.name.startswith('package/dist/web/') or member.name == 'package/LICENSE'):
                # Only flat web bundle files are published; never extract archive paths.
                (vendor / Path(member.name).name).write_bytes(package.extractfile(member).read())
    artifacts = ROOT / '.pio/build/esp32dev'
    merged = deps / 'merged.bin'
    run(python, PIO_HOME / 'packages/tool-esptoolpy/esptool.py', '--chip', 'esp32', 'merge_bin', '-o', merged,
        '--flash_mode', 'dio', '--flash_freq', '40m', '--flash_size', '4MB',
        '0x1000', artifacts / 'bootloader.bin', '0x8000', artifacts / 'partitions.bin',
        '0xe000', PIO_HOME / 'packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin',
        '0x10000', artifacts / 'firmware.bin')
    data = merged.read_bytes()
    assert data[0x1000] == 0xE9 and data[0x10000] == 0xE9
    assert data[0x1002:0x1004] == bytes([2, 0x20]), 'Expected DIO, 4MB, 40MHz boot header'
    digest = hashlib.sha256(data).hexdigest()
    commit = subprocess.check_output(['git', 'rev-parse', '--short', 'HEAD'], cwd=ROOT, text=True).strip()
    dirty = subprocess.check_output(['git', 'status', '--porcelain', '--', 'src', 'include', 'lib', 'platformio.ini'], cwd=ROOT, text=True).strip()
    version = f'{datetime.now(timezone.utc):%Y.%m.%d}-{commit}' + ('-modified' if dirty else '')
    firmware_path = f'firmware/vvvc-{digest[:16]}.bin'
    (output / 'firmware').mkdir(exist_ok=True)
    (output / firmware_path).write_bytes(data)
    manifest = {'name': 'VVVC', 'version': version, 'new_install_prompt_erase': True,
                'new_install_improv_wait_time': 0, 'builds': [{'chipFamily': 'ESP32', 'improv': False,
                'parts': [{'path': firmware_path, 'offset': 0}]}]}
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    release = {'version': version, 'source_commit': commit, 'firmware_sources_modified': bool(dirty),
               'built_utc': datetime.now(timezone.utc).isoformat(), 'firmware': firmware_path,
               'sha256': digest, 'bytes': len(data), 'esp_web_tools': TOOL_VERSION,
               'board': 'ESP32 Audio-Kit, ES8388, 4MB', 'flash_offset': 0}
    (output / 'release.json').write_text(json.dumps(release, indent=2) + '\n', encoding='utf-8')
    for source in (ROOT / 'web').iterdir():
        if source.is_file():
            (output / source.name).write_text(source.read_text(encoding='utf-8').replace('@@VERSION@@', version).replace('@@FIRMWARE@@', firmware_path), encoding='utf-8')
    shutil.copyfile(ROOT / 'output/pdf/VVVC_User_and_VIC20_Wiring_Guide_v2.pdf', output / 'guide.pdf')
    # Create the portable copy outside the site, so it cannot include itself.
    zip_path = ROOT / 'build/vvvc-local-installer.zip'
    import zipfile
    with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as portable:
        for file in output.rglob('*'):
            if file.is_file() and file.suffix != '.zip':
                portable.write(file, file.relative_to(output))
    shutil.copyfile(zip_path, output / zip_path.name)
    if args.publish:
        destination = args.publish.resolve()
        if destination.name.lower() != 'vvvc':
            parser.error('Publish to a dedicated folder named vvvc, not the web root.')
        destination.mkdir(parents=True, exist_ok=True)
        files = [p for p in output.rglob('*') if p.is_file()]
        # Dependencies first, entry point last. Existing releases are retained.
        files.sort(key=lambda p: (p.name in ('manifest.json', 'index.html'), p.name == 'index.html'))
        for source in files:
            target = destination / source.relative_to(output)
            target.parent.mkdir(parents=True, exist_ok=True)
            temporary = target.with_name(target.name + '.uploading')
            shutil.copyfile(source, temporary)
            temporary.replace(target)
            assert hashlib.sha256(target.read_bytes()).digest() == hashlib.sha256(source.read_bytes()).digest(), target
        print(f'Published and verified {len(files)} files: {destination}')
    print(f'Installer: {output}\nFirmware: {version}\nSHA-256: {digest}')


if __name__ == '__main__':
    main()
