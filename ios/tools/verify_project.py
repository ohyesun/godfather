"""Validate the portable iOS source bundle without claiming an Apple build."""
from pathlib import Path
import argparse
import base64
import hashlib
import importlib.util
import json
import plistlib
import xml.etree.ElementTree as ET

IOS = Path(__file__).resolve().parents[1]
BASE_SHA = '62a968e2d80bdaf95b6ddd80a038de3dd29622aa2e1f55cddde5f8e887800c44'


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''): digest.update(chunk)
    return digest.hexdigest()


def verify():
    root = IOS/'Zhanguo/Resources'
    asset_root = root/'GameAssets'
    manifest_path = asset_root/'assets-manifest.json'
    manifest = json.loads(manifest_path.read_text('utf-8'))
    marker = json.loads((asset_root/'.zg-assets-owned.json').read_text('utf-8'))
    assert manifest['source']['apkSHA256'] == BASE_SHA
    assert marker['manifestSHA256'] == sha(manifest_path)
    # Full direct-file verification includes the original packed asset file.
    for name, (length, digest) in manifest['direct'].items():
        path = (asset_root/'files'/name).resolve()
        assert path.is_relative_to((asset_root/'files').resolve()) and path.is_file(), name
        assert path.stat().st_size == length and sha(path) == digest, 'Changed base resource: '+name
    overlay = root/'GameOverlay'
    overlay_manifest = json.loads((overlay/'overlay-manifest.json').read_text('utf-8'))
    assert overlay_manifest['sourceAPK_SHA256'] == BASE_SHA
    assert set(overlay_manifest['files']) == {'index.html','client.js','native-ios.js','ios-startup.js'}
    for name, (length, digest) in overlay_manifest['files'].items():
        path = overlay/name
        assert path.stat().st_size == length and sha(path) == digest, 'Changed overlay: '+name
    assert (overlay/'native-ios.js').read_bytes() == (IOS/'Web/native-ios.js').read_bytes()
    assert (overlay/'ios-startup.js').read_bytes() == (IOS/'Web/ios-startup.js').read_bytes()
    assert (overlay/'native-ios.js').read_text('utf-8').count('__ZG_IOS_BOOTSTRAP_JSON__') == 1
    config = json.loads((root/'NativeConfig.json').read_text('utf-8'))
    assert config['baseRevision'] == 'r119'
    assert config['profile'] == json.loads((asset_root/'files/server-profile95.json').read_text('utf-8'))
    capability = base64.b64decode(config['launcherAuthenticationBase64'], validate=True)
    assert 0 < len(capability) <= 1024
    with (IOS/'Zhanguo/Info.plist').open('rb') as stream: info = plistlib.load(stream)
    assert info['CFBundleDisplayName'] == '战国' and info['UIRequiresFullScreen']
    ET.parse(root/'LaunchScreen.storyboard')
    ET.parse(IOS/'Zhanguo.xcodeproj/xcshareddata/xcschemes/Zhanguo.xcscheme')
    generation = json.loads((IOS/'tests/project-generation.json').read_text('utf-8'))
    assert generation['deploymentTarget'] == '16.4' and generation['libzLinked']
    actual_sources = sorted(p.relative_to(IOS).as_posix() for p in (IOS/'Zhanguo/Sources').rglob('*.swift'))
    actual_tests = sorted(p.relative_to(IOS).as_posix() for p in (IOS/'ZhanguoTests').rglob('*.swift'))
    assert generation['swiftSources'] == actual_sources, 'Regenerate project after changing Swift files'
    assert generation['swiftTests'] == actual_tests, 'Regenerate project after changing tests'
    project = (IOS/'Zhanguo.xcodeproj/project.pbxproj').read_text('utf-8')
    for path in actual_sources + actual_tests:
        assert project.count('"'+path+'"') == 1, path
    record = {'passed':True, 'baseRevision':'r119','baseAPK_SHA256':BASE_SHA,
        'directResourcesVerified':len(manifest['direct']), 'packedResourceRecords':len(manifest['packed']),
        'visibleResourcePaths':manifest['summary']['visibleCount'],'overlayResourcesVerified':len(overlay_manifest['files']),
        'swiftSourceFiles':len(actual_sources),'swiftTestFiles':len(actual_tests),
        'nativeConfigurationPresent':True,'secretsLogged':False,
        'appleCompilationExecuted':False,'iPhoneExecutionVerified':False,
        'remaining':'Run check_on_mac.sh with Xcode, then real-device login/scene/combat/movie/performance acceptance'}
    (IOS/'tests/project-verification.json').write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(record,ensure_ascii=False,indent=2))
    return record


if __name__ == '__main__': verify()
