import * as THREE from 'three';
import {GLTFLoader} from './vendor/examples/jsm/loaders/GLTFLoader.js';
const models=new Map();
function clone(source){const copy=source.clone(true),lookup=new Map();const pair=(a,b)=>{lookup.set(a,b);for(let i=0;i<a.children.length;i++)pair(a.children[i],b.children[i]);};pair(source,copy);source.traverse(original=>{if(!original.isSkinnedMesh)return;const mesh=lookup.get(original);mesh.skeleton=original.skeleton.clone();mesh.skeleton.bones=original.skeleton.bones.map(bone=>lookup.get(bone));mesh.bindMatrix.copy(original.bindMatrix);mesh.bind(mesh.skeleton,mesh.bindMatrix);});return copy;}
export class Peer84 {
 constructor(player){this.mesh=new THREE.Group();this.mesh.position.set(player.x,0,player.z);this.name=player.name;this.disposed=false;
  const role=({1:'shennong',2:'yinyang',3:'bing'})[player.profession]??'shennong',sex=player.sex===2&&player.profession===1?'junior':player.sex===1?'female':'male',key=role+'-'+sex;
  if(!models.has(key))models.set(key,new GLTFLoader().loadAsync('characters84/'+key+'.glb').catch(e=>{models.delete(key);throw e;}));
  models.get(key).then(g=>{if(this.disposed)return;this.model=clone(g.scene);this.model.scale.setScalar(.8);this.model.rotation.y=Math.PI;this.mesh.add(this.model);this.mixer=new THREE.AnimationMixer(this.model);this.actions=Object.fromEntries(g.animations.map(c=>[c.name,this.mixer.clipAction(c)]));}).catch(e=>{this.error=e.message;});
 }
 update(player,dt){const before=this.mesh.position.clone();this.mesh.position.lerp(new THREE.Vector3(player.x,0,player.z),Math.min(1,dt*14));const dx=this.mesh.position.x-before.x,dz=this.mesh.position.z-before.z,moving=Math.hypot(dx,dz)>.003;if(moving)this.mesh.rotation.y=Math.atan2(dx,dz);this.mesh.visible=player.dead!==true&&(player.hp==null||player.hp>0);
  const action=player.weak?'Game 0051':moving?'Game 0004':'Game 0001';if(this.current!==action&&this.actions?.[action]){this.actions[this.current]?.fadeOut(.12);this.actions[action].reset().fadeIn(.12).play();this.current=action;}
  if(this.mixer){this.mixer.update(dt);this.mesh.updateMatrixWorld(true);const box=new THREE.Box3().setFromObject(this.model,true).applyMatrix4(this.mesh.matrixWorld.clone().invert()),c=box.getCenter(new THREE.Vector3());this.model.position.x-=c.x;this.model.position.y-=box.min.y;this.model.position.z-=c.z;}
 }
 dispose(){this.disposed=true;this.mixer?.stopAllAction();if(this.model)this.mixer?.uncacheRoot(this.model);}
}
