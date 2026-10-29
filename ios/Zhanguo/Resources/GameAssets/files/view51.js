// A new blue gateway visual for the two native TownReturn NPC templates.
// This is display geometry, not a converted PC asset. Position, ownership,
// visibility, expiry, interaction and travel remain native scene data.
function pcIsTownPortal114(n){
 return n?.returnPortal===true&&(n.template===950||n.template===951);
}
function pcTownPortal114(n){
 const group=new THREE.Group();
 const material=new THREE.ShaderMaterial({
  transparent:true,depthWrite:false,side:THREE.DoubleSide,toneMapped:false,
  uniforms:{time:{value:0}},
  vertexShader:'varying vec2 portalUv;void main(){portalUv=uv;gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.);}',
  fragmentShader:`
   varying vec2 portalUv;uniform float time;
   void main(){
    vec2 p=(portalUv-.5)*2.;float r=length(p);if(r>1.)discard;
    float a=atan(p.y,p.x);
    float edge=exp(-pow((r-.83)*26.,2.));
    float halo=exp(-pow((r-.84)*9.,2.));
    float strands=pow(.5+.5*sin(a*19.-time*2.1+sin(a*7.+time)*.7),3.);
    float spiral=.5+.5*sin(r*24.-a*4.+time*2.3+sin(a*3.-time)*1.2);
    float eddies=.5+.5*sin(a*9.+r*11.-time*1.4);
    float inner=(1.-smoothstep(.73,.89,r));
    float mouth=smoothstep(.05,.8,r);
    vec3 dark=vec3(.006,.025,.095)+vec3(.012,.07,.19)*(spiral*.42+eddies*.16)*mouth;
    vec3 blue=vec3(.06,.51,1.0)*halo*(.35+strands*.5);
    vec3 rim=vec3(.36,.89,1.)*edge*(.65+strands*.55);
    float alpha=max(inner*.94,halo*(.3+strands*.55)+edge*.8);
    alpha*=1.-smoothstep(.96,1.,r);
    gl_FragColor=vec4(dark*inner+blue+rim,clamp(alpha,0.,1.));
   }`});
 const gate=new THREE.Mesh(new THREE.PlaneGeometry(2.65,3.9),material);
 gate.position.y=1.9;group.add(gate);
 const ground=new THREE.Mesh(new THREE.RingGeometry(.48,1.1,64),
  new THREE.MeshBasicMaterial({color:0x31a9ef,transparent:true,opacity:.3,
   blending:THREE.AdditiveBlending,side:THREE.DoubleSide,depthWrite:false,toneMapped:false}));
 ground.rotation.x=-Math.PI/2;ground.position.y=.025;group.add(ground);
 group.userData.pcTownPortal114={gate,ground,template:n.template,visualSource:'bridge-blue-gateway43'};
 return group;
}
function pcUpdateTownPortal114(group,at,camera){
 const effect=group.userData.pcTownPortal114;if(!effect)return;
 const t=Number.isFinite(at)?at%60000/1000:0;
 effect.gate.material.uniforms.time.value=t;
 effect.ground.material.opacity=.24+Math.sin(t*2)*.06;
 // Turn toward the camera around the vertical axis only; the gate stays
 // upright in map space instead of lying flat with the ground markers.
 if(camera?.position&&Number.isFinite(camera.position.x)&&Number.isFinite(camera.position.z))
  group.rotation.y=Math.atan2(camera.position.x-group.position.x,camera.position.z-group.position.z);
}

// Native quest projection identifies accepted, individually unfinished combat
// and gather targets. Labels110 uses a canvas, so detached-label CSS cannot
// display their markers. The yellow crossed blades match the PC affordance.
function pcQuestBladesDraw58(c,cx,cy){
 c.save();c.translate(cx,cy);c.lineJoin='round';c.lineWidth=1.2;
 c.shadowColor='#ffba19';c.shadowBlur=9;c.strokeStyle='#b07109';
 const gold=c.createLinearGradient(0,-15,0,14);gold.addColorStop(0,'#fffad0');gold.addColorStop(.5,'#ffd736');gold.addColorStop(1,'#f2a208');c.fillStyle=gold;
 for(const mirror of [1,-1]){c.save();c.scale(mirror,1);
  c.beginPath();c.moveTo(-14,-15);c.quadraticCurveTo(-13,-7,-7,-2);
  c.lineTo(6,10);c.lineTo(3,13);c.lineTo(5,15);c.lineTo(8,12);
  c.lineTo(10,15);c.lineTo(12,13);c.lineTo(9,10);c.lineTo(12,7);
  c.lineTo(10,5);c.lineTo(7,8);c.lineTo(-3,-5);
  c.quadraticCurveTo(-10,-9,-14,-15);c.closePath();c.fill();c.stroke();
  c.shadowBlur=0;c.strokeStyle='#fff6a5';c.lineWidth=1.2;c.beginPath();c.moveTo(-11,-10);c.lineTo(6,8);c.stroke();c.restore();
 }
 c.restore();
}
function pcQuestMarksInstall58(labels){
 if(!labels||labels.pcQuestMarks58)return;const draw=labels.draw;
 labels.pcQuestMarks58={drawn:[]};
 labels.draw=function(){
  draw.call(this);const c=this.ctx,drawn=[];c.save();c.globalAlpha=1;
  for(const {el,x,y} of this.jobs){
   if(el.dataset?.pcQuest114!=='target'||!(el.classList.contains('mob')||el.classList.contains('npc'))||el.style.display==='none'||!Number.isFinite(x)||!Number.isFinite(y))continue;
   const cy=Math.round(y-42),cx=Math.round(x);
   // Cache the glow/path raster once, just like Labels110's name glyphs.
   // Walking past many quest monsters adds only one drawImage per marker.
   let glyph=this.pcQuestMarks58.glyph;
   if(!glyph){glyph=document.createElement('canvas');glyph.width=52;glyph.height=56;pcQuestBladesDraw58(glyph.getContext('2d'),26,28);this.pcQuestMarks58.glyph=glyph;}
   c.drawImage(glyph,cx-26,cy-28);
   // The marker selects its own target exactly like the existing name label.
   this.hits.push({l:cx-16,r:cx+16,t:cy-17,b:cy+17,el});drawn.push({x:cx,y:cy});
  }
  c.restore();this.pcQuestMarks58.drawn=drawn;
 };
}

// Native status113 is the only player-stall sign trigger.
const pcStallSignImage114="data:image/png;base64,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";
function pcStallTitle114(player){
 const raw=player?.stallName;
 if(typeof raw!=='string'||!raw.length)return '';
 // The original PC compares the complete native title with GetName().
 // Custom titles stay unchanged; the raw title still identifies the shop.
 return raw===player.name?raw+'的摊位':raw;
}
function pcStallLabelsInstall114(labels){
 if(labels.pcStallDraw114)return;
 const draw=labels.draw,image=new Image(),glyphs=new Map();
 image.src=pcStallSignImage114;labels.pcStallDraw114={image,glyphs};
 // Labels110 paints detached labels on one canvas, so DOM background CSS
 // cannot produce a stall plaque. Keep its ordinary draw path unchanged.
 labels.draw=function(){
  const jobs=this.jobs,stalls=jobs.filter(job=>job.el.classList.contains('bridgeStallSign114'));
  this.jobs=jobs.filter(job=>!job.el.classList.contains('bridgeStallSign114'));
  try{draw.call(this);}finally{this.jobs=jobs;}
  const c=this.ctx,font='bold 14px "SimSun","Songti SC",serif',color='#f7cd8e';
  for(const {el,x,y}of stalls){
   const text=el.textContent;let glyph=glyphs.get(text);
   if(!glyph){
    c.font=font;const canvas=document.createElement('canvas');canvas.width=Math.ceil(c.measureText(text).width)+8;canvas.height=32;
    const g=canvas.getContext('2d');g.font=font;g.textBaseline='middle';g.lineJoin='round';g.lineWidth=2;
    g.strokeStyle='#080b09';g.fillStyle=color;g.strokeText(text,4,16);g.fillText(text,4,16);
    glyph={canvas};glyphs.set(text,glyph);if(glyphs.size>128)glyphs.delete(glyphs.keys().next().value);
   }
   const width=Math.max(198,glyph.canvas.width+40),left=Math.round(x-width/2),top=Math.round(y-32);
   c.save();c.globalAlpha=1;
   if(image.complete&&image.naturalWidth){
    const edge=Math.min(32,Math.floor(image.naturalWidth/2)),middle=image.naturalWidth-edge*2;
    c.drawImage(image,0,0,edge,image.naturalHeight,left,top,edge,32);
    c.drawImage(image,edge,0,middle,image.naturalHeight,left+edge,top,width-edge*2,32);
    c.drawImage(image,image.naturalWidth-edge,0,edge,image.naturalHeight,left+width-edge,top,edge,32);
   }else{
    c.fillStyle='#1e1e1e';c.fillRect(left,top,width,32);c.strokeStyle='#080808';c.strokeRect(left+.5,top+.5,width-1,31);
   }
   c.drawImage(glyph.canvas,Math.round(x-glyph.canvas.width/2),top);c.restore();
   this.hits.push({l:left,r:left+width,t:top,b:top+32,el});
  }
 };
}
function pcStallSign114(view,actor,player,position,height=1.8){
 const text=pcStallTitle114(player);
 if(!player?.stalling||!text){actor.stallSign?.remove();actor.stallSign=null;return;}
 pcStallLabelsInstall114(view.labels110);
 if(!actor.stallSign){const sign=document.createElement('div');sign.className='label player bridgeStallSign114';
  sign.setAttribute('role','button');sign.tabIndex=0;actor.stallSign=sign;
  const enter=event=>{event?.preventDefault();event?.stopPropagation();view.callbacks.stall?.(player.id??player.uid);};
  sign.onclick=enter;sign.onkeydown=e=>{if(['Enter',' '].includes(e.key))enter(e);};}
 if(actor.stallSign.textContent!==text)actor.stallSign.textContent=text;
 actor.stallSign.style.display=actor.mesh?.visible===false?'none':'';
 view.place(actor.stallSign,position.x,height,position.z);
}

