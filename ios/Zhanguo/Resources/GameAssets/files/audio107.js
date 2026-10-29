// Native shipped samples. Sound playback never controls combat or gameplay state.
export class GameAudio107 {
 constructor({error=()=>{},maxVoices=20,maxBytes=24*1024*1024}={}){
  this.error=error;this.maxVoices=maxVoices;this.maxBytes=maxBytes;this.cache=new Map();this.voices=new Set();this.pending=new Set();this.generation=0;this.enabled=localStorage.getItem('audio107')!=='off';
  this.ready=fetch('audio107/catalog.json').then(r=>{if(!r.ok)throw Error('声音资源读取失败');return r.json();}).then(d=>this.data=d).catch(error);
  document.addEventListener('pointerdown',()=>this.unlock(),{passive:true});
  document.addEventListener('visibilitychange',()=>document.hidden?this.suspend():this.unlock());
 }
 async unlock(){if(!this.enabled||document.hidden)return;try{if(!this.context){const Context=window.AudioContext||window.webkitAudioContext;if(!Context)return;this.context=new Context();this.master=this.context.createGain();this.master.gain.value=.55;this.master.connect(this.context.destination);}await this.context.resume();}catch(e){this.error(e);}}
 setEnabled(value){this.enabled=!!value;localStorage.setItem('audio107',this.enabled?'on':'off');if(this.enabled)this.unlock();else this.suspend();}
 suspend(){this.clear();this.context?.suspend().catch(this.error);}
 clear(){this.generation++;for(const p of this.pending)p.cancelled=true;for(const v of [...this.voices])this.stop(v);}
 stopOwner(owner,prefix=false){for(const p of this.pending)if(prefix?p.owner?.startsWith(owner):p.owner===owner)p.cancelled=true;for(const v of [...this.voices])if(prefix?v.owner?.startsWith(owner):v.owner===owner)this.stop(v);}
 stop(v){if(!this.voices.delete(v))return;try{v.source.stop();}catch{}v.source.disconnect();v.gain.disconnect();v.entry.refs--;this.prune();}
 prune(){let bytes=0;const idle=[];for(const [key,e]of this.cache){bytes+=e.bytes||0;if(!e.refs&&e.buffer)idle.push([key,e]);}idle.sort((a,b)=>a[1].at-b[1].at);while(idle.length&&(bytes>this.maxBytes||this.cache.size>48)){const [key,e]=idle.shift();this.cache.delete(key);bytes-=e.bytes;}}
 async load(file){let e=this.cache.get(file);if(!e){e={refs:0,at:performance.now(),bytes:0};e.promise=fetch('audio107/'+file).then(r=>{if(!r.ok)throw Error('声音资源不存在: '+file);return r.arrayBuffer();}).then(b=>this.context.decodeAudioData(b)).then(buffer=>{e.buffer=buffer;e.bytes=buffer.length*buffer.numberOfChannels*4;return buffer;}).catch(error=>{this.cache.delete(file);throw error;});this.cache.set(file,e);}e.at=performance.now();return e;}
 async file(file,{gain=1,delay=0,loop=false,tag=file,owner=null,limit=this.maxVoices}={}){
  if(!file||!this.enabled||document.hidden||!this.context||this.context.state!=='running')return null;
  const generation=this.generation,pending={owner,cancelled:false};this.pending.add(pending);let entry;
  try{entry=await this.load(file);entry.refs++;const buffer=await entry.promise;if(pending.cancelled||generation!==this.generation||!this.enabled||document.hidden){entry.refs--;this.prune();return null;}
   const matching=[...this.voices].filter(v=>v.tag===tag);while(matching.length>=Math.max(1,limit))this.stop(matching.shift());
   while(this.voices.size>=this.maxVoices)this.stop(this.voices.values().next().value);
   const source=this.context.createBufferSource(),volume=this.context.createGain();source.buffer=buffer;source.loop=loop;volume.gain.value=Math.max(0,Math.min(2,gain));source.connect(volume);volume.connect(this.master);const v={source,gain:volume,entry,tag,owner};this.voices.add(v);source.onended=()=>this.stop(v);source.start(this.context.currentTime+Math.max(0,Math.min(10,delay)));this.prune();return v;
  }catch(e){if(entry?.refs)entry.refs--;this.error(e);this.prune();return null;}finally{this.pending.delete(pending);}
 }
 sample(name,options){const sample=this.data?.samples?.[name.toLowerCase()];return sample?this.file(sample.file,options):Promise.resolve(null);}
 async event(name,{gain=1,loop=false,owner=null}={}){const e=this.data?.events?.[name.toLowerCase()];if(!e)return [];const tag=name.toLowerCase();return Promise.all(e.sounds.map(s=>{const total=s.waves.reduce((n,w)=>n+Math.max(0,w.weight),0);if(!total)return null;let n=Math.random()*total;const wave=s.waves.find(w=>(n-=Math.max(0,w.weight))<0)||s.waves.at(-1);return this.file(wave.file,{tag,owner,limit:(e.max||4)*e.sounds.length,gain:gain*e.gain*s.gain,delay:s.delay,loop:loop&&s.loop});}));}
}
