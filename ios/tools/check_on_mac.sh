#!/bin/bash
set -euo pipefail
task_root="$(cd "$(dirname "$0")/.." && pwd)"
if [ "$(uname -s)" != "Darwin" ]; then
  echo "This check requires macOS and Xcode; it does not run on Windows." >&2
  exit 2
fi
xcodebuild -version
python3 "$task_root/tools/generate_project.py"
python3 "$task_root/tools/verify_project.py"
plutil -lint "$task_root/Zhanguo/Info.plist" "$task_root/Zhanguo.xcodeproj/project.pbxproj"
if [ -n "${IOS_DESTINATION:-}" ]; then
  task_destination="$IOS_DESTINATION"
else
  task_device="$(xcrun simctl list devices available --json | python3 -c 'import json,sys; d=json.load(sys.stdin); a=[x for k,v in d["devices"].items() if "iOS" in k for x in v if x.get("isAvailable") and "iPhone" in x["name"]]; print(a[0]["udid"] if a else "")')"
  if [ -z "$task_device" ]; then
    echo "Install an iOS Simulator runtime in Xcode, or set IOS_DESTINATION." >&2
    exit 2
  fi
  task_destination="platform=iOS Simulator,id=$task_device"
fi
mkdir -p "$task_root/build"
xcodebuild -project "$task_root/Zhanguo.xcodeproj" -scheme Zhanguo -configuration Debug \
  -destination "$task_destination" -derivedDataPath "$task_root/DerivedData" \
  -resultBundlePath "$task_root/build/NativeTests-$(date +%Y%m%d-%H%M%S).xcresult" \
  CODE_SIGNING_ALLOWED=NO test
