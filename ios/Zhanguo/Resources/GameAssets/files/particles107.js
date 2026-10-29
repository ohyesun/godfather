import * as THREE from 'three';
const database=fetch('native107/particles107.json').then(r=>{if(!r.ok)throw Error('特效资源读取失败');return r.json();});
const loader=new THREE.TextureLoader(),textures=new Map();
const bits=new DataView(new ArrayBuffer(4));
const float=w=>{bits.setUint32(0,w,true);return bits.getFloat32(0,true);};
const mix=(a,b,t)=>a+(b-a)*t;
function frame(rows,index,time,fallback,color=false,integer=false){
 const r=rows[index-1];if(!r)return fallback;
 let i=1;while(i<5&&r[6+i]<time)i++;const t=Math.max(0,Math.min(1,(time-r[5+i])/Math.max(1,r[6+i]-r[5+i])));
 if(color)return [16,8,0].map(shift=>mix((r[i-1]>>>shift)&255,(r[i]>>>shift)&255,t)/255);
 return mix(integer?r[i-1]:float(r[i-1]),integer?r[i]:float(r[i]),t);
}
function pruneTextures107(){const idle=[...textures.entries()].filter(([,e])=>!e.refs&&e.ready).sort((a,b)=>a[1].used-b[1].used);let bytes=idle.reduce((n,[,e])=>n+e.bytes,0);while(idle.length>24||bytes>32*1024*1024){const [id,e]=idle.shift();bytes-=e.bytes;e.texture.dispose();textures.delete(id);}}
function texture(id,error){let e=textures.get(id);if(!e){e={refs:0,used:performance.now(),ready:false,bytes:0};e.texture=loader.load('native107/textures/'+id+'.png',t=>{e.ready=true;e.bytes=(t.image?.width??0)*(t.image?.height??0)*4;pruneTextures107();},undefined,error);e.texture.colorSpace=THREE.SRGBColorSpace;e.texture.wrapS=e.texture.wrapT=THREE.RepeatWrapping;textures.set(id,e);}e.refs++;return e;}
const vertex=`attribute vec3 center;attribute vec2 extent;attribute vec4 tint;attribute float spin;attribute vec4 tile;attribute vec3 direction;attribute vec3 velocity;uniform int face;
varying vec2 vUv;varying vec4 vTint;
vec3 safeUnit(vec3 v,vec3 fallback){return dot(v,v)>0.000001?normalize(v):fallback;}
void main(){vec4 p=modelViewMatrix*vec4(center,1.0);vec2 q=position.xy*extent*vec2(length(modelMatrix[0].xyz),length(modelMatrix[1].xyz));vec3 bx=vec3(1.,0.,0.),by=vec3(0.,1.,0.),bz=vec3(0.,0.,1.);
if(face==1){bz=safeUnit(mat3(modelViewMatrix)*direction,bz);vec3 up=mat3(viewMatrix)*vec3(0.,0.,1.);bx=cross(up,bz);if(dot(bx,bx)<0.000001)bx=cross(vec3(0.,1.,0.),bz);bx=safeUnit(bx,vec3(1.,0.,0.));by=cross(bz,bx);}
if(face==2){by=safeUnit(mat3(modelViewMatrix)*velocity,by);bx=safeUnit(cross(bz,by),bx);bz=safeUnit(cross(by,bx),bz);}
vec3 offset=bx*q.x+by*q.y;float c=cos(spin),s=sin(spin);offset=offset*c+cross(bz,offset)*s+bz*dot(bz,offset)*(1.-c);p.xyz+=offset;gl_Position=projectionMatrix*p;vUv=uv*tile.xy+tile.zw;vTint=tint;}`;
const fragment=`uniform sampler2D map;varying vec2 vUv;varying vec4 vTint;void main(){gl_FragColor=texture2D(map,vUv)*vTint;if(gl_FragColor.a<0.002)discard;}`;
class Emitter107{
 constructor(source,data,anchor,error){
  this.w=source.words;this.f=o=>float(this.w[o/4]);this.u=o=>this.w[o/4];this.data=data;this.anchor=anchor;this.age=0;this.credit=0;this.nextBurst=0;this.particles=[];
  this.capacity=Math.min(256,Math.max(16,Math.ceil(Math.max(this.f(60)*this.u(396)/1000,this.u(68)*4))));
  const g=this.geometry=new THREE.InstancedBufferGeometry(),quad=new THREE.PlaneGeometry(1,1);g.index=quad.index;g.attributes.position=quad.attributes.position;g.attributes.uv=quad.attributes.uv;
  for(const [key,size]of [['center',3],['extent',2],['tint',4],['spin',1],['tile',4],['direction',3],['velocity',3]])g.setAttribute(key,new THREE.InstancedBufferAttribute(new Float32Array(this.capacity*size),size).setUsage(THREE.DynamicDrawUsage));
  this.textureEntry=texture(source.textureId,error);this.material=new THREE.ShaderMaterial({uniforms:{map:{value:this.textureEntry.texture},face:{value:this.u(384)}},vertexShader:vertex,fragmentShader:fragment,transparent:true,depthWrite:false,side:THREE.DoubleSide,blending:this.u(504)?THREE.AdditiveBlending:THREE.NormalBlending});
  this.mesh=new THREE.Mesh(g,this.material);this.mesh.frustumCulled=false;this.mesh.renderOrder=2;this.inverse=new THREE.Matrix4();
 }
 value(o,index,progress){return frame(this.data.floatFrames,index,progress,this.f(o));}
 spawn(progress){
  if(this.particles.length>=this.capacity)return;
  const random=o=>this.f(o)+(Math.random()*2-1)*this.f(o+4);
  const size=[4,8,12].map((o,i)=>this.value(o,this.u(16+i*4),progress));
  const direction=new THREE.Vector3(this.f(36),this.f(40),this.f(44));if(direction.lengthSq()<1e-8)direction.set(0,1,0);direction.normalize();
  const axisX=new THREE.Vector3(Math.abs(direction.z)<.99?0:1,0,Math.abs(direction.z)<.99?1:0).cross(direction).normalize(),axisY=new THREE.Vector3().crossVectors(direction,axisX);
  const shape=this.u(0),boundary=!!this.u(28);let x=0,y=0,z=0;
  if(shape===0||shape===1){x=Math.random()-.5;y=Math.random()-.5;z=shape===0?Math.random()-.5:0;if(boundary){const side=Math.floor(Math.random()*(shape===0?3:2)),edge=Math.random()<.5?-.5:.5;if(side===0)x=edge;else if(side===1)y=edge;else z=edge;}}
  else if(shape===3){if(boundary){const a=Math.random()*Math.PI*2;x=Math.cos(a)*.5;y=Math.sin(a)*.5;}else{do{x=Math.random()-.5;y=Math.random()-.5;}while(x*x+y*y>.25);}}
  else if(shape===2){if(boundary){const a=Math.random()*Math.PI*2,b=Math.random()*Math.PI*2;x=Math.cos(a)*Math.sin(b)*.5;y=Math.sin(a)*Math.sin(b)*.5;z=Math.cos(b)*.5;}else{do{x=Math.random()-.5;y=Math.random()-.5;z=Math.random()-.5;}while(x*x+y*y+z*z>.25);}}
  const radial=axisX.clone().multiplyScalar(x*size[0]).addScaledVector(axisY,y*size[1]).addScaledVector(direction,z*size[2]);
  const p=radial.clone().add(new THREE.Vector3(this.f(104),this.f(108),this.f(112)));
  const velocityDirection=boundary&&this.u(188)&&radial.lengthSq()>1e-8?radial.clone().normalize():direction.clone();
  const cone=this.f(48)*(this.u(52)?Math.random():1);if(cone){const coneAxis=axisX.clone().applyAxisAngle(direction,Math.random()*Math.PI*2);velocityDirection.applyAxisAngle(coneAxis,cone).normalize();}
  const velocity=velocityDirection.multiplyScalar(this.value(388,this.u(392),progress));
  const gravity=new THREE.Vector3(this.f(84),this.f(88),this.f(92)).multiplyScalar(this.value(96,this.u(100),progress));
  const world=!!this.u(404);if(world){p.applyMatrix4(this.anchor.matrixWorld);direction.transformDirection(this.anchor.matrixWorld);velocity.transformDirection(this.anchor.matrixWorld).multiplyScalar(this.value(388,this.u(392),progress));gravity.transformDirection(this.anchor.matrixWorld).multiplyScalar(this.value(96,this.u(100),progress));}
  const life=Math.max(1,frame(this.data.colorFrames,this.u(400),progress,this.u(396),false,true))/1000;
  const height0=random(416),height1=random(432);
  this.particles.push({p,velocity,direction,gravity,world,age:0,life,width0:this.u(516)?height0:random(408),width1:this.u(516)?height1:random(424),height0,height1,spin:random(508),tile:this.u(500)?Math.floor(Math.random()*Math.max(1,this.u(488)*this.u(492))):0});
 }
 update(dt,emitting){
  this.age+=dt;const u=this.u,f=this.f,duration=Math.max(.001,u(32)/1000),start=u(200)/1000,progress=((this.age-start)%duration)/duration*100;
  this.anchor.updateWorldMatrix(true,false);this.inverse.copy(this.anchor.matrixWorld).invert();
  if(emitting&&this.age>=start&&(!u(180)||this.age-start<duration)){
   if(u(56)){this.nextBurst-=dt;if(this.nextBurst<=0){this.credit+=Math.max(0,frame(this.data.colorFrames,u(72),progress,u(68),false,true));this.nextBurst=Math.max(.001,(frame(this.data.colorFrames,u(80),progress,u(76),false,true)+(Math.random()*2-1)*u(196))/1000);}}
   else this.credit+=Math.max(0,this.value(60,u(64),progress))*dt;
   const count=Math.min(this.capacity,Math.floor(this.credit));this.credit-=count;for(let i=0;i<count;i++)this.spawn(progress);
  }
  const attrs=this.geometry.attributes;let i=0;
  for(let j=this.particles.length-1;j>=0;j--){const p=this.particles[j];p.age+=dt;if(p.age>=p.life){this.particles.splice(j,1);continue;}
   p.velocity.addScaledVector(p.gravity,dt);p.p.addScaledVector(p.velocity,dt);const position=p.p.clone();if(p.world)position.applyMatrix4(this.inverse);
   const t=p.age/p.life,color=frame(this.data.colorFrames,u(484),t*100,[16,8,0].map(shift=>((u(480)>>>shift)&255)/255),true),alpha=frame(this.data.floatFrames,u(476),t*100,f(472));
    attrs.center.setXYZ(i,position.x,position.y,position.z);attrs.extent.setXY(i,mix(p.width0,p.width1,t),mix(p.height0,p.height1,t));attrs.tint.setXYZW(i,...color,alpha);attrs.spin.setX(i,p.spin*p.age);
    const direction=p.direction.clone(),velocity=p.velocity.clone();if(p.world){direction.transformDirection(this.inverse);velocity.transformDirection(this.inverse);}attrs.direction.setXYZ(i,direction.x,direction.y,direction.z);attrs.velocity.setXYZ(i,velocity.x,velocity.y,velocity.z);
   const columns=u(488)||1,rows=u(492)||1,tile=(p.tile+(u(496)?Math.floor(p.age*1000/u(496)):0))%(columns*rows);attrs.tile.setXYZW(i,1/columns,1/rows,(tile%columns)/columns,1-(Math.floor(tile/columns)+1)/rows);i++;
  }
  this.geometry.instanceCount=i;for(const a of Object.values(attrs))if(a.isInstancedBufferAttribute)a.needsUpdate=true;
 }
  dispose(){this.geometry.dispose();this.material.dispose();this.textureEntry.refs--;this.textureEntry.used=performance.now();pruneTextures107();}
}
export class Particle107{
 constructor(id,anchor,{duration=Infinity,error=()=>{}}={}){this.anchor=anchor;this.duration=duration;this.age=0;this.emitters=[];this.disposed=false;this.ready=database.then(data=>{
  if(this.disposed)return;const system=data[id];if(!system)throw Error('缺少原版特效 '+id);
  for(const source of system.emitters){if(!source.textureId)continue;if(source.words[0]>3)throw Error('不支持的原版粒子形状 '+source.words[0]);const emitter=new Emitter107(source,system,anchor,error);this.emitters.push(emitter);anchor.add(emitter.mesh);}
 }).catch(error);}
 update(dt){if(this.disposed)return;this.age+=dt;for(const e of this.emitters)e.update(dt,this.age<this.duration);}
 dispose(){if(this.disposed)return;this.disposed=true;for(const e of this.emitters){this.anchor.remove(e.mesh);e.dispose();}this.emitters=[];}
}
