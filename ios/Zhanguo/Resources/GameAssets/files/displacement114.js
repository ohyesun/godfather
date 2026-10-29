// Visual prediction is correlated with a request, never sent as an authoritative position.
export class Displacement113 {
 constructor(){this.clear();}
 clear(){this.path=null;this.progress=0;this.finished=null;this.postStarted=false;}
 predict(request,skill,from,to,duration,now=performance.now()){
  this.path={id:'pending:'+request,request114:request,skill,from:{...from},to:{...to},duration,localStart:now,predicted:true,confirmed:false,serverDone:false};
  this.progress=0;this.postStarted=false;
 }
 reject(request){if(this.path?.request114!==request)return false;this.clear();return true;}
 accept(path,serverTime,rtt,now=performance.now()){
  const pending=this.path?.predicted&&!this.path.confirmed;
  if(pending&&path?.request114!==this.path.request114)return false;
  if(!path){const ended=!!this.path;this.clear();return ended;}
  if(!path.from||!path.to||![path.from.x,path.from.z,path.to.x,path.to.z,path.start,path.duration,serverTime].every(Number.isFinite)||path.duration<=0||path.duration>10000)return false;
  const same=this.path&&(this.path.id===path.id||pending&&this.path.request114===path.request114);
  if(!same&&path.active===false){this.finished=path.id;return false;}
  if(!same&&this.finished===path.id)return false;
  if(!same){this.progress=0;this.postStarted=false;}
  const prior=same?this.path:null;
  const localStart=prior?.predicted?prior.localStart:now-Math.max(0,serverTime-path.start)-Math.min(200,Math.max(0,rtt||0)/2);
  this.path={...path,localStart,predicted:!!prior?.predicted,confirmed:true,serverDone:path.active===false};return false;
 }
 sample(now=performance.now()){
  const p=this.path;if(!p)return null;
  if(p.predicted&&!p.confirmed&&now-p.localStart>5000)return {expired:true};
  this.progress=Math.max(this.progress,Math.min(1,Math.max(0,now-p.localStart)/p.duration));
  return {x:p.from.x+(p.to.x-p.from.x)*this.progress,z:p.from.z+(p.to.z-p.from.z)*this.progress,done:this.progress>=1};
 }
 finish(){const id=this.path?.id;this.clear();this.finished=id;}
}
