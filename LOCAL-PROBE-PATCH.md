# Local MuMu x86_64 probe result

Status: **DIAGNOSTIC ONLY — NOT F4 SOURCE ACCEPTANCE**

## Toolchain

- Android NDK r27d: `D:/Android/android-ndk-r27d`
- NDK CMake toolchain: `D:/Android/android-ndk-r27d/build/cmake/android.toolchain.cmake`
- CMake: Visual Studio 18 Insiders bundled, 4.3.1
- Ninja: Visual Studio 18 Insiders bundled
- Device: MuMu emulator `emulator-5554`, ABI `x86_64`

## Build

The probe is built from `harness/` against the 13-C portable library:

```text
android-ndk-r27d clang 18.0.4
target x86_64-none-linux-android24
```

The first build exposed a real compile error in the bundled adapter copy:

```text
vendor/adapter/zg116la_e4_range.c:473
error: member reference type 'le4_frame' is not a pointer;
       did you mean to use '.'?
```

`f` is a stack value in `zg116la_e4_range()`, so `&f->bridge` was corrected
to `&f.bridge` in the **bundle vendor copy only**.

- Old reviewed pin: `5c9ed17082c563b4f6ac542c27d9fc39cc72b42d633b598bc49fa938f1bf678e`
- Patched bundle copy: `f8db8601fa9ea4ee7a6d39890222192c3ceb1a5450e9b385c68d9ffaa73b13f6`

This is a one-line source fix, but it changes the frozen adapter identity.
It must not be described as the previously accepted F4 source. A new adapter
revision and independent source review are required before treating it as
accepted source.

## Probe result on MuMu

```text
zg335_probe state=1 raw_present=0 normal_count=0 overflow=0
zg335_probe bind_null=0
```

`state=1` is `ZG335116_HOST_BAD_INTERFACE` for the deliberately zeroed
configuration; the probe proves that the x86_64 static library links, loads
and can call the exported entry points on Android. It does not prove any
guest execution, composed behavior, runtime, SDK, OS or whole-game result.

- Probe binary SHA-256: `6f332dd83f2588cd04bbf6dfef817c4fcf1b65be27cb23e21cffc82c1919d56c`
- Probe size: 293 KiB

## Next required step

Create a new adapter revision containing the one-line fix, update the source
manifest pins, and re-run the independent source review. Until then this
probe is diagnostic evidence only.
