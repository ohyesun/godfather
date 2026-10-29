"""Offline fixtures for cloud source verification; never contact a network."""
import hashlib
import importlib.util
import json
from pathlib import Path
import stat
import tempfile
import unittest
from unittest.mock import patch
import zipfile

SPEC = importlib.util.spec_from_file_location('fetch_source', Path(__file__).with_name('fetch_source.py'))
fetch = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(fetch)


class SourceTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='zg-cloud-fixture-')
        self.root = Path(self.temporary.name)
        self.addCleanup(self.temporary.cleanup)

    def archive(self, mutation=None, extra=None):
        files = {'tools/build_unsigned_ipa.sh': b'fixture: no execution',
                 'tools/check_on_mac.sh': b'fixture: no execution',
                 'Zhanguo.xcodeproj/project.pbxproj': b'fixture',
                 'Zhanguo/Resources/NativeConfig.json': b'{"fixture":true}'}
        manifest = {'format': 1, 'baseRevision': 'r119',
                    'artifactKind': 'complete-Xcode-source-with-prepared-game-assets',
                    'files': {name: [len(data), hashlib.sha256(data).hexdigest()]
                              for name, data in files.items()}}
        if mutation:
            mutation(manifest)
        target = self.root / ('source-' + str(len(list(self.root.glob('*.zip')))) + '.zip')
        with zipfile.ZipFile(target, 'x') as archive:
            for name, data in files.items():
                archive.writestr('ios/' + name, data)
            archive.writestr('ios/source-manifest.json', json.dumps(manifest))
            if extra:
                extra(archive)
        return target

    def test_valid_archive_verifies_and_extracts_without_executing(self):
        source = self.archive()
        result = fetch.extract_verified(source, fetch.sha256(source), self.root / 'out')
        self.assertTrue(result['verified'])
        self.assertFalse(result['appleCompilationExecuted'])
        self.assertEqual(result['files'], 4)
        self.assertEqual((self.root/'out/ios/tools/build_unsigned_ipa.sh').read_bytes(), b'fixture: no execution')

    def test_hash_rejects_before_extract(self):
        source = self.archive()
        with self.assertRaises(fetch.SourceError):
            fetch.extract_verified(source, '0' * 64, self.root / 'out')
        self.assertFalse((self.root / 'out').exists())

    def test_manifest_content_and_file_list_checked(self):
        mutations = [lambda value: value['files']['tools/check_on_mac.sh'].__setitem__(1, '0'*64),
                     lambda value: value['files'].pop('tools/check_on_mac.sh')]
        for mutation in mutations:
            source = self.archive(mutation)
            with self.assertRaises(fetch.SourceError):
                fetch.extract_verified(source, fetch.sha256(source), self.root / 'out')
            self.assertFalse((self.root / 'out').exists())

    def test_traversal_foreign_root_and_noncanonical_paths_rejected(self):
        for name in ('ios/../../escape', '/ios/escape', 'other/file', 'ios/./file', 'ios\\file', 'ios/C:/file'):
            source = self.archive(extra=lambda archive: archive.writestr(name, 'bad'))
            with self.assertRaises(fetch.SourceError):
                fetch.extract_verified(source, fetch.sha256(source), self.root / 'out')

    def test_symlinks_duplicates_and_case_collisions_rejected(self):
        def link(archive):
            entry = zipfile.ZipInfo('ios/link')
            entry.create_system = 3
            entry.external_attr = (stat.S_IFLNK | 0o777) << 16
            archive.writestr(entry, '../../escape')
        def duplicate(archive):
            archive.writestr('ios/TOOLS/CHECK_ON_MAC.SH', 'bad')
        for extra in (link, duplicate):
            source = self.archive(extra=extra)
            with self.assertRaises(fetch.SourceError):
                fetch.extract_verified(source, fetch.sha256(source), self.root / 'out')

    def test_existing_directory_is_preserved(self):
        source = self.archive()
        destination = self.root / 'out'
        destination.mkdir()
        (destination / 'keep').write_text('unchanged')
        with self.assertRaises(fetch.SourceError):
            fetch.extract_verified(source, fetch.sha256(source), destination)
        self.assertEqual((destination / 'keep').read_text(), 'unchanged')

    def test_https_validation_and_redirect_downgrade(self):
        for value in ('', 'http://example.test/a', 'https://user:pass@example.test/a',
                      'https://example.test/a#fragment', 'file:///tmp/a'):
            with self.assertRaises(fetch.SourceError):
                fetch.check_url(value)
        self.assertEqual(fetch.check_url('https://example.test/a?private=token'), 'https://example.test/a?private=token')
        with self.assertRaises(fetch.SourceError):
            fetch.HTTPSRedirect().redirect_request(None, None, 302, 'Found', {}, 'http://example.test/a')

    def test_network_error_never_exposes_private_url(self):
        secret = 'https://example.test/a?private=never-print-me'
        with patch.object(fetch.urllib.request, 'build_opener', side_effect=None) as build:
            build.return_value.open.side_effect = RuntimeError(secret)
            with self.assertRaises(fetch.SourceError) as error:
                fetch.download(secret, self.root / 'download.zip')
        self.assertNotIn('never-print-me', str(error.exception))

    def test_archive_limits_fail_before_extract(self):
        source = self.archive()
        with patch.object(fetch, 'MAX_FILES', 1):
            with self.assertRaises(fetch.SourceError):
                fetch.extract_verified(source, fetch.sha256(source), self.root / 'out')
        with patch.object(fetch, 'MAX_DECODED', 1):
            with self.assertRaises(fetch.SourceError):
                fetch.extract_verified(source, fetch.sha256(source), self.root / 'out')


if __name__ == '__main__':
    unittest.main()
