export async function ladderPanel107(body,{el,button,row,request,mutate,error}){
 const owner={};body.ladderOwner107=owner;
 let tab='points',closed=false,loading=false;const alive=()=>body.ladderOwner107===owner&&body.isConnected&&!body.closest('.hidden')&&document.querySelector('#panelTitle')?.textContent==='天梯战场';
 const action=a=>mutate({type:'ladder',...a});
 async function load(){if(!alive()){closed=true;return;}const s=await request('/ladder');if(!alive()){closed=true;return;}
  body.replaceChildren();const p=s.me;body.append(row(p.name,`${p.grade.name} · ${p.rank.name} · ${p.rank.description}`),row(`积分 ${p.points} · 荣耀 ${p.honor}`,`胜率 ${p.winRate}% · ${p.matches}场 · 击杀 ${p.kills} · 助攻 ${p.assists} · 今日荣耀 ${p.todayHonor}`));
  if(s.match){body.append(row(s.match.mode==='team'?'3V3组队赛':'个人混战',s.match.mode==='team'?`公用复活 ${s.match.remaining.join(' : ')}`:'击杀2分、助攻1分，100分结束',button('弃权离开',()=>{if(confirm('离开本场将不发奖励，确定弃权？'))return action({operation:'leave'});})));for(const r of s.match.ranking)body.append(row(r.name,`积分 ${r.score} · 击杀 ${r.kills} · 助攻 ${r.assists} · 死亡 ${r.deaths}`));}
  else if(s.vote){const v=s.vote;body.append(row(`玩法投票 · 剩余${v.seconds}秒`,`3V3 ${v.team}票 · 个人混战 ${v.ffa}票 · 同票随机`,button('3V3组队赛',()=>action({operation:'vote',mode:'team',ballot:v.id}),!!v.mine),button('个人混战',()=>action({operation:'vote',mode:'ffa',ballot:v.id}),!!v.mine)));}
  else body.append(row(s.queued?'正在匹配…':'天梯匹配','六人到齐后全员投票选择玩法',button(s.queued?'取消匹配':'开始匹配',()=>action({operation:s.queued?'cancel':'join'}))));
  const tabs=el('div',undefined,'toolbar');for(const [id,name]of [['points','战神积分榜'],['kills','击杀榜'],['assists','助攻榜'],['shop','荣耀商店']])tabs.append(button(name,()=>{tab=id;return load();}));body.append(tabs);
  if(tab==='shop'){for(const o of s.shop.offers)body.append(row(`${o.item.name} ×${o.count}`,`${o.price}荣耀${o.bound?' · 绑定':''}`,button('兑换',()=>action({operation:'buy',template:o.template,shopRevision:s.shop.revision}),p.honor<o.price)));}
  else for(const r of s.leaderboards[tab])body.append(row(`${r.rank}. ${r.name}`,`${r[tab]} · ${r.title.name}`));
 }
  await load();const timer=setInterval(()=>{if(!alive()||closed){clearInterval(timer);return;}if(loading)return;loading=true;load().catch(error).finally(()=>loading=false);},2000);
}
