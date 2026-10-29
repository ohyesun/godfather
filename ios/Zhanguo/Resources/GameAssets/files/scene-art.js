import * as THREE from 'three';
import {setFootstepScene107} from './action-audio107.js';
import {nativePoint,spriteVertex,decodeChunk} from './scene-data.js';

const SIZE=1024,IMAGE_BUDGET=16*1024*1024;
function packVisible(ids,sprites){
 const pages=[],slots=new Map();
 for(const id of [...ids].sort((a,b)=>sprites[b][1]-sprites[a][1]||sprites[b][0]-sprites[a][0])){
  const [w,h]=sprites[id],rw=w+4,rh=h+4;let best=null;
  for(const page of pages)for(let i=0;i<page.free.length;i++){
   const [x,y,fw,fh]=page.free[i];if(fw<rw||fh<rh)continue;
   const waste=fw*fh-rw*rh;if(!best||waste<best.waste)best={page,i,waste};
  }
  if(!best){const canvas=document.createElement('canvas');canvas.width=canvas.height=SIZE;const page={id:pages.length,canvas,ctx:canvas.getContext('2d'),free:[[0,0,SIZE,SIZE]]};pages.push(page);best={page,i:0};}
  const {page,i}=best,[x,y,fw,fh]=page.free.splice(i,1)[0];if(fw>rw)page.free.push([x+rw,y,fw-rw,rh]);if(fh>rh)page.free.push([x,y+rh,fw,fh-rh]);slots.set(id,{page:page.id,x:x+2,y:y+2,w,h});
 }
 return {pages,slots};
}

