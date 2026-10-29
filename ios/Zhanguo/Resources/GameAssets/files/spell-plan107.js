// Visual scheduling only. Damage, targets and rewards remain server-authoritative.
export function spellPlan107(data,event,{random=Math.random,targets=()=>[],combos=new Map(),now=0}={}){
 const rows=[];
 function visit(id,start,ev,path){
  if(path.length>=13||path.includes(id)||rows.length>=512)return;
  const a=data.magic[id];if(!a)return;const next=[...path,id];
  for(const c of a.children??[])visit(c.id,start+c.start,ev,next);
  for(const item of a.items??[])rows.push({id:item.id,start:start+item.start,event:ev});
  if(a.combo){const key=ev.sourceKind+':'+ev.uid+':'+id,old=combos.get(key),previous=a.combo[old?.index],index=old&&old.index<9&&a.combo[old.index+1]?.id&&previous?.id&&previous.window&&now-old.at<previous.window?old.index+1:0;combos.set(key,{index,at:now});const item=a.combo[index];if(item?.id)rows.push({id:item.id,start,event:ev});}
  const f=a.fan;if(!f)return;
  const selected=f.targeted?targets(ev,f.radius).slice(0,f.count):null;
  const count=Math.min(128,selected?.length??f.count),width=f.degrees*Math.PI/180;
  for(let j=0;j<count;j++){
   let ticket=random()*100,choice=f.choices[0];
   for(const c of f.choices){ticket-=c.weight;if(ticket<=0){choice=c;break;}}
   if(!choice?.id)continue;
   const angle=(f.edgeStart?0:-width/2)+(f.uniform?(count>1?width*j/(count-1):width/2):random()*width);
   const dx=ev.toX-ev.x,dz=ev.toZ-ev.z,c=Math.cos(angle),s=Math.sin(angle);
   const target=selected?.[j];
   const out={...ev,...(target?.target?{target:target.target,targetKind:target.targetKind}:{}),toX:target?.x??ev.x+dx*c-dz*s,toZ:target?.z??ev.z+dx*s+dz*c};
   const index=f.order===2?count-1-j:j;
   const delay=f.order===1||f.order===2?(count>1?Math.trunc(f.duration/(count-1))*index:0):Math.trunc(random()*f.duration);
   rows.push({id:choice.id,start:start+delay,event:out});
  }
 }
 visit(event.action,0,event,[]);return rows;
}
