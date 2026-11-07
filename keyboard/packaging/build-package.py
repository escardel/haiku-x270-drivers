#!/usr/bin/env python3
"""Build the native keyboard package and a source-inclusive ZIP on Haiku."""
from pathlib import Path
import hashlib
import shutil
import subprocess
import tempfile
import zipfile

source = Path(__file__).resolve().parent.parent
out = source / 'dist'
out.mkdir(exist_ok=True)
subprocess.run(['make', '-C', str(source), 'fn_shortcuts', 'check'], check=True)
package_name = 'x270_keyboard_shortcuts-2026.11.07.1-1-x86_64.hpkg'
bundle_name = 'x270-keyboard-2026.11.07.1-native-x86_64'
files = ['FnShortcuts.cpp', 'Makefile', 'README.md', 'test.cpp', 'test-fullscreen.cpp',
         'test-popup.cpp', 'packaging/Keyboard.PackageInfo',
         'packaging/X270Keyboard-License-Notice', 'packaging/build-package.py']
with tempfile.TemporaryDirectory(prefix='keyboard-package-', dir=out) as temporary:
    stage = Path(temporary) / 'package'
    paths = {
        source / 'fn_shortcuts': 'add-ons/input_server/filters/fn_shortcuts',
        source / 'README.md': 'documentation/packages/x270_keyboard_shortcuts/README.md',
        source / 'packaging/X270Keyboard-License-Notice': 'data/licenses/X270Keyboard-License-Notice',
        source / 'packaging/Keyboard.PackageInfo': '.PackageInfo',
    }
    for src, relative in paths.items():
        dest = stage / relative
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dest)
    subprocess.run(['package', 'create', '-C', str(stage), str(out / package_name)], check=True)
    bundle = Path(temporary) / bundle_name
    bundle.mkdir()
    shutil.copy2(out / package_name, bundle / package_name)
    shutil.copy2(source / 'README.md', bundle / 'README.md')
    for name in files:
        dest = bundle / 'source' / name
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source / name, dest)
    manifest = ''.join(hashlib.sha256(p.read_bytes()).hexdigest() + '  ' + p.relative_to(bundle).as_posix() + '\n'
                       for p in sorted(bundle.rglob('*')) if p.is_file())
    (bundle / 'SHA256SUMS').write_text(manifest)
    archive = out / (bundle_name + '.zip')
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as writer:
        for path in sorted(bundle.rglob('*')):
            if path.is_file():
                writer.write(path, path.relative_to(bundle.parent))
checksums = ''.join(hashlib.sha256(p.read_bytes()).hexdigest() + '  ' + p.name + '\n'
                    for p in [out / package_name, archive])
(out / 'Keyboard-SHA256SUMS').write_text(checksums)
print(out / package_name)
print(archive)
