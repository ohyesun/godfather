"""Offline HTTP analogue of the manifest-constrained WK asset handler.
Only requested spans are decoded. No game TCP or external HTTP is used.
"""
from pathlib import Path
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from urllib.parse import unquote,urlsplit
import gzip,hashlib,json,mimetypes,zlib
ROOT=Path(__file__).resolve().parents[3]
ASSETS=ROOT/'ios/Zhanguo/Resources/GameAssets'
OVERLAY=ROOT/'ios/Zhanguo/Resources/GameOverlay'
manifest=json.loads((ASSETS/'assets-manifest.json').read_text('utf8'))
overlay=json.loads((OVERLAY/'overlay-manifest.json').read_text('utf8'))
assert manifest['source']['apkSHA256']==overlay['sourceAPK_SHA256']
requests=[]
class Handler(BaseHTTPRequestHandler):
 def do_GET(self):
  name=unquote(urlsplit(self.path).path).lstrip('/') or 'index.html'
  status=200;origin=''
  if name=='__audit_requests':body=json.dumps(requests).encode();mime='application/json'
  else:
   try:
    if '\\' in name or any(p in ('.','..','') for p in name.split('/')):raise KeyError(name)
    if name in overlay['files']:
     record=overlay['files'][name];body=(OVERLAY/name).read_bytes();origin='ios-overlay'
    elif name in manifest['direct']:
     record=manifest['direct'][name];body=(ASSETS/'files'/name).read_bytes();origin='frozen-apk-direct'
    else:
     offset,length,size,codec,expected=manifest['packed'][name]
     if length>24*1024*1024 or size>24*1024*1024:raise KeyError('audit size bound')
     with (ASSETS/'files/resources107.pak').open('rb') as handle:handle.seek(offset);body=handle.read(length)
     if codec==1:body=gzip.decompress(body)
     elif codec==2:body=zlib.decompress(body)
     elif codec!=0:raise ValueError('unsupported codec')
     record=[size,expected];origin='frozen-apk-indexed-span'
    assert len(body)==record[0] and hashlib.sha256(body).hexdigest()==record[1],'Manifest hash mismatch: '+name
    mime=mimetypes.guess_type(name)[0] or 'application/octet-stream'
    if name.endswith('.js'):mime='text/javascript'
   except KeyError:body=b'Offline asset unavailable';mime='text/plain';status=404
   except Exception as error:body=str(error).encode();mime='text/plain';status=500
   requests.append({'name':name,'status':status,'bytes':len(body),'source':origin})
  self.send_response(status);self.send_header('Content-Type',mime);self.send_header('Content-Length',str(len(body)));self.send_header('Cache-Control','no-store');self.end_headers()
  try:self.wfile.write(body)
  except (BrokenPipeError,ConnectionResetError):pass
 def log_message(self,*args):pass
server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
print(json.dumps({'port':server.server_port,'sourceAPK_SHA256':overlay['sourceAPK_SHA256']}),flush=True)
server.serve_forever()
