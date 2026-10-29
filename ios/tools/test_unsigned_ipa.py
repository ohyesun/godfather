"""Offline artifact fixtures, NOT an Apple build, signed app, or real IPA delivery."""
from contextlib import contextmanager
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import stat
import struct
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('unsigned_ipa', ROOT / 'package_unsigned_ipa.py')
subject = importlib.util.module_from_spec(spec)
spec.loader.exec_module(subject)


def command(kind, text, minimum=24):
    payload = text.encode() + b'\0'
    size = ((minimum + len(payload) + 7) // 8) * 8
    header = struct.pack('<III', kind, size, minimum)
    return header + b'\0' * (minimum - len(header)) + payload + b'\0' * (size - minimum - len(payload))


def macho(platform=2, file_type=2, dependency='@rpath/libswiftCore.dylib', cpu=subject.CPU_ARM64,
          minimum=0x00100400, signed=False, rpaths=None):
    commands = [struct.pack('<6I', subject.LC_BUILD_VERSION, 24, platform, minimum, 0x00120000, 0)]
    if dependency:
        commands.append(command(0xC, dependency))
    for value in rpaths if rpaths is not None else ['@executable_path/Frameworks', '/usr/lib/swift']:
        commands.append(command(subject.LC_RPATH, value, 12))
    if signed:
        commands.append(struct.pack('<4I', subject.LC_CODE_SIGNATURE, 16, 0, 0))
    joined = b''.join(commands)
    return struct.pack('<8I', 0xFEEDFACF, cpu, 0, file_type, len(commands), len(joined), 0, 0) + joined


@contextmanager
def fixture():
    with tempfile.TemporaryDirectory(prefix='zg-unsigned-fixture-') as temporary:
        root = Path(temporary)
        app, sdk = root / 'Zhanguo.app', root / 'iPhoneOS.sdk'
        app.mkdir(); (sdk / 'usr/lib/swift').mkdir(parents=True)
        # SDK stub is a fixture marker, not a redistributed Apple dylib.
        (sdk / 'usr/lib/swift/libswiftCore.tbd').write_text('fixture SDK symbol stub')
        info = {'CFBundlePackageType': 'APPL', 'CFBundleSupportedPlatforms': ['iPhoneOS'],
                'DTPlatformName': 'iphoneos', 'DTSDKName': 'iphoneos18.0',
                'LSRequiresIPhoneOS': True, 'MinimumOSVersion': '16.4',
                'CFBundleExecutable': 'Zhanguo', 'CFBundleIdentifier': 'fixture.zhanguo'}
        with (app / 'Info.plist').open('wb') as stream:
            plistlib.dump(info, stream, fmt=plistlib.FMT_BINARY)
        main = app / 'Zhanguo'; main.write_bytes(macho()); main.chmod(0o755)
        (app / 'GameAssets').mkdir(); (app / 'GameAssets/payload.bin').write_bytes(bytes(range(256)) * 17)
        if os.name == 'nt':
            # NTFS Python cannot expose POSIX executable mode on an extensionless
            # file. Emulate only that bit for synthetic fixture files. Production
            # code remains strict; the Mac shell never uses this test adapter.
            old_stat, old_lstat = Path.stat, Path.lstat
            def permissions(path, value):
                if path.name == 'Zhanguo' or path.suffix == '.dylib':
                    fields = list(value); fields[0] |= 0o111
                    return os.stat_result(fields)
                return value
            def test_stat(path, *args, **kwargs): return permissions(path, old_stat(path, *args, **kwargs))
            def test_lstat(path, *args, **kwargs): return permissions(path, old_lstat(path, *args, **kwargs))
            with patch.object(Path, 'stat', test_stat), patch.object(Path, 'lstat', test_lstat):
                yield root, app, sdk
        else:
            yield root, app, sdk


class UnsignedIPA(unittest.TestCase):
    def test_device_package_streams_real_entries_and_preserves_modes(self):
        with fixture() as (root, app, sdk):
            output, audit = root / 'fixture-unsigned.ipa', root / 'audit.json'
            record = subject.package(app, sdk, output, audit)
            self.assertFalse(record['signed']); self.assertFalse(record['installableWithoutSigning'])
            self.assertEqual(record['validation']['platform'], 'iPhoneOS')
            self.assertEqual(record['validation']['embeddedSwiftDylibs'], [])
            self.assertEqual(record['validation']['dependencies'][0]['resolved'], 'iOS-system')
            with zipfile.ZipFile(output) as archive:
                self.assertEqual(archive.read('Payload/Zhanguo.app/GameAssets/payload.bin'), (app / 'GameAssets/payload.bin').read_bytes())
                self.assertEqual(archive.getinfo('Payload/Zhanguo.app/Zhanguo').external_attr >> 16, (app / 'Zhanguo').lstat().st_mode)
                self.assertTrue(stat.S_ISDIR(archive.getinfo('Payload/Zhanguo.app/').external_attr >> 16))
                self.assertTrue(all(name.startswith('Payload/') for name in archive.namelist()))
            self.assertEqual(json.loads(audit.read_text())['sha256'], subject.sha256(output))

    def test_arm64_simulator_and_non_arm64_are_rejected(self):
        for platform, cpu in [(7, subject.CPU_ARM64), (6, subject.CPU_ARM64), (2, 0x01000007)]:
            with self.subTest(platform=platform, cpu=cpu), fixture() as (root, app, sdk):
                (app / 'Zhanguo').write_bytes(macho(platform=platform, cpu=cpu))
                with self.assertRaises(subject.InvalidApp): subject.package(app, sdk, root / 'bad.ipa')
                self.assertFalse((root / 'bad.ipa').exists())

    def test_plist_simulator_wrong_minimum_and_signing_remnants_rejected(self):
        for mutation in ['simulator', 'minimum', 'codesign', 'profile', 'signed-main']:
            with self.subTest(mutation=mutation), fixture() as (root, app, sdk):
                if mutation in {'simulator', 'minimum'}:
                    info = plistlib.loads((app / 'Info.plist').read_bytes())
                    info['CFBundleSupportedPlatforms' if mutation == 'simulator' else 'MinimumOSVersion'] = ['iPhoneSimulator'] if mutation == 'simulator' else '17.0'
                    (app / 'Info.plist').write_bytes(plistlib.dumps(info))
                elif mutation == 'codesign': (app / '_CodeSignature').mkdir()
                elif mutation == 'profile': (app / 'embedded.mobileprovision').write_bytes(b'fixture')
                else: (app / 'Zhanguo').write_bytes(macho(signed=True))
                with self.assertRaises(subject.InvalidApp): subject.validate_app(app, sdk)

    def test_missing_swift_library_fails_and_embedded_runtime_is_device_checked(self):
        with fixture() as (root, app, sdk):
            main = app / 'Zhanguo'; main.write_bytes(macho(dependency='@rpath/libswiftFixture.dylib'))
            with self.assertRaisesRegex(subject.InvalidApp, 'Missing embedded dependency'): subject.validate_app(app, sdk)
            library = app / 'Frameworks/libswiftFixture.dylib'; library.parent.mkdir()
            library.write_bytes(macho(platform=7, file_type=6, dependency='/usr/lib/swift/libswiftCore.dylib')); library.chmod(0o755)
            with self.assertRaisesRegex(subject.InvalidApp, 'platform iOS'): subject.validate_app(app, sdk)
            library.write_bytes(macho(file_type=6, dependency='/usr/lib/swift/libswiftCore.dylib'))
            record = subject.package(app, sdk, root / 'embedded.ipa')
            self.assertEqual(record['validation']['embeddedSwiftDylibs'], ['Frameworks/libswiftFixture.dylib'])
            self.assertEqual(record['validation']['validatedMachOFiles'], 2)
            library.write_bytes(macho(file_type=6, minimum=0x00110000, dependency='/usr/lib/swift/libswiftCore.dylib'))
            with self.assertRaisesRegex(subject.InvalidApp, 'newer OS'): subject.validate_app(app, sdk)

    def test_non_system_framework_dependency_must_exist_as_native_macho(self):
        with fixture() as (root, app, sdk):
            library = app / 'Frameworks/Foo.framework/Foo'; library.parent.mkdir(parents=True)
            (app / 'Zhanguo').write_bytes(macho(dependency='@rpath/Foo.framework/Foo'))
            library.write_text('not a Mach-O')
            with self.assertRaisesRegex(subject.InvalidApp, 'not validated Mach-O'): subject.validate_app(app, sdk)
            library.write_bytes(macho(file_type=6, dependency='/usr/lib/swift/libswiftCore.dylib'))
            self.assertEqual(subject.validate_app(app, sdk)[1]['validatedMachOFiles'], 2)

    def test_corrupt_load_commands_and_fat_boundaries_fail(self):
        for mutation in ['short-header', 'short-command', 'huge-command', 'fat-bounds', 'fat-overlap']:
            with self.subTest(mutation=mutation), fixture() as (root, app, sdk):
                raw = bytearray(macho())
                if mutation == 'short-header': raw = raw[:20]
                elif mutation == 'short-command': raw = raw[:-1]
                elif mutation == 'huge-command': struct.pack_into('<I', raw, 36, 0xFFFFFFF8)
                elif mutation == 'fat-bounds': raw = bytearray(struct.pack('>7I', 0xCAFEBABE, 1, subject.CPU_ARM64, 0, 4096, 4096, 12))
                else:
                    raw = bytearray(struct.pack('>2I', 0xCAFEBABE, 2) + struct.pack('>5I', subject.CPU_ARM64, 0, 4096, 4096, 12) * 2 + bytes(8192))
                (app / 'Zhanguo').write_bytes(raw)
                with self.assertRaises(subject.InvalidApp): subject.validate_app(app, sdk)

    def test_existing_output_and_report_remain_unchanged(self):
        with fixture() as (root, app, sdk):
            output = root / 'already.ipa'; output.write_bytes(b'preserve prior deliverable')
            with self.assertRaises(subject.InvalidApp): subject.package(app, sdk, output)
            self.assertEqual(output.read_bytes(), b'preserve prior deliverable')
            report = root / 'already.json'; report.write_text('preserve prior audit')
            with self.assertRaises(subject.InvalidApp): subject.package(app, sdk, root / 'unused.ipa', report)
            self.assertFalse((root / 'unused.ipa').exists())
            self.assertEqual(report.read_text(), 'preserve prior audit')

    def test_zip_metadata_encodes_symlink_without_following_it(self):
        # This assertion also runs on Windows when OS-level symlink creation is
        # unavailable. The separate integration case still tests real links.
        info = subject.zip_info('Payload/Zhanguo.app/alias', stat.S_IFLNK | 0o777, 0)
        self.assertEqual(info.create_system, 3)
        self.assertTrue(stat.S_ISLNK(info.external_attr >> 16))
        self.assertEqual(stat.S_IMODE(info.external_attr >> 16), 0o777)
        with tempfile.TemporaryDirectory(prefix='zg-link-zip-') as temporary:
            output = Path(temporary) / 'metadata.zip'
            with zipfile.ZipFile(output, 'w') as archive: archive.writestr(info, b'Frameworks/Current')
            with zipfile.ZipFile(output) as archive:
                self.assertTrue(stat.S_ISLNK(archive.getinfo(info.filename).external_attr >> 16))
                self.assertEqual(archive.read(info.filename), b'Frameworks/Current')

    def test_output_and_report_cannot_modify_input_app(self):
        with fixture() as (root, app, sdk):
            before = {path.relative_to(app).as_posix(): path.read_bytes() for path in app.rglob('*') if path.is_file()}
            for report in [app / 'new-audit.json', app / 'GameAssets/../new-audit.json']:
                with self.assertRaisesRegex(subject.InvalidApp, 'report must be outside'):
                    subject.package(app, sdk, root / 'should-not-exist.ipa', report)
                self.assertFalse((root / 'should-not-exist.ipa').exists())
            with self.assertRaisesRegex(subject.InvalidApp, 'Output must be outside'):
                subject.package(app, sdk, app / 'GameAssets/../new.ipa')
            self.assertEqual(before, {path.relative_to(app).as_posix(): path.read_bytes() for path in app.rglob('*') if path.is_file()})

    def test_relative_symlink_preserved_and_escape_rejected(self):
        with fixture() as (root, app, sdk):
            link = app / 'GameAssets/alias.bin'
            try: link.symlink_to('payload.bin')
            except OSError as error: self.skipTest('Host cannot create symlinks: ' + str(error))
            output = root / 'link.ipa'; subject.package(app, sdk, output)
            with zipfile.ZipFile(output) as archive:
                entry = archive.getinfo('Payload/Zhanguo.app/GameAssets/alias.bin')
                self.assertTrue(stat.S_ISLNK(entry.external_attr >> 16))
                self.assertEqual(archive.read(entry), b'payload.bin')
            link.unlink(); (root / 'outside').write_text('fixture')
            link.symlink_to('../../outside')
            with self.assertRaisesRegex(subject.InvalidApp, 'escapes'): subject.validate_app(app, sdk)
            link.unlink(); link.symlink_to((app / 'GameAssets/payload.bin').absolute())
            with self.assertRaisesRegex(subject.InvalidApp, 'Absolute'): subject.validate_app(app, sdk)
            link.unlink()
            parent_alias = root / 'app-alias'; parent_alias.symlink_to(app, target_is_directory=True)
            with self.assertRaisesRegex(subject.InvalidApp, 'Output must be outside'):
                subject.package(app, sdk, parent_alias / 'mutating.ipa')
            with self.assertRaisesRegex(subject.InvalidApp, 'report must be outside'):
                subject.package(app, sdk, root / 'nonmutating.ipa', parent_alias / 'mutating.json')

    def test_shell_explicitly_builds_device_release_without_accounts(self):
        shell = (ROOT / 'build_unsigned_ipa.sh').read_text()
        for token in ["-configuration Release -sdk iphoneos -destination 'generic/platform=iOS'", 'ARCHS=arm64',
                      'CODE_SIGNING_ALLOWED=NO', 'CODE_SIGNING_REQUIRED=NO', 'CODE_SIGN_IDENTITY= DEVELOPMENT_TEAM=',
                      'SKIP_INSTALL=NO', 'ALWAYS_EMBED_SWIFT_STANDARD_LIBRARIES=YES archive']:
            self.assertIn(token, shell)
        executable = '\n'.join(line for line in shell.splitlines() if not line.lstrip().startswith('#'))
        self.assertNotIn('-exportArchive', executable)
        self.assertNotIn('-allowProvisioningUpdates', executable)
        self.assertNotIn('iphonesimulator', executable)
        self.assertIn('mktemp -d', shell)


if __name__ == '__main__':
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(UnsignedIPA)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    report = {'result': 'pass' if result.wasSuccessful() else 'fail', 'testsRun': result.testsRun,
              'failures': len(result.failures), 'errors': len(result.errors), 'skipped': len(result.skipped),
              'syntheticMachOFixtures': True, 'appleXcodeBuildExecuted': False,
              'realDeviceIPAProduced': False, 'signedOrInstalled': False,
              'hostPlatform': os.name, 'windowsFixtureExecutableModeAdapter': os.name == 'nt',
              'realSymlinkIntegrationExecuted': not bool(result.skipped),
              'sourceSHA256': {name: subject.sha256(ROOT / name) for name in
                  ['build_unsigned_ipa.sh', 'package_unsigned_ipa.py', 'test_unsigned_ipa.py', 'UNSIGNED_IPA.md']},
              'scope': 'Offline parser, dependency, ZIP metadata, no-overwrite and shell contract tests only.'}
    (ROOT.parent / 'tests/review/unsigned-ipa-offline.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    raise SystemExit(0 if result.wasSuccessful() else 1)
