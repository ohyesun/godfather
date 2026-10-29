/* iOS overlay boot module. It imports the unchanged game entry only after
 * real runtime checks. It never logs in, sends a game action or fakes a codec. */
function fail(error){
 const show=()=>{const panel=document.createElement('section');panel.id='iosStartupError';
  panel.style.cssText='position:fixed;inset:0;z-index:10000;background:#111710;color:#f5ddb0;display:grid;align-content:center;justify-items:center;padding:24px;text-align:center;';
  const title=document.createElement('h2');title.textContent='游戏暂时无法启动';const text=document.createElement('p');text.textContent='当前运行环境未准备好，请关闭后重新打开应用。';panel.append(title,text);document.body.append(panel);};
 if(document.body)show();else document.addEventListener('DOMContentLoaded',show,{once:true});
 console.error('[iOS startup]',error?.message||String(error));globalThis.ZgIOSStartupAudit={ready:false,code:error?.code||'runtime_unavailable',message:String(error?.message||error)};
}
async function check(){
 if(globalThis.ZgNativeIOS?.version!==1||typeof globalThis.Lab?.request95!=='function')throw Error('native bridge missing');
 if(typeof crypto?.getRandomValues!=='function'||typeof crypto.randomUUID!=='function')throw Error('secure random unavailable');
 if(new TextDecoder('gbk').decode(new Uint8Array([0xd6,0xd0,0xce,0xc4]))!=='中文')throw Error('GBK decoder unavailable');
 if(typeof CompressionStream!=='function'||typeof DecompressionStream!=='function')throw Error('zlib stream unavailable');
 const plain=new Uint8Array([0,1,2,3,127,128,254,255]);
 const packed=await new Response(new Blob([plain]).stream().pipeThrough(new CompressionStream('deflate'))).arrayBuffer();
 const decoded=new Uint8Array(await new Response(new Blob([packed]).stream().pipeThrough(new DecompressionStream('deflate'))).arrayBuffer());
 if(decoded.length!==plain.length||decoded.some((byte,i)=>byte!==plain[i]))throw Error('zlib roundtrip failed');
 const canvas=document.createElement('canvas'),gl=canvas.getContext('webgl2');if(!gl)throw Error('WebGL2 unavailable');gl.getExtension('WEBGL_lose_context')?.loseContext();
 if(typeof AbortSignal.timeout!=='function')Object.defineProperty(AbortSignal,'timeout',{value(ms){
  const controller=new AbortController();setTimeout(()=>controller.abort(new DOMException('Timeout','TimeoutError')),ms);return controller.signal;}});
 globalThis.ZgIOSStartupAudit={ready:true,gbk:true,zlib:true,webgl2:true,secureRandom:true};
 await import('./client.js');
}
void check().catch(fail);