// Native source crops are packed per viewport. Uploading whole source libraries
// would use hundreds of MiB even when only a small part of each page is visible.
export class SceneArt{
  constructor(scene,onError,maxTextureSize=2048,renderer=null){
   this.renderer110=renderer;this.tiles110=new Map();this.atlases110=[];this.uploads110=0;this.cacheHits110=0;
   this.scene=scene;this.onError=onError;this.maxAtlas109=Math.min(4096,maxTextureSize);this.group=new THREE.Group();scene.add(this.group);
  this.images=new Map();this.chunks=new Map();this.serial=0;this.map=null;this.visibleKey='';this.runtimeTextures=[];
  this.report=window.sceneReport={version:51,status:'idle',map:null,scene:null,pages:0,placements:0,errors:[]};
  this.catalogue=this.json('scenes/catalogue.json').then(c=>{if(c.version!==51||!c.spriteRoot)throw Error('场景资源版本不匹配');this.catalog=c;return c;});this.catalogue.catch(e=>this.fail(e));
 }
 async json(url){const r=await fetch(url);if(!r.ok)throw Error('场景资源读取失败：'+url);return r.json();}
 fail(e){const message=e.message??String(e);this.report.errors.push(message);this.report.status='error';this.onError(new Error(message));}
 clearMeshes(){
  for(const mesh of [...this.group.children]){this.group.remove(mesh);mesh.geometry.dispose();if(!mesh.material.userData.cached110)mesh.material.dispose();}
  for(const texture of this.runtimeTextures){if(!texture.userData.cached110){texture.dispose();texture.image.width=texture.image.height=1;}}this.runtimeTextures=[];
 }
 clear(){this.serial++;this.map=null;this.visibleKey='';this.chunks.clear();this.clearMeshes();this.disposeCache110();this.report.status='idle';this.report.map=null;this.trim(new Set());}
  disposeCache110(){for(const e of [...this.tiles110.values(),...this.atlases110])for(const t of e.textures??[e.texture]){t.userData.material110?.dispose();t.dispose();t.image.width=t.image.height=1;}this.tiles110.clear();this.atlases110=[];}
  texture110(canvas){const t=new THREE.CanvasTexture(canvas);t.colorSpace=THREE.SRGBColorSpace;t.minFilter=t.magFilter=THREE.LinearFilter;t.generateMipmaps=false;t.userData.cached110=true;return t;}
  async upload110(t){if(t.userData.uploaded110)return;await this.yield114();if(t.image.width<=1)return;this.renderer110?.initTexture(t);t.userData.uploaded110=true;this.uploads110++;}
  trimCache110(active){const entries=[...this.tiles110.entries()].map(([key,e])=>({key,e,kind:0})).concat(this.atlases110.map(e=>({e,kind:1})));let bytes=entries.reduce((n,{e})=>n+(e.textures??[e.texture]).reduce((m,t)=>m+t.image.width*t.image.height*4,0),0);for(const {key,e,kind} of entries.sort((a,b)=>a.e.used-b.e.used)){if(bytes<=128*1024*1024)break;const ts=e.textures??[e.texture];if(ts.some(t=>active.has(t)))continue;for(const t of ts){bytes-=t.image.width*t.image.height*4;t.userData.material110?.dispose();t.dispose();t.image.width=t.image.height=1;}if(kind)this.atlases110=this.atlases110.filter(x=>x!==e);else this.tiles110.delete(key);}this.report.residentTextureMiB=Math.round(bytes/1048576);}
 async select(id){
  if(this.mapId===id&&this.map)return;
   this.clear();setFootstepScene107(null);this.mapId=id;const ticket=this.serial;this.report.map=id;this.report.status='loading';
  try{
   const c=await this.catalogue;if(ticket!==this.serial)return;const entry=c.maps[id];if(!entry)throw Error('这张地图的原版场景尚未转换');
   const map=await this.json(`scenes/${entry.scene}/map.json`);if(ticket!==this.serial)return;
    this.map={...map,folder:entry.scene};setFootstepScene107(entry.scene);this.report.scene=entry.scene;this.report.sourceSha256=map.sourceSha256;this.report.status='streaming';
  }catch(e){if(ticket===this.serial)this.fail(e);}
 }
 async chunk(key,ticket){
  if(this.chunks.has(key))return this.chunks.get(key);const filename=this.map.chunks[key];if(!filename)return [];
  const folder=this.map.folder,promise=fetch(`scenes/${folder}/${filename}`).then(r=>{if(!r.ok)throw Error('场景分块读取失败');return r.arrayBuffer();}).then(b=>decodeChunk(b,this.catalog.sprites.length));this.chunks.set(key,promise);
  try{return await promise;}catch(e){if(ticket===this.serial)this.chunks.delete(key);throw e;}
 }
 image(id){
  let item=this.images.get(id);if(item){item.used=performance.now();return item.promise;}
  const [w,h]=this.catalog.sprites[id];item={used:performance.now(),pixels:w*h,image:null};this.images.set(id,item);
  item.promise=new Promise((resolve,reject)=>{const image=new Image();image.onload=()=>{item.image=image;resolve(image);};image.onerror=()=>{this.images.delete(id);reject(Error('场景贴图读取失败：'+id));};image.src=`scenes/${this.catalog.spriteRoot}/${id}.webp`;});return item.promise;
 }
 trim(used){
  let pixels=[...this.images.values()].reduce((n,e)=>n+(e.image?e.pixels:0),0);
  const idle=[...this.images].filter(([id,item])=>!used.has(id)&&item.image).sort((a,b)=>a[1].used-b[1].used);
  while(pixels>IMAGE_BUDGET&&idle.length){const [id,item]=idle.shift();pixels-=item.pixels;this.images.delete(id);}
  this.report.cachedImages=this.images.size;this.report.decodedImageMiB=Math.round(pixels*4/1048576);
 }
 update(x,z,width,height,pixelsPerUnit){
  if(!this.map)return;const p=nativePoint(x,z),hw=width/pixelsPerUnit*32,hh=height/pixelsPerUnit*32;
  const box={l:Math.floor((p.x-hw-320)/SIZE)*SIZE,r:Math.ceil((p.x+hw+320)/SIZE)*SIZE,t:Math.floor((p.y-hh-320)/SIZE)*SIZE,b:Math.ceil((p.y+hh+320)/SIZE)*SIZE};
  const key=[this.mapId,box.l,box.r,box.t,box.b,pixelsPerUnit].join(':');
  if(key===this.visibleKey||this.building||performance.now()-(this.builtAt109??0)<350)return;this.builtAt109=performance.now();this.visibleKey=key;this.building=true;const ticket=this.serial;
  this.build(box,ticket,key).catch(e=>{if(ticket===this.serial&&key===this.visibleKey)this.fail(e);}).finally(()=>{this.building=false;});
 }
 async build(box,ticket,key){
  let pendingPacked114=null,pendingObjectTextures114=null,pendingAtlas114=null,completed114=false;
  const pendingGround114=[];
  try{
  const keys=[],size=this.map.chunkSize;
  for(let y=Math.floor(box.t/size);y<=Math.floor(box.b/size);y++)for(let x=Math.floor(box.l/size);x<=Math.floor(box.r/size);x++)keys.push(`${x},${y}`);
  const pieces=await Promise.all(keys.map(k=>this.chunk(k,ticket)));if(ticket!==this.serial)return;
  const unique=new Map();for(const rows of pieces)for(const row of rows){const [w,h]=this.catalog.sprites[row[0]];if(row[1]+w>=box.l&&row[1]<=box.r&&row[2]+h>=box.t&&row[2]<=box.b)unique.set(row[5],row);}
  const ids=new Set([...unique.values()].map(r=>r[0])),objectIds=new Set([...unique.values()].filter(r=>r[4]===2).map(r=>r[0])),atlasHit=this.atlases110.find(a=>a.mapId===this.mapId&&[...objectIds].every(id=>a.packed.slots.has(id))),packed=atlasHit?.packed??packVisible(objectIds,this.catalog.sprites),queue=[...ids];if(atlasHit)this.cacheHits110++;pendingPacked114=atlasHit?null:packed;
  // Only a few images decode at once, and no source atlas is uploaded to WebGL.
  await this.loadImages114(queue,packed,atlasHit,ticket);
  if(ticket!==this.serial){if(!atlasHit)for(const p of packed.pages)p.canvas.width=p.canvas.height=1;return;}
   // Preserve native painter order while reducing hundreds of alternating
   // atlas draw calls to one. Only the visible sprites are combined, capped
   // by the device texture limit and 4096 px (64 MiB at the maximum size).
   const cols=Math.ceil(Math.sqrt(packed.pages.length)),rows=Math.ceil(packed.pages.length/Math.max(1,cols));
   let objectWidth=atlasHit?.width??SIZE,objectHeight=atlasHit?.height??SIZE;
   if(!atlasHit&&packed.pages.length>1&&cols*SIZE<=this.maxAtlas109&&rows*SIZE<=this.maxAtlas109){
    const atlas=pendingAtlas114=document.createElement('canvas');atlas.width=cols*SIZE;atlas.height=rows*SIZE;const ctx=atlas.getContext('2d');
    for(const p of packed.pages){ctx.drawImage(p.canvas,p.id%cols*SIZE,Math.floor(p.id/cols)*SIZE);await this.yield114();if(ticket!==this.serial){atlas.width=atlas.height=1;for(const q of packed.pages)q.canvas.width=q.canvas.height=1;return;}}
    for(const slot of packed.slots.values()){slot.x+=slot.page%cols*SIZE;slot.y+=Math.floor(slot.page/cols)*SIZE;slot.page=0;}
    for(const p of packed.pages)p.canvas.width=p.canvas.height=1;packed.pages=[{id:0,canvas:atlas}];objectWidth=atlas.width;objectHeight=atlas.height;pendingAtlas114=null;
   }
  // Composite ground in the original layer/record order. Alpha-blended road
  // cuts cannot be reordered by atlas without leaving rectangular seams.
  const grounds=[...unique.values()].filter(r=>r[4]!==2).sort((a,b)=>a[4]-b[4]||a[5]-b[5]),groundPages=[];
  for(let gy=Math.floor(box.t/SIZE);gy<Math.ceil(box.b/SIZE);gy++)for(let gx=Math.floor(box.l/SIZE);gx<Math.ceil(box.r/SIZE);gx++){
   const key110=this.mapId+":"+gx+","+gy,hit110=this.tiles110.get(key110);if(hit110){hit110.used=performance.now();groundPages.push(hit110);this.cacheHits110++;continue;}
   const canvas=document.createElement('canvas');canvas.width=canvas.height=SIZE;const ctx=canvas.getContext('2d'),x=gx*SIZE,y=gy*SIZE;
   for(const row of grounds){const image=this.images.get(row[0]).image;if(row[1]+image.width<x||row[1]>x+SIZE||row[2]+image.height<y||row[2]>y+SIZE)continue;ctx.drawImage(image,row[1]-x,row[2]-y);}
   const entry110={canvas,x,y,texture:this.texture110(canvas),used:performance.now()};this.tiles110.set(key110,entry110);pendingGround114.push([key110,entry110]);groundPages.push(entry110);await this.upload110(entry110.texture);
   await this.yield114();if(ticket!==this.serial){if(!atlasHit)for(const p of packed.pages)p.canvas.width=p.canvas.height=1;return;}
  }
  const batches=[];
  // Painter order between transparent building cuts; depth-test against the
  // character still decides whether it is in front of or behind each cut.
  for(const row of [...unique.values()].filter(r=>r[4]===2).sort((a,b)=>a[3]-b[3]||a[5]-b[5])){
   const [sid,x,y,depth,layer,uid]=row,{page,x:ax,y:ay,w,h}=packed.slots.get(sid);let b=batches[batches.length-1];
   if(!b||b.page!==page){b={page,positions:[],uv:[],indices:[]};batches.push(b);}
   const offset=b.positions.length/3;
   for(const [px,py]of [[x,y],[x+w,y],[x+w,y+h],[x,y+h]]){const v=spriteVertex(px,py,depth,layer===2);if(layer!==2)v[1]=-.03+(layer===1?.005+uid*1e-8:0);b.positions.push(...v);}
    b.uv.push(ax/objectWidth,1-ay/objectHeight,(ax+w)/objectWidth,1-ay/objectHeight,(ax+w)/objectWidth,1-(ay+h)/objectHeight,ax/objectWidth,1-(ay+h)/objectHeight);b.indices.push(offset,offset+3,offset+2,offset,offset+2,offset+1);
  }
  const objectTextures=atlasHit?.textures??packed.pages.map(p=>this.texture110(p.canvas));pendingObjectTextures114=atlasHit?null:objectTextures;for(const t of objectTextures){await this.upload110(t);if(ticket!==this.serial)return;}if(atlasHit)atlasHit.used=performance.now();else this.atlases110.push({mapId:this.mapId,packed,textures:objectTextures,width:objectWidth,height:objectHeight,used:performance.now()});pendingPacked114=null;pendingObjectTextures114=null;this.clearMeshes();this.runtimeTextures=[...objectTextures,...groundPages.map(p=>p.texture)];
  for(const p of groundPages){
   // Native cut anchors are sorting baselines, not geometry feet: shadows and
   // ledges may extend below them. Ground must not clip those lower pixels.
   const g=new THREE.PlaneGeometry(SIZE/64,SIZE/32),material=p.texture.userData.material110??(p.texture.userData.material110=new THREE.MeshBasicMaterial({map:p.texture,alphaTest:.001,toneMapped:false,depthWrite:false,depthTest:false})),cached110=(material.userData.cached110=true),mesh=new THREE.Mesh(g,material);mesh.renderOrder=-100;mesh.rotation.x=-Math.PI/2;mesh.position.set((p.x+SIZE/2)/64,-.03,(p.y+SIZE/2)/32);this.group.add(mesh);
  }
  for(const [order,b]of batches.entries()){
   const g=new THREE.BufferGeometry();g.setAttribute('position',new THREE.Float32BufferAttribute(b.positions,3));g.setAttribute('uv',new THREE.Float32BufferAttribute(b.uv,2));g.setIndex(b.indices);g.computeBoundingSphere();
   const texture110=objectTextures[b.page],material=texture110.userData.material110??(texture110.userData.material110=new THREE.MeshBasicMaterial({map:texture110,side:THREE.DoubleSide,alphaTest:.01,transparent:true,depthWrite:false,toneMapped:false})),cached110=(material.userData.cached110=true),mesh=new THREE.Mesh(g,material);mesh.renderOrder=2+order;this.group.add(mesh);
  }
  const keep=new Set(keys.map(k=>this.mapId+":"+k));for(const k of this.chunks.keys())if(!keep.has(k))this.chunks.delete(k);this.trim(ids);this.trimCache110(new Set(this.runtimeTextures));
   Object.assign(this.report,{status:'ready',uploads110:this.uploads110,cacheHits110:this.cacheHits110,pages:this.runtimeTextures.length,textureMiB:Math.round(this.runtimeTextures.reduce((n,t)=>n+t.image.width*t.image.height*4,0)/1048576),placements:unique.size,chunks:keys.length,drawCalls:batches.length+groundPages.length});completed114=true;
  }finally{
   if(pendingObjectTextures114)for(const texture of pendingObjectTextures114){texture.dispose();texture.image.width=texture.image.height=1;}
   if(pendingPacked114)for(const p of pendingPacked114.pages)p.canvas.width=p.canvas.height=1;
   if(pendingAtlas114)pendingAtlas114.width=pendingAtlas114.height=1;
   for(const [key,entry]of pendingGround114)if(!entry.texture.userData.uploaded110){
    if(this.tiles110.get(key)===entry)this.tiles110.delete(key);
    entry.texture.dispose();entry.canvas.width=entry.canvas.height=1;
   }
   if(!completed114&&ticket===this.serial){this.trim(new Set());this.trimCache110(new Set(this.runtimeTextures));}
  }
 }
}

