export function mergeState110(previous,incoming){
 if(!incoming?.state110)return incoming;
 if(previous?.state110>incoming.state110)return previous;if(previous?.state110===incoming.state110){if(previous.meta?.bridge==='pc6020'&&incoming.meta?.bridge==='pc6020'&&incoming.fullState110)return incoming;return previous;}
 if(incoming.fullState110)return incoming;
 if(!previous||previous.state110!==incoming.baseState110)throw Error('角色数据需要重新同步');
 const next={...previous,...incoming.changesState110,state110:incoming.state110,fullState110:true};for(const key of incoming.removeState110??[])delete next[key];return next;
}
