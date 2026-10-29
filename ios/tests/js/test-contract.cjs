'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),{webcrypto}=require('node:crypto');
const root=path.resolve(__dirname,'../../..'),overlay=path.join(root,'ios/Zhanguo/Resources/GameOverlay');
const client=fs.readFileSync(path.join(overlay,'client.js'),'utf8'),source=fs.readFileSync(path.join(root,'ios/Web/native-ios.js'),'utf8');
const nonce='contract-nonce-0123456789';
function clock(){let n=0;const timers=new Map();return {setTimeout(fn,ms){const id=++n;timers.set(id,{fn,ms});return id;},clearTimeout(id){timers.delete(id);},timers};}
function bridge(postError=false){
 const t=clock(),posts=[],events={};
 const crypto={getRandomValues:array=>webcrypto.getRandomValues(array)};
 const ctx=vm.createContext({console,Map,Set,Error,JSON,Number,Uint8Array,queueMicrotask,crypto,...t,performance,webkit:{messageHandlers:{zgNative:{postMessage(value){if(postError)throw Error('post rejected');posts.push(value);}}}},addEventListener:(key,fn)=>events[key]=fn});
 vm.runInContext(source.replace('__ZG_IOS_BOOTSTRAP_JSON__',JSON.stringify({v:1,nonce})),ctx);
 // Exact nativeReply function from the frozen game, with only its surrounding
 // DOM-independent map/report bindings supplied by this contract harness.
 const reply=client.slice(client.indexOf('window.nativeReply='),client.indexOf('function previewRequest94'));
 vm.runInContext('globalThis.window=globalThis;const native=new Map(),report={};globalThis.nativePending=native;'+reply,ctx);
 function request(id,route='/direct/open',body={host:'offline.invalid',port:6020}){
  const p=new Promise((resolve,reject)=>ctx.nativePending.set(id,{resolve,reject,timer:0,route,started:0}));
  ctx.Lab.request95(id,'http://127.0.0.1:27150',route,'','',JSON.stringify(body));return p;
 }
 function respond(id,result={status:200,body:'{"ok":true}'},extra={}){return ctx.ZgNativeIOS.receive({v:1,nonce,kind:'request95',id,ok:true,result,...extra});}
 return {ctx,posts,events,request,respond,t};
}
function actualModules(){
 const t=clock(),ctx=vm.createContext({console,TextEncoder,TextDecoder,Uint8Array,ArrayBuffer,DataView,Buffer,CompressionStream,DecompressionStream,Response,Blob,...t});
 const first=client.indexOf('const __zgModules='),end=client.indexOf("const __zgEntry=__zgLoad('DirectAppSession.js');",first);
 assert(first>=0&&end>first);vm.runInContext(client.slice(first,end)+';globalThis.load=__zgLoad;',ctx,{timeout:30000});return {ctx,t,load:ctx.load};
}
const tick=()=>new Promise(resolve=>setImmediate(resolve));
(async()=>{
 assert.equal(source,fs.readFileSync(path.join(overlay,'native-ios.js'),'utf8'),'Test the generated bridge bytes');
 const report={source:'Actual iOS overlay module factories and original r119 nativeReply; native channel and TCP replies are mocked.',overlay:JSON.parse(fs.readFileSync(path.join(overlay,'overlay-manifest.json'),'utf8')),cases:[]};
 {
  const b=bridge(),p1=b.request(1),p2=b.request(2);assert.equal(b.posts.length,2);assert.equal(b.posts[0].nonce,nonce);assert.equal(b.posts[0].kind,'request95');assert.equal(typeof b.posts[0].body,'string');
  assert.equal(b.respond(1,undefined,{nonce:'stale-owner'}),false);assert.equal(b.respond(99),false);b.respond(2,{status:200,body:'{"value":2}'});b.respond(1,{status:200,body:'{"value":1}'});
  assert.deepEqual(await p1,{value:1});assert.deepEqual(await p2,{value:2});assert.equal(b.respond(1),false);
  const p3=b.request(3,'/direct/send',{connectionId:'old',packetHex:'00'});b.respond(3,{status:503,body:'{"error":"old socket","code":"stale_connection","delivery":"not_sent"}'});await assert.rejects(p3,{code:'stale_connection'});assert.equal(b.posts.length,3);
  const p4=b.request(4);b.respond(4,{status:200,body:{invalid:true}});await assert.rejects(p4,{code:'invalid_native_reply'});
  const p5=b.request(5,'/unsupported');await assert.rejects(p5,{code:'unsupported_native_route'});assert.equal(b.posts.length,4);
  const p6=b.request(6);assert.throws(()=>b.ctx.Lab.request95(6,'','/direct/open','','','{}'),{code:'duplicate_native_id'});b.respond(6);await p6;
  assert.equal(b.ctx.Lab.request,undefined);assert.equal(b.ctx.Lab.openStream109,undefined);assert.equal(b.ctx.Lab.offlineRecord49,undefined);assert.equal(b.ctx.nonce,undefined);
  const uuid=b.ctx.crypto.randomUUID();assert.match(uuid,/^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/);
  report.cases.push('Exact original nativeReply parses JSON-string bodies; reordered/duplicate/stale-nonce/unknown callbacks isolated; native error codes retained; unsupported routes never sent; no fake sync/stream APIs; cryptographic UUID fallback.');b.ctx.ZgNativeIOS.dispose();
 }
 {
  const b=bridge(),device=b.ctx.ZgNativeIOS.storage.device(),read=b.ctx.ZgNativeIOS.storage.read(),record=b.ctx.ZgNativeIOS.storage.record('{"account":"fixture"}');
  for(const [index,result]of [[0,'{"deviceId":"fixture"}'],[1,''],[2,true]]){const m=b.posts[index];assert.equal(m.kind,'storage');b.ctx.ZgNativeIOS.receive({v:1,nonce,kind:'storage',id:m.id,ok:true,result});}
  assert.equal(await device,'{"deviceId":"fixture"}');assert.equal(await read,'');assert.equal(await record,true);
  const bad=b.ctx.ZgNativeIOS.storage.forget(),m=b.posts.at(-1);b.ctx.ZgNativeIOS.receive({v:1,nonce,kind:'storage',id:m.id,ok:true,result:'true'});await assert.rejects(bad,{code:'invalid_native_reply'});
  const pending=b.request(10),rejected=assert.rejects(pending,{code:'native_bridge_closed'});b.events.pagehide();await rejected;assert.equal(b.respond(10),false);assert.equal(b.t.timers.size,0);
  report.cases.push('Storage is separate async API with string/strict-boolean validation; page disposal rejects once and clears all timers, old callback ignored.');
 }
 {
  const b=bridge(true),p=b.request(20);await assert.rejects(p,{code:'native_post_failed'});assert.equal(b.posts.length,0);b.ctx.ZgNativeIOS.dispose();
  const c=bridge(),p2=c.request(21),rejected=assert.rejects(p2,{code:'native_timeout'});[...c.t.timers.values()].find(row=>row.ms===55000).fn();await rejected;assert.equal(c.respond(21),false);assert.equal(c.posts.length,1);c.ctx.ZgNativeIOS.dispose();
  report.cases.push('Post failure and timeout reject without retries; timeout is unknown-delivery and late reply ignored.');
 }
 {
  const {load}=actualModules(),{DirectJsonCodec}=load('DirectJsonCodec.js'),{encodeGBK}=load('Gbk.js');
  assert.deepEqual([...encodeGBK('中文')],[0xd6,0xd0,0xce,0xc4]);
  const sender=new DirectJsonCodec(),receiver=new DirectJsonCodec(),body=new TextEncoder().encode('{"fixture":"codec"}'),wire=await sender.build(body,true),packets=[];
  for(let i=0;i<wire.length;i+=3)packets.push(...await receiver.push(wire.slice(i,i+3)));assert.equal(packets.length,1);assert.deepEqual(JSON.parse(packets[0].bodyText),{fixture:'codec'});
  const key=Uint8Array.from([1,2,3,4,5,6,7,8]);sender.useSessionKey(key);receiver.useSessionKey(key);const des=await sender.build(Uint8Array.from([5,12,123,0,255]));
  const decoded=await receiver.push(des);assert.equal(decoded.length,1);assert(decoded[0].bodyHex.startsWith('050c7b00ff'));
  report.cases.push('Actual frozen GBK encoder and original FrameCodec compressed RC5 fragmented roundtrip plus DES session-key roundtrip; no native codec mock.');
 }
 {
  const {load,t}=actualModules(),{DirectJsonConnection}=load('DirectJsonTransport.js'),{ConnState}=load('Connection.js'),calls=[],packets=[];
  const request=(route,body)=>new Promise((resolve,reject)=>calls.push({route,body,resolve,reject}));
  const a=new DirectJsonConnection({request,onPacket:p=>packets.push(p)}),b=new DirectJsonConnection({request,onPacket:p=>packets.push(p)});
  const pa=a.connect('offline.invalid',6020);a.close();const pb=b.connect('offline.invalid',6020);calls[1].resolve({connected:true,connectionId:'new'});await pb;calls[0].resolve({connected:true,connectionId:'old'});await assert.rejects(pa,{code:'connection_cancelled'});await tick();
  assert.equal(b.isConnected,true);assert.equal(calls.find(row=>row.route==='/direct/close').body.connectionId,'old');
  b.send(Uint8Array.from([0,255]));const send=calls.at(-1);assert.deepEqual(JSON.parse(JSON.stringify(send.body)),{connectionId:'new',packetHex:'00ff'});send.resolve({dataHex:'',connectionId:'new'});await tick();
  const poll=b.poll(),receive=calls.at(-1);assert.equal(receive.body.connectionId,'new');b.close();const reopen=b.connect('offline.invalid',6020);const next=calls.at(-1);next.resolve({connected:true,connectionId:'newer'});await reopen;
  receive.resolve({connectionId:'new',dataHex:'bad'});await poll;assert.equal(b.isConnected,true);assert.equal(b.iosConnectionId,'newer');
  const oldGeneration=b.iosGeneration,oldId=b.iosConnectionId;let release,pushes=0;const delayed={push:()=>{++pushes;return new Promise(resolve=>release=resolve);}};
  const decode=b.decode(Uint8Array.from([1]),oldGeneration,oldId,delayed);await tick();const queued=b.decode(Uint8Array.from([2]),oldGeneration,oldId,delayed);await tick();assert.equal(pushes,1,'same decoder must not overlap while async inflate is pending');
  b.close();const again=b.connect('offline.invalid',6020);calls.at(-1).resolve({connected:true,connectionId:'latest'});await again;release([{kind:'old-packet'}]);await Promise.all([decode,queued]);assert.equal(pushes,1,'old queued chunk never enters new decoder');assert.equal(packets.length,0);assert.equal(b.isConnected,true);
  const {DirectJsonCodec}=load('DirectJsonCodec.js'),encoder=new DirectJsonCodec(),wire=await encoder.build(Uint8Array.from([3,10,7]));let raw;
  b.addListener({onData:value=>{raw=[...value];}});b.consume({connectionId:'latest',dataHex:Buffer.from(wire).toString('hex')});await tick();
  assert.deepEqual(raw,[...wire]);assert.equal(packets.length,1);assert(packets[0].bodyHex.startsWith('030a07'));
  b.send(Uint8Array.from([1]));calls.at(-1).resolve({connectionId:'wrong'});await tick();assert.equal(b.state,ConnState.Error);assert.equal(calls.at(-1).route,'/direct/close');assert.equal(calls.at(-1).body.connectionId,'latest');
  assert(calls.filter(c=>c.route!=='/direct/open').every(c=>typeof c.body.connectionId==='string'&&c.body.connectionId));assert.equal(t.timers.size,0);
  report.cases.push('Actual generated transport: concurrent instances, close-before-open, late receive, late decoder and send-ID mismatch cannot affect newer connection; every send/receive/close carries explicit instance ID; per-generation decode queue serializes asynchronous inflation and discards old queued chunks.');
 }
 report.passed=true;fs.writeFileSync(path.join(__dirname,'contract-report.json'),JSON.stringify(report,null,2));console.log(JSON.stringify(report,null,2));
})().catch(error=>{console.error(error);process.exitCode=1;});
