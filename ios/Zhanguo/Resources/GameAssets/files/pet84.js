import * as THREE from 'three';
import {GLTFLoader} from './vendor/examples/jsm/loaders/GLTFLoader.js';
export class Pet84 {
 constructor(){this.root=new THREE.Group();this.disposed=false;new GLTFLoader().load('companions84/feiyan.glb',g=>{if(this.disposed){this.release(g.scene);return;}this.model=g.scene;this.model.rotation.y=Math.PI;this.root.add(this.model);this.mixer=new THREE.AnimationMixer(this.model);this.actions=Object.fromEntries(g.animations.map(c=>[c.name,this.mixer.clipAction(c)]));},undefined,e=>{this.error=e.message;});}
 update(p,dt,now){this.root.visible=!!p;if(!p)return;this.root.position.set(p.x,.25,p.z);this.root.rotation.y=p.heading;const action=p.procUntil>now?'Proc flare':p.moving?'Floating follow':'Hover idle';if(this.current!==action&&this.actions?.[action]){this.actions[this.current]?.fadeOut(.15);this.actions[action].reset().fadeIn(.15).play();this.current=action;}this.mixer?.update(dt);}
 release(root){root.traverse(o=>{o.skeleton?.dispose();o.geometry?.dispose();for(const m of o.material?(Array.isArray(o.material)?o.material:[o.material]):[]){for(const t of Object.values(m))if(t?.isTexture)t.dispose();m.dispose();}});}
 dispose(){this.disposed=true;this.mixer?.stopAllAction();if(this.model)this.release(this.model);}
}
