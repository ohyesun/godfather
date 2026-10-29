"""Inventory frozen APK JavaScript/native references, without extracting large assets."""
from pathlib import Path
import gzip,hashlib,json,re,zipfile,zlib
ROOT=Path(__file__).resolve().parents[3]
APK=ROOT/'local-test/direct-acceptance-r119/aligned-direct-login-r119.apk'
OUT=Path(__file__).resolve().parent
with zipfile.ZipFile(APK) as z:
 entries=[];snippets=[];marker_counts={key:0 for key in ['readPacked107','offlineDevice49','offlineRead49','offlineRecord49','offlineForget49']}
 for info in z.infolist():
  if not info.filename.startswith('assets/') or not info.filename.endswith(('.js','.html')) or info.file_size>24*1024*1024:continue
  data=z.read(info);text=data.decode('utf8',errors='replace')
  for marker in marker_counts:marker_counts[marker]+=text.count(marker)
  refs=list(re.finditer(r'(?:window\.|globalThis\.)?Lab\??\.(\w+)|window\[.Lab.\]',text))
  if not refs:continue
  entries.append({'entry':info.filename,'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data),
   'methods':sorted(set(m.group(1) for m in refs if m.group(1)))})
  for m in refs:
   snippets.append({'entry':info.filename,'offset':m.start(),'text':text[max(0,m.start()-160):m.end()+230]})
 client=z.read('assets/client.js').decode('utf8')
 anchors=['nativeReply','setNativeRequest','openStream109','sendStream109','closeStream109','request95','offlineDevice49','report(']
 client_snippets={a:[client[max(0,m.start()-250):m.end()+750] for m in list(re.finditer(re.escape(a),client))[:8]] for a in anchors}
assets=ROOT/'ios/Zhanguo/Resources/GameAssets';manifest=json.loads((assets/'assets-manifest.json').read_text('utf8'));packed_only=[]
for name,(offset,length,size,codec,expected) in manifest['packed'].items():
 if name in manifest['direct'] or not name.endswith(('.js','.html')):continue
 assert length<24*1024*1024 and size<24*1024*1024
 with (assets/'files/resources107.pak').open('rb') as raw:raw.seek(offset);data=raw.read(length)
 if codec==1:data=gzip.decompress(data)
 elif codec==2:data=zlib.decompress(data)
 assert len(data)==size and hashlib.sha256(data).hexdigest()==expected
 text=data.decode('utf8');refs=sorted(set(re.findall(r'\bLab\b|readPacked107|offline\w+49',text)))
 packed_only.append({'name':name,'bytes':size,'sha256':expected,'nativeMarkers':refs})
report={'apk':str(APK),'entries':entries,'snippets':snippets,'clientSnippets':client_snippets,'directNativeMarkerCounts':marker_counts,
 'packedOnlyJavaScriptOrHTML':packed_only,'packedSourceAPK_SHA256':manifest['source']['apkSHA256']}
(OUT/'r119-native-inventory.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),'utf8')
print(json.dumps({'entries':entries,'anchors':{a:len(v) for a,v in client_snippets.items()}},ensure_ascii=False,indent=2))