import {playerMovementAction112} from './locomotion112.js';
import {animatesCaster112} from './visual-policy112.js';
import {Labels110} from './labels110.js';
import {qualityColor110,setText110} from './controls110.js';
import {navigation109,Prediction109,canWalk109} from './navigation109.js';
import {Native107} from './native107.js';
import {SpellScene107} from './spells107.js';
import {SpriteEffect107} from './sprite-effects107.js';
// PC map59 already contains the beacon platform. Model6000 is its tiny native
// click anchor; the PC status table supplies the missing visible effects.
// Only native NPC/state records create these effects. This is display only.
const pcNoviceBeaconEffects117=Object.freeze({
 45818:Object.freeze({id:588,pivot:Object.freeze([415,910]),sprite:Object.freeze({group:38,image:3,frameMs:40,blend:0,color:4294967295}),duration:null}),
 45820:Object.freeze({id:587,pivot:Object.freeze([415,1110]),sprite:Object.freeze({group:38,image:2,frameMs:60,blend:0,color:4294967295}),duration:null}),
 45828:Object.freeze({id:589,pivot:Object.freeze([415,910]),sprite:Object.freeze({group:38,image:4,frameMs:40,blend:3,color:4294967295}),duration:null}),
 45819:Object.freeze({id:1076,pivot:Object.freeze([415,1110]),sprite:Object.freeze({group:38,image:1,frameMs:50,blend:0,color:4294967295}),duration:2.4})
});
function pcClearNoviceBeacon117(actor){
 const entries=actor?.noviceBeaconEffects117;if(!entries)return;
 for(const entry of entries.values()){entry.visual.dispose();entry.anchor.removeFromParent();}
 delete actor.noviceBeaconEffects117;
}
function pcUpdateNoviceBeacon117(view,actor,n,dt){
 const map=Number(view.state?.map?.id),base=Number(n.template??n.baseId??n.baseID),
  states=Array.isArray(n.states)?n.states:[],has=id=>states.some(value=>Number.isInteger(value)&&((value>>>16)&65535)===id);
 const valid=map===59&&[1846,1847].includes(base)&&Number.isInteger(n.id)&&n.id>0&&
  n.hidden!==true&&n.dead!==true&&n.alive!==false&&n.hp!==0;
 // The native body states replace one another. A separately native 45828
 // prompt overlays the unlit body instead of hiding that original wood pile.
 const body=valid?(has(45820)?45820:has(45819)?45819:has(45818)?45818:0):0,
  wanted=new Set(valid?[body,has(45828)?45828:0].filter(Boolean):[]);
 let entries=actor.noviceBeaconEffects117;
 if(entries&&[...entries.values()].some(entry=>entry.uid!==n.id)){pcClearNoviceBeacon117(actor);entries=null;}
 if(!wanted.size){pcClearNoviceBeacon117(actor);return;}
 if(!entries)actor.noviceBeaconEffects117=entries=new Map();
 for(const [status,entry] of entries)if(!wanted.has(status)){
  entry.visual.dispose();entry.anchor.removeFromParent();entries.delete(status);
 }
 for(const status of wanted){
 const source=pcNoviceBeaconEffects117[status];let entry=entries.get(status);
 if(!entry){const anchor=new THREE.Group();anchor.scale.setScalar(.8);actor.mesh.add(anchor);
  entry={status,uid:n.id,age:0,anchor,visual:new SpriteEffect107(source.sprite,anchor,view.callbacks.error),finished:false};entries.set(status,entry);
 }
 if(entry.finished)continue;
 entry.age+=Math.max(0,Math.min(.1,Number.isFinite(dt)?dt:0));
 // The native "ignition process" has a finite original 2400ms visual. Its
 // retained status is not permission to replay it on every world poll.
 if(source.duration!==null&&entry.age>=source.duration){
  entry.visual.dispose();entry.anchor.removeFromParent();entry.finished=true;continue;
 }
 entry.visual.update(dt);
 // effect+56/+60 are the original PC image anchor, not half the canvas.
 // Reuse the packed frame crop while keeping the fire on the platform.
 const visual=entry.visual,frame=visual.sequence?.frames?.[visual.last];
 if(frame?.atlas&&visual.sprite){const width=frame.atlas[3],height=frame.atlas[4];
  visual.sprite.center.set((source.pivot[0]-frame.rect[0])/width,1-(source.pivot[1]-frame.rect[1])/height);
 }
 }
}

const visualKeys109=new WeakMap();const visualKey107=n=>{if(!n)return null;let key=visualKeys109.get(n);if(!key){key=JSON.stringify([n.model,n.parts,n.visibleMeshes,n.textures,n.weapons,n.scale,n.effects]);visualKeys109.set(n,key);}return key;};
const label109=(el,text)=>{if(el.textContent!==text)el.textContent=text;};
import * as THREE from 'three';
import {GLTFLoader} from './vendor/examples/jsm/loaders/GLTFLoader.js';
import {SceneArt} from './scene-art.js';
import {CAMERA_COS,CAMERA_SIN} from './scene-data.js';
import {Peer84} from './peer84.js';

// view51.js owns THREE and the orthographic camera.  Expose only this tiny
// projection helper to client.js so the picker can turn a WebView pointer into
// the same ground point used by the existing scene click handler.
globalThis.bridgeMotionProject114=(view,clientX,clientY)=>{
 try{
  const canvas=view?.renderer?.domElement;if(!canvas||!view?.camera)return null;
  const r=canvas.getBoundingClientRect(),ray=new THREE.Raycaster();
  ray.setFromCamera(new THREE.Vector2((clientX-r.left)/r.width*2-1,1-(clientY-r.top)/r.height*2),view.camera);
  const point=ray.ray.intersectPlane(new THREE.Plane(new THREE.Vector3(0,1,0),0),new THREE.Vector3());
  return point?{x:point.x,z:point.z}:null;
 }catch{return null;}
};

// Display-only interpolation from the last visible pose to the confirmed PC
// endpoint. Nothing here is used as a gameplay coordinate or sent to the server.
function pcMotionDuration114(view,animation){
 const name='Game '+String(animation).padStart(4,'0');
 const clip=(view.nativePlayer107?.actions?.[name]??view.actions?.[name])?.getClip();
 return Math.max(200,Math.min(1500,(clip?.duration??.625)*1000));
}
// The cast request uses the PC north/east/south/west angle convention, while
// Three.js yaw uses the visible world displacement.  The 12/36/12/07 packet
// already contains the authoritative endpoint, so face the exact accepted
// segment instead of reusing a joystick vector that may have changed or been
// clipped by a wall.  This also prevents a backward-looking jump when the
// server's final endpoint differs from the original input direction.
function facePcMotionEndpoint114(view,from,to){
 const dx=to.x-from.x,dz=to.z-from.z;
 if(Math.hypot(dx,dz)>.01)view.avatar.rotation.y=Math.atan2(dx,dz);
}
function beginPcMotion114(view,player,event,now=performance.now()){
 const animation=event.type==='jump'?20:144;
 const duration=pcMotionDuration114(view,animation);
 const from={x:view.display.x,z:view.display.z},to={x:player.x,z:player.z};
 if(![from.x,from.z,to.x,to.z].every(Number.isFinite))return;
 // A single cast can produce both 12/36 and 12/77 movement frames (and the
 // direct session may label those frames differently). The same server
 // endpoint can therefore reach the view more than once while the native
 // clip is still playing. Do not restart interpolation from its current
 // pose: that makes a roll/jump appear to land twice and, for a zero-length
 // restart, causes a visible second landing flash.
 const active=view.pcMotion114;
 // A single PC movement release can produce both 12/36 and 12/77 frames.
 // They have no cast SID, and the two frames can use different `kind` values
 // or slightly different endpoints.  Keep one visual motion window keyed by
 // the locally dispatched request; update its endpoint in place instead of
 // restarting the clip from the first landing point.
 const group=view.motionGroup114;
 if(active&&group&&active.groupRequest===group.request&&
    now-group.openedAt<group.windowMs){
  active.to=to;active.kind=event.type;active.confirmed=true;
  active.windowUntil=Math.max(active.windowUntil??0,now+group.windowMs);
  view.lastMotionEndpoint114={x:to.x,z:to.z};
  view.attackUntil=Math.max(view.attackUntil??0,active.start+active.duration);
  return;
 }
 view.pcMotion114={from,to,start:now,duration,kind:event.type,
  groupRequest:group?.request,confirmed:false,windowUntil:now+1200};
 facePcMotionEndpoint114(view,from,to);
 view.attackUntil=now+duration;
}
function advancePcMotion114(view,player,now=performance.now()){
 const m=view.pcMotion114;
 if(!m)return false;
 if(player.dead||player.weak){view.pcMotion114=null;return false;}
 const t=Math.max(0,Math.min(1,(now-m.start)/m.duration));
 view.display.x=m.from.x+(m.to.x-m.from.x)*t;
 view.display.z=m.from.z+(m.to.z-m.from.z)*t;
 view.display.y=player.y??0;
 if(t>=1){
  // Keep the one clip alive until the short release window closes.  A later
  // movement frame may replace `to` with the final authoritative endpoint;
  // it must never start a second roll/jump animation.
  if(now<(m.windowUntil??m.start+m.duration))return true;
  view.pcMotion114=null;
  if(view.motionGroup114&&now-view.motionGroup114.openedAt>=view.motionGroup114.windowMs)view.motionGroup114=null;
  if(m.predicted&&!m.confirmed){view.predict109.reset(player);view.predict109.history111=[];view.pendingMotion113=null;}
 }
 return true;
}

