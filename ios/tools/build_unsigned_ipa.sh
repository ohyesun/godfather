#!/bin/bash
# Produces a real iPhoneOS Release app without signing credentials. Run on Mac.
set -euo pipefail
task_root="$(cd "$(dirname "$0")/.." && pwd)"
if [ "$(uname -s)" != "Darwin" ]; then
  echo "Unsigned device IPA build requires macOS with the full Xcode iOS SDK." >&2
  exit 2
fi
for task_tool in xcodebuild xcrun python3; do
  if ! command -v "$task_tool" >/dev/null 2>&1; then
    echo "Missing required tool: $task_tool" >&2; exit 2
  fi
done
task_sdk="$(xcrun --sdk iphoneos --show-sdk-path)"
if [ ! -d "$task_sdk" ]; then echo "Select a full Xcode with an iPhoneOS SDK." >&2; exit 2; fi
task_build_root="${IOS_UNSIGNED_BUILD_ROOT:-$task_root/build}"
mkdir -p "$task_build_root"
task_run="$(mktemp -d "$task_build_root/Zhanguo-unsigned-$(date -u +%Y%m%d-%H%M%S)-XXXXXX")"
task_archive="$task_run/Zhanguo.xcarchive"
trap 'task_status=$?; if [ "$task_status" -ne 0 ]; then echo "Build failed; preserved diagnostics: $task_run" >&2; fi' EXIT
xcodebuild -version > "$task_run/xcode-version.txt"
python3 "$task_root/tools/generate_project.py"
python3 "$task_root/tools/verify_project.py"
# Explicit CLI settings override any development team inherited from Xcode.
# No -allowProvisioningUpdates, exportArchive, certificate or Apple login is used.
xcodebuild -project "$task_root/Zhanguo.xcodeproj" -scheme Zhanguo \
  -configuration Release -sdk iphoneos -destination 'generic/platform=iOS' \
  -archivePath "$task_archive" -derivedDataPath "$task_run/DerivedData" \
  ARCHS=arm64 ONLY_ACTIVE_ARCH=NO SKIP_INSTALL=NO \
  CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO CODE_SIGN_IDENTITY= DEVELOPMENT_TEAM= \
  ALWAYS_EMBED_SWIFT_STANDARD_LIBRARIES=YES archive 2>&1 | tee "$task_run/xcodebuild.log"
python3 "$task_root/tools/package_unsigned_ipa.py" \
  --app "$task_archive/Products/Applications/Zhanguo.app" \
  --sdk-root "$task_sdk" --output "$task_run/Zhanguo-unsigned.ipa" \
  --report "$task_run/unsigned-ipa-audit.json"
printf 'Unsigned device IPA: %s\nAudit: %s\n' "$task_run/Zhanguo-unsigned.ipa" "$task_run/unsigned-ipa-audit.json"
echo 'This IPA still needs signing/provisioning and iPhone testing; it cannot be installed unsigned.'
