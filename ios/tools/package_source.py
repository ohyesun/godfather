"""Package the complete prepared Xcode source/resources; this is not an IPA."""
from pathlib import Path
import argparse
import hashlib
import json
import zipfile

IOS = Path(__file__).resolve().parents[1]
OUTPUT = IOS.parent/'output/zhanguo-ios-r119-source.zip'
EXCLUDED_PARTS = {'__pycache__', 'DerivedData', 'build', 'xcuserdata', '.DS_Store'}


def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for part in iter(lambda: stream.read(1024 * 1024), b''): value.update(part)
    return value.hexdigest()


def package(output=OUTPUT):
    output = Path(output).resolve()
    if output.exists(): raise FileExistsError('Preserve previously delivered source archive: '+str(output))
    manifest_path = IOS/'source-manifest.json'
    paths=[]
    for path in sorted(IOS.rglob('*')):
        relative=path.relative_to(IOS)
        if any(part in EXCLUDED_PARTS for part in relative.parts) or path == manifest_path: continue
        if path.is_symlink(): raise ValueError('Do not package a symbolic link: '+str(relative))
        if path.is_file(): paths.append(path)
    files={path.relative_to(IOS).as_posix():[path.stat().st_size,digest(path)] for path in paths}
    manifest={'format':1,'artifactKind':'complete-Xcode-source-with-prepared-game-assets','baseRevision':'r119',
              'appleSDKBuildExecuted':False,'installableIPA':False,'files':files}
    manifest_path.write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    paths.append(manifest_path)
    output.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(output,'x',allowZip64=True) as archive:
        for path in paths:
            method=zipfile.ZIP_STORED if path.stat().st_size>100*1024*1024 else zipfile.ZIP_DEFLATED
            archive.write(path,'ios/'+path.relative_to(IOS).as_posix(),compress_type=method,compresslevel=6)
    record={'archive':str(output),'sha256':digest(output),'bytes':output.stat().st_size,
            'fileCount':len(paths),'baseRevision':'r119','installableIPA':False,
            'appleSDKBuildExecuted':False,'gameAccountOrLiveServerTested':False}
    output.with_suffix('.json').write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(record,ensure_ascii=False,indent=2))


if __name__=='__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=OUTPUT,
                        help='New source ZIP path. Existing ZIPs are never overwritten.')
    package(parser.parse_args().output)