// Responsive PC presentation. Preview endpoints never enter gateway requests.
function previewPcMotion114(view,kind,request,direction,now=performance.now()){
 if(typeof direction==='number'){now=direction;direction=undefined;}
 const p=view.state?.player;if(!p||p.dead||p.sitting===true||p.weak||now<(view.motionFeedbackUntil113??0))return false;
 const animation=kind==='jump'?20:144,duration=pcMotionDuration114(view,animation);
 // Use the exact normalized vector captured on the outgoing action.  The
 // server converts this vector to PC map angle while the renderer uses a
 // Three.js yaw; deriving both from one vector prevents an opposite-facing
 // clip when the joystick changes during request dispatch.
 const raw=direction&&Number.isFinite(direction.x)&&Number.isFinite(direction.z)?direction:(view.predict109.dir??{x:0,z:0});
 const n=Math.hypot(raw.x,raw.z),pcDir=Number(view.state?.player?.dir);
 const dx=n>.05?raw.x/n:Number.isFinite(pcDir)?Math.sin(pcDir*Math.PI/180):Math.sin(view.avatar.rotation.y);
 const dz=n>.05?raw.z/n:Number.isFinite(pcDir)?-Math.cos(pcDir*Math.PI/180):-Math.cos(view.avatar.rotation.y);
 const from={x:view.display.x,z:view.display.z};
 // Do not invent a displacement while waiting for 12/36 or 12/77.  The
 // previous fixed 3/5-unit preview made a roll visibly run forward before
 // the server endpoint arrived, and could leave an extra step after landing.
 // The renderer also must not start the jump clip here: PC sends the
 // movement frame asynchronously, and playing the clip at the old point
 // makes a flying jump look like an in-place hop followed by a second jump
 // when the 12/36 or 12/77 endpoint arrives.  Keep a deferred visual marker
 // only; acceptPcFrame114 starts one clip at the first authoritative endpoint.
 const to={...from};
 const windowMs=1200;
 view.motionGroup114={request,openedAt:now,windowMs,kind};
 view.pcMotion114={from,to,start:now,duration,kind,predicted:true,confirmed:false,deferred:true,request,groupRequest:request,windowUntil:now+windowMs};
 view.pendingMotion113={request,at:now,skill:kind==='jump'?10201:20050};
 // Keep the short admission window so a second button press cannot queue a
 // competing cast, but leave the actor in its normal pose until the server
 // confirms a real endpoint.
 view.motionFeedbackUntil113=now+duration+windowMs;
 if(Math.hypot(dx,dz)>.01)view.avatar.rotation.y=Math.atan2(dx,dz);
 return true;
}

// A /step request can be rejected before the native scene emits any movement
// frame (for example when the authoritative endurance snapshot is below the
// PC skill cost).  Explicitly cancel that one request and restore the visible
// pose to its pre-motion anchor.  This also covers a deferred preview whose
// server reply never arrives; it must not later turn into a fake roll/jump or
// make the next stale native frame move the avatar backwards.
function rejectPcMotion114(view,request){
 const m=view.pcMotion114;
 if(!m||m.predicted!==true||(request!==undefined&&m.request!==request))return false;
 const from=m.from&&Number.isFinite(m.from.x)&&Number.isFinite(m.from.z)?m.from:null;
 view.pcMotion114=null;view.pendingMotion113=null;view.attackUntil=0;view.motionFeedbackUntil113=0;
 if(from){view.display.x=from.x;view.display.z=from.z;view.display.y=0;}
 const p=view.state?.player;
 if(p&&view.predict109){view.predict109.reset?.(p);if(Array.isArray(view.predict109.history111))view.predict109.history111=[];}
 return true;
}
function acceptPcFrame114(view,s,now=performance.now()){
 if(s.meta?.bridge!=='pc6020')return;
 const changed=!view.state||s.generation!==view.state.generation||s.map.id!==view.state.map.id;
 const correction=s.positionCorrection;
 if(changed){view.pcWalkingUntil114=0;view.pcMotion114=null;view.pendingMotion113=null;view.lastMotionCorrection114=0;view.lastMotionEndpoint114=null;view.pcCastPreviews114=new Set();view.pcCorrection114=correction?.seq;view.predict109.reset(s.player);return;}
 if(correction&&correction.seq!==view.pcCorrection114){
  view.pcCorrection114=correction.seq;
  // Native correction and a later ordinary dispatch can share one world
  // frame. Use the correction's own endpoint for its animation/reset; the
  // next frame can resume following the newer dispatched endpoint.
  const cx=correction.x,cz=correction.z??correction.y;
  const correctedPlayer=Number.isFinite(cx)&&Number.isFinite(cz)?{
   ...s.player,x:cx,z:cz,nativePosition:{x:cx,y:cz,z:cz},positionSource:'native-self',
   routeActive:false,routeTarget:null,routePath:[]}:s.player;
  const motion=view.pcMotion114;
  // 12/36 and 12/77 can each generate a self_snap for the same authoritative
  // destination.  Treat a same-endpoint motion correction arriving shortly
  // after the first one as the same landing.  Advancing the correction seq
  // still prevents stale frames from being accepted, while preserving the
  // in-flight interpolation and animation.
  const duplicateMotion=correction.motion===true&&view.lastMotionEndpoint114&&
   now-(view.lastMotionCorrection114??-Infinity)<800&&
   Math.hypot((correctedPlayer.x??0)-view.lastMotionEndpoint114.x,
              (correctedPlayer.z??0)-view.lastMotionEndpoint114.z)<.05;
  if(duplicateMotion)return;
  if(correction.motion&&motion?.predicted&&!motion.confirmed){
   // A jump/roll correction is the authoritative endpoint of the motion.
   // Keep stale 05/01 self frames from rebasing the predictor back to the
   // pre-motion cell while the native animation is still being shown.
   view.lastMotionCorrection114=now;
   view.lastMotionEndpoint114={x:correctedPlayer.x,z:correctedPlayer.z};
   // A deferred preview has not consumed any animation time.  Start the
   // single PC clip now and give it its full duration, so the character moves
   // directly from the visible origin to the authoritative far endpoint.
   const duration=motion.deferred===true?motion.duration:Math.max(80,motion.start+motion.duration-now);
   const visibleFrom={x:view.display.x,z:view.display.z},endpoint={x:correctedPlayer.x,z:correctedPlayer.z};
   facePcMotionEndpoint114(view,visibleFrom,endpoint);
   view.pcMotion114={...motion,from:visibleFrom,to:endpoint,start:now,duration,confirmed:true,deferred:false,
    // Keep the request window open so a second 12/36/12/77 frame updates this
    // endpoint in place instead of starting another interpolation.
    windowUntil:Math.max(motion.windowUntil??0,now+400)};
   if(motion.deferred===true){
    const animation=motion.kind==='jump'?20:144;
    view.nativeCast107={castAt:Date.now(),animation};view.attackUntil=now+duration;
    view.motionFeedbackUntil113=now+duration;
    view.play('Game '+String(animation).padStart(4,'0'));
   }
   view.pendingMotion113=null;
  }else if(correction.motion&&motion?.confirmed&&
           view.motionGroup114&&now-view.motionGroup114.openedAt<view.motionGroup114.windowMs){
   // A second movement frame for the same local request has no SID.  Preserve
   // the original animation start and replace only the authoritative landing
   // point, even when this frame's endpoint differs from the first one.
   const visibleFrom={x:view.display.x,z:view.display.z},endpoint={x:correctedPlayer.x,z:correctedPlayer.z};
   motion.to=endpoint;
   facePcMotionEndpoint114(view,visibleFrom,endpoint);
   motion.windowUntil=Math.max(motion.windowUntil??0,now+400);
   view.lastMotionCorrection114=now;
   view.lastMotionEndpoint114={x:correctedPlayer.x,z:correctedPlayer.z};
  }else if(correction.motion){
   // A server displacement can arrive without a local preview (for example
   // the native 12/07 fly packet or a repeated teleport). It is still the
   // authoritative endpoint. Applying the ordinary stale-frame branch here
   // would immediately restore the pre-teleport cell and look like a step
   // backwards.
   view.lastMotionCorrection114=now;
   view.lastMotionEndpoint114={x:correctedPlayer.x,z:correctedPlayer.z};
   view.pcMotion114=null;view.pendingMotion113=null;
   view.attackUntil=0;view.motionFeedbackUntil113=0;
   view.display.x=correctedPlayer.x;view.display.z=correctedPlayer.z;view.display.y=correctedPlayer.y??0;
  }else{view.lastMotionCorrection114=0;view.lastMotionEndpoint114=null;view.pcMotion114=null;view.pendingMotion113=null;view.attackUntil=0;view.motionFeedbackUntil113=0;}
  view.predict109.reset(correctedPlayer);view.predict109.history111=[];
 }
}
function acceptPcAssistPosition114(view,s){
 // Routes expose the last dispatched point. The joystick predictor's slower
 // reconciliation falls behind until it resets; display interpolation already
 // animates these accepted points without projecting any new route steps.
 if(s.meta?.bridge!=='pc6020')return;
 // The server's motion correction arrives before the next ordinary self
 // snapshot.  That snapshot can still contain the old cell; defer it for one
 // short animation window so jump/roll never visibly steps backwards.
 if(view.lastMotionCorrection114&&performance.now()-view.lastMotionCorrection114<1000)return;
 // Automatic combat now uses the same routeTarget/routeActive projection as
 // map navigation. Do not overwrite that continuous predictor with the stale
 // native 05/01 coordinate carried by the current frame.
 if(s.player?.routeActive===true||s.player?.routeTarget)return;
 const p=view.predict109,d=p.dir??{x:0,z:0};
 // A /state or /open response can carry the same lagging native packet as a
 // /step response. Once the joystick has been released, apply the stale-point
 // guard to either response kind; a new direction or explicit correction
 // clears stoppedAt114 in navigation109.js.
 if(s.meta?.positionSource==='pc-snapshot'&&!(p.stoppedAt114??0))return;
 if(Math.hypot(d.x,d.z)>.05)return;
 if(view.pcMotion114||p.displacement113?.path)return;
 // Idle 05/01 frames can continue carrying the previous position while the
 // final walk write is being acknowledged. Preserve the just-finished local
 // endpoint until a newer native point arrives or the user starts moving;
 // 04/5c corrections are handled above and remain authoritative.
 const stoppedAt=p.stoppedAt114??0;
 if(!s.positionCorrection&&stoppedAt&&
    Math.hypot((s.player.x??0)-(p.x??0),(s.player.z??0)-(p.z??0))>.06){
  // predict109.accept() has already moved its server anchor to the stale
  // native point. Restore the endpoint that was visible at release so the
  // next tick cannot interpolate the player backwards by one step.
  p.server={x:p.x,z:p.z};p.received=performance.now();p.remoteMoving111=false;
  p.ack112=null;p.history111=[];return;
 }
 p.x=s.player.x;p.z=s.player.z;p.history111=[];
}
function previewPcCast114(view,event){
 if(!view.state?.player||view.state.player.dead)return;
 view.pcCastPreviews114??=new Set();
 view.pcCastPreviews114.add(event.previewId);
 while(view.pcCastPreviews114.size>64)view.pcCastPreviews114.delete(view.pcCastPreviews114.values().next().value);
 if(event.action)view.spells107.cast(event);
 view.nativeCast107={castAt:event.at,animation:14};view.attackUntil=performance.now()+450;
 if(Math.hypot(event.toX-event.x,event.toZ-event.z)>.01)view.avatar.rotation.y=Math.atan2(event.toX-event.x,event.toZ-event.z);
 view.play('Game 0014');
}
function consumePcCastPreview114(view,event){
 if(!event.previewId||!view.pcCastPreviews114?.has(event.previewId))return false;
 view.pcCastPreviews114.delete(event.previewId);return true;
}

