'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),{spawn}=require('node:child_process'),readline=require('node:readline'),{chromium}=require('playwright');
const root=path.resolve(__dirname,'../../..'),here=__dirname,python='C:/Users/36525/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe';
(async()=>{
 const server=spawn(python,['-X','utf8',path.join(here,'serve-overlay.py')],{cwd:root,stdio:['ignore','pipe','pipe'],windowsHide:true});
 const ready=await new Promise((resolve,reject)=>{readline.createInterface({input:server.stdout}).once('line',line=>resolve(JSON.parse(line)));server.once('error',reject);});
 const base='http://127.0.0.1:'+ready.port,browser=await chromium.launch({channel:'msedge',headless:true});
 const shim=fs.readFileSync(path.join(root,'ios/Zhanguo/Resources/GameOverlay/native-ios.js'),'utf8').replace('__ZG_IOS_BOOTSTRAP_JSON__',JSON.stringify({v:1,nonce:'offline-browser-owner-0123456789'}));
 const report={sourceAPK_SHA256:ready.sourceAPK_SHA256,overlay:JSON.parse(fs.readFileSync(path.join(root,'ios/Zhanguo/Resources/GameOverlay/overlay-manifest.json'),'utf8')),scope:'Actual GameOverlay index/startup/client and manifest-verified frozen APK assets served over localhost. WK channel/native zone replies mocked; no external networking, game login, device or Swift execution. HTTP analogue does not establish custom-scheme WKWebView compatibility.',viewports:[],failures:[]};
 try{
 for(const viewport of [{width:1280,height:720},{width:390,height:844}]){
  const ctx=await browser.newContext({viewport}),external=[],errors=[];await ctx.route('**/*',route=>{const url=route.request().url();if(url.startsWith(base+'/'))return route.continue();external.push(url);return route.abort();});if(ctx.routeWebSocket)await ctx.routeWebSocket('**/*',socket=>socket.close());
  await ctx.addInitScript({content:`globalThis.iosNativePosts=[];Object.defineProperty(globalThis,'webkit',{value:{messageHandlers:{zgNative:{postMessage(message){iosNativePosts.push(message);if(message.kind==='report')return;queueMicrotask(()=>ZgNativeIOS.receive({v:1,nonce:message.nonce,kind:message.kind,id:message.id,ok:false,error:{code:'offline_fixture',message:'离线测试：未连接列表服务器',delivery:'not_sent'}}));}}}}});\n${shim}`});
  const page=await ctx.newPage();page.on('pageerror',error=>errors.push(error.message));await page.goto(base+'/',{waitUntil:'load'});await page.waitForFunction(()=>globalThis.ZgIOSStartupAudit?.ready===true&&typeof globalThis.nativeReply==='function'&&!!document.querySelector('#user'),{timeout:30000});await page.waitForTimeout(1200);
  const state=await page.evaluate(()=>({audit:ZgIOSStartupAudit,bridge:ZgNativeIOS.version,request95:typeof Lab.request95,oldSync:typeof Lab.offlineRecord49,stream:typeof Lab.openStream109,user:!!document.querySelector('#user'),password:!!document.querySelector('input[type="password"]'),fatal:!!document.querySelector('#iosStartupError'),aimDisplay:document.querySelector('#motionAim114')?getComputedStyle(document.querySelector('#motionAim114')).display:null,posts:iosNativePosts.map(({kind,route})=>({kind,route})),text:document.body.innerText.slice(-1800)}));
  assert.equal(state.fatal,false);assert.equal(state.request95,'function');assert.equal(state.oldSync,'undefined');assert.equal(state.stream,'undefined');assert.equal(state.user,true);assert.equal(state.password,true);assert.equal(state.aimDisplay,'none');assert.deepEqual(errors,[]);assert.equal(external.length,0);assert(!state.posts.some(row=>['/direct/open','/direct/send','/direct/receive'].includes(row.route)));
  await page.screenshot({path:path.join(here,`cold-start-${viewport.width}.png`)});report.viewports.push({viewport,state,errors,external});await ctx.close();
 }
 // Real client parses a native JSON-string launcher response into a dynamic
 // seven-zone list. All values are synthetic offline inputs, never live data.
 {
  const ctx=await browser.newContext({viewport:{width:800,height:600}});await ctx.route('**/*',route=>route.request().url().startsWith(base+'/')?route.continue():route.abort());
  await ctx.addInitScript({content:`globalThis.iosNativePosts=[];Object.defineProperty(globalThis,'webkit',{value:{messageHandlers:{zgNative:{postMessage(message){iosNativePosts.push(message);if(message.kind==='report')return;const config='[server]\\n'+Array.from({length:7},(_,i)=>'z'+i+'=测试|离线测试'+(i+1)+'区|fixture'+i+'|203.0.113.1|7000|'+(i+1)+'|0|6020').join('\\n');queueMicrotask(()=>ZgNativeIOS.receive({v:1,nonce:message.nonce,kind:message.kind,id:message.id,ok:true,result:{status:200,body:JSON.stringify({configText:config}),encoding:'identity'}}));}}}}});\n${shim}`});
  const p=await ctx.newPage();await p.goto(base+'/',{waitUntil:'load'});await p.waitForFunction(()=>document.querySelectorAll('#serverZones114 button').length===7);assert.equal(await p.locator('#serverZone option').count(),7);
  await p.getByRole('radio',{name:'离线测试7区',exact:true}).click();assert.equal(await p.locator('#serverZone').inputValue(),'fixture6|203.0.113.1|7000');
  const packed=await p.evaluate(async()=>{const r=await fetch('native107/spells/textures/0008_3.png');const bytes=await r.arrayBuffer(),bitmap=await createImageBitmap(new Blob([bytes],{type:'image/png'}));return{status:r.status,encoding:r.headers.get('Content-Encoding'),bytes:bytes.byteLength,width:bitmap.width,height:bitmap.height};});assert.equal(packed.status,200);assert.equal(packed.encoding,null);assert.equal(packed.bytes,24153);assert(packed.width>0&&packed.height>0);
  const posted=await p.evaluate(()=>iosNativePosts.filter(p=>p.kind==='request95').map(({route,endpoint})=>({route,endpoint})));assert.deepEqual(posted.map(r=>r.route),['/direct/zones']);assert(posted[0].endpoint.startsWith('zg+tcp://'));
  report.dynamicZones={count:7,selected:'离线测试7区',fixtureValuesOnly:true,noGameConnect:true};report.packedAsset=packed;await p.screenshot({path:path.join(here,'cold-start-zones-fixture.png')});await ctx.close();
 }
 // Feature/bridge failure must show a readable error before importing client.
 {
  const ctx=await browser.newContext();await ctx.route('**/*',route=>route.request().url().startsWith(base+'/')?route.continue():route.abort());const p=await ctx.newPage();await p.goto(base+'/',{waitUntil:'load'});await p.waitForSelector('#iosStartupError');
  const snapshot=await p.evaluate(()=>({audit:ZgIOSStartupAudit,title:document.querySelector('#iosStartupError h2').textContent,clientImported:typeof nativeReply==='function'}));assert.equal(snapshot.clientImported,false);assert.equal(snapshot.audit.ready,false);report.failures.push({case:'missing-native-bridge',...snapshot});await p.screenshot({path:path.join(here,'cold-start-bridge-failure.png')});await ctx.close();
 }
 const requests=await (await fetch(base+'/__audit_requests')).json();assert(requests.some(row=>row.name==='client.js'&&row.source==='ios-overlay'&&row.status===200));assert(requests.some(row=>row.name==='ios-startup.js'&&row.source==='ios-overlay'&&row.status===200));assert(!requests.some(row=>row.status===500));report.assetRequests=requests;report.passed=true;
 fs.writeFileSync(path.join(here,'cold-start-report.json'),JSON.stringify(report,null,2));console.log(JSON.stringify({passed:true,viewports:report.viewports.map(v=>v.viewport),assetRequests:requests.length,failures:report.failures.map(x=>x.case)}));
 }finally{await browser.close();server.kill();}
})().catch(error=>{console.error(error);process.exitCode=1;});
