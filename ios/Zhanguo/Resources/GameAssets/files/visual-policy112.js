// Native material address flags are per axis. UV scaling is centered so that
// an expanding impact does not tile into a grid or crawl across the mesh.
export function materialAddress112(header=[]){return {repeatU:header[4]===1,repeatV:header[5]===1};}
// Splash reports can share the same impact location for many overlapped targets.
// Coalesce only their presentation; hit, damage and loot events are untouched.
export function allowChildVisual112(recent,event,now){
 if(event.skill!==0||event.action!==4205)return true;
 for(const [key,v]of recent)if(now-v.at>150)recent.delete(key);
 if([...recent.values()].some(v=>v.uid===event.uid&&Math.hypot(v.x-event.toX,v.z-event.toZ)<.75))return false;
 recent.set(event.id??now,{uid:event.uid,x:event.toX,z:event.toZ,at:now});return true;
}
export const animatesCaster112=e=>Number.isInteger(e.animation)&&e.animation>0;
