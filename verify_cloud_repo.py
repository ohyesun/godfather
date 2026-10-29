"""Pre-push self-check for the Codemagic repo.

Runs the same two gates that check_on_mac.sh runs before xcodebuild, inside a
throw-away copy so the working tree stays clean, and validates codemagic.yaml
structure against what Codemagic actually needs.
"""
import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
IOS = ROOT / 'ios'
PAK = IOS / 'Zhanguo/Resources/GameAssets/files/resources107.pak'
MANIFEST = IOS / 'Zhanguo/Resources/GameAssets/assets-manifest.json'
PAK_BYTES = 29940179
PAK_SHA = '6d92cadb3d24a8e3478d544768c2b8a766944d94b52188228687e650c5c462fe'


def sha256(path):
    import hashlib
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def check_layout(problems):
    for path in (PAK, MANIFEST, IOS / 'Zhanguo.xcodeproj/project.pbxproj',
                 IOS / 'tools/check_on_mac.sh', IOS / 'tools/build_unsigned_ipa.sh',
                 IOS / 'tools/cloud_step.sh',
                 IOS / 'Zhanguo/Sources/Native/Micro/ZGMicroClient.swift',
                 IOS / 'Zhanguo/Sources/Native/Micro/ZGMicroFetcher.swift',
                 IOS / 'ZhanguoTests/Micro/ZGMicroClientTests.swift',
                 IOS / 'Zhanguo/Resources/MicroClient.json'):
        if not path.is_file():
            problems.append('missing: ' + path.relative_to(ROOT).as_posix())
    if PAK.is_file():
        if PAK.stat().st_size != PAK_BYTES:
            problems.append('pak bytes %d != %d' % (PAK.stat().st_size, PAK_BYTES))
        elif sha256(PAK) != PAK_SHA:
            problems.append('pak sha256 mismatch')
    if MANIFEST.is_file():
        packed = json.loads(MANIFEST.read_text('utf-8'))['packed']
        remote = sum(1 for row in packed.values() if row[0] == -1)
        if len(packed) != 82344:
            problems.append('packed records %d != 82344' % len(packed))
        if remote != 80457:
            problems.append('remote records %d != 80457' % remote)
        return {'packed': len(packed), 'remote': remote}
    return {}


def check_yaml(problems):
    try:
        import yaml
    except ImportError:
        problems.append('PyYAML missing; cannot parse codemagic.yaml')
        return {}
    raw = (ROOT / 'codemagic.yaml').read_text('utf-8')
    doc = yaml.safe_load(raw)
    flows = doc.get('workflows') or {}
    for name in ('swift-compile', 'zhanguo-ios-minimal'):
        if name not in flows:
            problems.append('workflow missing: ' + name)
            continue
        flow = flows[name]
        if flow.get('instance_type') != 'mac_mini_m2':
            problems.append(name + ': instance_type not mac_mini_m2')
        if flow.get('environment', {}).get('xcode') != '16.4':
            problems.append(name + ': xcode not pinned to 16.4')
        if not flow.get('scripts'):
            problems.append(name + ': no scripts')
        joined = json.dumps(flow.get('scripts'))
        if name == 'swift-compile':
            for needle in ('tools/cloud_step.sh', 'generate_project.py', 'verify_project.py'):
                if needle not in joined:
                    problems.append(name + ': script does not reference ' + needle)
        else:
            # This workflow delegates the two gates to check_on_mac.sh; assert it really does.
            if 'tools/cloud_step.sh' not in joined:
                problems.append(name + ': script does not use tools/cloud_step.sh')
            gate = (IOS / 'tools/check_on_mac.sh').read_text('utf-8')
            for needle in ('generate_project.py', 'verify_project.py'):
                if needle not in gate:
                    problems.append('check_on_mac.sh does not run ' + needle)
    minimal = flows.get('zhanguo-ios-minimal', {})
    joined = json.dumps(minimal.get('scripts'))
    for needle in ('check_on_mac.sh', 'build_unsigned_ipa.sh'):
        if needle not in joined:
            problems.append('zhanguo-ios-minimal: missing ' + needle)
    arts = json.dumps(minimal.get('artifacts', []))
    if '*.ipa' not in arts:
        problems.append('zhanguo-ios-minimal: artifacts do not collect the IPA')
    forbidden = ('codesign', 'exportArchive', 'altool', 'notarytool', 'xcodebuild -exportArchive')
    for needle in forbidden:
        if needle in raw:
            problems.append('yaml must stay unsigned; found ' + needle)
    compile_flow = flows.get('swift-compile', {})
    if 'build-for-testing' not in json.dumps(compile_flow.get('scripts')):
        problems.append('swift-compile: expected build-for-testing')
    return {'workflows': sorted(flows)}


def run_gates(problems):
    """Run generate + verify in a clean copy, exactly as the cloud step does."""
    work = Path(tempfile.mkdtemp(prefix='zg-cloud-gates-'))
    try:
        target = work / 'ios'
        shutil.copytree(IOS, target, ignore=shutil.ignore_patterns('build', 'DerivedData', '__pycache__'))
        result = {}
        for step, script in (('generate', 'generate_project.py'), ('verify', 'verify_project.py')):
            proc = subprocess.run([sys.executable, str(Path('tools') / script)], cwd=target,
                                  capture_output=True, text=True)
            result[step] = proc.returncode
            if proc.returncode != 0:
                tail = (proc.stderr or proc.stdout).strip().splitlines()[-12:]
                problems.append('%s failed: %s' % (step, ' | '.join(tail)))
            elif step == 'verify':
                result['verified'] = json.loads(proc.stdout)
        return result
    finally:
        shutil.rmtree(work, ignore_errors=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--skip-gates', action='store_true', help='only structural checks')
    args = parser.parse_args()
    problems = []
    report = {'layout': {}, 'yaml': {}, 'gates': {}}
    report['layout'] = check_layout(problems)
    report['yaml'] = check_yaml(problems)
    if not args.skip_gates:
        report['gates'] = run_gates(problems)
    verified = report['gates'].pop('verified', None)
    if verified:
        report['gates']['verified'] = {k: verified[k] for k in
                                       ('directResourcesVerified', 'packedResourceRecords',
                                        'swiftSourceFiles', 'swiftTestFiles')}
    report['problems'] = problems
    report['verdict'] = 'PASS' if not problems else 'FAIL'
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0 if not problems else 1


if __name__ == '__main__':
    sys.exit(main())