// PC light movement is aimed at the mouse direction.  Mobile supplies that
// direction with this one-shot gesture; it never reuses an old ground skill
// target and never invents a movement endpoint before the scene reply.
let bridgeMotionAim114=null;
function bridgeMotionDirection114(raw){
 if(!raw||!Number.isFinite(raw.x)||!Number.isFinite(raw.z))return null;
 const length=Math.hypot(raw.x,raw.z);if(!(length>.05))return null;
 // PC movement is eight-way. Snap the display and request to the same
 // 45-degree compass sector so an analog joystick cannot disagree with the
 // arrow by a few degrees near a diagonal.
 const sector=Math.round(Math.atan2(raw.x,-raw.z)/(Math.PI/4));
 const angle=sector*(Math.PI/4);
 const x=Math.sin(angle),z=-Math.cos(angle);
 return {x:Math.abs(x)<1e-9?0:x,z:Math.abs(z)<1e-9?0:z};
}
function bridgeMotionAimInstall114({buttons,decorations=[],canvas,labels,ready,scope,origin,aim,project,current=()=>null,
 preview,send,prepare=()=>{},notice=()=>{},status,doc=document,now=()=>Date.now()}){
 const list=[...buttons],arrowButtons=[...list,...decorations];let selection=null,suppressedClick=null,hint=null,hintText=null,lastDirection={x:0,z:-1};
 const liveDirection=()=>{const d=bridgeMotionDirection114(current?.());if(d)lastDirection=d;return lastDirection;};
 const arrows=new Map();
 const block=e=>{e.preventDefault?.();e.stopImmediatePropagation?.();e.stopPropagation?.();};
 const worldTarget=target=>target===canvas||target===labels||labels?.contains?.(target)===true;
 const motionButton=target=>list.find(button=>button===target||button.contains?.(target));
 if(!status){
  hint=doc.createElement('div');hint.id='motionAim114';hint.hidden=true;
  hint.style.cssText='position:fixed;left:50%;bottom:24%;transform:translateX(-50%);z-index:190;display:none;align-items:center;gap:12px;padding:10px 14px;background:rgba(12,15,10,.94);border:1px solid #ae8736;color:#ffe2a1;max-width:75vw;';
  hintText=doc.createElement('span');const cancelButton=doc.createElement('button');cancelButton.textContent='取消';
  cancelButton.onpointerdown=e=>e.stopPropagation();cancelButton.onclick=e=>{block(e);cancel();};
  hint.append(hintText,cancelButton);doc.body.append(hint);
  status=value=>{hint.hidden=!value;hint.style.display=value?'flex':'none';
   if(value)hintText.textContent=(value.kind==='jump'?'飞跃':'翻滚')+'：'+(value.drag?'松手释放':'拖动选方向，或点地图释放');};
 }
 function render(){
  for(const button of arrowButtons){const selected=button.dataset.motion===selection?.kind;
   button.classList.toggle('selected109',selected);button.setAttribute('aria-pressed',String(selected));
   const arrow=arrows.get(button);if(!arrow)continue;
   const direction=selected?(selection?.direction??liveDirection()):liveDirection();
   arrow.hidden=!direction;
   if(direction){
    // The arrow glyph points up for PC north (-z); the same angle is the
    // protocol convention atan2(x,-z), so the visual and packet cannot drift.
    arrow.style.transform=`translateY(-50%) rotate(${Math.atan2(direction.x,-direction.z)*180/Math.PI}deg)`;
   }else arrow.style.transform='translateY(-50%) rotate(0deg)';
  }
  status(selection?{kind:selection.kind,drag:selection.drag}:null);
 }
 function releaseCapture(s){if(s?.button&&s.pointer!=null){try{s.button.releasePointerCapture?.(s.pointer);}catch{}}}
 function cancel(){const prior=selection;selection=null;preview(null);render();releaseCapture(prior);}
 function observe(){if(selection&&(selection.scope!==scope()||!ready()))cancel();}
 function arm(kind){
  if(!['dodge','jump'].includes(kind)||!ready())return false;
  cancel();prepare();selection={kind,scope:scope(),pointer:null,button:null,start:null,drag:false,direction:liveDirection()};
  const o=origin(),d=selection.direction;preview(d&&o?{x:o.x+d.x*30,z:o.z+d.z*30}:null);render();return true;
 }
 function pointDirection(point){
  const from=origin();if(!from||!point||![from.x,from.z,point.x,point.z].every(Number.isFinite))return null;
  return bridgeMotionDirection114({x:point.x-from.x,z:point.z-from.z});
 }
 function choose(point){if(!selection)return;selection.direction=pointDirection(point);preview(selection.direction?point:null);render();}
 function commit(){
  observe();const s=selection;if(!s)return false;
  const direction=bridgeMotionDirection114(s.direction);
  if(!direction){s.pointer=null;s.button=null;s.start=null;s.drag=false;preview(null);render();notice('请选择人物前方的落点方向');return false;}
  const action={kind:s.kind,direction113:{...direction}};
  // Clear UI and capture before dispatch: a rejected request or scene change
  // must not leave a second, armed release behind.
  cancel();return send(action)===true;
 }
 function suppressClick(e){suppressedClick={target:e.target,pointer:e.pointerId,until:now()+650};}
 function down(e){
  observe();if(!selection)return;
  const button=motionButton(e.target);if(button)return;
  if(selection.pointer!==null){cancel();if(worldTarget(e.target))block(e);return;}
  if(!worldTarget(e.target)){cancel();return;}
  block(e);selection.pointer=e.pointerId;selection.start={x:e.clientX,y:e.clientY};selection.button=null;selection.drag=true;
  choose(project(e.clientX,e.clientY));
 }
 function move(e){
  observe();const s=selection;if(!s)return;
  if(s.pointer===null){if(worldTarget(e.target))choose(project(e.clientX,e.clientY));return;}
  if(e.pointerId!==s.pointer)return;block(e);
  if(s.button){const dx=e.clientX-s.start.x,dy=e.clientY-s.start.y;
   if(!s.drag&&Math.hypot(dx,dy)<12)return;s.drag=true;choose(aim(dx,dy,s.kind));}
  else choose(project(e.clientX,e.clientY));
 }
 function up(e){
  observe();const s=selection;if(!s)return;
  if(e.pointerId!==s.pointer){
   // Detached Labels110 can synthesize a canvas pointerup without a down.
   // Consume it while aiming so it cannot select an NPC or ground skill.
   if(worldTarget(e.target))block(e);return;
  }
  block(e);suppressClick(e);const wasButton=s.button;
  if(!wasButton)choose(project(e.clientX,e.clientY));
  if(s.drag&&s.direction){commit();return;}
  if(wasButton){
   // A normal tap is a complete one-step action along the live movement
   // direction shown by the arrow. Dragging the same button overrides it.
   s.direction=liveDirection()??s.direction;
   if(s.direction){commit();return;}
   cancel();notice('请先移动或选择人物朝向');return;
  }
  s.pointer=null;s.button=null;s.start=null;s.drag=false;s.direction=null;preview(null);render();releaseCapture(s);
 }
 function interrupted(e){if(selection&&e.pointerId===selection.pointer){block(e);cancel();}}
 function click(e){
  if(suppressedClick&&now()<=suppressedClick.until&&
     (e.target===suppressedClick.target||motionButton(e.target)||worldTarget(e.target))){block(e);suppressedClick=null;}
 }
 for(const button of arrowButtons){
  button.style.touchAction='none';button.setAttribute('aria-label',button.dataset.motion?
   ((button.dataset.motion==='jump'?'飞跃':'翻滚')+'，方向箭头跟随移动方向，点击沿箭头动作，拖动可临时改向'):
   '冲刺，方向箭头跟随移动方向');
  if(!arrows.has(button)){
   if(getComputedStyle(button).position==='static')button.style.position='relative';
   const arrow=doc.createElement('span');arrow.className='motionAimArrow114';arrow.textContent='↑';
   arrow.hidden=true;arrow.setAttribute('aria-hidden','true');
   arrow.style.cssText='position:absolute;left:50%;top:29%;z-index:2;pointer-events:none;transform-origin:50% 50%;font-size:20px;line-height:1;color:#ffe19a;text-shadow:0 1px 3px #000;';
   for(const child of button.children??[])if(String(child.tagName).toLowerCase()==='svg')child.style.opacity='0';
   button.append(arrow);arrows.set(button,arrow);
  }
  if(!button.dataset.motion)continue;
  button.onpointerdown=e=>{
   if(button.disabled||e.isPrimary===false||e.button>0)return;block(e);
   if(selection?.pointer!=null){cancel();return;}
   if(selection?.kind===button.dataset.motion){cancel();suppressClick(e);return;}
   if(!arm(button.dataset.motion))return;
   selection.pointer=e.pointerId;selection.button=button;selection.start={x:e.clientX,y:e.clientY};
   try{button.setPointerCapture?.(e.pointerId);}catch{}
  };
  button.onpointermove=null;button.onpointerup=null;button.onpointercancel=null;
  button.onlostpointercapture=e=>{if(selection?.button===button&&selection.pointer===e.pointerId)cancel();};
  button.onclick=e=>{block(e);if(e.detail===0&&!button.disabled)arm(button.dataset.motion);};
 }
 for(const [type,handler]of [['pointerdown',down],['pointermove',move],['pointerup',up],['pointercancel',interrupted],['click',click]])
  doc.addEventListener(type,handler,{capture:true,passive:false});
 const escape=e=>{if(e.key==='Escape'&&selection){block(e);cancel();}};doc.addEventListener('keydown',escape,true);
 return {arm,cancel,observe,update:render,direction:liveDirection,get active(){return !!selection;},
  dispose(){cancel();for(const [type,handler]of [['pointerdown',down],['pointermove',move],['pointerup',up],['pointercancel',interrupted],['click',click]])doc.removeEventListener(type,handler,true);doc.removeEventListener('keydown',escape,true);hint?.remove();for(const arrow of arrows.values())arrow.remove();arrows.clear();}};
}

