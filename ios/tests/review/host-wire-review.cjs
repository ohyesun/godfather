'use strict';
// Executes the actual iOS JS shim. Swift field extraction is a source audit,
// not a claim that Swift/WKWebView/Keychain ran on this Windows machine.
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict'),crypto=require('node:crypto'),path=require('node:path');
const root=path.resolve(__dirname,'../../..'),messages=[],nonce='00000000-0000-4000-8000-000000000001';
const source=fs.readFileSync(path.join(root,'ios/Web/native-ios.js'),'utf8');
const box={crypto:crypto.webcrypto,queueMicrotask,setTimeout:()=>0,clearTimeout(){},addEventListener(){},
 webkit:{messageHandlers:{zgNative:{postMessage:m=>messages.push(JSON.parse(JSON.stringify(m)))}}}};
vm.runInNewContext(source.replace('__ZG_IOS_BOOTSTRAP_JSON__',JSON.stringify({v:1,nonce})),box);
const promise=box.ZgNativeIOS.storage.record(JSON.stringify({account:'fixture',startKey:'fixture-start'}));promise.catch(()=>{});
box.Lab.report(JSON.stringify({fps:61}));
const record=messages.find(m=>m.kind==='storage'),report=messages.find(m=>m.kind==='report');
assert.equal(record.operation,'record');assert.equal(typeof record.body,'string');assert.equal(typeof report.body,'string');
const bridge=fs.readFileSync(path.join(root,'ios/Zhanguo/Sources/Native/Bridge/ZGScriptBridge.swift'),'utf8');
const storageField=bridge.match(/storage\.request\(operation: operation, value: body\["([^"]+)"\]/)?.[1];
const reportField=bridge.match(/collectPerformance\(body\["([^"]+)"\]/)?.[1];
assert.equal(typeof record[storageField],'string','Actual JS record body must reach Swift storage');
assert.equal(typeof report[reportField],'string','Actual JS report body must reach performance collection');
const controller=fs.readFileSync(path.join(root,'ios/Zhanguo/Sources/App/GameViewController.swift'),'utf8');
assert(controller.includes('hasAllowedMainNavigation && !fragmentOnly'),'All replacement documents, not only reload, need a new owner');
assert(controller.includes('pendingDialogCancel?()'),'Outstanding JS dialogs need a cancellation completion on shutdown');
assert(controller.includes('[weak self, weak webView]')&&controller.includes('self.webView === webView')&&controller.includes('applicationState != .background'),
 'Queued navigation rebuilding must not resurrect a destroyed/background/replaced WebView');
const result={result:'pass',actualJSShimExecuted:true,swiftCompiled:false,actualWKWebViewOrKeychainExecuted:false,
 recordWireField:storageField,reportWireField:reportField,navigationReplacementSourceGuard:true,queuedNavigationIdentityAndBackgroundGuard:true,
 nativeMacTests:'ios/ZhanguoTests/Host/ZGHostContractTests.swift',
 remainingMacChecks:['Actual WK shim->Host->Keychain roundtrip','New nonce on location.assign/index link','Queued old navigation cannot reopen after shutdown/background','Old native callback never reaches replacement document'],
 sourceSHA256:Object.fromEntries(['ios/Web/native-ios.js','ios/Zhanguo/Sources/Native/Bridge/ZGScriptBridge.swift','ios/Zhanguo/Sources/Native/Storage/ZGSecureStorage.swift','ios/Zhanguo/Sources/App/GameViewController.swift'].map(p=>[p,crypto.createHash('sha256').update(fs.readFileSync(path.join(root,p))).digest('hex')]))};
fs.writeFileSync(path.join(__dirname,'host-wire-review.json'),JSON.stringify(result,null,2)+'\n');
console.log(JSON.stringify(result,null,2));
