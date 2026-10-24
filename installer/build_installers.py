"""Build four self-contained, revision-specific Haiku installer ZIPs."""
from pathlib import Path
import hashlib
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DIST = ROOT / 'dist'
VERSION = '2026.10.23.1'
BASE = ROOT / 'artifacts/haiku-x86_64-hrev59866+92'
BT = ROOT / 'artifacts/bluetooth-haiku-x86_64-hrev59866+92'
DATA = '/boot/home/config/non-packaged/data/x270-drivers'
USER = '/boot/home/config/non-packaged'
SYSTEM = '/boot/system/non-packaged'

COMPONENTS = {
    'trackpoint-touchpad': [
        (BASE/'ps2', USER+'/add-ons/kernel/bus_managers/ps2'),
        (BASE/'mouse', USER+'/add-ons/input_server/devices/mouse'),
    ],
    'webcam': [(BASE/'aukey_webcam_v4.media_addon', USER+'/add-ons/media/aukey_webcam_v4.media_addon')],
    'microphone': [
        (BASE/'hda', USER+'/add-ons/kernel/drivers/bin/hda'),
        (BASE/'libmedia.so', SYSTEM+'/lib/libmedia.so'),
        (BASE/'mixer.media_addon', USER+'/add-ons/media/mixer.media_addon'),
        (BASE/'SoundRecorder-X270', USER+'/bin/SoundRecorder-X270'),
        (BASE/'audio_settings', DATA+'/microphone/audio_settings'),
        (BASE/'media_output_setup', DATA+'/microphone/media_output_setup'),
    ],
    'bluetooth': [
        (BT/'libbluetooth-X270.so', SYSTEM+'/lib/libbluetooth.so'),
        *[(BT/name, DATA+'/bluetooth/'+name) for name in (
            'bluetooth_server-X270', 'intel_firmware_loader', 'bt_probe',
            'ibt-0041-0041.sfi', 'ibt-0041-0041.ddc')],
        (ROOT/'bluetooth/start-bluetooth.sh', DATA+'/bluetooth/start-bluetooth.sh'),
    ],
}


def copy(source, dest):
    dest.parent.mkdir(parents=True, exist_ok=True)
    if source.suffix in ('.sh', '.py', '.md', '.patch', '.cpp', '.h', '.txt'):
        dest.write_text(source.read_text(encoding='utf-8-sig'), encoding='utf-8', newline='\n')
    else:
        shutil.copyfile(source, dest)


def hashes(directory):
    return ''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.relative_to(directory).as_posix()+'\n'
                   for p in sorted(directory.rglob('*')) if p.is_file() and p.name != 'SHA256SUMS')


