export function mergeFrame109(previous,incoming){
 if(!incoming.frame109||incoming.full109)return incoming;
 if(!previous||previous.frame109!==incoming.base109)throw Error('画面同步已重置，正在重连');
 const frame={...previous,...incoming.changes109,time:incoming.time,ack:incoming.ack,events:incoming.events,frame109:incoming.frame109};
 for(const [key,delta]of Object.entries(incoming.entities109??{})){const rows=new Map((previous[key]??[]).map(v=>[v.id,v]));for(const id of delta.remove)rows.delete(id);for(const value of delta.put)rows.set(value.id,value);for(const p of delta.patch110??[]){if(!rows.has(p.id))throw Error('画面同步已重置，正在重连');const value={...rows.get(p.id),...p.set};for(const name of p.unset??[])delete value[name];rows.set(p.id,value);}frame[key]=[...rows.values()];}
 return frame;
}
