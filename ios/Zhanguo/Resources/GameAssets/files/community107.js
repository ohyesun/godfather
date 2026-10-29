const channels={1:'附近',3:'队伍',7:'世界喇叭',8:'好友',16:'帮派'};
export async function communityPanel107(id,body,{el,button,row,itemButton,request,mutate,state,render}){
 if(id==='rankings'){
  const select=el('select'),list=el('div');for(const [id,name]of [[1,'全部职业'],[2,'兵家'],[3,'神农'],[4,'阴阳']]){const o=el('option',name);o.value=id;select.append(o);}let revision=0;
  const fill=async()=>{const current=++revision,q=await request('/systems',{kind:'rankings',selector:+select.value});if(!body.isConnected||current!==revision)return;list.replaceChildren(...q.rows.map(r=>row(`${r.rank}. ${r.name}`,`${r.level}级 · ${['','神农','阴阳','兵家'][r.profession]}${r.guild?' · '+r.guild:''}`)));};select.onchange=fill;body.append(select,list,button('刷新榜单',fill),button('天梯积分、击杀与助攻榜',()=>render('battles')));await fill();return;
 }
 if(id==='events'){
  const q=await request('/systems',{kind:'world-events'});if(!body.isConnected)return;body.append(el('h2','世界首领'));
  for(const b of q.bosses)body.append(row(b.name,`${b.locations.map(l=>l.name).join('、')} · ${b.alive?(b.hp!==null?b.hp+' / '+b.maxHp+'（'+b.percent+'%）':'存活 · '+b.percent+'%'):'已击败 · '+Math.max(0,Math.ceil((b.respawnAt-Date.now())/1000))+'秒后刷新'}`));
  body.append(button('刷新首领状态',()=>render('events')),el('h2','通缉榜'));for(const w of state.wanted)body.append(row(w.name,`赏金 ${w.bounty} · ${w.maps.join('、')}`));for(const b of state.bounties)body.append(row('可领取通缉奖励',b.name??String(b.template)));body.append(el('p','前往通缉榜 NPC 领取已经完成的赏金。'));return;
 }
 if(id==='chat'){
  const q=await request('/chat');if(!body.isConnected)return;const list=el('div');for(const m of q.messages)list.append(row(`[${channels[m.channel]??'私聊'}] ${m.name}`,m.text));body.append(list);
  const channel=el('select'),text=el('input');for(const [id,name]of Object.entries(channels)){const o=el('option',name);o.value=id;channel.append(o);}text.placeholder='输入消息';text.maxLength=200;body.append(channel,text,button('发送',async()=>{await mutate({type:'chat',channel:+channel.value,text:text.value});render('chat');}),button('刷新消息',()=>render('chat')));body.append(el('p','世界喇叭沿用电脑版的道具消耗；队伍、好友和帮派频道只发送给对应成员。'));return;
 }
 if(id!=='stalls')return;const q=await request('/stalls');if(!body.isConnected)return;
 if(q.mine){body.append(row('我的摊位 · '+q.mine.name,`${q.mine.entries.length} 件商品`,button('收摊并取回物品',()=>mutate({type:'stall',operation:'close'}))));for(const e of q.mine.entries)body.append(row(e.item.name,`${e.item.count} 件 · ${e.price} ${e.currency===1?'金币':'元宝'}${e.mode?' / 整组':' / 件'}`));}
 else {
  const name=el('input');name.placeholder='摊位名称';name.maxLength=30;const selected=[],list=el('div'),summary=el('div');
  const update=()=>{summary.replaceChildren(...selected.map((e,i)=>row(e.name,`${e.count} 件 · ${e.price} ${e.currency===1?'金币':'元宝'}${e.mode?' / 整组':' / 件'}`,button('移除',()=>{selected.splice(i,1);update();}))));};
  for(const item of state.inventory.filter(i=>i.container===1&&!i.binding)){
   const count=el('input'),price=el('input'),currency=el('select'),mode=el('select');count.type=price.type='number';count.min=price.min='1';count.max=item.count;count.value='1';price.value='100';count.setAttribute('aria-label','数量');price.setAttribute('aria-label','价格');
   for(const [v,t]of [[1,'金币'],[2,'元宝']]){const o=el('option',t);o.value=v;currency.append(o);}for(const [v,t]of [[0,'单件价格'],[1,'整组价格']]){const o=el('option',t);o.value=v;mode.append(o);}
   const line=row(item.name,`持有 ${item.count} 件`,count,price,currency,mode,button('加入摊位',()=>{if(selected.some(e=>e.uid===item.uid))throw Error('该物品已经加入');if(selected.length>=50)throw Error('最多50件商品');if(!Number.isInteger(+count.value)||+count.value<1||+count.value>item.count||!Number.isInteger(+price.value)||+price.value<1)throw Error('数量或价格无效');selected.push({uid:item.uid,name:item.name,count:+count.value,price:+price.value,currency:+currency.value,mode:+mode.value});update();}));list.append(line);
  }
  const details=el('details'),heading=el('summary','选择要售卖的背包物品');details.append(heading,list);body.append(el('h2','摆摊'),el('p','在咸阳主城开摊，价格、绑定与背包容量沿用电脑版规则。'),name,details,summary,button('确认开摊',()=>mutate({type:'stall',operation:'open',name:name.value,entries:selected.map(({name,...e})=>e)})));
 }
 body.append(el('h2','附近摊位'));if(!q.nearby.length)body.append(el('p','附近没有摊位，走近其他玩家的摊位后刷新。'));
 for(const s of q.nearby){body.append(el('h3',s.name+' · '+s.ownerName));for(const e of s.entries){const count=el('input');count.type='number';count.min=1;count.max=e.item.count;count.value=e.mode?e.item.count:1;count.disabled=!!e.mode;body.append(row(e.item.name,`${e.item.count} 件 · ${e.price} ${e.currency===1?'金币':'元宝'}${e.mode?' / 整组':' / 件'}`,itemButton(e.item,()=>{}),count,button('购买',()=>mutate({type:'stall',operation:'buy',seller:s.owner,generation:s.generation,uid:e.item.uid,price:e.price,currency:e.currency,mode:e.mode,count:+count.value}))));}}
 body.append(button('刷新摊位',()=>render('stalls')),el('h2','我的成交记录'));for(const h of q.history)body.append(row(h.seller===q.me?'售出':'购入',`${h.count} 件 · ${h.price} ${h.currency===1?'金币':'元宝'} · ${new Date(h.at).toLocaleString()}`));
}
