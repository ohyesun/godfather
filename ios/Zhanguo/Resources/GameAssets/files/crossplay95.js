import {mergeState110} from './state110.js';
export function crossplayTransport95(raw){
 let account='',token='',state110=null,tail110=Promise.resolve(),epoch110=0,logout110=null;
 const current110=epoch=>{if(epoch!==epoch110)throw Error('登录已切换，请重新读取角色');};
 const exchange110=(route,body)=>{const epoch=epoch110,user=account,secret=token;const run=async()=>{current110(epoch);let result=await raw(route,secret,user,{...body,state110:true,baseState110:state110?.state110??0});current110(epoch);if(result.state){try{state110=mergeState110(state110,result.state);}catch{const refreshed=await raw('/mobile/state',secret,user,{state110:true,baseState110:0});current110(epoch);state110=mergeState110(null,refreshed.state);}result={...result,state:state110};}return result;};const result=tail110.then(run);tail110=result.catch(()=>{});return result;};
 return async(route,body={})=>{if(logout110&&route!=='/logout')throw Error('正在退出账号，请等待区服处理');
  if(route==='/login'||route==='/register'){
   const epoch=++epoch110,user=body.user;account=user;token='';state110=null;tail110=Promise.resolve();const zone=document.getElementById('serverZone')?.value||'';if(!zone)throw Error('\u8bf7\u5148\u9009\u62e9\u533a\u670d');if(route==='/register'){const registered=await raw('/player/register','',user,{account:user,password:body.password,qq:body.qq||'',zone});current110(epoch);return registered;}
   const login=await raw('/player/login','',user,{account:user,password:body.password,zone});current110(epoch);token=login.token;
   const result=await exchange110('/mobile/open',{});return {token,state:result.state};
  }
  if(route==='/catalogue')return raw('/mobile/catalogue','',body.account||'',body);
if(route==='/offline'){
const op=body.op;if(['status','resume','stop'].includes(op)){
const result=await raw('/mobile/offline','',body.account,body);
if(op==='resume'&&result.token){const epoch=++epoch110;account=body.account;token=result.token;state110=null;tail110=Promise.resolve();
const fresh=await exchange110('/mobile/state',{});current110(epoch);return {...result,state:fresh.state};}return result;}
if(!token)throw Error('请先进入角色');const epoch=epoch110,result=await raw('/mobile/offline',token,account,body);current110(epoch);
if(op==='start'&&result.ok===true){++epoch110;token='';state110=null;tail110=Promise.resolve();}return result;}
if(!token)throw Error('登录已过期，请重新登录');
  if(!['/state','/step','/action','/logout','/catalogue','/systems','/party','/guild','/auction','/social','/ladder','/stalls','/chat','/presence'].includes(route))throw Error('此功能的共通服接口仍在适配');
  if(route==='/logout'){if(logout110)return logout110;const secret=token,user=account,epoch=++epoch110;
state110=null;tail110=Promise.resolve();const task=Promise.resolve().then(()=>raw('/mobile/logout',secret,user,body)).then(result=>{
if(result?.ok!==true)throw Error(result?.error||'区服尚未确认退出账号，请重试退出');current110(epoch);
token='';account='';state110=null;tail110=Promise.resolve();return result;
}).finally(()=>{if(logout110===task)logout110=null;});logout110=task;return task;}
  const epoch=epoch110,result=['/state','/action'].includes(route)?await exchange110('/mobile'+route,body):await raw('/mobile'+route,token,account,body);current110(epoch);return result;
 };
}
