"""Build a narrow iOS overlay from frozen r119 assets; never edit GameAssets."""
from pathlib import Path
import argparse,hashlib,json
ROOT=Path(__file__).resolve().parents[2]
WEB=Path(__file__).resolve().parent
def sha(payload):return hashlib.sha256(payload).hexdigest()
def compose(assets,output):
 manifest=json.loads((assets/'assets-manifest.json').read_text('utf8'))
 source=assets/'files'
 raw=(source/'client.js').read_bytes();index=(source/'index.html').read_bytes()
 assert sha(raw)==manifest['direct']['client.js'][1]
 assert sha(index)==manifest['direct']['index.html'][1]
 assert sha(raw)=='de88b63c601b537ad8d737aae27b8dbd8ab4a127b8581f5ee13065d2dcfe0b87','Expected frozen r119 client'
 text=raw.decode('utf8');start=text.index('class DirectJsonConnection extends Connection_1.Connection {')
 ending='exports.DirectJsonConnection = DirectJsonConnection;';end=text.index(ending,start)+len(ending)
 replacement=(WEB/'direct-json-transport-ios.js').read_text('utf8')
 prefix=text[:start].encode('utf8');suffix=text[end:].encode('utf8');replacement_bytes=replacement.encode('utf8')
 generated=prefix+replacement_bytes+suffix
 html=index.decode('utf8');old='<script type="module" src="client.js"></script>'
 assert html.count(old)==1,'Unexpected frozen index entry module'
 html=html.replace(old,'<script type="module" src="ios-startup.js"></script>')
 files={'client.js':generated,'index.html':html.encode('utf8'),
  'native-ios.js':(WEB/'native-ios.js').read_bytes(),'ios-startup.js':(WEB/'ios-startup.js').read_bytes()}
 output.mkdir(parents=True,exist_ok=True)
 for name,body in files.items():(output/name).write_bytes(body)
 overlay={'schemaVersion':1,'sourceAPK_SHA256':manifest['source']['apkSHA256'],
  'files':{name:[len(body),sha(body)] for name,body in files.items()},
  'changes':['DirectJsonConnection instance-scoped connectionId and async-generation guards','index entry runtime checks before importing original game client','document-start native bridge resource; nonce injected by Swift host, never HTML']}
 (output/'overlay-manifest.json').write_text(json.dumps(overlay,ensure_ascii=False,indent=2),'utf8')
 report={'sourceAPK_SHA256':overlay['sourceAPK_SHA256'],'sourceClientSHA256':sha(raw),'sourceIndexSHA256':sha(index),
  'clientPrefixUnchanged':generated[:len(prefix)]==prefix,
  'clientSuffixUnchanged':generated[len(prefix)+len(replacement_bytes):]==suffix,
  'nativeBridgeBootstrapTokenCount':files['native-ios.js'].count(b'__ZG_IOS_BOOTSTRAP_JSON__'),
  'overlay':str(output),'files':overlay['files']}
 assert report['clientPrefixUnchanged'] and report['clientSuffixUnchanged'] and report['nativeBridgeBootstrapTokenCount']==1
 return report
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--assets',type=Path,default=ROOT/'ios/Zhanguo/Resources/GameAssets');parser.add_argument('--output',type=Path,default=ROOT/'ios/Zhanguo/Resources/GameOverlay');args=parser.parse_args()
 report=compose(args.assets,args.output)
 audit=ROOT/'ios/tests/js/overlay-build.json';audit.parent.mkdir(parents=True,exist_ok=True);audit.write_text(json.dumps(report,indent=2),'utf8')
 print(json.dumps(report))
