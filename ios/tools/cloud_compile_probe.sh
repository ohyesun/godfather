#!/bin/bash
# Compile the app and the test bundle without running anything.
# First try a generic Simulator destination; if Xcode rejects it, resolve a
# concrete available device and retry. Both attempts leave logs in build/logs/.
set -uo pipefail
task_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$task_root" || exit 2

task_common=(
  -project Zhanguo.xcodeproj -scheme Zhanguo -configuration Debug
  -derivedDataPath build/DerivedData
  CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO
  build-for-testing
)

if bash tools/cloud_step.sh compile xcodebuild "${task_common[@]}" \
     -destination 'generic/platform=iOS Simulator'; then
  exit 0
else
  task_status=$?
fi

# 127 means xcodebuild itself is absent; retrying elsewhere would only hide that.
if [ "$task_status" -eq 127 ]; then
  echo "xcodebuild is not available on this machine" >&2
  exit 127
fi

echo "generic simulator destination rejected; resolving a concrete device" >&2
task_udid="$(xcrun simctl list devices available --json 2>/dev/null | python3 -c '
import json, sys
try:
    data = json.load(sys.stdin)
except Exception:
    sys.exit(0)
for runtime, devices in data.get("devices", {}).items():
    if "iOS" not in runtime:
        continue
    for device in devices:
        if device.get("isAvailable") and "iPhone" in device.get("name", ""):
            print(device["udid"])
            sys.exit(0)
')"

if [ -z "$task_udid" ]; then
  echo "no available iPhone simulator found; cannot compile the test bundle" >&2
  exit 2
fi
echo "using simulator $task_udid" >&2
bash tools/cloud_step.sh compile-sim xcodebuild "${task_common[@]}" \
  -destination "platform=iOS Simulator,id=$task_udid"
