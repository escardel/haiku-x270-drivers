#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
action=${1:---help}
case "$action" in --verify|--install|--uninstall) ;; *)
 echo 'Usage: bash install.sh --verify | --install | --uninstall'
 echo 'Exit F7 fullscreen windows before installing or removing this filter.'
 exit 0;; esac
[[ $(uname -s) == Haiku && $(uname -m) == x86_64 ]] || { echo 'Requires Haiku x86_64.'; exit 1; }
addons=$(finddir B_USER_NONPACKAGED_ADDONS_DIRECTORY)
settings=$(finddir B_USER_SETTINGS_DIRECTORY)
target="$addons/input_server/filters/fn_shortcuts"
state="$settings/x270-drivers/keyboard"
if [[ $action == --uninstall ]]; then
 [[ -f "$state/installed.sha256" ]] || { echo 'No installation recorded.'; exit 1; }
 if [[ -e "$target" ]]; then
  [[ $(sha256sum "$target" | awk '{print $1}') == $(cat "$state/installed.sha256") ]] || {
   echo 'Installed filter changed; leaving it untouched.'; exit 1; }
  rm "$target"
  sleep 2
 fi
 if [[ -f "$state/original" ]]; then cp "$state/original" "$target"; fi
 rm -rf "$state"
 echo 'Removed keyboard package; restored previous filter if present.'
 exit 0
fi
sha256sum -c SHA256SUMS
[[ $action == --verify ]] && exit 0
[[ $(uname -v) == *hrev59866+92* ]] || { echo 'Rebuild this filter for your Haiku revision.'; exit 1; }
[[ ! -e "$state" ]] || { echo 'Uninstall the recorded keyboard package first.'; exit 1; }
mkdir -p "$state" "$(dirname "$target")"
if [[ -e "$target" ]]; then cp "$target" "$state/original"; fi
# Prepare the new binary outside the watched filter directory.
cp payload/fn_shortcuts "$state/new"
sha256sum payload/fn_shortcuts | awk '{print $1}' > "$state/installed.sha256"
rm -f "$target"
sleep 2
cp "$state/new" "$target"
rm "$state/new"
echo 'Keyboard filter installed; input_server loads it automatically.'
