"""Package only the small cloud build repository template, never game/config assets."""
import hashlib
import json
from pathlib import Path
import zipfile

IOS = Path(__file__).resolve().parents[1]
ROOT = IOS / 'CloudBuild'
OUTPUT = IOS.parent / 'output/zhanguo-ios-r119-cloud-build-kit.zip'
EXPECTED = {'.gitignore', 'README.md', 'codemagic.yaml',
            'scripts/fetch_source.py', 'scripts/test_fetch_source.py'}


def package():
    paths = {path.relative_to(ROOT).as_posix(): path for path in ROOT.rglob('*')
             if path.is_file() and '__pycache__' not in path.relative_to(ROOT).parts}
    if set(paths) != EXPECTED or any(path.is_symlink() for path in paths.values()):
        raise ValueError('Unexpected cloud template files; do not accidentally package private game/config resources.')
    if OUTPUT.exists() or OUTPUT.with_suffix('.json').exists():
        raise FileExistsError('Preserve the existing cloud template artifact.')
    files = {name: [path.stat().st_size, hashlib.sha256(path.read_bytes()).hexdigest()]
             for name, path in sorted(paths.items())}
    manifest = {'format': 1, 'artifactKind': 'cloud-build-repository-template',
                'containsGameResources': False, 'containsNativeConfiguration': False,
                'installableIPA': False, 'files': files}
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(OUTPUT, 'x', compression=zipfile.ZIP_DEFLATED) as archive:
        for name, path in sorted(paths.items()):
            archive.write(path, name)
        archive.writestr('cloud-kit-manifest.json', json.dumps(manifest, ensure_ascii=False, indent=2))
    record = {'archive': str(OUTPUT), 'bytes': OUTPUT.stat().st_size,
              'sha256': hashlib.sha256(OUTPUT.read_bytes()).hexdigest(),
              'artifactKind': manifest['artifactKind'], 'installableIPA': False,
              'cloudBuildExecuted': False, 'containsNativeConfiguration': False,
              'fileCount': len(files) + 1}
    OUTPUT.with_suffix('.json').write_text(json.dumps(record, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(record, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    package()
