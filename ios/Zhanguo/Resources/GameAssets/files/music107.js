// Stream long native music tracks instead of keeping decoded PCM in the FX cache.
export class SceneMusic107 {
 constructor(audio){this.audio=audio;this.tracks=new Map();this.scene=null;this.generation=0;document.addEventListener('pointerdown',()=>this.resume(),{passive:true});document.addEventListener('visibilitychange',()=>document.hidden?this.pause():this.resume());}
 pause(){for(const t of this.tracks.values())t.element.pause();}
 resume(){if(!this.audio.enabled||document.hidden)return;for(const t of this.tracks.values())t.element.play().catch(e=>{if(e.name!=='NotAllowedError'&&e.name!=='AbortError')this.audio.error(e);});}
 clear(){this.generation++;this.scene=null;for(const t of this.tracks.values()){t.element.pause();t.element.removeAttribute('src');t.element.load();}this.tracks.clear();}
 async select(map,region=0){const key=map+':'+region;if(key===this.scene)return;this.scene=key;const generation=++this.generation;await this.audio.ready;if(generation!==this.generation)return;const d=this.audio.data,m=d?.mapSounds?.[map],r=m?.regions?.[region]??m;for(const [kind,name]of [['music',r?.music],['ambience',r?.ambience]]){
   const suffix='/'+String(name).toLowerCase(),path=Object.keys(d?.events??{}).find(p=>p.endsWith(suffix)),event=d?.events?.[path],sound=event?.sounds?.[0],wave=sound?.waves?.[0];
   // Region data sometimes names an event removed from the shipped FEV. Silence
   // is preferable to substituting an unrelated track.
   const file=wave?.file??d?.music?.[String(name).toLowerCase()],prior=this.tracks.get(kind);if(prior?.file===file)continue;
   if(prior){prior.element.pause();prior.element.removeAttribute('src');prior.element.load();this.tracks.delete(kind);}if(!file)continue;
   const element=new Audio('audio107/'+file);element.preload='none';element.loop=true;element.volume=Math.min(1,(kind==='music'?.24:.18)*(event?.gain??1)*(sound?.gain??1));element.onerror=()=>this.audio.error(Error('场景声音读取失败: '+file));this.tracks.set(kind,{file,element});
  }this.resume();}
}
