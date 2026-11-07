#!/usr/bin/env python3
"""Package the tested x86_64 filter without local logs or test executables."""
from pathlib import Path
import hashlib
import shutil
import zipfile

repo = Path(__file__).resolve().parent.parent
name = 'x270-keyboard-2026.11.07.1-hrev59866+92-x86_64'
source = repo / 'keyboard'
artifact = repo / 'artifacts/keyboard-haiku-x86_64-hrev59866+92'
out = repo / 'dist'
stage = out / name
if stage.exists():
    shutil.rmtree(stage)
(stage / 'payload').mkdir(parents=True)
files = ['FnShortcuts.cpp', 'Makefile', 'README.md', 'test.cpp',
         'test-fullscreen.cpp', 'test-popup.cpp', 'install.sh', 'build_bundle.py']
for filename in files:
    shutil.copy2(source / filename, stage / filename)
shutil.copy2(artifact / 'fn_shortcuts', stage / 'payload/fn_shortcuts')
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
(artifact / 'SHA256SUMS').write_text(digest(artifact / 'fn_shortcuts') + '  fn_shortcuts\n')
manifest = ''.join(digest(p) + '  ' + p.relative_to(stage).as_posix() + '\n'
                   for p in sorted(stage.rglob('*')) if p.is_file())
(stage / 'SHA256SUMS').write_text(manifest)
archive = out / (name + '.zip')
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as bundle:
    for path in sorted(stage.rglob('*')):
        if path.is_file():
            bundle.write(path, path.relative_to(out))
(archive.with_suffix('.zip.sha256')).write_text(digest(archive) + '  ' + archive.name + '\n')
print(archive)