// Display movement also comes from automatic routes, without joystick input.
function pcWalking114(view,player,travel,dt,now){
 if(view.state?.meta?.bridge!=='pc6020'||player.dead||view.pcMotion114)return false;
 const fresh=now-view.predict109.received<1500;
 const speed=Math.max(1,player.moveSpeed??5);
 if(fresh&&dt>0&&travel>Math.max(.001,dt*.04)&&travel<speed*dt*4+.1){
  // Route snapshots can arrive slower than the render loop (especially on
  // the PC bridge). Keep the run pose through the next expected snapshot,
  // while retaining a short minimum hold so one quiet frame cannot flicker
  // back to idle. Clamp the hold to avoid masking a genuine arrival.
  const previous=view.pcWalkSample114;
  const interval=previous==null?0:Math.max(0,now-previous);
  view.pcWalkSample114=now;
  view.pcWalkingUntil114=now+Math.min(400,Math.max(120,interval*1.5));
 }
 return fresh&&now<(view.pcWalkingUntil114??0);
}
function pcGuideArrow114(n){
 const shape=new THREE.Shape();
 shape.moveTo(-.23,-1);shape.lineTo(.23,-1);shape.lineTo(.23,.2);
 shape.lineTo(.7,.2);shape.lineTo(0,1.1);shape.lineTo(-.7,.2);
 shape.lineTo(-.23,.2);shape.closePath();
 const mesh=new THREE.Mesh(new THREE.ShapeGeometry(shape),new THREE.MeshBasicMaterial({
  color:0xffd54a,side:THREE.DoubleSide,transparent:true,opacity:.9,depthWrite:false}));
 mesh.rotation.x=-Math.PI/2;mesh.position.y=.15;
 const group=new THREE.Group();group.add(mesh);group.rotation.y=-(n.direction??0)*Math.PI/180;
 return group;
}
// A server-created 60003 exit is a luminous gateway, not its placeholder rig.
function pcExitPortal114(){
 const group=new THREE.Group(),parts=[];
 for(let i=0;i<3;i++){
  const ring=new THREE.Mesh(new THREE.RingGeometry(.55+i*.28,.65+i*.28,64),
   new THREE.MeshBasicMaterial({color:i===1?0xffde93:0xff603a,transparent:true,
    opacity:.65,side:THREE.DoubleSide,blending:THREE.AdditiveBlending,depthWrite:false}));
  ring.rotation.x=-Math.PI/2;ring.position.y=.05+i*.035;group.add(ring);parts.push(ring);
 }
 const glow=new THREE.Mesh(new THREE.CylinderGeometry(.38,1,3.8,40,1,true),
  new THREE.ShaderMaterial({transparent:true,depthWrite:false,side:THREE.DoubleSide,
   blending:THREE.AdditiveBlending,uniforms:{t:{value:0}},
   vertexShader:'varying vec2 v;void main(){v=uv;gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.);}',
   fragmentShader:'varying vec2 v;uniform float t;void main(){float rays=pow(.5+.5*sin(v.x*75.+t*2.),10.);float spiral=pow(.5+.5*sin(v.y*22.-v.x*12.-t*3.),7.);float fade=pow(1.-v.y,1.6);gl_FragColor=vec4(1.,.19+.5*rays,.08+.22*rays,fade*(.09+.55*rays+.25*spiral));}'}));
 glow.position.y=1.9;group.add(glow);
 group.userData.pcPortal114={parts,glow};return group;
}
function pcUpdateExitPortal114(group,at){
 const p=group.userData.pcPortal114;if(!p)return;
 const t=(at%60000)/1000;p.glow.material.uniforms.t.value=t;
 p.parts.forEach((ring,i)=>{const s=1+Math.sin(t*2+i)*.08;ring.scale.set(s,s,s);});
}

