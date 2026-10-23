from pathlib import Path
import shutil

prefs = Path('/boot/home/config/settings/Mozilla/Firefox/zh71vmo7.default-default/prefs.js')
backup = Path('/boot/home/x270-camera-mic/backups/prefs-before-firefox-identity.js')
shutil.copy2(prefs, backup)
text = prefs.read_text()
lines = text.splitlines(keepends=True)
removed = [line for line in lines if line.startswith('user_pref("general.useragent.override",')]
if not removed:
    print('Firefox already uses its default user-agent.')
    raise SystemExit(0)
if len(removed) != 1:
    raise SystemExit(f'Expected one user-agent override; found {len(removed)}. No change made.')
if not backup.exists():
    shutil.copy2(prefs, backup)
prefs.write_text(''.join(line for line in lines if line not in removed))
print('Restored Firefox default user-agent; original preferences backed up.')