// Stream the original scene assets. No substitute pixels or game-state changes.
const SCENE_READ_TIMEOUT114=12000;
async function sceneRead114(url,read){
 const controller=new AbortController();let timer;
 try{return await Promise.race([
  fetch(url,{signal:controller.signal}).then(r=>{
   if(!r.ok){const e=Error('场景资源读取失败：'+url);e.scenePermanent114=r.status>=400&&r.status<500&&r.status!==408&&r.status!==429;throw e;}
   return read(r);
  }),
  new Promise((_,reject)=>{timer=setTimeout(()=>{controller.abort();reject(Error('场景资源读取超时：'+url));},SCENE_READ_TIMEOUT114);})
 ]);}finally{clearTimeout(timer);}
}
function installSceneStreaming114(SceneArt){
 const proto=SceneArt.prototype,originalClear=proto.clear;
 proto.json=function(url){return sceneRead114(url,r=>r.json());};
 proto.clear=function(){
  // An earlier scene can still be waiting for an image. Its finally block must
  // neither block nor unlock a build owned by the newly selected scene.
  this.buildOwner114=null;this.building=false;this.request114=null;this.pendingMap114=null;this.mapLoading114=false;
  clearTimeout(this.selectTimer114);this.selectTimer114=null;
  this.pause114=null;this.workAt114=performance.now();
  this.retryAt114=0;this.failures114=0;this.lastFailure114='';
  originalClear.call(this);
 };
 proto.select=async function(id){
  if(this.pendingMap114===id)return;
  if(this.mapId===id&&this.map&&!this.mapLoading114)return;
  const failures=this.mapId===id?(this.selectFailures114??0):0;
  // Invalidate the old build while retaining its committed meshes as a visual
  // fallback. The new map replaces them only when its next viewport commits.
  this.buildOwner114=null;this.building=false;this.request114=null;this.serial++;
  clearTimeout(this.selectTimer114);this.selectTimer114=null;this.pause114=null;
  this.pendingMap114=id;this.mapLoading114=true;this.mapId=id;
  const ticket=this.serial;this.report.map=id;this.report.status='loading';
  try{
   if(!this.catalogue)this.catalogue=this.json('scenes/catalogue.json').then(c=>{
    if(c.version!==51||!c.spriteRoot){const e=Error('场景资源版本不匹配');e.scenePermanent114=true;throw e;}
    this.catalog=c;return c;
   });
   const c=await this.catalogue;if(ticket!==this.serial)return;
   const entry=c.maps[id];
   if(!entry){const e=Error('指定地图的原版场景尚未转换');e.scenePermanent114=true;throw e;}
   const map=await this.json(`scenes/${entry.scene}/map.json`);if(ticket!==this.serial)return;
   this.map={...map,folder:entry.scene};this.selectFailures114=0;this.pendingMap114=null;this.mapLoading114=false;
   this.chunks.clear();this.visibleKey='';
   setFootstepScene107(entry.scene);this.report.scene=entry.scene;
   this.report.sourceSha256=map.sourceSha256;this.report.status='streaming';
  }catch(e){
   if(ticket!==this.serial)return;
   if(!this.catalog)this.catalogue=null;
   this.selectFailures114=failures+1;
   if(!failures)this.fail(e);
   if(!e.scenePermanent114)this.selectTimer114=setTimeout(()=>{
    this.selectTimer114=null;
    if(ticket===this.serial&&this.mapId===id&&this.pendingMap114===id){this.pendingMap114=null;this.select(id);}
   },Math.min(4000,500*2**Math.min(3,failures)));
  }
 };
 proto.chunk=function(key,ticket){
  if(this.chunks.has(key))return this.chunks.get(key);
  const filename=this.map.chunks[key];if(!filename)return Promise.resolve([]);
  const folder=this.map.folder;
  const promise=sceneRead114(`scenes/${folder}/${filename}`,r=>r.arrayBuffer())
   .then(b=>decodeChunk(b,this.catalog.sprites.length)).catch(e=>{
    if(ticket===this.serial&&this.chunks.get(key)===promise)this.chunks.delete(key);
    throw e;
   });
  this.chunks.set(key,promise);return promise;
 };
 proto.image=function(id){
  let item=this.images.get(id);if(item){item.used=performance.now();return item.promise;}
  const [w,h]=this.catalog.sprites[id];
  item={used:performance.now(),pixels:w*h,image:null};this.images.set(id,item);
  item.promise=new Promise((resolve,reject)=>{
   const image=new Image();let timer,settled=false;
   const done=(error)=>{
    if(settled)return;settled=true;clearTimeout(timer);image.onload=image.onerror=null;
    if(error){if(this.images.get(id)===item)this.images.delete(id);image.src='';reject(error);}
    else{item.image=image;resolve(image);}
   };
   image.onload=()=>done();image.onerror=()=>done(Error('场景贴图读取失败：'+id));
   timer=setTimeout(()=>done(Error('场景贴图读取超时：'+id)),SCENE_READ_TIMEOUT114);
   image.src=`scenes/${this.catalog.spriteRoot}/${id}.webp`;
  });return item.promise;
 };
 proto.yield114=async function(){
  // Cached data needs no mandatory animation frame. Yield after a small actual
  // work budget; concurrent decode workers share that one frame boundary.
  const now=performance.now();this.workAt114??=now;
  if(!this.pause114&&now-this.workAt114<6)return;
  if(!this.pause114){
   const pause=new Promise(requestAnimationFrame).then(()=>{
    if(this.pause114!==pause)return;
    this.workAt114=performance.now();this.pause114=null;
   });
   this.pause114=pause;
   if(this.buildOwner114)this.buildOwner114.stats.yields++;
  }
  await this.pause114;
 };
 proto.loadImages114=async function(queue,packed,atlasHit,ticket){
  let cursor=0,error=null,active=0;
  const stats=this.buildOwner114?.ticket===ticket?this.buildOwner114.stats:null;
  if(stats)stats.images=queue.length;
  const worker=async()=>{
   while(!error&&ticket===this.serial&&cursor<queue.length){
    const id=queue[cursor++];
    try{
     active++;if(stats){stats.maxImageWorkers=Math.max(stats.maxImageWorkers,active);if(this.images.get(id)?.image)stats.cachedImages++;else stats.imageReads++;}
     const image=await this.image(id);if(ticket!==this.serial)return;
     const s=packed.slots.get(id);
     if(!atlasHit&&s){
      // Several decode lanes may resume in the same microtask turn. Recheck
      // the budget after every shared frame before doing another crop.
      this.workAt114??=performance.now();
      while(this.pause114||performance.now()-this.workAt114>=6){
       await this.yield114();if(ticket!==this.serial)return;
      }
      const ctx=packed.pages[s.page].ctx,{x,y,w,h}=s;
      ctx.drawImage(image,x,y);ctx.drawImage(image,0,0,1,h,x-2,y,2,h);
      ctx.drawImage(image,w-1,0,1,h,x+w,y,2,h);
      ctx.drawImage(image,0,0,w,1,x,y-2,w,2);ctx.drawImage(image,0,h-1,w,1,x,y+h,w,2);
     }
     await this.yield114();
    }catch(e){error=e;}finally{active--;}
   }
  };
  // One slow sprite no longer holds the other seven decode lanes idle.
  await Promise.all(Array.from({length:Math.min(8,queue.length)},worker));
  if(error)throw error;
 };
 proto.update=function(x,z,width,height,pixelsPerUnit){
  if(!this.map||this.mapLoading114)return;
  const p=nativePoint(x,z),hw=width/pixelsPerUnit*32,hh=height/pixelsPerUnit*32;
  const box={l:Math.floor((p.x-hw-320)/SIZE)*SIZE,r:Math.ceil((p.x+hw+320)/SIZE)*SIZE,
   t:Math.floor((p.y-hh-320)/SIZE)*SIZE,b:Math.ceil((p.y+hh+320)/SIZE)*SIZE};
  const key=[this.mapId,box.l,box.r,box.t,box.b,pixelsPerUnit].join(':');
  const requestedAt=this.request114?.key===key?this.request114.requestedAt:performance.now();
  this.request114={x,z,width,height,pixelsPerUnit,key,requestedAt};
  if(this.buildOwner114)this.buildOwner114.stats.queuedViewports=key!==this.buildOwner114.key?1:0;
  if(key===this.visibleKey||this.building||performance.now()<(this.retryAt114??0))return;
  const owner={ticket:this.serial,key,started:performance.now(),stats:{buildMs:null,queueMs:Math.round(performance.now()-requestedAt),images:0,imageReads:0,cachedImages:0,maxImageWorkers:0,yields:0,queuedViewports:0}};
  this.buildOwner114=owner;this.building=true;this.report.stream114=owner.stats;
  this.builtAt109=performance.now();this.workAt114=this.builtAt109;
  this.report.status='streaming';
  Promise.resolve().then(()=>{
   if(this.buildOwner114===owner&&this.serial===owner.ticket)return this.build(box,owner.ticket,key);
  }).then(()=>{
   if(this.buildOwner114!==owner||this.serial!==owner.ticket)return;
   owner.stats.buildMs=Math.round(performance.now()-owner.started);
   // A viewport is complete only after its real ground and objects are ready.
   this.visibleKey=key;this.retryAt114=0;this.failures114=0;this.lastFailure114='';
  }).catch(e=>{
   if(this.buildOwner114!==owner||this.serial!==owner.ticket)return;
   owner.stats.buildMs=Math.round(performance.now()-owner.started);owner.stats.failed=true;
   this.failures114=(this.failures114??0)+1;
   this.retryAt114=performance.now()+Math.min(4000,500*2**Math.min(3,this.failures114-1));
   if(this.lastFailure114!==e.message){this.lastFailure114=e.message;this.fail(e);}
  }).finally(()=>{
   if(this.buildOwner114!==owner)return;
   this.buildOwner114=null;this.building=false;
   const next=this.request114;
   if(next&&next.key!==this.visibleKey&&performance.now()>=(this.retryAt114??0))
    this.update(next.x,next.z,next.width,next.height,next.pixelsPerUnit);
  });
 };
}
installSceneStreaming114(SceneArt);
