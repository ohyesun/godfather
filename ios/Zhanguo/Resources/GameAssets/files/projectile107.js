export function projectilePoint107(source,target,speed,age,returnAfter,owner){
 const dx=target.x-source.x,dz=target.z-source.z,len=Math.hypot(dx,dz)||1;
 const outbound=returnAfter==null?age:Math.min(age,returnAfter),x=source.x+dx/len*speed*outbound,z=source.z+dz/len*speed*outbound;
 if(returnAfter==null||age<=returnAfter)return {x,z};
 const end=owner??source,rx=end.x-x,rz=end.z-z,remaining=Math.hypot(rx,rz),step=Math.min(remaining,speed*(age-returnAfter));
 return {x:x+rx/(remaining||1)*step,z:z+rz/(remaining||1)*step};
}
export function seekProjectile107(position,target,speed,dt){
 const dx=target.x-position.x,dz=target.z-position.z,length=Math.hypot(dx,dz),step=Math.min(length,Math.max(0,speed*dt));
 return {x:position.x+dx/(length||1)*step,z:position.z+dz/(length||1)*step};
}