export class GameView{
 constructor(canvas,labels,callbacks){
  this.predict109=new Prediction109();this.projection109=new THREE.Vector3();this.callbacks=callbacks;this.labels=labels;this.objects=new Map();this.damage=[];this.seen=0;this.state=null;this.display=new THREE.Vector3();
  this.renderer=new THREE.WebGLRenderer({canvas,antialias:false,powerPreference:"high-performance"});this.renderer.debug.checkShaderErrors=true;this.renderer.debug.onShaderError=(gl,program,vs,fs)=>{window.__diagErrors114??=[];window.__diagErrors114.push("shader "+String(gl.getProgramInfoLog(program)||"")+" | vs "+String(gl.getShaderInfoLog(vs)||"")+" | fs "+String(gl.getShaderInfoLog(fs)||""));};this.renderScale110=Math.min(devicePixelRatio,1);this.renderer.setPixelRatio(this.renderScale110);
  this.scene=new THREE.Scene();this.scene.background=new THREE.Color(0x232728);
  this.scene.add(new THREE.HemisphereLight(0xf4e8cf,0x415e60,2.5));const light=new THREE.DirectionalLight(0xffedd1,2.5);light.position.set(4,12,6);this.scene.add(light);
  this.camera=new THREE.OrthographicCamera(-16,16,9,-9,.1,500);
   canvas.addEventListener('pointerup',e=>{if(!this.state||globalThis.bridgeScenePointerAllowed114?.(e,'world')===false)return;const r=canvas.getBoundingClientRect(),ray=new THREE.Raycaster();ray.setFromCamera(new THREE.Vector2((e.clientX-r.left)/r.width*2-1,1-(e.clientY-r.top)/r.height*2),this.camera);const actors=[...this.objects.values()];
    for(const hit of ray.intersectObjects(actors.map(o=>o.mesh),true)){
      let visible=true,actor=null;
      for(let part=hit.object;part;part=part.parent){if(part.visible===false)visible=false;actor??=actors.find(o=>o.mesh===part);}
      if(!visible||!actor||typeof actor.label?.onclick!=='function')continue;
      if(actor.native&&!globalThis.bridgeNativeEntity107?.(hit.object))continue;
      actor.label.onclick();return;
    }
    const point=ray.ray.intersectPlane(new THREE.Plane(new THREE.Vector3(0,1,0),0),new THREE.Vector3());if(point)this.callbacks.ground?.({x:point.x,z:point.z});});
  this.labels110=new Labels110(labels,e=>canvas.dispatchEvent(new PointerEvent('pointerup',{clientX:e.clientX,clientY:e.clientY,bubbles:false})));pcQuestMarksInstall58(this.labels110);this.ownLabel114=document.createElement('div');this.ownLabel114.className='label player';this.ownLabel114.style.color='#8fe7e9';labels.append(this.ownLabel114);
  this.art=new SceneArt(this.scene,callbacks.error,this.renderer.capabilities.maxTextureSize,this.renderer);
  this.spells107=new SpellScene107(this.scene,(uid,kind)=>uid===this.state?.player.uid?this.avatar:this.objects.get((kind===1?'player':'mob')+uid)?.mesh,callbacks.error,callbacks.sound);
  this.spells107.targets=(event,radius)=>{const kind=event.targetKind||((event.sourceKind??1)===2?1:2),candidates=kind===1?[this.state?.player,...this.state?.players??[]]:this.state?.mobs??[];return candidates.filter(m=>m&&m.alive!==false&&m.hp>0&&(m.uid??m.id)!==event.uid&&Math.hypot(m.x-event.toX,m.z-event.toZ)<=radius).map(m=>({x:m.x,z:m.z,target:m.uid??m.id,targetKind:kind}));};
  if(new URL(location.href).searchParams.has('sceneQA'))window.sceneQA=this;
  this.avatar=new THREE.Group();this.scene.add(this.avatar);
  const fallback=new THREE.Mesh(new THREE.CapsuleGeometry(.35,1,4,8),new THREE.MeshStandardMaterial({color:0xd9bb80}));fallback.position.y=.8;this.avatar.add(fallback);
  this.fallback=fallback;this.modelRequest84=0;
  this.shadow=new THREE.Mesh(new THREE.CircleGeometry(.5,24),new THREE.MeshBasicMaterial({color:0x0d1318,transparent:true,opacity:.32,depthWrite:false}));this.shadow.rotation.x=-Math.PI/2;this.shadow.scale.y=.75;this.shadow.renderOrder=1;this.scene.add(this.shadow);
  window.addEventListener('resize',()=>this.resize());this.resize();this.last=performance.now();this.tick();
 }
 loadAvatar84(player){
   if(player.visual?.model){
    const key=JSON.stringify(player.visual);
    if(this.nativePlayerKey107!==key){
     this.nativePlayerKey107=key;
     if(this.nativePlayer107){this.avatar.remove(this.nativePlayer107.mesh);this.nativePlayer107.dispose();}
     this.nativePlayer107=new Native107({...player.visual,x:0,z:0,player:true},this.callbacks.error);
     this.avatar.add(this.nativePlayer107.mesh);
     // Native107 loads the GLB asynchronously.  The render loop used to
     // hide this group while `model` was still undefined and never made it
     // visible again when the promise completed, leaving the local player
     // invisible for the whole scene.  Restore visibility from the same
     // instance after the asset is ready; keep the fallback visible on a
     // failed load so a missing optional native rig cannot hide the actor.
     const nativeRef=this.nativePlayer107;
     nativeRef.entry?.promise?.then(()=>{
      if(this.nativePlayer107!==nativeRef||nativeRef.disposed)return;
      let ready=false;
      nativeRef.model?.traverseVisible(o=>{if(o.isMesh&&o.geometry?.attributes?.position?.count>0)ready=true;});
      nativeRef.mesh.visible=ready;
      if(ready){this.fallback.visible=false;if(this.model)this.model.visible=false;}
     }).catch(()=>{
      if(this.nativePlayer107!==nativeRef||nativeRef.disposed)return;
      nativeRef.mesh.visible=false;
      this.fallback.visible=!this.model;
     });
    }
    let ready=false;
    this.nativePlayer107.model?.traverseVisible(o=>{if(o.isMesh&&o.geometry?.attributes?.position?.count>0)ready=true;});
    this.nativePlayer107.mesh.visible=ready;
    if(ready){this.fallback.visible=false;if(this.model)this.model.visible=false;return;}
   }else if(this.nativePlayer107){
    this.avatar.remove(this.nativePlayer107.mesh);this.nativePlayer107.dispose();
    this.nativePlayer107=null;this.nativePlayerKey107=null;
   }
   if(this.model)this.model.visible=true;
   this.fallback.visible=!this.model;

   const role=({1:'shennong',2:'yinyang',3:'bing'})[player.profession]??'shennong',sex=player.sex===2&&player.profession===1?'junior':player.sex===1?'female':'male',key=role+'-'+sex;
   if(this.avatarKey84===key)return;this.avatarKey84=key;const request=++this.modelRequest84;
   new GLTFLoader().load('characters84/'+key+'.glb',g=>{
    const dispose=root=>root.traverse(o=>{o.geometry?.dispose();for(const m of o.material?(Array.isArray(o.material)?o.material:[o.material]):[]){for(const value of Object.values(m))if(value?.isTexture)value.dispose();m.dispose();}});
    if(request!==this.modelRequest84){dispose(g.scene);return;}
    this.mixer?.stopAllAction();if(this.model){this.avatar.remove(this.model);dispose(this.model);}
    this.model=g.scene;this.fallback.visible=false;this.model.traverse(o=>{if(o.isMesh)for(const material of Array.isArray(o.material)?o.material:[o.material]){material.transparent=false;material.alphaTest=.05;material.depthWrite=true;material.needsUpdate=true;}});
    this.model.scale.setScalar(.8);this.model.rotation.y=Math.PI;this.avatar.add(this.model);this.mixer=new THREE.AnimationMixer(this.model);this.actions=Object.fromEntries(g.animations.map(c=>[c.name,this.mixer.clipAction(c)]));this.current=null;this.play('Game 0001');window.avatarReport84={role:key,actions:Object.keys(this.actions),loaded:true};
   },undefined,e=>{if(request===this.modelRequest84){this.avatarKey84=null;this.callbacks.error(e);}});
  }
resize(){
  this.renderer.setSize(innerWidth,innerHeight);this.pixelsPerUnit=Math.max(32,Math.min(48,innerHeight/16));
  this.camera.left=-innerWidth/(2*this.pixelsPerUnit);this.camera.right=-this.camera.left;this.camera.top=innerHeight/(2*this.pixelsPerUnit);this.camera.bottom=-this.camera.top;this.camera.updateProjectionMatrix();
 }
 play(name){if(this.current===name||!this.actions?.[name])return;this.actions[this.current]?.fadeOut(.12);this.actions[name].reset().fadeIn(.12).play();this.current=name;}
 clear(){this.pcMotion114=null;this.ownLabel114?.remove();this.ownLabel114=null;this.ownStallSign114?.stallSign?.remove();this.ownStallSign114=null;this.spells107.clear();if(this.mount107){this.scene.remove(this.mount107.mesh);this.mount107.dispose();this.mount107=null;}this.state=null;this.pendingMotion113=null;this.predict109.displacement113.clear();this.predict109.ready=false;this.seen=0;if(this.pet84){this.scene.remove(this.pet84.mesh);this.pet84.dispose();this.pet84=null;}this.art.clear();for(const [id]of this.objects)this.remove(id);for(const n of this.damage)n.el.remove();this.damage=[];}
  remove(id){const o=this.objects.get(id);pcClearNoviceBeacon117(o);o.stallSign?.remove();for(const c of [o.pet,o.mount])if(c){this.scene.remove(c.mesh);c.dispose();}this.scene.remove(o.mesh);if(o.peer)o.peer.dispose();else if(o.native)o.native.dispose();else o.mesh.traverse(m=>{m.geometry?.dispose();if(m.material)m.material.dispose();});o.label.remove();this.objects.delete(id);}
  pcCastFeedback114(event){previewPcCast114(this,event);}
  rejectMotionRequest113(request){return rejectPcMotion114(this,request);}
  motionFeedback113(kind,request,direction){if(this.state?.meta?.bridge==='pc6020')return previewPcMotion114(this,kind,request,direction);
   const p=this.state?.player,now=performance.now();if(!p||p.dead||p.weak||now<(this.motionFeedbackUntil113??0))return false;
   const effective=kind==='dodge'&&p.sprinting?'jump':kind,rule=this.state.movementRules113?.[effective];
   if(rule&&p.endurance.current<rule.cost)return false;
   const duration=Math.max(150,rule?.duration??(effective==='jump'?650:450)),animation=effective==='jump'?20:144;
   this.pendingMotion113={request,at:now,skill:effective==='jump'?10201:20050};if(rule&&this.state.motionProtocol114){const d=this.predict109.dir,n=Math.hypot(d.x,d.z),dx=n>.05?d.x/n:Math.sin(this.avatar.rotation.y),dz=n>.05?d.z/n:Math.cos(this.avatar.rotation.y),distance=rule.distance;this.predict109.beginMotion114(request,this.pendingMotion113.skill,{x:Math.max(0,Math.min((this.nav109?.width??65535)-.01,this.predict109.x+dx*distance)),z:Math.max(0,Math.min((this.nav109?.height??65535)-.01,this.predict109.z+dz*distance))},duration);}this.motionFeedbackUntil113=now+duration;
   this.nativeCast107={castAt:Date.now(),animation};this.attackUntil=now+duration;this.play('Game '+String(animation).padStart(4,'0'));
   return true;
  }
  motionReply113(e){if(!e.accepted)this.rejectMotionRequest113?.(e.request);if(!e.accepted)this.predict109.rejectMotion114(e.request);if(this.pendingMotion113?.request!==e.request)return;if(!e.accepted){this.attackUntil=0;this.motionFeedbackUntil113=0;if(this.nativePlayer107)this.nativePlayer107.attackUntil=0;this.pendingMotion113=null;}}
  motionEvent113(e){const predicted=this.pendingMotion113&&this.pendingMotion113.skill===e.skill&&performance.now()-this.pendingMotion113.at<5000;if(predicted){this.pendingMotion113=null;return;}const animation=e.type==='jump'?20:144;this.nativeCast107={castAt:e.at,animation};this.play('Game '+String(animation).padStart(4,'0'));this.attackUntil=performance.now()+(e.type==='jump'?650:450);}
  input109(dir){this.predict109.input(dir);}
  aim111(dx,dy,range=30){const z=dy/CAMERA_SIN,length=Math.hypot(dx,z)||1,distance=Math.min(30,Math.max(2,range));return {x:this.display.x+dx/length*distance,z:this.display.z+z/length*distance};}
  previewAim111(point){this.aimPoint111=point;if(!this.aimArrow111){this.aimArrow111=new THREE.ArrowHelper(new THREE.Vector3(0,0,1),new THREE.Vector3(),5,0xe6c16d,1,.5);this.scene.add(this.aimArrow111);}this.aimArrow111.visible=!!point;}
 followAcceptedEndpoint109(dt){if(this.nav109||!this.predict109.ready)return;const dir=this.predict109.dir??{x:0,z:0};if(Math.hypot(dir.x,dir.z)>.05)return;const target=this.predict109.server;if(!target||![target.x,target.z].every(Number.isFinite))return;const f=1-Math.exp(-Math.min(.05,Math.max(0,dt))*14),p=this.predict109;p.x+=(target.x-p.x)*f;p.z+=(target.z-p.z)*f;}