DIST.mkdir(exist_ok=True)
for component, files in COMPONENTS.items():
    name = f'x270-{component}-{VERSION}-hrev59866+92-x86_64'
    bundle = DIST/name
    # Each generated directory is exclusively owned by this builder.
    if bundle.exists():
        assert bundle.resolve().parent == DIST.resolve()
        shutil.rmtree(bundle)
    bundle.mkdir()
    copy(ROOT/'installer/install.sh', bundle/'install.sh')
    (bundle/'COMPONENT').write_text(component+'\n', encoding='ascii', newline='\n')
    records = []
    for source, target in files:
        copy(source, bundle/'payload'/source.name)
        mode = '644' if source.suffix in ('.sfi', '.ddc') else '755'
        records.append(f'file|{source.name}|{target}|{mode}\n')
    if component == 'microphone':
        records.append(f'link|../../../bin/hda|{USER}/add-ons/kernel/drivers/dev/audio/hmulti/hda|0\n')
    (bundle/'files.list').write_text(''.join(records), encoding='ascii', newline='\n')
    for license in (ROOT/'licenses').iterdir():
        if license.is_file(): copy(license, bundle/'licenses'/license.name)
    if component == 'bluetooth': copy(ROOT/'bluetooth/LICENCE.ibt_firmware', bundle/'licenses/LICENCE.ibt_firmware')
    sources = {
        'trackpoint-touchpad': [
            'trackpoint-touchpad/patches', 'trackpoint-touchpad/gesture-addon',
            'trackpoint-touchpad/README.md',
            'trackpoint-touchpad/validation/TOUCHPAD-GESTURES.md',
            'trackpoint-touchpad/validation/gesture-test.log',
        ],
        'webcam': ['patches/uvc', 'BUILD.md', 'README.md'],
        'microphone': ['patches/haiku', 'installer/audio_settings.cpp', 'media_output_setup.cpp'],
        'bluetooth': ['bluetooth/patches', 'bluetooth/intel_firmware_loader.cpp', 'bluetooth/bt_probe.cpp', 'bluetooth/link.sh', 'bluetooth/README.md'],
    }[component]
    for relative in sources:
        source = ROOT/relative
        if source.is_dir():
            for p in source.rglob('*'):
                if p.is_file(): copy(p, bundle/'source'/relative/p.relative_to(source))
        else: copy(source, bundle/'source'/relative)
    details = {
        'trackpoint-touchpad': 'Includes the existing tested PS/2 module plus a native input_server mouse add-on. Single-finger taps left-click; three-finger taps middle-click. Two-finger scrolling keeps pointer motion suppressed through uneven finger lift-off. Enable tapping in Input preferences; zero tap sensitivity disables it. Existing scroll preferences are preserved. Reboot after installation.',
        'webcam': 'SunplusIT 5986:2118 UVC. Requires `libjpeg_turbo` (`pkgman install libjpeg_turbo` if absent). Validated at 640x480. Stops streaming when unused. High-resolution reopen and long-term USB reliability remain experimental.',
        'microphone': 'Realtek ALC298 on Intel 8086:9d71. Includes HDA, Media Kit recorder and mixer fixes, SoundRecorder, saved ADC9 selector 5/gain 12 dB, and HD Audio output selection. Installation briefly stops media services. Uninstall restores the saved pre-install audio settings.',
        'bluetooth': 'Intel AX210 8087:0032 only. Signed volatile firmware bootstrap and Bluetooth Kit initialization fix. Inquiry validated; pairing, HID, BLE, file transfer and Bluetooth audio unverified. Adds a background startup entry.',
    }[component]
    (bundle/'README.md').write_text(f'''# X270 {component} installer {VERSION}

Experimental binaries for **Haiku hrev59866+92 x86_64** on the tested ThinkPad X270. The installer rejects other revisions and architectures. Keep a working boot option; save work before installation. These packages do not install a compiler or rebuild Haiku.

{details}

Extract with Expander, open Terminal in this directory, and run:

```sh
bash install.sh --verify
bash install.sh --install
```

Reboot the X270 after installation. Install only the components needed. TrackPoint/touchpad are combined; webcam, microphone/audio, and Bluetooth are separate packages. Webcam and microphone can be installed independently. Standard system libraries must be present; see source and project build notes for dependencies.

To undo this component:

```sh
bash install.sh --uninstall
```

To upgrade an existing component, first run `--uninstall` from its previous extracted installer, then run this installer's `--install`. Keep the previous installer and its receipt backups until the upgrade is verified. An existing receipt causes `--install` to stop without replacing files.

Receipts/backups: `/boot/home/config/settings/x270-drivers/{component}`. Previous custom overrides are backed up, and packaged system files remain intact. Uninstall refuses to overwrite changed installed binaries. Completed uninstall receipts are archived with a timestamp, allowing reinstall. A failed installation attempts to restore prior files; retain its receipt for diagnosis.

If a kernel override prevents booting, use Haiku's boot menu to disable user add-ons, then restore the affected module from the receipt backups or remove that override. Bluetooth can be disabled by removing its marked UserBootscript command and quitting its custom server. No installer reboots automatically, changes partitions, or edits Firefox profiles.

Source, patches, validation and rebuild instructions: https://github.com/escardel/haiku-x270-drivers . Relevant patches and new source are included in `source/`. Upstream Haiku and UVC code retain their licenses. Intel firmware is unmodified and carries its redistribution license. The Bluetooth bootstrap is GPL-2.0-or-later; other new installer code is MIT. This release contains no microphone recordings, webcam images, browser preferences, or pairing keys.
''', encoding='utf-8', newline='\n')
    (bundle/'SHA256SUMS').write_text(hashes(bundle), encoding='ascii', newline='\n')
    with zipfile.ZipFile(DIST/(name+'.zip'), 'w', zipfile.ZIP_DEFLATED) as archive:
        for p in sorted(bundle.rglob('*')):
            if p.is_file(): archive.write(p, name+'/'+p.relative_to(bundle).as_posix())
    print(name+'.zip')

archives = sorted(DIST.glob(f'x270-*-{VERSION}-hrev59866+92-x86_64.zip'))
(DIST/'SHA256SUMS').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n' for p in archives), encoding='ascii', newline='\n')
