"""Copy only the existing r119 native configuration into the iOS bundle.

The launcher capability is already shipped in the Android container. Never
print it or put it in a build log. No account password or game token is read.
"""
from pathlib import Path
import argparse
import base64
import hashlib
import json
import re
import zipfile

ROOT = Path(__file__).resolve().parents[2]
BASE_SHA = '62a968e2d80bdaf95b6ddd80a038de3dd29622aa2e1f55cddde5f8e887800c44'


def prepare(apk, source, destination):
    with apk.open('rb') as stream:
        digest = hashlib.sha256()
        for chunk in iter(lambda: stream.read(1024 * 1024), b''): digest.update(chunk)
        if digest.hexdigest() != BASE_SHA:
            raise ValueError('Expected the frozen signed r119 APK')
    with zipfile.ZipFile(apk) as archive:
        profile = json.loads(archive.read('assets/server-profile95.json'))
    raw = source.read_bytes()
    match = re.search(rb'\bLAUNCHER_AUTH\s*=\s*"([^"\\\r\n]+)"\s*;', raw)
    if not match or not match[1].isascii():
        raise ValueError('Expected one existing literal launcher capability')
    if len(re.findall(rb'\bLAUNCHER_AUTH\s*=', raw)) != 1:
        raise ValueError('Ambiguous launcher capability source')
    if profile.get('role') != 'direct-game' or profile.get('transport') != 'direct-tcp':
        raise ValueError('This source bundle targets the current direct-game configuration')
    config = {'profile': profile, 'baseRevision': 'r119',
              'launcherAuthenticationBase64': base64.b64encode(match[1]).decode('ascii')}
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(config, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'prepared': True, 'destination': str(destination), 'baseRevision': 'r119',
        'launcherCapability': 'copied privately; not logged', 'accountCredentialsRead': False}, ensure_ascii=False))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apk', type=Path, default=ROOT/'output/apk114-direct-game-45.124.77.131-login-r119.apk')
    parser.add_argument('--source', type=Path, default=ROOT/'packaging/native49/TcpTransport95.java')
    parser.add_argument('--destination', type=Path, default=ROOT/'ios/Zhanguo/Resources/NativeConfig.json')
    args = parser.parse_args()
    prepare(args.apk, args.source, args.destination)
