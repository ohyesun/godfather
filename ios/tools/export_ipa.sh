#!/bin/bash
set -euo pipefail
task_root="$(cd "$(dirname "$0")/.." && pwd)"
if [ "$(uname -s)" != "Darwin" ]; then echo "IPA export requires macOS and Xcode." >&2; exit 2; fi
if [ -z "${DEVELOPMENT_TEAM:-}" ]; then
  echo "Set DEVELOPMENT_TEAM to your Apple team ID; select that team in Xcode first." >&2
  exit 2
fi
task_method="${IOS_EXPORT_METHOD:-debugging}"
case "$task_method" in debugging|release-testing|app-store-connect) ;; *) echo "Unsupported IOS_EXPORT_METHOD" >&2; exit 2;; esac
python3 "$task_root/tools/verify_project.py"
mkdir -p "$task_root/build"
task_archive="$task_root/build/Zhanguo-$(date +%Y%m%d-%H%M%S).xcarchive"
task_export="${task_archive%.xcarchive}-export"
task_options="${task_archive%.xcarchive}-ExportOptions.plist"
python3 - "$task_options" "$DEVELOPMENT_TEAM" "$task_method" <<'PY'
import plistlib,sys
with open(sys.argv[1], 'wb') as f:
    plistlib.dump({'method':sys.argv[3], 'teamID':sys.argv[2], 'signingStyle':'automatic',
                  'destination':'export', 'stripSwiftSymbols':True},f)
PY
task_provisioning=()
if [ "${IOS_ALLOW_PROVISIONING_UPDATES:-0}" = "1" ]; then task_provisioning=(-allowProvisioningUpdates); fi
xcodebuild -project "$task_root/Zhanguo.xcodeproj" -scheme Zhanguo -configuration Release \
  -destination 'generic/platform=iOS' -archivePath "$task_archive" \
  DEVELOPMENT_TEAM="$DEVELOPMENT_TEAM" CODE_SIGN_STYLE=Automatic "${task_provisioning[@]}" archive
xcodebuild -exportArchive -archivePath "$task_archive" -exportPath "$task_export" \
  -exportOptionsPlist "$task_options" "${task_provisioning[@]}"
echo "Exported IPA directory: $task_export"
