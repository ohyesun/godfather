let data={},feet={},sink={},terrain=null,scene=null,selection=0;
export const actionAudioReady107=Promise.all([fetch('audio107/actions.json').then(r=>r.json()).then(d=>data=d),fetch('audio107/footsteps.json').then(r=>r.json()).then(d=>feet=d)]).catch(()=>{});
export function setActionAudio107(value){sink=value;}
export async function setFootstepScene107(name){
 if(scene===name)return;scene=name;terrain=null;const ticket=++selection;
 await actionAudioReady107;const info=feet.maps?.[name];if(!info||ticket!==selection)return;
 try{const r=await fetch(`audio107/terrain/${name}.bin`);if(!r.ok)return;const cells=new Uint8Array(await r.arrayBuffer());if(ticket===selection&&cells.length===info.width*info.height)terrain={...info,cells};}catch{}
}
export function footstepEvent107(type,object){
 if(!terrain)return;const p=object.getWorldPosition(object.position.clone()),x=Math.floor(p.x),y=Math.floor(p.z);
 if(x<0||y<0||x>=terrain.width||y>=terrain.height)return;
 return feet.terrain?.[`${terrain.cells[y*terrain.width+x]}:${type}`];
}
export class ActionAudio107{
 constructor(model,object){this.model=model;this.object=object;this.owner='actor:'+object.uuid;}
  play(id,duration,once){sink.stop?.(this.owner);this.id=id;this.age=0;this.duration=duration;this.once=once;}
  update(dt){if(this.id==null||!(dt>0))return;const from=this.age,to=from+dt;this.age=to;const duration=this.duration||1;
   const cues=data[this.model]?.[this.id]??[],contacts=feet.actions?.[this.model]?.[this.id]??[];
   // Long background stalls must not replay minutes of stale sounds.
   for(let loop=this.once?0:Math.max(Math.floor(from/duration),Math.floor(to/duration)-1);loop<=(this.once?0:Math.floor(to/duration));loop++){
    for(const cue of cues){const at=loop*duration+cue.delay/1000;if(at>=from&&at<to&&to-at<.25)sink.play?.(cue.event,this.object,this.owner);}
    for(const cue of contacts){const at=loop*duration+cue.delay;if(at>=from&&at<to&&to-at<.25){const event=footstepEvent107(cue.type,this.object);if(event)sink.play?.(event,this.object,this.owner);}}
   }
 }
 dispose(){sink.stop?.(this.owner);this.id=null;}
}
