/* Inject at document start. The host replaces the single bootstrap token with
 * its JSON object literal. No network/game/codec success is synthesized here. */
(function(root){
 'use strict';
 const bootstrap = __ZG_IOS_BOOTSTRAP_JSON__;
 if(!bootstrap||bootstrap.v!==1||typeof bootstrap.nonce!=='string'||!/^[A-Za-z0-9_-]{16,256}$/.test(bootstrap.nonce))throw Error('iOS原生桥初始化无效');
 const nonce=bootstrap.nonce,handler=root.webkit?.messageHandlers?.zgNative;
 if(!handler||typeof handler.postMessage!=='function')throw Error('iOS原生消息通道不可用');
 if(root.Lab||root.ZgNativeIOS)throw Error('iOS原生桥已存在');
 const post=handler.postMessage.bind(handler),pending=new Map(),MAX_BODY=2*1024*1024,MAX_PENDING=96,TIMEOUT=55000;
 const routes=new Set(['/direct/open','/direct/send','/direct/receive','/direct/close','/direct/zones','/direct/register']);
 let storageSerial=0,closed=false;
 const key=(kind,id)=>kind+':'+typeof id+':'+id;
 function bridgeError(message,code,delivery='unknown'){return Object.assign(Error(message),{code,delivery});}
 function nativeFailure(id,error){queueMicrotask(()=>{
  if(typeof root.nativeReply==='function')root.nativeReply(id,{status:503,body:JSON.stringify({error:error.message,code:error.code,delivery:error.delivery}),encoding:'identity'});
 });}
 function rejectRow(row,error){if(row.kind==='request95')nativeFailure(row.id,error);else row.reject(error);}
 function remove(row){pending.delete(key(row.kind,row.id));clearTimeout(row.timer);}
 function begin(row,message){
  if(closed)throw bridgeError('iOS原生连接已关闭','native_bridge_closed','not_sent');
  if(pending.size>=MAX_PENDING)throw bridgeError('iOS原生请求过多，请稍候','native_bridge_busy','not_sent');
  const name=key(row.kind,row.id);if(pending.has(name))throw bridgeError('iOS原生请求编号重复','duplicate_native_id','not_sent');
  row.timer=setTimeout(()=>{if(pending.get(name)!==row)return;remove(row);rejectRow(row,bridgeError('iOS原生请求超时','native_timeout'));},TIMEOUT);
  pending.set(name,row);
  try{post({v:1,nonce,...message});}
  catch(error){remove(row);rejectRow(row,bridgeError(error?.message||'iOS原生请求未发送','native_post_failed','not_sent'));}
 }
 function request95(id,endpoint,route,credential,account,body){
  if(!Number.isSafeInteger(id)||id<1)throw bridgeError('iOS原生请求编号无效','invalid_native_id','not_sent');
  const row={kind:'request95',id};
  if(pending.has(key(row.kind,id)))throw bridgeError('iOS原生请求编号重复','duplicate_native_id','not_sent');
  try{
   if(!routes.has(route))throw bridgeError('iOS直连请求不支持该路由','unsupported_native_route','not_sent');
   if(typeof endpoint!=='string'||endpoint.length>1024||typeof credential!=='string'||typeof account!=='string'||
    typeof body!=='string'||body.length>MAX_BODY)throw bridgeError('iOS原生请求参数无效','invalid_native_request','not_sent');
   const parsed=JSON.parse(body);if(!parsed||typeof parsed!=='object'||Array.isArray(parsed))throw Error('请求体必须为JSON对象');
   begin(row,{kind:'request95',id,endpoint,route,credential,account,body});
  }catch(error){nativeFailure(id,error?.code?error:bridgeError(error?.message||'请求无效','invalid_native_request','not_sent'));}
 }
 function storage(operation,value){return new Promise((resolve,reject)=>{
  const id='storage:'+(++storageSerial),row={kind:'storage',id,operation,resolve,reject};
  try{
   if(operation==='record'&&(typeof value!=='string'||value.length>8192))throw bridgeError('本机凭证内容无效','invalid_storage_record','not_sent');
   begin(row,{kind:'storage',id,operation,...(operation==='record'?{body:value}:{})});
  }catch(error){reject(error);}
 });}
 function receive(message){
  if(closed||!message||message.v!==1||message.nonce!==nonce||!['request95','storage'].includes(message.kind))return false;
  const row=pending.get(key(message.kind,message.id));if(!row)return false;
  remove(row);
  if(message.ok!==true){const e=message.error;rejectRow(row,bridgeError(typeof e?.message==='string'?e.message:'iOS原生操作失败',
   typeof e?.code==='string'?e.code:'native_failure',e?.delivery==='not_sent'?'not_sent':'unknown'));return true;}
  try{
   const result=message.result;
   if(row.kind==='request95'){
    if(!result||!Number.isInteger(result.status)||result.status<100||result.status>599||typeof result.body!=='string')throw Error('iOS原生回包格式无效');
    JSON.parse(result.body);
    if(typeof root.nativeReply!=='function')throw Error('游戏回包处理尚未初始化');
    root.nativeReply(row.id,result);
   }else{
    if(['device','read'].includes(row.operation)?typeof result!=='string':typeof result!=='boolean')throw Error('iOS本机凭证回包格式无效');
    row.resolve(result);
   }
  }catch(error){rejectRow(row,bridgeError(error?.message||'iOS原生回包无效','invalid_native_reply'));}
  return true;
 }
 function dispose(reason='iOS页面已关闭'){
  if(closed)return;closed=true;for(const row of [...pending.values()]){remove(row);rejectRow(row,bridgeError(reason,'native_bridge_closed'));}
 }
 function report(body){if(closed||typeof body!=='string'||body.length>128*1024)return;
  try{JSON.parse(body);post({v:1,nonce,kind:'report',body});}catch(_){/* diagnostics never invent an action result */}}
 if(typeof root.crypto?.randomUUID!=='function'){
  if(typeof root.crypto?.getRandomValues!=='function')throw Error('此iOS运行环境缺少安全随机数');
  Object.defineProperty(root.crypto,'randomUUID',{value(){const bytes=new Uint8Array(16);root.crypto.getRandomValues(bytes);
   bytes[6]=(bytes[6]&15)|64;bytes[8]=(bytes[8]&63)|128;const h=[...bytes].map(b=>b.toString(16).padStart(2,'0')).join('');
   return h.slice(0,8)+'-'+h.slice(8,12)+'-'+h.slice(12,16)+'-'+h.slice(16,20)+'-'+h.slice(20);},configurable:false,writable:false});
 }
 const api=Object.freeze({version:1,platform:'ios',receive,dispose,storage:Object.freeze({
  device:()=>storage('device'),read:()=>storage('read'),record:value=>storage('record',value),forget:()=>storage('forget')
 }),capabilities:Object.freeze({directTCP:true,legacyRelayStream:false,synchronousOfflineStorage:false})});
 Object.defineProperty(root,'ZgNativeIOS',{value:api,writable:false,configurable:false});
 Object.defineProperty(root,'Lab',{value:Object.freeze({request95,report}),writable:false,configurable:false});
 root.addEventListener?.('pagehide',()=>dispose(),{once:true});
})(globalThis);
