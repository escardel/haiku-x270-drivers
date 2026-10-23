#!/bin/bash
# SPDX-License-Identifier: MIT
# Revision-specific X270 component installer. Invoke with bash.
set -euo pipefail
cd "$(dirname "$0")"
component=$(cat COMPONENT)
case "$component" in trackpoint-touchpad|webcam|microphone|bluetooth) ;; *) exit 2;; esac
action=${1:---help}
case "$action" in --install|--uninstall|--verify) ;; *)
    echo "X270 $component installer for Haiku hrev59866+92 x86_64"
    echo 'Usage: bash install.sh --verify | --install | --uninstall'
    echo 'Installation changes system components. Save work first; reboot afterward.'
    exit 0;; esac
[[ $(uname -s) == Haiku && $(uname -m) == x86_64 ]] || {
    echo 'Run this installer on Haiku x86_64.'; exit 1; }
[[ $action == --uninstall || $(uname -v) == *hrev59866+92* ]] || {
    echo 'This build requires Haiku hrev59866+92 x86_64. Rebuild for other revisions.'; exit 1; }
[[ $HOME == /boot/home ]] || { echo 'This X270 build expects HOME=/boot/home.'; exit 1; }
sha256sum -c SHA256SUMS
[[ $action == --verify ]] && exit 0
if [[ $component == webcam && $action == --install && ! -e /boot/system/lib/libturbojpeg.so.0 ]]; then
    echo 'Missing camera dependency. Run: pkgman install libjpeg_turbo'
    exit 1
