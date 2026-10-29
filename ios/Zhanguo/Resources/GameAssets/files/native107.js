import {playerMovementAction112} from './locomotion112.js';
import {ActionAudio107} from './action-audio107.js';
import {restoreTimeline110} from './animation110.js';
import {reactionAnimation111} from './reaction111.js';
import * as THREE from 'three';
import {GLTFLoader} from './vendor/examples/jsm/loaders/GLTFLoader.js';
import {Particle107} from './particles107.js';
const cache=new Map(),loader=new GLTFLoader();
const bones107=fetch('native107/bones.json').then(r=>r.json());
const modelAliases107=fetch('native107/model-aliases107.json').then(r=>r.json());
const timelines110=fetch('animation110.json').then(r=>r.json());
function disposeAsset(g){const geometries=new Set(),materials=new Set(),textures=new Set();g.scene.traverse(o=>{if(o.geometry)geometries.add(o.geometry);for(const m of o.material?(Array.isArray(o.material)?o.material:[o.material]):[]){materials.add(m);for(const v of Object.values(m))if(v?.isTexture)textures.add(v);}});for(const x of [...geometries,...materials,...textures])x.dispose();}
function prune(){const idle=[...cache.entries()].filter(([,v])=>v.refs===0&&v.asset).sort((a,b)=>a[1].used-b[1].used);while(idle.length>8){const [id,entry]=idle.shift();disposeAsset(entry.asset);cache.delete(id);}}
function acquire(id){let e=cache.get(id);if(!e){e={refs:0,used:0};e.promise=Promise.all([modelAliases107,timelines110]).then(async([a,t])=>{const source=a[id]??id,g=await loader.loadAsync('native107/'+source+'.glb');return restoreTimeline110(g,t[source]);}).then(g=>{e.asset=g;prune();return g;}).catch(error=>{cache.delete(id);throw error;});cache.set(id,e);}e.refs++;return e;}
export function cloneNative107(source){const copy=source.clone(true),lookup=new Map();const pair=(a,b)=>{lookup.set(a,b);a.children.forEach((c,i)=>pair(c,b.children[i]));};pair(source,copy);source.traverse(o=>{if(!o.isSkinnedMesh)return;const c=lookup.get(o);c.skeleton=o.skeleton.clone();c.skeleton.bones=o.skeleton.bones.map(b=>lookup.get(b));c.bind(c.skeleton,o.bindMatrix);});return copy;}
const clone=cloneNative107;
function nativeNpcYaw107(n){return !n.player&&typeof n.dir==='number'&&Number.isFinite(n.dir)?(180-((n.dir%360+360)%360))*Math.PI/180:null;}
// Bind only the per-instance outer Group. GLB userData and this.id describe
// shared model resources, not the runtime NPC/player identity sent to the zone.
const nativeEntityRoots107=new WeakMap();
function bindNativeEntity107(root,n={}){
 const previous=nativeEntityRoots107.get(root),id=n.id===undefined?previous?.id:n.id;
 if(!Number.isInteger(id)||id<=0||id>0xffffffff){nativeEntityRoots107.delete(root);if(root.userData)delete root.userData.nativeEntity107;return;}
 const kind=n.player===undefined?(previous?.kind??'npc'):(n.player?'player':'npc');
 const active=n.hp!==undefined||n.dead!==undefined||n.alive!==undefined?n.hp!==0&&n.dead!==true&&n.alive!==false:(previous?.active??true);
 if(previous&&previous.id===id&&previous.kind===kind&&previous.active===active)return;
 const entity=Object.freeze({id,kind,active});nativeEntityRoots107.set(root,entity);root.userData??={};root.userData.nativeEntity107=entity;
}
export function nativeEntityFromObject107(object){
 let entity=null;const visited=new Set();
 for(let node=object;node;node=node.parent){
  if(visited.has(node)||node.visible===false)return null;visited.add(node);
  const candidate=nativeEntityRoots107.get(node);
  if(!entity&&candidate){if(!candidate.active)return null;entity=candidate;}
 }
 return entity?{id:entity.id,kind:entity.kind}:null;
}
globalThis.bridgeNativeEntity107=nativeEntityFromObject107;
export class Native107{
 constructor(n,error=()=>{}){this.mesh=new THREE.Group();bindNativeEntity107(this.mesh,n);this.mesh.position.set(n.x,0,n.z);const yaw=nativeNpcYaw107(n);if(yaw!==null)this.mesh.rotation.y=yaw;this.disposed=false;this.id=n.model;this.weaponType111=n.weaponType??0;this.actionAudio107=new ActionAudio107(this.id,this.mesh);this.entry=acquire(this.id);this.error=error;
  this.entry.promise.then(g=>{if(this.disposed)return;this.model=clone(g.scene);this.mesh.add(this.model);this.model.rotation.y=Math.PI;this.model.scale.setScalar((n.scale??1)*.8);this.mixer=new THREE.AnimationMixer(this.model);this.actions=Object.fromEntries(g.animations.map(c=>[c.name,this.mixer.clipAction(c)]));this.variants(n.parts,n.visibleMeshes,n.textures);this.attachments(n.weapons).catch(this.error);this.effects107=(n.effects??[]).filter(e=>e.kind===2).map(e=>new Particle107(e.resource,this.model.getObjectByName('Bone_'+e.bone)??this.model,{error:this.error}));this.play(1);this.model.updateMatrixWorld(true);window.nativeReport107??={loaded:{},errors:[],details:{}};window.nativeReport107.details??={};let renderableCount=0,visibleCount=0,frustumCulledCount=0;this.model.traverse(o=>{if(!(o.isMesh||o.isSkinnedMesh))return;renderableCount++;if(o.visible)visibleCount++;if(o.frustumCulled){frustumCulledCount++;o.frustumCulled=false;}});this.model.visible=true;this.mesh.visible=true;window.nativeReport107.details[this.id]={meshVisible:this.mesh.visible,modelVisible:this.model.visible,renderableCount,visibleCount,frustumCulledCount,scale:this.model.scale.toArray(),position:this.mesh.position.toArray()};window.nativeReport107.loaded[this.id]=true;}).catch(e=>{window.nativeReport107??={loaded:{},errors:[]};window.nativeReport107.errors.push({model:this.id,error:e.message});error(e);});
  }
  async attachments(weapons=[]){
   const bones=await bones107;if(this.disposed)return;this.weaponEntries=[];
   for(const weapon of weapons){
    const index=bones[this.id]?.indexOf(weapon.hand);if(!(index>=0))continue;
    const bone=this.model.getObjectByName('Bone_'+index);if(!bone)continue;
    const entry=acquire(weapon.model);this.weaponEntries.push(entry);
    try{const g=await entry.promise;if(this.disposed)return;bone.add(clone(g.scene));if(weapon.particle){this.weaponParticles107??=[];this.weaponParticles107.push(new Particle107(weapon.particle,bone,{error:this.error}));}}catch(e){this.error(e);}
   }
  }
 variants(parts={},visible,textures={}){this.ownedMaterials=[];this.ownedTextures=[];this.model?.traverse(o=>{if(o.userData.part!==undefined){o.visible=visible?visible.includes(o.name):o.userData.variant===(parts[o.userData.part]??o.userData.defaultVariant);const tid=textures[o.name];if(tid)o.traverse(m=>{if(!m.material)return;m.material=m.material.clone();this.ownedMaterials.push(m.material);const t=new THREE.TextureLoader().load('native107/textures/'+tid+'.png',()=>{if(this.disposed)t.dispose();},undefined,this.error);t.flipY=false;t.colorSpace=THREE.SRGBColorSpace;m.material.map=t;this.ownedTextures.push(t);});}});}
  play(id,once=false){const name=`Game ${String(id).padStart(4,'0')}`;if(!this.actions?.[name])return false;if(this.current===name&&!once)return true;this.actions[this.current]?.fadeOut(.1);const a=this.actions[name];a.setLoop(once?THREE.LoopOnce:THREE.LoopRepeat,once?1:Infinity);a.clampWhenFinished=once;a.reset().fadeIn(.1).play();this.actionAudio107.play(id,a.getClip().duration,once);this.current=name;return true;}
  update(n,dt){bindNativeEntity107(this.mesh,n);const now=performance.now(),p=this.mesh.position,dx=n.x-p.x,dz=n.z-p.z;if(Math.hypot(dx,dz)>.04)this.movingUntil110=now+150;const moving=n.moving??(now<(this.movingUntil110??0));const f=1-Math.exp(-14*dt);p.x+=dx*f;p.z+=dz*f;if(moving&&Math.hypot(dx,dz)>.025)this.mesh.rotation.y=Math.atan2(dx,dz);this.mesh.visible=true;
   const reacting111=reactionAnimation111(this,n,now);if(!n.stalling&&!n.sitting&&!reacting111&&n.castAt&&n.castAt!==this.castAt){this.castAt=n.castAt;const weaponAction={101:201,104:211,105:221,106:241,107:64,131:261,132:231,133:231,134:251,135:64}[this.weaponType111]??56,animation=n.animation>=52||n.animation===20?n.animation:weaponAction;const continuing=n.continuous111&&this.continuous111&&now<(this.attackUntil??0);if(!continuing)this.play(animation,!n.continuous111)||this.play(weaponAction,!n.continuous111)||this.play(56,true)||this.play(57,true)||this.play(67,true);this.continuous111=!!n.continuous111;this.attackUntil=now+Math.max(150,(this.actions?.[this.current]?.getClip().duration??.65)*1000);if(Number.isFinite(n.facing110))this.mesh.rotation.y=n.facing110;}
   if(!moving&&!reacting111&&now>=(this.attackUntil??0)){const yaw=nativeNpcYaw107(n);if(yaw!==null)this.mesh.rotation.y=yaw;}
  if(n.hp===0){if(!this.dead107){this.play(103,true)||this.play(113,true);this.dead107=true;}}else{if(this.dead107){this.dead107=false;this.play(1);}if(n.stalling||n.sitting)this.play(7);else if(n.weak)this.play(n.player?playerMovementAction112(n,moving):79)||this.play(1);else if(!reacting111&&performance.now()>(this.attackUntil??0)){if(moving){this.play(n.player?playerMovementAction112(n,true):(n.sprinting?5:4))||this.play(5)||this.play(1);}else this.play(n.player?playerMovementAction112(n,false):1);}}
   this.mixer?.update(dt);this.actionAudio107.update(dt);for(const effect of [...this.effects107??[],...this.weaponParticles107??[]])effect.update(dt);
 }
  dispose(){if(this.disposed)return;this.disposed=true;bindNativeEntity107(this.mesh,{id:0});this.actionAudio107.dispose();for(const effect of [...this.effects107??[],...this.weaponParticles107??[]])effect.dispose();this.mixer?.stopAllAction();if(this.model){this.mixer?.uncacheRoot(this.model);this.model.traverse(o=>{if(o.isSkinnedMesh)o.skeleton.dispose();});}for(const x of [...this.ownedMaterials??[],...this.ownedTextures??[]])x.dispose();for(const entry of this.weaponEntries??[]){entry.refs--;entry.used=performance.now();}this.entry.refs--;this.entry.used=performance.now();prune();}
}
