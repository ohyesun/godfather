// 12/5e is a toggle: only an observed own pose permits its opposite action.
function bridgeSitHudInstall114({hud,mount,state,world,mutate,error,toast}){
 const button=document.createElement('button');button.id='sit114';button.type='button';
 button.textContent='打坐';button.hidden=true;hud.append(button);
 let currentScope=null,local=null,lastMessage='';
 const scope=(s,w)=>JSON.stringify([s?.character?.id,s?.inputGeneration,w?.generation,w?.sceneToken,
  w?.map?.id,w?.map?.instanceId,w?.player?.uid]);
 // The HUD is installed before login, when there is no role or study feed.
 // Two missing IDs must not be treated as a matching owner.
 const feed=s=>Number.isInteger(s?.character?.id)&&s.character.id>0&&s?.studyDao&&
  s.studyDao.characterId===s.character.id?s.studyDao:{};
 const ready=(s,w,f)=>s?.meta?.bridge==='pc6020'&&Number.isInteger(s?.character?.id)&&s.character.id>0&&
  w?.map?.id===s?.position?.map&&f.ready===true&&typeof f.sitting==='boolean';
 const reconcile=f=>{
  const result=f.lastResult;
  if(local&&result&&JSON.stringify(result)!==local.resultBefore&&result.operation===local.operation&&
   (!local.key||result.key===local.key)&&['confirmed','not_sent'].includes(result.outcome))local=null;
 };
 function update(w=world()){
  const s=state(),f=feed(s),next=scope(s,w);
  if(next!==currentScope){currentScope=next;local=null;lastMessage='';}
  button.hidden=s?.meta?.bridge!=='pc6020';if(button.hidden)return;
  reconcile(f);
  if(local&&!local.key&&f.pending?.operation===local.operation)local.key=f.pending.key;
  button.textContent=f.sitting===true?'起身':'打坐';button.setAttribute('aria-label',button.textContent);
  button.setAttribute('aria-pressed',String(f.sitting===true));
  const waiting=!!local||!!f.pending||f.unknown===true;
  const unavailable=!ready(s,w,f)||w?.player?.dead===true;
  button.disabled=waiting||unavailable||(f.sitting===true?f.canStand:f.canSit)!==true;
  const reason=f.unknown===true||local?.status==='unknown'?'打坐操作结果尚未确认，请勿重复切换':
   waiting?'正在等待区服确认打坐状态':unavailable?'打坐状态尚未报告':
   f.sitting===true&&f.activities?.length?'起身后，已购时长仍继续计时':button.textContent;
  button.title=reason;button.setAttribute('aria-busy',String(waiting));
  // Mount may be hidden without an owned horse; its computed position remains
  // the anchor so an ordinary character still has a visible sit shortcut.
  if(typeof getComputedStyle==='function'&&typeof button.getBoundingClientRect==='function'){
   const style=getComputedStyle(mount),bottom=parseFloat(style.bottom),left=parseFloat(style.left);
   if(Number.isFinite(left))button.style.left=left+'px';
   if(Number.isFinite(bottom))button.style.bottom=Math.max(8,bottom-button.getBoundingClientRect().height-6)+'px';
  }
  const result=f.lastResult;
  if(result&&['sit','stand'].includes(result.operation)&&result.message){
   const fingerprint=JSON.stringify(result);
   if(fingerprint!==lastMessage){lastMessage=fingerprint;toast?.(result.message);}
  }
 }
 button.onclick=async event=>{
  event?.preventDefault();event?.stopPropagation();update();
  const s=state(),w=world(),f=feed(s);
  if(button.disabled||!ready(s,w,f))return;
  const record={operation:f.sitting?'stand':'sit',scope:currentScope,resultBefore:JSON.stringify(f.lastResult),status:'pending'};
  local=record;update(w);
  try{
   await mutate({type:'study_dao',operation:record.operation});
   if(local!==record||record.scope!==scope(state(),world()))return;
   update();
  }catch(e){
   if(local!==record||record.scope!==scope(state(),world()))return;
   const noWrite=['communication_unavailable','stale_revision','invalid_key','invalid_study_dao'].includes(e?.code)||
    /^study_(action|owner|unobserved|pending|pose)$/.test(e?.code||'');
   if(noWrite)local=null;else record.status='unknown';
   error?.(e);update();
  }
 };
 update();return {update,button};
}