fi
state="$HOME/config/settings/x270-drivers/$component"
data="$HOME/config/non-packaged/data/x270-drivers/$component"
boot="$HOME/config/settings/boot/UserBootscript"
media_stopped=0
stop_media() {
    launch_roster stop x-vnd.haiku-media_server
    media_stopped=1
    sleep 1
    local pid
    pid=$(ps | awk '$1 ~ /(^|\/)media_addon_server$/ {print $2; exit}')
    [[ -z $pid ]] || hey -o "$pid" quit
    sleep 2
    [[ -z $(ps | awk '$1 ~ /(^|\/)media_addon_server$/ {print $2; exit}') ]] || {
        echo 'Media add-on server did not quit; no installation performed.'; exit 1; }
    media_stopped=1
}
resume_media() {
    if [[ $media_stopped == 1 ]]; then
        launch_roster start x-vnd.haiku-media_server || true
        media_stopped=0
    fi
}
stop_bluetooth() {
    local pid
    pid=$(ps | awk '$1 ~ /(^|\/)bluetooth_server(-X270)?$/ {print $2; exit}')
    [[ -z $pid ]] || hey -o "$pid" quit
    sleep 1
    [[ -z $(ps | awk '$1 ~ /(^|\/)bluetooth_server(-X270)?$/ {print $2; exit}') ]] || {
        echo 'Bluetooth server did not quit; no installation performed.'; exit 1; }
}
safe_path() {
    [[ $1 != *'/../'* && $1 != *'|'* ]] || return 1
    case "$1" in /boot/home/config/non-packaged/*|/boot/system/non-packaged/lib/*|/boot/home/config/settings/Media/*) return 0;; *) return 1;; esac
}
restore_files() {
    local target existed saved
    while IFS='|' read -r target existed saved; do
        safe_path "$target" || { echo 'Invalid saved installation path'; return 1; }
        if [[ $existed == 1 ]]; then
            mkdir -p "$(dirname "$target")"
            cp -Pp "$state/backups/$saved" "$target.x270-restore"
            mv -f "$target.x270-restore" "$target"
        else
            rm -f "$target"
        fi
    done < "$state/backups.list"
}
remove_boot_entry() {
    [[ -f $boot ]] || return 0
    awk '$0 != "# X270 managed Bluetooth startup" && index($0,"/x270-drivers/bluetooth/start-bluetooth.sh") == 0 {print}' "$boot" > "$boot.x270-new"
    mv -f "$boot.x270-new" "$boot"
}
if [[ $action == --uninstall ]]; then
    [[ -f $state/complete ]] || { echo 'No complete installation receipt found.'; exit 1; }
    # Preserve later user or driver changes instead of silently overwriting them.
    sha256sum -c "$state/installed.sha256" || {
        echo 'Installed files changed. Restore or review them before uninstalling.'; exit 1; }
    while IFS='|' read -r kind source target mode; do
        if [[ $kind == link && $(readlink "$target") != "$source" ]]; then
            echo "Installed link changed: $target"; exit 1
        fi
    done < files.list
    [[ $component != microphone ]] || stop_media
    if [[ $component == bluetooth ]]; then stop_bluetooth; remove_boot_entry; fi
    trap resume_media EXIT
    restore_files
    mv "$state/complete" "$state/uninstalled"
    resume_media
    mv "$state" "$state.uninstalled.$(date +%Y%m%d-%H%M%S)"
    echo "$component removed; prior files restored. Reboot the X270."
    exit 0
fi
[[ ! -e $state/backups.list ]] || {
    echo "A receipt already exists at $state. Uninstall first; keep backups for recovery."; exit 1; }
mkdir -p "$state/backups"
: > "$state/backups.list"
backup() {
    local target=$1 index
    safe_path "$target" || { echo "Invalid destination: $target"; exit 1; }
    index=$(wc -l < "$state/backups.list")
    if [[ -e $target || -L $target ]]; then
        cp -Pp "$target" "$state/backups/$index"
        printf '%s|1|%s\n' "$target" "$index" >> "$state/backups.list"
    else
        printf '%s|0|%s\n' "$target" "$index" >> "$state/backups.list"
    fi
}
committed=0
rollback_on_error() {
    local status=$?
    if [[ $status != 0 && $committed == 0 ]]; then
        echo 'Installation failed; restoring backed-up files.'
        if restore_files; then
            mv "$state" "$state.failed.$(date +%Y%m%d-%H%M%S)"
        else
            echo "Rollback needs review; retain $state for recovery."
        fi
        [[ $component != bluetooth ]] || remove_boot_entry
    fi
    resume_media
}
trap rollback_on_error EXIT
while IFS='|' read -r kind source target mode; do backup "$target"; done < files.list
if [[ $component == microphone ]]; then
    backup "$HOME/config/settings/Media/MDefaultManager"
    backup "$HOME/config/settings/Media/multi_audio_settings"
    chmod 755 ./payload/media_output_setup
    ./payload/media_output_setup
    stop_media
fi
[[ $component != bluetooth ]] || stop_bluetooth
while IFS='|' read -r kind source target mode; do
    mkdir -p "$(dirname "$target")"
    if [[ $kind == link ]]; then
        ln -s "$source" "$target.x270-new"
    else
        cp -p "payload/$source" "$target.x270-new"
        chmod "$mode" "$target.x270-new"
    fi
    mv -f "$target.x270-new" "$target"
done < files.list
if [[ $component == microphone ]]; then
    "$data/audio_settings" "$HOME/config/settings/Media/multi_audio_settings"
fi
if [[ $component == bluetooth ]]; then
    mkdir -p "$(dirname "$boot")"
    [[ -f $boot ]] || printf '#!/bin/bash\n' > "$boot"
    cp -p "$boot" "$state/UserBootscript.before"
    printf '\n# X270 managed Bluetooth startup\n/bin/bash "%s/start-bluetooth.sh" > "%s/boot.log" 2>&1 &\n' "$data" "$data" >> "$boot"
fi
: > "$state/installed.sha256"
while IFS='|' read -r kind source target mode; do
    [[ $kind != file ]] || sha256sum "$target" >> "$state/installed.sha256"
done < files.list
# Settings backups are restored on rollback/uninstall. They are not hashed:
# normal mixer changes must not prevent uninstalling the driver binaries.
touch "$state/complete"
committed=1
resume_media
echo "$component installed. Receipt and backups: $state"
echo 'Reboot the X270 to activate all installed components.'
