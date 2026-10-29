// Native-rule system panels shared by desktop preview and Android WebView.
export function systemPanels(api){
 const {el,button,row,request,mutate,itemButton,details,toast,close,act,difficulty,render,closest}=api;
 const number=(label,value=1,min=1,max=99)=>{const e=el('input');e.type='number';e.value=value;e.min=min;e.max=max;e.setAttribute('aria-label',label);return e;};
 const select=(label,entries)=>{const e=el('select');e.setAttribute('aria-label',label);for(const [value,text]of entries){const o=el('option',text);o.value=value;e.append(o);}return e;};
 const note=(body,text)=>body.append(el('p',text,'muted'));
 const heading=(body,text)=>body.append(el('h2',text));
 const time=stamp=>stamp?new Date(stamp).toLocaleString():'永久';
 const itemSelect=(label,items)=>select(label,items.length?items.map(i=>[i.uid,`${i.name} ×${i.count}`]):[['','暂无符合条件的物品']]);
 function listing(body,rows,draw){
  const search=el('input');search.placeholder='搜索名称或编号';search.setAttribute('aria-label','搜索名称或编号');const list=el('div'),controls=el('div',undefined,'toolbar');let page=0;
  const fill=()=>{const q=search.value.trim(),matched=rows.filter(r=>r.name?.includes(q)||String(r.id)===q);page=Math.min(page,Math.max(0,Math.ceil(matched.length/30)-1));list.replaceChildren(...matched.slice(page*30,page*30+30).map(draw));controls.replaceChildren(button('上一页',()=>{page--;fill();},page===0),el('span',`${page+1} / ${Math.max(1,Math.ceil(matched.length/30))} 页 · ${matched.length} 项`),button('下一页',()=>{page++;fill();},(page+1)*30>=matched.length));};
  search.oninput=()=>{page=0;fill();};body.append(search,list,controls);fill();
 }
 async function catalogue(body,kind,draw){const loading=el('p','正在读取…');body.append(loading);const data=await request('/systems',{kind});if(!body.isConnected||api.menu()!==kind&&kind!=='craft'&&kind!=='battles')return;loading.remove();draw(data);}
 function character(body,s){
  const x=s.systems;body.append(row(`${s.character.name} · ${s.character.level}级`,`经验 ${s.character.experience} / ${x.experienceNeeded} · 金币 ${s.gold} · 元宝 ${s.yuanbao}`));
  const summary=el('div',undefined,'statGrid');for(const [name,value]of [['生命',`${s.resources.hp}/${s.stats.maxHp}`],['法力',`${s.resources.mp}/${s.stats.maxMp}`],['基础伤害',x.summary.attack.join('—')],['防御',x.summary.defence],['移动速度',x.summary.moveSpeed],['寻宝',x.summary.rarity]])summary.append(row(name,String(value)));body.append(summary);
  heading(body,'人物天赋');for(const t of x.talents)body.append(row(t.name,t.description));
  heading(body,'套装与五行刻印');if(!x.sets.sets.length&&!x.imprints.length)note(body,'当前装备没有套装或五行刻印。');
  for(const set of x.sets.sets)body.append(row(`${set.name} · ${set.count}件`,set.bonuses.map(b=>`${b.count<=set.count?'已激活':'未激活'}：${b.text}`).join('\n')));
  for(const e of x.imprints)body.append(row(e.name,`${e.element} · ${e.level}级 · 暗属性${e.active?'已激活':'未激活'}`));
  if(x.sets.pending.length)note(body,'尚未接入的套装触发效果：'+x.sets.pending.join('、'));
  heading(body,'详细属性');listing(body,s.stats.values,v=>row(v.name,String(v.value)));
  heading(body,'等级封印');const seal=x.levelSeal;body.append(row(`${seal.level}级封印`,seal.deadline?`解除条件：倒计时结束后击败封印兽 · ${time(seal.deadline)}`:'尚未触发封印倒计时'));
 }
 function skills(body,s){
  note(body,`可用技能点 ${s.learning.points} · 快捷栏 ${s.preferences.hotbar.length}/6 · 技能书、等级和前置条件以当前角色实际状态检查。`);
  const choices=select('技能类型',[['all','全部技能'],['active','主动技能'],['passive','被动技能']]),list=el('div');body.append(choices,list);
  const fill=()=>{list.replaceChildren();const normal=s.skills.find(k=>k.id===20002);if(normal){const active=s.preferences.hotbar.includes(20002);list.append(row('普通攻击','使用当前装备的武器',button(active?'移出快捷栏':'加入快捷栏',()=>toggle(normal.id),!active&&s.preferences.hotbar.length>=6)));}
   for(const k of s.systems.skillTree.filter(k=>choices.value==='all'||(k.kind==='passive')===(choices.value==='passive'))){
    const active=s.preferences.hotbar.includes(k.id),buttons=[];
    if(k.kind!=='passive'&&k.level>0&&s.skills.some(v=>v.id===k.id))buttons.push(button(active?'移出快捷栏':'加入快捷栏',()=>toggle(k.id),!active&&s.preferences.hotbar.length>=6));
    buttons.push(button(k.next?'学习 / 升级':'已满级',()=>mutate({type:'learn',skill:k.id}),!k.next||!!k.next.reason));
    list.append(row(`${k.name} · ${k.level}级${k.effective>k.level?'（装备加成后'+k.effective+'级）':''} · ${k.kind==='passive'?'被动':'主动'}`,`${k.description}\n${k.kind==='passive'?'学习后生效':'法力消耗 '+k.mana}${k.next?'\n下级：角色'+k.next.requiredLevel+'级 · 1技能点'+(k.next.book?' · '+k.next.bookName:'')+(k.next.prerequisites.length?' · '+k.next.prerequisites.map(p=>`${p.name}${p.level}级${p.met?'✓':''}`).join('、'):'')+(k.next.reason?' · '+k.next.reason:''):''}`,...buttons));
   }
  };
  const toggle=id=>mutate({type:'preferences',hotbar:s.preferences.hotbar.includes(id)?s.preferences.hotbar.filter(v=>v!==id):[...s.preferences.hotbar,id]});choices.onchange=fill;fill();
 }
 async function craft(body,s){
  note(body,`金币 ${s.gold} · 成功或失败均按当前游戏规则结算。神石改造、刺绣前请把目标物品卸到背包。`);
  const gear=s.inventory.filter(i=>i.slots.length),bag=s.inventory.filter(i=>i.container===1),target=itemSelect('加工装备',gear),detail=el('div',undefined,'detail');body.append(el('label','目标装备'),target,detail);
  const show=()=>{const it=gear.find(i=>i.uid===+target.value);detail.textContent=it?details(it):'背包或装备栏中没有可加工装备';};target.onchange=show;show();
  heading(body,'鉴定、开光、祝福与修理');
  const scroll=itemSelect('鉴定卷轴',bag.filter(i=>i.identifyScroll)),oil=itemSelect('祝福油',bag.filter(i=>[6251,6252].includes(i.template)));
  body.append(scroll,button('鉴定一条属性',()=>mutate({type:'identify',uid:+target.value,scrollUid:+scroll.value}),!scroll.value),oil,button('使用祝福油',()=>mutate({type:'bless',uid:+target.value,oilUid:+oil.value}),!oil.value),button('金币开光',()=>mutate({type:'openlight',uid:+target.value}),!target.value),button('修理装备',()=>{if(s.systems.services.some(e=>e.service==='repair'&&e.active))return mutate({type:'portable-repair',uid:+target.value});const n=closest('weapons')??closest('armour');if(!n)throw Error('请到装备商人旁边修理，或使用随身修理契约');return mutate({type:'repair',uid:+target.value,npc:n.uid});},!target.value));
  heading(body,'宝石镶嵌与合成');const gem=itemSelect('宝石',bag.filter(i=>i.gem)),slot=select('镶嵌孔位',Array.from({length:4},(_,n)=>[n,'第'+(n+1)+'孔']));
  body.append(gem,slot,button('镶嵌宝石',()=>mutate({type:'gem-socket',uid:+target.value,gemUid:+gem.value,slot:+slot.value}),!gem.value||!target.value),button('合成宝石',()=>mutate({type:'gem-compose',uid:+gem.value}),!gem.value));
  const data=await request('/systems',{kind:'craft'});if(!body.isConnected||api.menu()!=='craft')return;
  heading(body,'神石改造与锁定');const stones=itemSelect('改造神石',bag.filter(i=>data.godstones.some(r=>r.id===i.template))),desc=el('p');
  const stoneInfo=()=>{const it=bag.find(i=>i.uid===+stones.value),r=data.godstones.find(r=>r.id===it?.template);desc.textContent=r?`${r.description}\n消耗：1颗神石 + ${r.cost}金币${r.available?'':' · 当前规则未恢复，暂不可使用'}`:'背包中没有神石，可从怪物掉落或商城获得。';};stones.onchange=stoneInfo;stoneInfo();
  body.append(stones,desc,button('使用神石改造',()=>mutate({type:'godstone',uid:+target.value,material:+stones.value}),!stones.value||!target.value));
  for(const i of bag.filter(i=>i.mirror))body.append(row(i.name,`镜像有效期 ${time(i.mirror.expiresAt)}`,button('还原镜像',()=>mutate({type:'mirror-restore',uid:i.uid}))));
  heading(body,'神石合成');listing(body,s.stoneRecipes,r=>row(r.name,r.materials.map(m=>`${m.name} ${bag.filter(i=>i.template===m.template).reduce((n,i)=>n+i.count,0)}/${m.count}`).join(' · '),button('合成1个',()=>mutate({type:'stone-compose',recipe:r.id,count:1}))));
  heading(body,'时装刺绣与丝线合成');const silk=itemSelect('刺绣丝线',bag.filter(i=>data.silk.some(r=>r.id===i.template)));body.append(silk,button('为所选时装刺绣',()=>mutate({type:'embroidery',uid:+target.value,material:+silk.value}),!silk.value||!target.value));
  for(const r of data.silk)body.append(row(r.name,`${r.count}根 → ${r.outputName} · ${r.price}金币 · ${r.chance}%成功率`,button(r.available?'合成丝线':'条件待恢复',()=>mutate({type:'silk',template:r.id}),!r.available||bag.filter(i=>i.template===r.id).reduce((n,i)=>n+i.count,0)<r.count)));
  heading(body,'装备生产');note(body,'已恢复的生产配方可在工匠旁打造；特殊配方保留原始材料清单，未恢复的生成规则不会扣款。');
  const filter=select('生产配方范围',[['ready','可制作配方'],['all','全部原版配方']]),list=el('div');body.append(filter,list);const fill=()=>{list.replaceChildren();listing(list,data.forge.filter(r=>filter.value==='all'||r.available),r=>row(r.name,`${r.level}级 · ${r.cost}金币\n${r.materials.map(m=>m.name+'×'+m.count).join('、')}${r.reason?'\n'+r.reason:''}`,button(r.available?'打造':'尚未开放',()=>{const n=s.npcs.find(n=>r.npcs.includes(n.template));if(!n)throw Error('当前地图没有配方指定工匠');return mutate({type:'forge',recipe:r.id,npc:n.uid});},!r.available)));};filter.onchange=fill;fill();
 }
 async function dungeons(body,s){
  note(body,'从原地图入口选择普通或噩梦；噩梦消耗1枚噩梦令牌。击败首领后结算。');
  if(s.preferences.instance){const n=s.preferences.instance;body.append(row('当前副本',n.complete?'首领已击败，副本已结算':'副本进行中',button('离开副本',()=>mutate({type:'leave'}))));}
  let page=0;const search=el('input');search.placeholder='搜索副本';const list=el('div'),paging=el('div',undefined,'toolbar');body.append(search,list,paging);let generation=0;
  const load=async()=>{const gen=++generation,d=await request('/systems',{kind:'dungeons',search:search.value,page});if(gen!==generation||api.menu()!=='dungeons')return;list.replaceChildren(...d.rows.map(e=>row(e.name,`入口：${e.mapName} (${e.x},${e.y}) · 需求 ${e.level}级`,button('前往入口',()=>{if(e.map!==s.position.map)throw Error('请先通过传送阵前往'+e.mapName);const n=s.npcs.find(n=>n.template===e.npc);close();if(n)act({kind:'navigate',target:n.uid});else act({kind:'navigate-portal',portal:100000+e.id});},e.map!==s.position.map),button('选择难度',()=>difficulty(e.npc?{type:'dungeon',map:e.id,npc:e.npc}:{type:'portal',portal:100000+e.id}),e.map!==s.position.map))));paging.replaceChildren(button('上一页',()=>{page--;return load();},!page),el('span',`${page+1}/${Math.max(1,Math.ceil(d.total/30))}`),button('下一页',()=>{page++;return load();},(page+1)*30>=d.total));};search.oninput=()=>{page=0;load().catch(api.error);};await load();
 }
 async function monsters(body,s){
  note(body,`${s.map.name} · 普通怪、野外精英、副本词缀怪和首领分别按当前配置显示。`);
  const d=await request('/systems',{kind:'monsters'});if(api.menu()!=='monsters')return;
  // Each species/rarity is separate; paginate the server catalogue without rerolling loot.
  const rows=[...d.rows];for(let page=1;page*30<d.total;page++)rows.push(...(await request('/systems',{kind:'monsters',page})).rows);
  const lootText=n=>n.customLoot?'自定义掉落：'+(n.customLoot.map(r=>`${r.name} ${r.chance/100}% ×${r.min}—${r.max}`).join('、')||'不掉落'):null;
  const kinds={white:'普通',blue:'蓝色精英',gold:'金色精英',boss:'首领',worldBoss:'世界首领'};
  listing(body,rows,n=>row(`${n.name} · ${n.level}级 · ${kinds[n.rank]}`,`${lootText(n)?lootText(n)+'\n':''}生命 ${n.hp} · 攻击 ${n.attack??'—'} · 存活 ${n.alive??'—'}/${n.count??'—'}${n.affixes.length?'\n词缀：'+n.affixes.map(a=>a.name).join('、'):''}\n掉落尝试 ${n.loot.attempts.join('—')}次 · 每次基础命中率 ${n.loot.chance}%\n类别权重：${n.loot.categories.map(([key,w])=>({equipment:'装备',money:'金币',stone:'神石',gem:'宝石',material:'材料',potion:'药水',gift:'礼包'}[key]??key)+' '+w).join('、')}`));
  note(body,'寻宝影响稀有品质权重，掉落增强影响数量；掉落编辑器中的自定义配置优先生效。宝石掉落限制为1—5级。');
 }
 function growth(body,s){
  body.append(row(`VIP ${s.vip.level}`,`VIP经验 ${s.systems.vipExperience} · 精力 ${s.activity.energy}/${s.activity.maximum} · 疲劳 ${s.activity.fatigue}`,button('领取可用VIP邮件',()=>mutate({type:'vip-mail'}))));
  heading(body,'内功');for(const h of s.systems.inner){body.append(row(h.name,h.learned?(h.active?'当前已启用':'已学习，未启用'):'40级使用帝王丹开启后，购买功法并在背包使用',...(h.learned?[button(h.active?'停用':'启用',()=>mutate({type:'inner-activate',id:h.id,enabled:!h.active}))]:[])));if(h.learned)for(const p of h.palaces)body.append(row(`${p.name} · ${p.level}级`,p.cost===undefined?'无法升级':`${p.cost}金币、${p.xp}经验${p.reason?' · '+p.reason:''}`,button('修炼',()=>mutate({type:'inner',id:h.id,palace:p.palace}),!!p.reason)));}
  heading(body,'VIP特权');for(const v of s.vipLevels)body.append(row(`VIP ${v.level}${s.vip.level>=v.level?' · 已达到':''}`,`需要 ${v.experience}VIP经验\n${v.description.replace(/<[^>]*>/g,'')}`));
 }
 function bank(body,s){
  note(body,`金币 ${s.gold} · 元宝 ${s.yuanbao} · 点数 ${s.systems.points} · 精力 ${s.activity.energy}/${s.activity.maximum}`);
  const npc=()=>{const n=closest('bank');if(!n)throw Error('当前地图没有钱庄 NPC');return n.uid;};
  for(const r of s.systems.bank)body.append(row('元宝兑换金币',`${r.cost}元宝 · 基础汇率1:${r.rate}，VIP典当加成另计`,button('兑换',()=>mutate({type:'pawn',mode:r.id,npc:npc()}))));
  const amount=number('兑换点数',10,10,1000000000);body.append(amount,button('10点数兑换1元宝',()=>mutate({type:'exchange-points',amount:+amount.value})));
  body.append(row('购买精力',`${s.systems.activityRules.energyPurchaseAmount}精力 / ${s.systems.activityRules.energyPurchaseYuanbao}元宝`,button('在钱庄购买',()=>mutate({type:'energy',npc:npc()}))));
 }
 async function contracts(body,s){
  heading(body,'当前生效契约');if(!s.systems.services.length)note(body,'使用背包中的天子契约后，会在这里显示期限与服务。');for(const e of s.systems.services)body.append(row(e.name,`${e.active?'生效中':'已到期'} · ${time(e.expiresAt)}`));
  const has=id=>s.systems.services.some(e=>e.service===id&&e.active);if(has('repair'))body.append(button('随身修理全部装备',()=>mutate({type:'portable-repair'})));
  body.append(button('打开邮件',()=>render('mail')),button('自动分解设置',()=>render('pets')));
  if(has('shop')){heading(body,'随身药铺');const data=await request('/systems',{kind:'portable-shop'});if(api.menu()!=='contracts')return;for(const o of data.rows)body.append(row(o.name,`${o.price}金币`,button('购买1件',()=>mutate({type:'portable-buy',template:o.template,count:1}))));}
 }
 async function battles(body,s){
  note(body,'当前接入独立演武：15分钟内击败12名守军和1名首领，胜利可推进战令战场任务。');
  if(s.systems.battle){const b=s.systems.battle;body.append(row('当前演武',`${b.killed.length}/13 · ${b.status==='won'?'已胜利':b.status==='expired'?'已超时':'进行中'} · 截止 ${time(b.expiresAt)}`,button('离开演武',()=>mutate({type:'leave'}))));return;}
  const d=await request('/systems',{kind:'battles'});if(api.menu()!=='battles')return;for(const m of d.rows)body.append(row(m.name,`最低${m.minLevel}级 · 胜利奖励：角色等级×${d.rules.rewardGoldPerLevel}金币`,button('进入演武',async()=>{await mutate({type:'battle-enter',map:m.id});close();},s.character.level<m.minLevel)));
 }
 function shop(body,s){
  note(body,`元宝 ${s.yuanbao} · 按原商城价格购买，受 VIP、数量和背包容量限制。`);
  const categories=select('商城分类',[['all','全部'],...Array.from(new Set(s.shop.map(o=>o.category))).map(n=>[n,'分类 '+n])]),count=number('购买数量',1,1,99),list=el('div');body.append(categories,el('label','购买数量'),count,list);
  const fill=()=>{list.replaceChildren();listing(list,s.shop.filter(o=>categories.value==='all'||o.category===+categories.value),o=>{const r=row(o.name,`${o.price}元宝 / 件${o.description?"\n"+o.description:""}`,button('购买',()=>mutate({type:'shop',template:o.id,count:+count.value})));if(o.icon){const img=el('img');img.src=o.icon;img.alt='';img.width=40;img.height=40;r.prepend(img);}return r;});};categories.onchange=fill;fill();
 }
 const handlers={character,skills,craft,dungeons,monsters,growth,bank,contracts,battles,shop};
 return (id,parent)=>{if(!handlers[id])return false;const body=el('section');parent.append(body);Promise.resolve(handlers[id](body,api.state())).catch(api.error);return true;};
}
