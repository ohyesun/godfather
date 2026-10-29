// This is presentation of a server-owned path, never a client position request.
export class Displacement113 {
 constructor(){this.clear();}
 clear(){this.path=null;this.progress=0;this.finished=null;}
 accept(path,serverTime,rtt,now=performance.now()){
  if(!path){const ended=!!this.path;this.clear();return ended;}
  if(!path.from||!path.to||![path.from.x,path.from.z,path.to.x,path.to.z,path.start,path.duration,serverTime].every(Number.isFinite)||path.duration<=0||path.duration>10000)return false;
  if(path.active===false){const ended=!!this.path||this.finished!==path.id;this.path=null;this.finished=path.id;return ended;}
  if(this.finished===path.id)return false;
  const same=this.path?.id===path.id;
  if(!same)this.progress=0;
  // RTT is used only for drawing; cap the lead during spikes, keep progress monotonic.
  this.path={...path,received:now,elapsed:Math.max(0,serverTime-path.start),lead:Math.min(200,Math.max(0,rtt||0)/2)};
  return false;
 }
 sample(now=performance.now()){
  const p=this.path;if(!p)return null;
  this.progress=Math.max(this.progress,Math.min(1,(p.elapsed+p.lead+Math.max(0,now-p.received))/p.duration));
  return {x:p.from.x+(p.to.x-p.from.x)*this.progress,z:p.from.z+(p.to.z-p.from.z)*this.progress};
 }
}
