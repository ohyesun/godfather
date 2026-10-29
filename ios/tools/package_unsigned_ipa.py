"""Validate an iPhoneOS app and stream it to an unsigned IPA, preserving Unix metadata.

The build shell supplies a Release archive made by the real iPhoneOS SDK. This
module can also test constructed fixtures; validation alone is not an Xcode run.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import plistlib
import stat
import struct
import time
import zipfile

CPU_ARM64 = 0x0100000C
LC_BUILD_VERSION, LC_VERSION_MIN_IPHONEOS = 0x32, 0x25
LC_RPATH, LC_CODE_SIGNATURE = 0x8000001C, 0x1D
DYLIB_COMMANDS = {0xC, 0x80000018, 0x8000001F, 0x20, 0x80000023}
MACH_MAGICS = {b'\xcf\xfa\xed\xfe', b'\xfe\xed\xfa\xcf', b'\xce\xfa\xed\xfe', b'\xfe\xed\xfa\xce', b'\xca\xfe\xba\xbe',
               b'\xbe\xba\xfe\xca', b'\xca\xfe\xba\xbf', b'\xbf\xba\xfe\xca'}


class InvalidApp(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise InvalidApp(message)


def sha256(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for part in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(part)
    return value.hexdigest()


def version(value):
    require(isinstance(value, str), 'Missing platform deployment version')
    try:
        parts = tuple(int(part) for part in value.split('.'))
    except ValueError as error:
        raise InvalidApp('Invalid deployment version') from error
    require(1 <= len(parts) <= 3 and all(0 <= x <= 65535 for x in parts), 'Invalid deployment version')
    return (parts + (0, 0))[:3]


def packed_version(value):
    return (value >> 16, (value >> 8) & 255, value & 255)


def read_macho(path):
    """Read bounded Mach-O load commands; reject non-device slices even in FAT."""
    length = path.stat().st_size
    with path.open('rb') as stream:
        magic = stream.read(4)
        if magic in {b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca', b'\xca\xfe\xba\xbf', b'\xbf\xba\xfe\xca'}:
            endian = '>' if magic[:1] == b'\xca' else '<'
            fat64 = magic in {b'\xca\xfe\xba\xbf', b'\xbf\xba\xfe\xca'}
            raw = stream.read(4)
            require(len(raw) == 4, 'Truncated FAT header: ' + path.name)
            count = struct.unpack(endian + 'I', raw)[0]
            require(1 <= count <= 16, 'Invalid FAT slice count: ' + path.name)
            width = 32 if fat64 else 20
            table = stream.read(count * width)
            require(len(table) == count * width, 'Truncated FAT table: ' + path.name)
            slices = []
            for index in range(count):
                row = table[index * width:(index + 1) * width]
                cpu, subtype, offset, size, align = struct.unpack(endian + ('IIQQI' if fat64 else 'IIIII'), row[:28 if fat64 else 20])
                require(cpu == CPU_ARM64, 'Non-arm64 FAT slice: ' + path.name)
                require(align <= 30 and offset % (1 << align) == 0, 'Invalid FAT alignment: ' + path.name)
                require(offset >= 8 + count * width and size >= 32 and offset + size <= length, 'Invalid FAT bounds: ' + path.name)
                require(all(offset + size <= old[0] or offset >= old[0] + old[1] for old in slices), 'Overlapping FAT slices: ' + path.name)
                slices.append((offset, size, subtype))
        else:
            slices = [(0, length, None)]
        result = []
        for offset, size, outer_subtype in slices:
            stream.seek(offset)
            header = stream.read(32)
            require(len(header) == 32 and header[:4] in {b'\xcf\xfa\xed\xfe', b'\xfe\xed\xfa\xcf'}, 'Expected 64-bit Mach-O: ' + path.name)
            endian = '<' if header[:4] == b'\xcf\xfa\xed\xfe' else '>'
            _, cpu, subtype, file_type, ncmds, command_bytes, flags, _ = struct.unpack(endian + '8I', header)
            require(cpu == CPU_ARM64, 'Expected arm64 CPU: ' + path.name)
            require(outer_subtype is None or outer_subtype == subtype, 'FAT architecture mismatch: ' + path.name)
            require(0 < ncmds <= 4096 and ncmds * 8 <= command_bytes <= 4 * 1024 * 1024 and 32 + command_bytes <= size, 'Invalid load-command bounds: ' + path.name)
            commands = stream.read(command_bytes)
            require(len(commands) == command_bytes, 'Truncated load commands: ' + path.name)
            cursor, builds, legacy, loads, rpaths, signature = 0, [], [], [], [], False
            for _ in range(ncmds):
                require(cursor + 8 <= len(commands), 'Truncated command: ' + path.name)
                kind, width = struct.unpack_from(endian + 'II', commands, cursor)
                require(width >= 8 and width % 8 == 0 and cursor + width <= len(commands), 'Invalid command size: ' + path.name)
                block = commands[cursor:cursor + width]
                if kind == LC_BUILD_VERSION:
                    require(width >= 24, 'Truncated build version: ' + path.name)
                    platform, minimum, sdk, tools = struct.unpack_from(endian + '4I', block, 8)
                    require(24 + tools * 8 <= width, 'Truncated build tools: ' + path.name)
                    builds.append((platform, packed_version(minimum), packed_version(sdk)))
                elif kind == LC_VERSION_MIN_IPHONEOS:
                    require(width >= 16, 'Truncated legacy deployment: ' + path.name)
                    legacy.append(struct.unpack_from(endian + 'II', block, 8))
                elif kind in DYLIB_COMMANDS or kind == LC_RPATH:
                    minimum = 12 if kind == LC_RPATH else 24
                    require(width >= minimum, 'Truncated dylib/rpath command: ' + path.name)
                    start = struct.unpack_from(endian + 'I', block, 8)[0]
                    require(minimum <= start < width, 'Invalid dylib/rpath string: ' + path.name)
                    raw = block[start:]
                    require(b'\0' in raw, 'Unterminated dylib/rpath: ' + path.name)
                    try:
                        text = raw.split(b'\0', 1)[0].decode('utf-8')
                    except UnicodeDecodeError as error:
                        raise InvalidApp('Invalid dependency encoding: ' + path.name) from error
                    require(text and '\\' not in text, 'Invalid dependency path: ' + path.name)
                    (rpaths if kind == LC_RPATH else loads).append(text)
                elif kind == LC_CODE_SIGNATURE:
                    signature = True
                cursor += width
            require(cursor == command_bytes, 'Trailing load-command bytes: ' + path.name)
            # Modern Xcode/iOS16.4 products include an unambiguous build platform.
            # CPU alone and legacy VERSION_MIN are insufficient to reject arm64 simulator.
            require(len(builds) == 1 and builds[0][0] == 2, 'Expected LC_BUILD_VERSION platform iOS(2), not simulator/Catalyst: ' + path.name)
            require(not legacy, 'Ambiguous legacy platform commands: ' + path.name)
            result.append({'cpu': 'arm64', 'subtype': subtype, 'fileType': file_type,
                           'platform': 2, 'minimumOS': builds[0][1], 'sdk': builds[0][2],
                           'loads': loads, 'rpaths': rpaths, 'codeSignature': signature})
        return result


def app_entries(app):
    """lstat-based traversal does not follow directory symlinks."""
    entries = [app]
    def walk(directory):
        for child in sorted(directory.iterdir(), key=lambda value: value.name):
            entries.append(child)
            info = child.lstat()
            if stat.S_ISDIR(info.st_mode):
                walk(child)
    walk(app)
    folded = set()
    for path in entries:
        relative = path.relative_to(app.parent).as_posix()
        require(relative.casefold() not in folded, 'Case-colliding app entries: ' + relative)
        folded.add(relative.casefold())
        require('\\' not in relative, 'Non-portable app path: ' + relative)
        mode = path.lstat().st_mode
        require(stat.S_ISDIR(mode) or stat.S_ISREG(mode) or stat.S_ISLNK(mode), 'Unsupported app entry: ' + relative)
        if stat.S_ISLNK(mode):
            target = os.readlink(path)
            require(target and not os.path.isabs(target) and '\\' not in target, 'Absolute/invalid symlink: ' + relative)
            try:
                resolved = path.resolve(strict=True)
            except (OSError, RuntimeError) as error:
                raise InvalidApp('Dangling/cyclic symlink: ' + relative) from error
            require(resolved.is_relative_to(app), 'Symlink escapes app: ' + relative)
    return entries


def validate_app(app, sdk_root):
    require(app.name == 'Zhanguo.app' and app.is_dir() and not app.is_symlink(), 'Expected physical Zhanguo.app directory')
    app = app.resolve()
    entries = app_entries(app)
    require(not any(path.name in {'embedded.mobileprovision', '_CodeSignature'} for path in entries), 'Signing/provisioning remnants in unsigned app')
    with (app / 'Info.plist').open('rb') as stream:
        info = plistlib.load(stream)
    require(info.get('CFBundlePackageType') == 'APPL', 'App package type is not APPL')
    require(info.get('CFBundleSupportedPlatforms') == ['iPhoneOS'], 'Info.plist is not iPhoneOS')
    require(info.get('DTPlatformName') == 'iphoneos' and str(info.get('DTSDKName', '')).startswith('iphoneos'), 'SDK metadata is not iPhoneOS')
    require(info.get('LSRequiresIPhoneOS') is True, 'LSRequiresIPhoneOS is not true')
    minimum = version(info.get('MinimumOSVersion'))
    require(minimum >= (16, 4, 0), 'Deployment target below supported iOS16.4')
    executable = info.get('CFBundleExecutable')
    require(isinstance(executable, str) and executable == 'Zhanguo', 'Unexpected CFBundleExecutable')
    require(isinstance(info.get('CFBundleIdentifier'), str) and '$(' not in info['CFBundleIdentifier'] and info['CFBundleIdentifier'], 'Unresolved/empty bundle identifier')
    main = app / executable
    require(main.is_file() and not main.is_symlink() and main.stat().st_mode & 0o111, 'Main executable absent or not executable')
    main_slices = read_macho(main)
    require(all(row['fileType'] == 2 and not row['codeSignature'] for row in main_slices), 'Main executable must be unsigned MH_EXECUTE')
    require(all(row['minimumOS'] == minimum for row in main_slices), 'Mach-O and plist deployment targets differ')
    binaries = {main: main_slices}
    for path in entries:
        if path == main or not stat.S_ISREG(path.lstat().st_mode):
            continue
        with path.open('rb') as stream:
            magic = stream.read(4)
        if magic in MACH_MAGICS:
            rows = read_macho(path)
            require(all(row['fileType'] in {6, 8} for row in rows), 'Unexpected nested executable: ' + str(path.relative_to(app)))
            require(all(row['minimumOS'] <= minimum for row in rows), 'Embedded code requires newer OS than app: ' + str(path.relative_to(app)))
            binaries[path] = rows
    resolutions, swift_count = [], 0
    def local_path(text, loader):
        if text == '@loader_path' or text.startswith('@loader_path/'):
            return loader.parent / text[len('@loader_path'):].lstrip('/')
        if text == '@executable_path' or text.startswith('@executable_path/'):
            return main.parent / text[len('@executable_path'):].lstrip('/')
        return None
    def sdk_swift(name):
        return any((sdk_root / 'usr/lib/swift' / (name[:-6] + suffix)).is_file() for suffix in ['.tbd', '.dylib'])
    for binary, rows in binaries.items():
        for row in rows:
            for dependency in row['loads']:
                is_swift = PurePosixPath(dependency).name.startswith('libswift')
                swift_count += int(is_swift)
                candidates = []
                if dependency.startswith('@rpath/'):
                    suffix = dependency[len('@rpath/'):]
                    for runpath in row['rpaths'] + main_slices[0]['rpaths']:
                        base = local_path(runpath, binary)
                        if base is not None:
                            candidates.append(base / suffix)
                        elif runpath == '/usr/lib/swift' and is_swift and '/' not in suffix and sdk_swift(suffix):
                            candidates.append('/usr/lib/swift/' + suffix)
                else:
                    candidate = local_path(dependency, binary)
                    if candidate is not None:
                        candidates.append(candidate)
                    elif dependency.startswith(('/System/Library/', '/usr/lib/')):
                        require(not is_swift or sdk_swift(PurePosixPath(dependency).name), 'Swift runtime absent from SDK and app: ' + dependency)
                        candidates.append(dependency)
                resolved = None
                for candidate in candidates:
                    if isinstance(candidate, str):
                        resolved = 'iOS-system'
                        break
                    require(candidate.resolve().is_relative_to(app), 'Dependency escapes app: ' + dependency)
                    if candidate.is_file():
                        require(candidate.resolve() in binaries, 'Embedded dependency is not validated Mach-O: ' + dependency)
                        resolved = candidate.resolve().relative_to(app).as_posix()
                        break
                require(resolved is not None, 'Missing embedded dependency/runtime: ' + dependency)
                resolutions.append({'binary': binary.relative_to(app).as_posix(), 'dependency': dependency, 'resolved': resolved})
    require(swift_count > 0, 'Expected actual Swift runtime dependencies in this Swift app')
    return entries, {'bundleIdentifier': info['CFBundleIdentifier'], 'minimumOS': info['MinimumOSVersion'],
        'platform': 'iPhoneOS', 'architecture': 'arm64', 'mainExecutableSHA256': sha256(main),
        'validatedMachOFiles': len(binaries), 'swiftDependencyCount': swift_count,
        'embeddedSwiftDylibs': sorted(path.relative_to(app).as_posix() for path in binaries if path.name.startswith('libswift')),
        'dependencies': resolutions, 'mainExecutableSigned': False, 'embeddedProvisioningProfile': False}


def zip_info(name, mode, mtime):
    stamp = time.localtime(mtime)[:6]
    if stamp[0] < 1980:
        stamp = (1980, 1, 1, 0, 0, 0)
    elif stamp[0] > 2107:
        stamp = (2107, 12, 31, 23, 59, 58)
    info = zipfile.ZipInfo(name, date_time=stamp)
    info.create_system = 3
    info.external_attr = mode << 16
    if stat.S_ISDIR(mode):
        info.external_attr |= 0x10
    info.compress_type = zipfile.ZIP_DEFLATED
    return info


def package(app, sdk_root, output, report_path=None):
    app, sdk_root, output = Path(app).absolute(), Path(sdk_root).absolute(), Path(output).absolute()
    require(sdk_root.is_dir(), 'A real iPhoneOS SDK root is required')
    require(not output.exists() and not output.is_symlink(), 'Refusing to overwrite existing IPA')
    app_real = app.resolve()
    output_real = output.resolve(strict=False)
    require(not output_real.is_relative_to(app_real), 'Output must be outside app bundle')
    if report_path is not None:
        report_path = Path(report_path).absolute()
        report_real = report_path.resolve(strict=False)
        require(not report_path.exists() and not report_path.is_symlink() and report_real != output_real, 'Refusing to overwrite audit/output')
        require(not report_real.is_relative_to(app_real), 'Audit report must be outside app bundle')
    entries, audit = validate_app(app, sdk_root)
    app = app.resolve()
    # Validation is complete before creating an output. Never rewrite an app.
    output.parent.mkdir(parents=True, exist_ok=True)
    created = False
    try:
        with output.open('xb') as raw:
            created = True
            with zipfile.ZipFile(raw, 'w', allowZip64=True, compresslevel=6) as archive:
                archive.writestr(zip_info('Payload/', stat.S_IFDIR | 0o755, app.stat().st_mtime), b'')
                for path in entries:
                    status = path.lstat()
                    name = 'Payload/' + path.relative_to(app.parent).as_posix()
                    if stat.S_ISDIR(status.st_mode):
                        archive.writestr(zip_info(name + '/', status.st_mode, status.st_mtime), b'')
                    elif stat.S_ISLNK(status.st_mode):
                        archive.writestr(zip_info(name, status.st_mode, status.st_mtime), os.fsencode(os.readlink(path)))
                    else:
                        info = zip_info(name, status.st_mode, status.st_mtime)
                        # Already-compressed multi-GB assets need no extra RAM or CPU.
                        if status.st_size >= 100 * 1024 * 1024:
                            info.compress_type = zipfile.ZIP_STORED
                        with path.open('rb') as source, archive.open(info, 'w', force_zip64=status.st_size >= zipfile.ZIP64_LIMIT) as target:
                            for part in iter(lambda: source.read(1024 * 1024), b''):
                                target.write(part)
        # Reopen the final archive and stream-check every entry's CRC. Metadata
        # and names must match the lstat inventory; symlinks remain links.
        with zipfile.ZipFile(output) as archive:
            names = archive.namelist()
            require(len(names) == len(set(names)) == len(entries) + 1, 'Duplicate/missing packaged entry')
            require(archive.testzip() is None, 'Packaged entry CRC failed')
            for path in entries:
                status = path.lstat()
                name = 'Payload/' + path.relative_to(app.parent).as_posix() + ('/' if stat.S_ISDIR(status.st_mode) else '')
                entry = archive.getinfo(name)
                require(entry.create_system == 3 and entry.external_attr >> 16 == status.st_mode, 'Lost Unix mode: ' + name)
                if stat.S_ISLNK(status.st_mode):
                    require(archive.read(entry) == os.fsencode(os.readlink(path)), 'Lost symlink target: ' + name)
        record = {'format': 1, 'artifactKind': 'unsigned-iPhoneOS-device-IPA', 'signed': False,
            'installableWithoutSigning': False, 'xcodeExecutionClaim': 'The shell creates the archive; this validator only checks supplied app bytes.',
            'ipa': str(output), 'bytes': output.stat().st_size, 'sha256': sha256(output),
            'zipEntryCount': len(entries) + 1, 'permissionsPreserved': True, 'symlinksPreserved': True,
            'allEntryCRCsChecked': True, 'validation': audit,
            'remaining': 'Sign with an appropriate provisioning profile/certificate, then perform iPhone runtime acceptance.'}
        if report_path is not None:
            report_path.parent.mkdir(parents=True, exist_ok=True)
            with report_path.open('x', encoding='utf-8') as stream:
                json.dump(record, stream, ensure_ascii=False, indent=2)
        return record
    except BaseException:
        if created:
            output.unlink(missing_ok=True)
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', type=Path, required=True)
    parser.add_argument('--sdk-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(package(args.app, args.sdk_root, args.output, args.report), ensure_ascii=False, indent=2))
    except (InvalidApp, OSError, plistlib.InvalidFileException, zipfile.BadZipFile) as error:
        parser.exit(1, 'Unsigned IPA validation/packaging failed: ' + str(error) + '\n')


if __name__ == '__main__':
    main()