// Input gestures only: the shared server owns range, collision and damage.
export function bindAim111(button,skill,{cast,hold,aim,preview,allowHold=true}){
 let pointer=null,start=null,point=null,drag=false,fired=false,timer;
 const continuous=allowHold&&(skill.type===17||[1230,3280].includes(skill.id-skill.id%10));
 const clear=()=>{clearTimeout(timer);pointer=null;point=null;preview(null);if(continuous)hold(0,null);};
 button.onpointerdown=e=>{if(button.disabled||pointer!==null)return;e.preventDefault();pointer=e.pointerId;start={x:e.clientX,y:e.clientY};point=null;drag=false;fired=false;button.setPointerCapture(pointer);if(continuous)timer=setTimeout(()=>{if(pointer===null)return;fired=true;hold(skill.id,point);cast(skill.id,point);},160);};
 button.onpointermove=e=>{if(e.pointerId!==pointer)return;const dx=e.clientX-start.x,dy=e.clientY-start.y;if(Math.hypot(dx,dy)<12&&!drag)return;drag=true;point=aim(dx,dy,skill.range??30);preview(point);if(continuous&&fired)hold(skill.id,point);};
 button.onpointerup=e=>{if(e.pointerId!==pointer)return;e.preventDefault();if(!fired)cast(skill.id,point);clear();};
 button.onpointercancel=button.onlostpointercapture=()=>{if(pointer!==null)clear();};
 // Real pointer clicks are handled above; accessibility and keyboard clicks remain valid.
 button.onclick=e=>{if(e.detail===0&&!button.disabled)cast(skill.id,null);};
}
// Shared by client panels and world controls, using the PC map's grid distance.
function pcInteractionPosition114(player){
 const p=player?.nativePosition;
 if(p&&Number.isFinite(p.x)&&Number.isFinite(p.z??p.y))return p;
 return player?.positionSource==='native-dispatched-self'?null:player;
}
function pcNoviceBeacon117(n){
 return n?.noviceBeacon===true&&[1846,1847].includes(Number(n.template??n.baseID));
}
function pcNoviceBeaconLabel117(n){
 if(!pcNoviceBeacon117(n))return '';
 if(n.beaconLit===true)return '烽火台已点燃';
 if(n.beaconIgnitePending===true||['pending','submitted'].includes(n.beaconIgniteStatus))return '正在点燃烽火台…';
 if(n.beaconIgniteStatus==='unknown')return '点火结果待确认';
 return n.beaconIgnitable===true?(n.interactionLabel||'点燃烽火台'):'烽火台';
}
function pcNoviceBeaconBusy117(n){
 return pcNoviceBeacon117(n)&&(n.beaconIgnitable!==true||n.beaconLit===true||n.beaconIgnitePending===true||
  ['pending','submitted','unknown'].includes(n.beaconIgniteStatus));
}
function pcNpcInRange114(n,p){
 if(!n||!p||p.dead||n.guideArrow||n.interactable===false)return false;
 const r=Number.isInteger(n.interactionRange)&&n.interactionRange>=0?n.interactionRange:2;
 const native=pcInteractionPosition114(p),x=native?.x,y=native?.z??native?.y,nx=n.x,ny=n.z??n.y;
 return [x,y,nx,ny].every(Number.isFinite)&&(pcNoviceBeacon117(n)?Math.hypot(nx-x,ny-y):Math.max(Math.abs(nx-x),Math.abs(ny-y)))<=r;
}
function pcNearestTalkNpc114(w){
 if(!w?.player||w.player.dead)return null;
 const native=pcInteractionPosition114(w.player);if(!native)return null;
 let best=null,limit=Infinity;
 for(const n of w.npcs??[]){
  if(pcNoviceBeacon117(n)&&n.beaconLit===true)continue;
  if(n.pet===true||n.interactable===false||Number(n.owner||n.masterId||0)>0||
     [1900,1901,1902,1903,1904,1905,1906,1907].includes(Number(n.template??n.baseID))||
     n.camp!==0&&!pcNoviceBeacon117(n)||!Number.isInteger(n.id??n.uid)||(n.id??n.uid)<=0||
     typeof n.name!=='string'||!n.name.trim()||n.dead===true||
     !pcNpcInRange114(n,w.player))continue;
  const d=Math.max(Math.abs(n.x-native.x),Math.abs((n.z??n.y)-(native.z??native.y)));
  if(d<limit){best=n;limit=d;}
 }
 return best;
}
// Region entrances are verified map locations. They are not NPCs or convey
// stations; a live 30/29 offer is still required before entering a dungeon.
function pcNearbyDungeonRegion114(s,w){
 if(!s?.character||s.dungeons?.inside===true||!w?.player||w.player.dead||
    w.map?.id!==s.position?.map||!Array.isArray(s.dungeons?.catalog))return null;
 const native=pcInteractionPosition114(w.player),x=native?.x,y=native?.z??native?.y;
 if(!Number.isFinite(x)||!Number.isFinite(y))return null;
 let best=null,limit=Infinity;
 for(const dungeon of s.dungeons.catalog){
  if(!Number.isInteger(dungeon.id)||dungeon.id<=0)continue;
  for(const entrance of dungeon.entrances??[]){
   if(entrance.kind!=='region'||entrance.mapId!==w.map.id||
      !Number.isFinite(entrance.x)||!Number.isFinite(entrance.y)||
      !Number.isFinite(entrance.width)||!Number.isFinite(entrance.height))continue;
   const distance=Math.max(Math.abs(x-entrance.x),Math.abs(y-entrance.y));
   const radius=Math.max(2,Math.min(entrance.width,entrance.height)/2);
   if(distance<=radius&&distance<limit){best={id:dungeon.id,entrance};limit=distance;}
  }
 }
 return best;
}

