"""Verify release ZIPs on Haiku; exercise install/uninstall in isolated paths."""
from pathlib import Path
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(args, cwd, log, success=True, env=None):
    result = subprocess.run(args, cwd=cwd, env=env, capture_output=True, text=True)
    with log.open('a') as output:
        output.write('$ ' + ' '.join(map(str, args)) + '\n')
        output.write(result.stdout + result.stderr)
    assert (result.returncode == 0) == success, (args, result.returncode, log)
    return result


def lifecycle(bundle, workspace, prior_mouse=False, fail_rename=False):
    case = workspace / ('rename-failure' if fail_rename else 'existing-mouse' if prior_mouse else 'fresh-mouse')
    copy = case / 'installer'
    shutil.copytree(bundle, copy)
    home = case / 'home'
    ps2 = home / 'config/non-packaged/add-ons/kernel/bus_managers/ps2'
    mouse = home / 'config/non-packaged/add-ons/input_server/devices/mouse'
    ps2.parent.mkdir(parents=True)
    ps2.write_bytes(b'previous kernel override\n')
    if prior_mouse:
        mouse.parent.mkdir(parents=True)
        mouse.write_bytes(b'previous mouse override\n')
    # Redirect only this copy of the installer and its manifest. Keep HOME and
    # the running input server untouched. All actual destinations are audited.
    script = copy / 'install.sh'
    script.write_text(script.read_text().replace('$HOME/config/', str(home) + '/config/')
                      .replace('/boot/home/config/', str(home) + '/config/'))
    manifest = copy / 'files.list'
    manifest.write_text(manifest.read_text().replace('/boot/home/config/', str(home) + '/config/'))
    records = [line.split('|') for line in manifest.read_text().splitlines()]
    assert len(records) == 2
    assert all(Path(item[2]).is_relative_to(home) for item in records)
    sums = copy / 'SHA256SUMS'
    sums.write_text(''.join(digest(copy / line.split('  ', 1)[1]) + '  ' + line.split('  ', 1)[1] + '\n'
                           for line in sums.read_text().splitlines()))
    log = case / 'lifecycle.log'
    env = os.environ.copy()
    if fail_rename:
        commands = case / 'bin'
        commands.mkdir()
        wrapper = commands / 'mv'
        # Fail the second payload's commit after PS/2 has already been replaced.
        wrapper.write_text('#!/bin/bash\n'
                           f'[[ ${{@: -1}} != "{mouse}" ]] || exit 71\n'
                           f'exec "{shutil.which("mv")}" "$@"\n')
        wrapper.chmod(0o755)
        env['PATH'] = str(commands) + os.pathsep + env['PATH']
    run(['bash', 'install.sh', '--install'], copy, log, success=not fail_rename, env=env)
    if fail_rename:
        assert ps2.read_bytes() == b'previous kernel override\n'
        assert not mouse.exists()
        assert list((home / 'config/settings/x270-drivers').glob('trackpoint-touchpad.failed.*'))
        return
    assert ps2.read_bytes() == (copy / 'payload/ps2').read_bytes()
    assert mouse.read_bytes() == (copy / 'payload/mouse').read_bytes()
    if prior_mouse:
        mouse.write_bytes(mouse.read_bytes() + b'tampered')
        run(['bash', 'install.sh', '--uninstall'], copy, log, success=False)
        assert mouse.read_bytes().endswith(b'tampered')
        shutil.copyfile(copy / 'payload/mouse', mouse)
    run(['bash', 'install.sh', '--uninstall'], copy, log)
    assert ps2.read_bytes() == b'previous kernel override\n'
    assert mouse.read_bytes() == b'previous mouse override\n' if prior_mouse else not mouse.exists()


def main():
    assert sys.platform.startswith('haiku'), 'Run on the matching Haiku target.'
    directory = Path(sys.argv[1]).resolve()
    workspace = Path(tempfile.mkdtemp(prefix='installer-check-', dir=directory))
    summary = []
    def passed(message):
        summary.append('PASS: ' + message)
        print(summary[-1], flush=True)
    entries = (directory / 'SHA256SUMS').read_text().splitlines()
    bundles = {}
    for line in entries:
        expected, name = line.split('  ', 1)
        archive = directory / name
        assert digest(archive) == expected
        with zipfile.ZipFile(archive) as zipped:
            assert all(not Path(n).is_absolute() and '..' not in Path(n).parts for n in zipped.namelist())
            zipped.extractall(workspace)
        bundle = workspace / name.removesuffix('.zip')
        component = (bundle / 'COMPONENT').read_text().strip()
        bundles[component] = bundle
        run(['bash', '-n', 'install.sh'], bundle, workspace / 'verify.log')
        run(['bash', 'install.sh', '--verify'], bundle, workspace / 'verify.log')
        passed(component + ' ZIP, shell syntax and all packaged checksums')
    assert len(bundles) == 4
    bundle = bundles['trackpoint-touchpad']
    for name, deployed in [
        ('ps2', '/boot/home/config/non-packaged/add-ons/kernel/bus_managers/ps2'),
        ('mouse', '/boot/home/config/non-packaged/add-ons/input_server/devices/mouse'),
    ]:
        assert digest(bundle / 'payload' / name) == digest(Path(deployed))
    passed('TrackPoint/touchpad payload exactly matches both deployed working binaries')
    source = bundle / 'source/trackpoint-touchpad/gesture-addon'
    run(['bash', 'build.sh'], source, workspace / 'build.log')
    passed('native add-on rebuild and all 22 gesture checks')
    checkout = Path('/boot/home/haiku')
    if checkout.exists():
        patch = bundle / 'source/trackpoint-touchpad/patches/0003-touchpad-gestures.patch'
        run(['git', 'apply', '--check', str(patch)], checkout, workspace / 'patch.log')
        passed('gesture patch applies cleanly to the target source baseline (check only)')
    lifecycle(bundle, workspace)
    passed('isolated install/uninstall restores the old kernel and removes a newly added mouse override')
    lifecycle(bundle, workspace, prior_mouse=True)
    passed('isolated install/uninstall preserves a pre-existing mouse override and rejects changed installed files')
    lifecycle(bundle, workspace, fail_rename=True)
    passed('isolated second-file failure rolls back the already replaced kernel and preserves the failure receipt')
    (directory / 'package-validation.log').write_text('\n'.join(summary) + '\n')
    print('No live installation was changed. Detailed logs: ' + str(workspace))


if __name__ == '__main__':
    main()
