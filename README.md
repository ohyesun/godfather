# Codemagic portable C11 bundle

This directory is a self-contained repository payload for Codemagic.

It contains:

- `design/` — frozen CMake design, README, build preflight, public headers and `find_package` config template.
- `vendor/legacy/` — 8 reviewed legacy C translation units and headers.
- `vendor/adapter/` — 5 reviewed A1-A5 adapter C/header files.
- `vendor/generated/` — generated umbrella header, hash-pinned but not a CMake build input.
- `codemagic.yaml` — Android arm64 and iOS device arm64 workflows on `mac_mini_m2`.

No fixture wrapper, fixture macro redirection, Host runner, signing identity or
runtime oracle is included. The output is a static C library plus installed
headers, not an APK, AAR, IPA or app.

## Push to your Git repository

From this directory, replace the placeholder repository URL before running the
push commands:

```bash
git init
git add .
git commit -m "Add zg335 portable Codemagic build"
git branch -M main
git remote add origin <YOUR_GITHUB_OR_GITLAB_REPO_URL>
git push -u origin main
```

## Run on Codemagic

1. Sign in to Codemagic with the Git provider.
2. Add application and select this repository.
3. Let Codemagic scan the repository for `codemagic.yaml`.
4. Run `android-portable` and `ios-portable`.

Free-tier allowance is currently about 500 macOS M2 minutes per month, one
parallel build, 120-minute maximum build duration and 30-day artifact retention.
Check Codemagic's current pricing page before relying on the allowance.

## Artifacts

```text
out/android-arm64/lib/libzg335_portable.a
out/android-arm64/include/zg335/*.h
out/ios-device-arm64/lib/libzg335_portable.a
out/ios-device-arm64/include/zg335/*.h
```

The exact library filename and install layout follow the frozen CMake design.

## Not covered

- iOS simulator slices and XCFramework assembly
- Android x86_64 emulator ABI
- APK/AAB/IPA packaging or signing
- device/simulator smoke tests
- Host, runtime, SDK, OS, native ABI, CRT, SEH, atomicity or allocation acceptance

F4's five runtime gaps remain excluded and are not inherited by this CI payload.
