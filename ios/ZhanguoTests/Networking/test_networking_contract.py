"""Offline original-Java wire fixtures + iOS source contract audit.

This executes Python assertions, not Swift or Network.framework. XCTest is the
native runtime validation and must run on a Mac/iOS simulator before shipping.
No fixture contains the application's actual launcher authentication value.
"""
from pathlib import Path
import hashlib, json, re, shutil, struct, unittest

ROOT=Path(__file__).resolve().parents[3]
NATIVE=ROOT/'ios/Zhanguo/Sources/Native/Networking'
OUT=Path(__file__).resolve().parent
JAVA=ROOT/'packaging/native49/TcpTransport95.java'
JAVA96=ROOT/'local-test/direct-apk-build-20261006/src/com/zhanguo/chapterone/TcpTransport96.java'

class NetworkingContractTests(unittest.TestCase):
 def test_actual_android_entry_and_secret_migration_consistency(self):
  a,b=JAVA.read_text('utf8'),JAVA96.read_text('utf8')
  pattern=r'private static final String LAUNCHER_AUTH\s*=\s*"([^"\n]+)";'
  x,y=re.search(pattern,a),re.search(pattern,b)
  self.assertTrue(x and y and x.group(1)==y.group(1) and x.group(1).isascii())
  # Neither actual secret, its base64, nor a digest enters the report.
  smali=(ROOT/'local-test/direct-combat-console-r110/smali-verified/com/zhanguo/chapterone/MainActivity$Bridge.smali').read_text('utf8')
  self.assertIn('TcpTransport96;->request(',smali)
  self.assertIn('ByteBuffer.allocate(12).order(ByteOrder.LITTLE_ENDIAN)',a)
  self.assertIn('ByteBuffer.allocate(20 + filename.length)',a)
 def test_original_launcher_and_tls_golden_bytes(self):
  payload=b'fixture-auth';request=struct.pack('<BBHII',1,9,0,len(payload),0)+payload
  self.assertEqual(request.hex(),'010900000c00000000000000666978747572652d61757468')
  name=b'LauncherConfig.txt';file=struct.pack('<QII',0,0,len(name))+name+b'\0'*4
  self.assertEqual(len(file),38)
  relay=b'ZGTP'+struct.pack('>BBQI',1,0,0x0102030405060708,2)+b'{}'
  self.assertEqual(relay.hex(),'5a47545001000102030405060708000000027b7d')
  header=struct.pack('<BBHIQI',1,0,0,11,1234,0)
  self.assertEqual(len(header),20);self.assertEqual(struct.unpack_from('<I',header,4)[0],11)
  self.assertEqual(struct.unpack_from('<Q',header,8)[0],1234)
 def test_original_gbk_registration_layout(self):
  fields=[b'tester1',b'secret1','&zone=一区'.encode('gbk')]
  payload=struct.pack('<III',*[len(f) for f in fields])+b''.join(fields)
  self.assertEqual([len(f) for f in fields],[7,7,10]);self.assertEqual(len(payload),36)
  self.assertEqual('测试'.encode('gbk').hex(),'b2e2cad4')
  with self.assertRaises(UnicodeEncodeError): '🙂'.encode('gbk')
 def test_swift_contract_is_strict_and_has_no_native_game_crypto(self):
  service=(NATIVE/'ZGNativeNetworking.swift').read_text('utf8')
  types=(NATIVE/'ZGNetworkTypes.swift').read_text('utf8')
  channel=(NATIVE/'ZGByteChannel.swift').read_text('utf8')
  wire=(NATIVE/'ZGWireCodec.swift').read_text('utf8')
  for marker in ['public func replaceOwner','public func cancelAll','public func request','session.id == id','self.direct === session',
                 'self.current(write.ticket)','trustedEndpoints.contains','catalogueEndpoints(text)','write.started = true']:
   self.assertIn(marker,service)
  for marker in ['public var dictionary','public func toDictionary','public typealias ZGNativeTransportError','case notSent = "not_sent", unknown']:
   self.assertIn(marker,types)
  self.assertIn('contentProcessed',channel);self.assertIn('sec_protocol_options_set_verify_block',channel)
  self.assertIn('SecCertificateCopyData(leaf) as Data == der',channel);self.assertIn('ZGCertificateValidity.containsNow',channel)
  self.assertIn('appendLE(0, bytes: 4, to: &result); return result',wire)
  self.assertNotRegex(service+r'\n'+wire,r'(?:func|class|struct)\s+(?:Rc5|Des|FrameCodec|encrypt|decrypt)\b')
  for source in [service,types,channel,wire]:
   self.assertNotIn('print(',source);self.assertNotIn('NSLog(',source);self.assertNotIn('evaluateJavaScript',source)
  self.assertTrue((OUT/'ZGNativeNetworkingTests.swift').is_file())

if __name__=='__main__':
 suite=unittest.defaultTestLoader.loadTestsFromTestCase(NetworkingContractTests)
 result=unittest.TextTestRunner(verbosity=2).run(suite)
 report={'result':'pass' if result.wasSuccessful() else 'fail','offlinePythonTestsRun':result.testsRun,
  'actualSwiftOrNetworkFrameworkExecution':False,'swiftCompilerAvailable':bool(shutil.which('swiftc')),
  'xcodeSimulatorDeviceOrLiveNetworkOperation':False,'originalJavaContractCompared':True,
  'launcherAuthenticationPrintedOrStoredInReport':False,
  'requiredNextValidation':'Run ZGNativeNetworkingTests XCTest on macOS/iOS Simulator; this Windows audit is not an iOS compilation.',
  'sourceSHA256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(NATIVE.glob('*.swift'))}}
 (OUT/'networking-offline-audit.json').write_text(json.dumps(report,indent=2)+'\n','utf8')
 raise SystemExit(0 if result.wasSuccessful() else 1)
