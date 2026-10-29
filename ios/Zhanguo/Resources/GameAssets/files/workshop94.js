export function workshopPanel94(id,body,api){
 if(!['imprint','manualSalvage'].includes(id))return false;
 const {el,button,row,request,mutate,itemButton,details}=api,s=api.state(),bag=s.inventory.filter(i=>i.container===1),smith=s.npcs.filter(n=>[501,1093,1029,824,820,816,812].includes(n.template)).sort((a,b)=>Math.hypot(a.x-s.position.x,a.y-s.position.y)-Math.hypot(b.x-s.position.x,b.y-s.position.y))[0];
 const nearby=smith&&Math.hypot(smith.x-s.position.x,smith.y-s.position.y)<=8;
 body.append(el('p',nearby?`铁匠：${smith.name} · 金币 ${s.gold}`:'请先走到铁匠身边。可先查看背包装备。','muted'));
 if(id==='manualSalvage'){
  body.append(el('p','免费分解，100%产出材料。先点装备，再确认分解；传奇和套装还需输入物品原名。'));
  const list=el('div',undefined,'grid'),detail=el('section');body.append(list,detail);
  for(const item of bag.filter(i=>i.canSalvage))list.append(itemButton(item,()=>{
   detail.replaceChildren(el('h2',item.name),el('p',details(item)),el('p','分解后装备消失，产物直接放入背包。'));
   const confirm=el('input');confirm.placeholder='输入物品原名：'+item.originalName;confirm.setAttribute('aria-label','确认分解物品原名');
   if(item.quality>=7)detail.append(confirm);
   detail.append(button('确认分解',()=>mutate({type:'manual-salvage',npc:smith?.uid,uid:item.uid,confirmName:confirm.value}),!nearby));detail.scrollIntoView({block:'nearest'});
  }));if(!list.childElementCount)body.append(el('p','没有可分解装备。白装、时装、已穿戴或锁定装备不可分解。'));return true;
 }
 body.append(el('p','同级或更高每件8%，低一级5%，低两级2%，低三级0%但仍消耗材料和金币。成功或失败均收费。'));
 const targets=bag.filter(i=>i.canImprint&&i.imprint.level>=1&&i.imprint.level<10),select=el('select');select.setAttribute('aria-label','刻印主装备');
 for(const it of targets){const o=el('option',`${it.name} · ${it.imprint.level}阶`);o.value=it.uid;select.append(o);}body.append(select);
 const materials=el('div'),quoteBox=el('section');body.append(materials,quoteBox);let selected=new Set(),generation=0;
 const render=()=>{generation++;selected=new Set();quoteBox.replaceChildren();materials.replaceChildren();const target=targets.find(i=>i.uid===+select.value);if(!target){materials.append(el('p','没有可升级的1—9阶背包传奇装备。'));return;}
  for(const it of bag.filter(i=>i.canImprint&&i.uid!==target.uid&&target.imprint.level-i.imprint.level<=3)){
   const label=el('label',undefined,'check'),check=el('input');check.type='checkbox';check.onchange=()=>{check.checked?selected.add(it.uid):selected.delete(it.uid);generation++;quoteBox.replaceChildren();};label.append(check,el('span',`${it.name} · ${it.imprint.level}阶`));materials.append(label);
  }
 };select.onchange=render;render();
 body.append(button('查看费用与成功率',async()=>{const version=++generation,action={npc:smith?.uid,uid:+select.value,materials:[...selected]};const q=await request('/systems',{kind:'imprint-quote',...action});if(!body.isConnected||version!==generation)return;quoteBox.replaceChildren(row(`升至${q.next}阶 · 成功率${q.percent}%`,`消耗 ${q.materials.length}件材料 + ${q.cost}金币`),button('确认消耗并升级',()=>{if(version!==generation)throw Error('材料选择已经改变，请重新报价');return mutate({type:'imprint-upgrade',...action,quote:q.token});},!q.affordable));quoteBox.scrollIntoView({block:'nearest'});},!nearby||!targets.length));
 return true;
}