  update(s){if(!this.ownLabel114){this.ownLabel114=document.createElement('div');this.ownLabel114.className='label player';this.ownLabel114.style.color='#8fe7e9';this.labels.append(this.ownLabel114);}
   if(!(this.lastMotionCorrection114&&performance.now()-this.lastMotionCorrection114<1000))this.predict109.accept(s.player,s.rtt109,!this.state||s.generation!==this.state.generation,s.motion112,s.motion113,s.time,this.nav109);acceptPcAssistPosition114(this,s);acceptPcFrame114(this,s);
  if(!this.state||s.generation!==this.state.generation||s.map.id!==this.state.map.id){this.spells107.clear();for(const [id]of this.objects)this.remove(id);this.pcMotion114=null;this.display.set(s.player.x,0,s.player.z);this.art.select(s.map.id);this.nav109=null;const id=s.map.id;navigation109(id).then(n=>{if(this.state?.map.id===id)this.nav109=n;}).catch(this.callbacks.error);}
  this.mount107=this.companion107(this.mount107,s.mount);this.pet84=this.companion107(this.pet84,s.pet);this.state=s;this.loadAvatar84(s.player);
  for(const e of s.events){
   if(e.id<=this.seen)continue;this.seen=e.id;
   if(e.type==='impact')this.spells107.impact(e);
   if(e.type==='cast-stop'){this.spells107.stop(e.uid);if(e.uid===s.player.uid&&this.nativePlayer107){this.nativePlayer107.attackUntil=0;this.nativePlayer107.continuous111=false;}}
    if(e.type==='cast-aim')this.spells107.aim(e.uid,e.x,e.z);
    if(e.type==='damage'){while(this.damage.length>=64)this.damage.shift().el.remove();const d=document.createElement('div');d.className='damage';d.textContent=e.damage;this.damage.push({el:d,x:e.x,z:e.z,at:performance.now()});}
   if(e.type==='cast'){if(s.meta?.bridge==='pc6020'&&consumePcCastPreview114(this,e))continue;this.spells107.cast(e);if(!animatesCaster112(e))continue;if(e.uid===s.player.uid){this.nativeCast107={castAt:e.at,animation:e.animation,continuous111:e.skill>=1190&&e.skill<=1199};if(Math.hypot(e.toX-e.x,e.toZ-e.z)>.01)this.avatar.rotation.y=Math.atan2(e.toX-e.x,e.toZ-e.z);this.play('Game 0056');this.attackUntil=performance.now()+450;}else{const actor=s.mobs.find(n=>n.id===e.uid)||s.players.find(n=>n.id===e.uid);if(actor){actor.castAt=e.at;actor.animation=e.animation??56;actor.continuous111=e.skill>=1190&&e.skill<=1199;actor.facing110=Math.atan2(e.toX-e.x,e.toZ-e.z);}}}
   if(['dodge','jump'].includes(e.type)&&e.uid===s.player.uid){if(!(s.meta?.bridge==='pc6020'&&this.pcMotion114?.predicted)){this.motionEvent113(e);if(s.meta?.bridge==='pc6020')beginPcMotion114(this,s.player,e);}}
   
    if(e.type==='motion-result113')this.motionReply113(e);
  }
 }
 actor(n,kind,dt){if(n.hidden===true)return 'hidden'+kind+n.id;
  const key=kind+n.id,visualKey=visualKey107(n);let o=this.objects.get(key);if(o&&o.visualKey!==visualKey){this.remove(key);o=null;}
  if(!o){
   const color=kind==='npc'?0x7fae93:kind==='drop'?0xd6b767:n.boss?0xb25044:0x9a7860;
   const native=!pcIsTownPortal114(n)&&!n.guideArrow&&!n.portalEffect&&kind!=='drop'&&n.model?new Native107(n,this.callbacks.error):null;const mesh=pcIsTownPortal114(n)?pcTownPortal114(n):n.portalEffect?pcExitPortal114():n.guideArrow?pcGuideArrow114(n):native?.mesh??(kind==='drop'?new THREE.Group():new THREE.Mesh(new THREE.TorusGeometry(.9,.12,8,32),new THREE.MeshStandardMaterial({color,roughness:.85})));
   mesh.position.set(n.x,native?0:n.exit?.15:kind==='drop'?.35:0,n.z);if(n.exit&&!native&&!n.portalEffect&&!pcIsTownPortal114(n))mesh.rotation.x=-Math.PI/2;this.scene.add(mesh);
   const label=document.createElement('div');label.className='label '+kind;label.onclick=()=>this.callbacks[kind==='npc'?'npc':kind==='drop'?'pickup':'target'](n.id);o={mesh,label,native,visualKey};this.objects.set(key,o);
  }
  if(kind==='drop'){if(!o.drop110){const img=document.createElement('img'),name=document.createElement('span');if(n.icon){img.src=n.icon;img.alt='';o.label.append(img);}o.label.append(name);o.drop110=name;}setText110(o.drop110,n.name+(n.count>1?' ×'+n.count:''));o.label.style.color=n.qualityColor||qualityColor110(n);}else label109(o.label,kind==='mob'?`${n.name}${Number.isFinite(n.level)?' '+n.level+'级':''}${n.hpKnown===false?'':' '+(n.hp>=1e8?(n.hp/1e8).toFixed(0)+'亿':n.hp)+'/'+(n.maxHp>=1e8?(n.maxHp/1e8).toFixed(0)+'亿':n.maxHp)}`:n.name);
   const movieCull114=globalThis.bridgeNoviceCameraFrame114?.(this)??{x:this.display.x,z:this.display.z,zoom:1};if(Math.abs(n.x-movieCull114.x)>-this.camera.left/movieCull114.zoom+4||Math.abs(n.z-movieCull114.z)>this.camera.top/CAMERA_SIN/movieCull114.zoom+5){o.mesh.visible=false;o.label.style.display='none';return key;}
   if(n.guideArrow)o.mesh.rotation.y=-(n.direction??0)*Math.PI/180;if(n.portalEffect)pcUpdateExitPortal114(o.mesh,this.state.time);if(pcIsTownPortal114(n))pcUpdateTownPortal114(o.mesh,this.state.time,this.camera);o.label.dataset.pcQuest114=n.questReady?'ready':n.questTarget?'target':'';if(kind==='mob'){o.label.style.color=n.nameColor||'';o.label.dataset.grade=n.grade||'normal';}o.native?.update({...n,serverTime111:this.state.time},dt);pcUpdateNoviceBeacon117(this,o,n,dt);o.mesh.visible=o.native?n.dead!==true:(n.hp==null||n.hp>0);o.label.style.display=o.mesh.visible?'':'none';if(!o.native){o.mesh.position.x+=(n.x-o.mesh.position.x)*.25;o.mesh.position.z+=(n.z-o.mesh.position.z)*.25;}
  this.place(o.label,o.mesh.position.x,kind==='drop'?.8:2.8,o.mesh.position.z);return key;
 }
  companion107(actor,data){
   const key=visualKey107(data);
   if(actor&&actor.visualKey107!==key){this.scene.remove(actor.mesh);actor.dispose();actor=null;}
   if(data&&!actor){actor=new Native107(data,this.callbacks.error);actor.visualKey107=key;this.scene.add(actor.mesh);}
   return actor;
  }
  peerActor84(n,dt){
   const key='player'+n.id,visualKey=visualKey107(n.visual);let o=this.objects.get(key);
   if(o&&o.visualKey!==visualKey){this.remove(key);o=null;}
   if(!o){const peer=n.visual?new Native107({...n,...n.visual,player:true},this.callbacks.error):new Peer84(n),label=document.createElement('div');label.className='label player';label.style.color='#8fe7e9';label.onclick=()=>this.callbacks.player?.(n.id);o={peer,mesh:peer.mesh,label,visualKey};this.scene.add(o.mesh);this.objects.set(key,o);}
   o.pet=this.companion107(o.pet,n.pet);o.mount=this.companion107(o.mount,n.mount);
   o.peer.update({...n,player:true},dt);o.mesh.position.y=o.mount?1.1:0;
   o.pet?.update(n.pet,dt);o.mount?.update({...n.mount,x:o.mesh.position.x,z:o.mesh.position.z},dt);
   if(o.mount)o.mount.mesh.rotation.y=o.mesh.rotation.y;
   label109(o.label,n.name+(n.teammate?' · 队友':''));o.label.style.display=o.mesh.visible?'':'none';this.place(o.label,o.mesh.position.x,o.mount?3.9:2.8,o.mesh.position.z);pcStallSign114(this,o,n,o.mesh.position);return key;
  }
 place(el,x,y,z){const p=this.projection109.set(x,y,z).project(this.camera);this.labels110.place(el,(p.x*.5+.5)*innerWidth,(-p.y*.5+.5)*innerHeight,!(p.z>1||p.z< -1||Math.abs(p.x)>1.1||Math.abs(p.y)>1.1));}
 tick(){
  requestAnimationFrame(()=>this.tick());const now=performance.now(),dt=Math.min(.05,(now-this.last)/1000);this.last=now;this.frames109=(this.frames109??0)+1;if(now-(this.fpsAt109??0)>1000){window.renderReport109={fps:Math.round(this.frames109*1000/(now-(this.fpsAt109??now-1000))),drawCalls:this.renderer.info.render.calls,triangles:this.renderer.info.render.triangles,textures:this.renderer.info.memory.textures,actors:this.objects.size};if(!this.scaleAt110||now-this.scaleAt110>3000){const fps110=window.renderReport109.fps,next110=fps110<28?Math.max(.65,this.renderScale110-.1):fps110>55?Math.min(1,this.renderScale110+.05):this.renderScale110;if(Math.abs(next110-this.renderScale110)>.01){this.renderScale110=next110;this.renderer.setPixelRatio(next110);}this.scaleAt110=now;}window.renderReport109.renderScale110=this.renderScale110;this.fpsAt109=now;this.frames109=0;}if(document.hidden)return;this.labels110.begin();
  if(this.state){
   this.pet84?.update(this.state.pet,dt);const p=this.state.player,old=this.display.clone();if(p.actionsBlocked){this.pcMotion114=null;this.predict109.dir={x:0,z:0};this.predict109.x=p.x;this.predict109.z=p.z;}else this.predict109.tick(dt,this.nav109);this.followAcceptedEndpoint109(dt);if(!advancePcMotion114(this,p,now))this.display.lerp(new THREE.Vector3(this.predict109.x,p.y??0,this.predict109.z),Math.min(1,dt*(now<(this.predict109.fastUntil113??0)?60:20)));
   const travel110=Math.hypot(this.display.x-old.x,this.display.z-old.z),input111=this.predict109.dir,hasInput111=Math.hypot(input111.x,input111.z)>.05;const moving=!p.dead&&!p.actionsBlocked&&(hasInput111||pcWalking114(this,p,travel110,dt,now));this.nativePlayer107?.update({...p,...this.nativeCast107,x:0,z:0,moving,player:true},dt);if(hasInput111&&now>(this.attackUntil??0))this.avatar.rotation.y=Math.atan2(input111.x,input111.z);else if(p.routeActive&&p.routeTarget&&Number.isFinite(p.routeTarget.x)&&Number.isFinite(p.routeTarget.z)&&now>(this.attackUntil??0))this.avatar.rotation.y=Math.atan2(p.routeTarget.x-this.display.x,p.routeTarget.z-this.display.z);else if(!hasInput111&&this.predict109.remoteMoving111&&now>(this.attackUntil??0))this.avatar.rotation.y=this.predict109.remoteFacing111;this.avatar.position.copy(this.display);if(this.ownLabel114){label109(this.ownLabel114,p.name||'');this.ownLabel114.style.display=p.name&&this.avatar.visible!==false?'':'none';this.place(this.ownLabel114,this.display.x,this.mount107?3.9:2.8,this.display.z);}this.ownStallSign114??={mesh:this.avatar};pcStallSign114(this,this.ownStallSign114,p,this.display);if(this.mount107){this.mount107.update({...this.state.mount,x:this.display.x,z:this.display.z,moving},dt);this.mount107.mesh.position.set(this.display.x,0,this.display.z);this.avatar.position.y+=this.state.meta?.bridge==='pc6020'?1.45:1.1;this.mount107.mesh.rotation.y=this.avatar.rotation.y;}
   if(p.stalling||p.sitting||p.weak||now>(this.attackUntil??0))this.play('Game '+String(playerMovementAction112(p,moving)).padStart(4,'0'));
   const movieCamera114=globalThis.bridgeNoviceCameraFrame114?.(this)??{x:this.display.x,z:this.display.z,zoom:1};if(this.camera.zoom!==movieCamera114.zoom){this.camera.zoom=movieCamera114.zoom;this.camera.updateProjectionMatrix();}this.camera.position.set(movieCamera114.x,CAMERA_SIN*100,movieCamera114.z+CAMERA_COS*100);this.camera.lookAt(movieCamera114.x,0,movieCamera114.z);this.camera.updateMatrixWorld();
   if(this.aimPoint111&&this.aimArrow111){const direction=new THREE.Vector3(this.aimPoint111.x-this.display.x,0,this.aimPoint111.z-this.display.z),length=direction.length();this.aimArrow111.position.set(this.display.x,.2,this.display.z);this.aimArrow111.setDirection(direction.normalize());this.aimArrow111.setLength(Math.min(10,length),1,.5);}this.shadow.visible=true;this.shadow.position.set(this.display.x,.01,this.display.z);this.art.update(movieCamera114.x,movieCamera114.z,innerWidth,innerHeight,this.pixelsPerUnit*movieCamera114.zoom);
    const live=new Set();for(const n of this.state.players??[])live.add(this.peerActor84(n,dt));for(const [kind,list]of [['mob',this.state.mobs],['npc',this.state.npcs],['drop',this.state.drops]])for(const n of list)live.add(this.actor(n,kind,dt));for(const [id]of this.objects)if(!live.has(id))this.remove(id);
  }else{this.camera.position.set(0,50,86.6);this.camera.lookAt(0,0,0);this.avatar.position.set(0,0,0);this.shadow.visible=false;}
  if(this.mixer&&this.model?.visible){this.mixer.update(dt);this.avatar.updateMatrixWorld(true);const box=new THREE.Box3().setFromObject(this.model,true).applyMatrix4(this.avatar.matrixWorld.clone().invert()),c=box.getCenter(new THREE.Vector3());this.model.position.x-=c.x;this.model.position.y-=box.min.y;this.model.position.z-=c.z;}
  for(let i=this.damage.length-1;i>=0;i--){const n=this.damage[i],t=(now-n.at)/900;if(t>1){n.el.remove();this.damage.splice(i,1);}else{this.place(n.el,n.x,2+t*2,n.z);n.el.style.opacity=1-t;}}
  this.spells107.owner110=this.state?.player?.uid;this.spells107.update(dt,this.camera);this.renderer.render(this.scene,this.camera);this.labels110.draw();
 }
}
