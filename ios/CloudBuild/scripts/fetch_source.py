"""Fetch an explicitly configured private source ZIP, verify it, then extract.

IOS_SOURCE_URL and IOS_SOURCE_SHA256 must be provided as CI variables. The URL
may contain a temporary access token: it is never printed or passed to a shell.
Only the Python standard library is required (Python 3.9+).
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import tempfile
import urllib.parse
import urllib.request
import zipfile

MAX_ARCHIVE = 4 * 1024**3
MAX_DECODED = 6 * 1024**3
MAX_FILES = 10000
CHUNK = 1024 * 1024


class SourceError(ValueError):
    pass


def sha256(path):
    value = hashlib.sha256()
    with Path(path).open('rb') as source:
        for part in iter(lambda: source.read(CHUNK), b''):
            value.update(part)
    return value.hexdigest()


def check_url(value):
    try:
        parsed = urllib.parse.urlsplit(value)
        if (parsed.scheme != 'https' or not parsed.hostname or parsed.username
                or parsed.password or parsed.fragment):
            raise ValueError()
        parsed.port
    except ValueError:
        raise SourceError('IOS_SOURCE_URL must be a private HTTPS download URL, without userinfo or fragment.') from None
    return value


class HTTPSRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        check_url(newurl)
        return super().redirect_request(req, fp, code, msg, headers, newurl)


def download(url, target):
    check_url(url)
    opener = urllib.request.build_opener(HTTPSRedirect())
    try:
        request = urllib.request.Request(url, headers={'User-Agent': 'Zhanguo-Source-Fetch/1'})
        with opener.open(request, timeout=120) as response, target.open('xb') as output:
            check_url(response.geturl())
            length = response.headers.get('Content-Length')
            if length and int(length) > MAX_ARCHIVE:
                raise SourceError('Source ZIP exceeds the configured size limit.')
            total = 0
            while True:
                part = response.read(CHUNK)
                if not part:
                    break
                total += len(part)
                if total > MAX_ARCHIVE:
                    raise SourceError('Source ZIP exceeds the configured size limit.')
                output.write(part)
    except SourceError:
        raise
    except Exception:
        raise SourceError('Source download failed; check private HTTPS URL, expiry and access.') from None


def checked_members(archive):
    members, seen, total = [], set(), 0
    entries = archive.infolist()
    if len(entries) > MAX_FILES:
        raise SourceError('Too many source ZIP entries.')
    for entry in entries:
        name = entry.filename
        path = PurePosixPath(name)
        canonical = path.as_posix() + ('/' if entry.is_dir() else '')
        mode = entry.external_attr >> 16
        kind = stat.S_IFMT(mode)
        if (not name or name != canonical or '\\' in name or ':' in name
                or path.is_absolute() or '..' in path.parts or not path.parts
                or path.parts[0] != 'ios' or (len(path.parts) == 1 and not entry.is_dir())
                or kind not in (0, stat.S_IFREG, stat.S_IFDIR)
                or bool(entry.flag_bits & 1)):
            raise SourceError('Unsafe source ZIP entry.')
        key = path.as_posix().casefold()
        if key in seen:
            raise SourceError('Duplicate source ZIP entry.')
        seen.add(key)
        total += entry.file_size
        if total > MAX_DECODED:
            raise SourceError('Decoded source ZIP exceeds configured size limit.')
        members.append(entry)
    return members


def extract_verified(source_zip, expected_sha, destination):
    destination = Path(destination).absolute()
    if not re.fullmatch(r'[a-fA-F0-9]{64}', expected_sha):
        raise SourceError('IOS_SOURCE_SHA256 must be the exact SHA256 from the source bundle JSON.')
    if destination.exists() or destination.is_symlink():
        raise SourceError('Destination already exists; use a new build directory.')
    if source_zip.stat().st_size > MAX_ARCHIVE or sha256(source_zip) != expected_sha.lower():
        raise SourceError('Source ZIP SHA256/size does not match; no build code was executed.')
    destination.parent.mkdir(parents=True, exist_ok=True)
    # All temporary files live in a newly created directory, never in an existing checkout.
    with tempfile.TemporaryDirectory(prefix='.zg-source-', dir=destination.parent) as temporary:
        stage = Path(temporary) / 'stage'
        stage.mkdir()
        with zipfile.ZipFile(source_zip) as archive:
            for entry in checked_members(archive):
                output = stage.joinpath(*PurePosixPath(entry.filename).parts)
                if entry.is_dir():
                    output.mkdir(parents=True, exist_ok=True)
                    continue
                output.parent.mkdir(parents=True, exist_ok=True)
                with archive.open(entry) as input_file, output.open('xb') as output_file:
                    shutil.copyfileobj(input_file, output_file, CHUNK)
                output.chmod(0o644)
        root = stage / 'ios'
        manifest = json.loads((root / 'source-manifest.json').read_text('utf-8'))
        if (manifest.get('format') != 1 or manifest.get('baseRevision') != 'r119'
                or manifest.get('artifactKind') != 'complete-Xcode-source-with-prepared-game-assets'):
            raise SourceError('Unsupported complete source manifest.')
        files = manifest.get('files')
        actual = {p.relative_to(root).as_posix() for p in root.rglob('*')
                  if p.is_file() and p != root / 'source-manifest.json'}
        if not isinstance(files, dict) or actual != set(files):
            raise SourceError('Source manifest file list does not match archive.')
        for name, expected in files.items():
            path = root / name
            if (not isinstance(expected, list) or len(expected) != 2
                    or path.stat().st_size != expected[0] or sha256(path) != expected[1]):
                raise SourceError('Source manifest content verification failed.')
        required = ('tools/build_unsigned_ipa.sh', 'tools/check_on_mac.sh',
                    'Zhanguo.xcodeproj/project.pbxproj', 'Zhanguo/Resources/NativeConfig.json')
        if any(not (root / name).is_file() for name in required):
            raise SourceError('This source bundle lacks the complete unsigned-IPA build path; use the cloud-ready bundle.')
        if destination.exists() or destination.is_symlink():
            raise SourceError('Destination appeared during extraction; refusing replacement.')
        stage.rename(destination)
        return {'verified': True, 'files': len(actual), 'sourceSHA256': expected_sha.lower(),
                'appleCompilationExecuted': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--destination', type=Path, required=True)
    args = parser.parse_args()
    try:
        expected = os.environ.get('IOS_SOURCE_SHA256', '')
        if not re.fullmatch(r'[a-fA-F0-9]{64}', expected):
            raise SourceError('Set IOS_SOURCE_SHA256 in the ios_unsigned_source CI variable group.')
        url = os.environ.get('IOS_SOURCE_URL', '')
        check_url(url)
        destination = args.destination.absolute()
        if destination.exists() or destination.is_symlink():
            raise SourceError('Destination already exists; use a new build directory.')
        destination.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='.zg-download-', dir=destination.parent) as temporary:
            source_zip = Path(temporary) / 'source.zip'
            download(url, source_zip)
            print(json.dumps(extract_verified(source_zip, expected, destination), indent=2))
    except Exception as error:
        # Never expose URL tokens, config contents, server replies or raw library errors.
        message = str(error) if isinstance(error, SourceError) else 'Source validation failed; no build code was executed.'
        parser.exit(2, message + '\n')


if __name__ == '__main__':
    main()