export function nearestNpc111(w){if(!w?.player||w.player.dead)return null;if(w.meta?.bridge==='pc6020')return pcNearestTalkNpc114(w);let best=null,limit=8;for(const n of w.npcs??[]){const d=Math.hypot(n.x-w.player.x,n.z-w.player.z);if(d<=limit){best=n;limit=d;}}return best;}
export function installWorldControls111({state,world,openNpc,open,openDungeon=()=>{},mutate,toast,error}){
 const hud=document.getElementById('hud'),talk=document.createElement('button'),mount=document.createElement('button'),channel=document.createElement('div');
 talk.id='nearNpc111';talk.hidden=true;mount.id='mount111';mount.textContent='上坐骑';channel.id='channel111';channel.hidden=true;const title=document.createElement('span'),progress=document.createElement('progress');channel.append(title,progress);hud.append(talk,mount,channel);const sit114=bridgeSitHudInstall114({hud,mount,state,world,mutate,error,toast});const regionTalk114=document.createElement('button');regionTalk114.id='nearRegion114';regionTalk114.hidden=true;hud.append(regionTalk114);let npc=null,mountBusy=false,lastMount=0,talkBusy114=false;
 talk.onclick=async()=>{const s=state(),w=world();npc=s?.meta?.bridge==='pc6020'&&w?.map?.id!==s?.position?.map?null:nearestNpc111(w);talk.hidden=!npc;if(!npc||talkBusy114)return;talkBusy114=true;talk.disabled=true;try{await openNpc(npc.id??npc.uid)}catch(e){error(e)}finally{talkBusy114=false;talk.disabled=false;}};
 regionTalk114.onclick=()=>{const entry=pcNearbyDungeonRegion114(state(),world());if(entry&&!talkBusy114)openDungeon(entry.id);};mount.onclick=async()=>{if(mountBusy)return;if(state()?.meta?.bridge==='pc6020'){const s=state(),m=world()?.mountStatus,c=s.meta?.companions;if(m?.unknown===true||(!m&&c?.unknown===true)){toast('坐骑结果尚未确认，请重新登录核对');return;}if(m?m.pending!=null:c?.pending!=null){toast('正在等待区服处理，请稍候');return;}if(c?.enabled!==true){open('mounts111');return;}if(m?.riding!==true&&(!Number.isInteger(m?.mountedHorseId)||m.mountedHorseId<0)){open('mounts111');return;}}const s=state(),w=world(),active=s?.meta?.bridge==='pc6020'?(w?.mountStatus?.riding===true||Number.isInteger(w?.mountStatus?.mountedHorseId)&&w.mountStatus.mountedHorseId>0):!!w?.mount;if(w?.channel?.until>w.time){toast('正在读条，使用技能可以取消');return;}const owned=(s?.companions.mounts??[]).filter(n=>!n.expired),candidate=owned.find(n=>n.template===lastMount)??owned.find(n=>n.active)??owned[0];if(!active&&!candidate){open('mounts111');return;}if(candidate)lastMount=candidate.template;mountBusy=true;try{await mutate({type:'companion',kind:'mount',template:active?0:candidate.template});}catch(e){error(e);}finally{mountBusy=false;}};
 function update(){const w=world(),bridge=state()?.meta?.bridge==='pc6020';npc=bridge&&w?.map?.id!==state()?.position?.map?null:nearestNpc111(w);talk.hidden=!npc;talk.disabled=talkBusy114||bridge&&pcNoviceBeaconBusy117(npc);if(npc){const label=bridge?(pcNoviceBeacon117(npc)?pcNoviceBeaconLabel117(npc):npc.returnPortal?(npc.portalTitle||npc.name):npc.portalEffect?'离开副本':npc.npcType===26?(npc.gatherVerb||'互动')+' '+npc.name:'和'+npc.name+'交谈'):(npc.exit?'离开 · ':npc.chest?'宝箱 · ':'交谈 · ')+npc.name;if(talk.textContent!==label)talk.textContent=label;}
  const entrance114=bridge&&!npc?pcNearbyDungeonRegion114(state(),w):null;regionTalk114.hidden=!entrance114;if(entrance114)regionTalk114.textContent=state()?.dungeons?.offer?.instanceId===entrance114.id?'选择副本难度':'查看副本入口';const ride=w?.mountStatus,owned=state()?.companions?.mounts;mount.hidden=bridge&&!(ride?.riding===true||ride?.mountedHorseId>0||ride?.pending!=null||Array.isArray(owned)&&owned.some(m=>Number.isInteger(m.template)&&m.template>0));const label=bridge?(w?.channel?.kind!=='town_return'&&w?.channel?.until>w.time?'坐骑读条中…':ride?.pending!=null?'坐骑处理中…':(ride?.riding===true||ride?.mountedHorseId>0)?'下马':'上马'):w?.channel?.until>w.time?'坐骑读条中…':w?.mount?'下坐骑':'上坐骑';if(mount.textContent!==label)mount.textContent=label;mount.disabled=!!w?.player.dead||mountBusy;sit114.update(w);
  const sprint=document.getElementById('sprint'),on=!!w?.player.sprinting;const sprintKnown=!bridge||typeof w?.player.sprinting==='boolean';if(!sprintKnown)sprint.removeAttribute('aria-pressed');else sprint.setAttribute('aria-pressed',String(on));sprint.dataset.enabled=sprintKnown?String(on):'unknown';let text=sprint.querySelector('.sprintState111');if(!text){for(const n of [...sprint.childNodes])if(n.nodeType===3)n.remove();text=document.createElement('span');text.className='sprintState111';sprint.append(text);}text.textContent=w?.player.weak===true?'虚弱':!sprintKnown?'冲刺':on?'冲刺 · 开':'冲刺 · 关';
  const c=w?.channel;channel.hidden=!(c?.until>w?.time);if(!channel.hidden){title.textContent=c.title;progress.max=Math.max(1,c.until-c.start);progress.value=Math.max(0,w.time-c.start);}
 }
 return {update};
}
export function companionPanel111(body,{state,button,row,mutate,kind='mount',settings}){
 const nav=document.createElement('nav'),content=document.createElement('section');nav.className='tabs109';content.className='companionContent111';body.classList.add('companions111');body.append(nav,content);
 const draw=selected=>{for(const b of nav.children)b.classList.toggle('selected109',b.dataset.kind===selected);content.replaceChildren();if(selected==='settings'){settings(content);return;}const collection=state.companions.collection,p=document.createElement('p');p.textContent=`收藏属性：全属性 +${collection.allStats}　寻宝 +${collection.treasurePercent}%　掉落数量 +${collection.quantityPercent}%`;content.append(p);const entries=state.companions[selected==='mount'?'mounts':'pets'];if(!entries.length){const empty=document.createElement('p');empty.textContent=selected==='mount'?'尚未收藏坐骑':'尚未收藏宠物';content.append(empty);}for(const n of entries){const r=row(n.name+(n.active?' · 已启用':''),`${n.description??''}\n${n.expired?'已过期':n.expiresAt?'有效期至 '+new Date(n.expiresAt).toLocaleString():'永久'}`,button(n.active?'收回':'召唤',()=>mutate({type:'companion',kind:selected,template:n.active?0:n.template}),n.expired));if(n.icon){const img=document.createElement('img');img.src=n.icon;img.width=52;img.height=52;r.prepend(img);}content.append(r);}};
 for(const [id,title]of [['mount','坐骑'],['pet','宠物'],['settings','拾取设置']]){const b=button(title,()=>draw(id));b.dataset.kind=id;nav.append(b);}draw(kind);
}
