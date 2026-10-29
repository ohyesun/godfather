let revealing84=false;
export function seasonPanel84(body,{state,el,button,itemButton,mutate,error}){
 const s=state(),season=s.season;
 body.classList.add('season84');
 const hero=el('div',undefined,'season84-hero');hero.append(el('h2','烽火召唤'),el('p','绯烟 · 暗金火焰之灵'),el('p',`当前元宝 ${s.yuanbao}`));const portrait=el('img');portrait.src='season84/feiyan-portrait.png';portrait.alt='绯烟';hero.append(portrait);body.append(hero);
 const rewards=el('div',undefined,'season84-results');rewards.setAttribute('aria-live','polite');body.append(rewards);
 const controls=el('div',undefined,'toolbar');let drawing=false;
 async function draw(ten){
  if(drawing||revealing84)return;drawing=true;revealing84=true;for(const b of controls.querySelectorAll('button'))b.disabled=true;
  try{
   const result=await mutate({type:'season-draw',ten,expectedDraws:state().season.drawRevision});
   if(!result)return;
   const host=document.querySelector('.season84-results');if(!host)return;
   host.replaceChildren();host.classList.add('summoning84');
   for(const prize of result.seasonResult.rewards){
    await new Promise(r=>setTimeout(r,320));if(!host.isConnected)break;
    const card=itemButton(prize,()=>{});card.classList.add('season84-prize');host.append(card);
   }
   host.classList.remove('summoning84');
  }finally{drawing=false;revealing84=false;const current=state();for(const b of document.querySelectorAll('.season84 .toolbar button'))b.disabled=!current.season.enabled||current.yuanbao<Number(b.dataset.cost);}
 }
 const single=button(`召唤一次 · ${season.singlePrice}元宝`,()=>draw(false),revealing84||!season.enabled||s.yuanbao<season.singlePrice),ten=button(`十连赠一 · ${season.tenPrice}元宝`,()=>draw(true),revealing84||!season.enabled||s.yuanbao<season.tenPrice);single.dataset.cost=season.singlePrice;ten.dataset.cost=season.tenPrice;controls.append(single,ten);body.insertBefore(controls,rewards);
 body.append(el('h3','奖池预览'));const pool=el('div',undefined,'grid');for(const p of season.pool)pool.append(itemButton(p,()=>{}));body.append(pool);
 const history=el('details');history.append(el('summary','召唤记录'));for(const entry of [...season.history].reverse())history.append(el('p',`${new Date(entry.at).toLocaleString()} · ${entry.rewards.map(r=>r.name+' ×'+r.count).join('、')}`));body.append(history);
}
export function seasonExchangePanel84(body,{state,el,button,row,mutate}){
 body.append(el('p','周幽王 · 使用绑定火星兑换，限购次数在本赛季累计。'));
 for(const o of state().season.exchange)body.append(row(o.item.name+' ×'+o.count,`${o.price}火星 · 剩余${Math.max(0,o.limit-o.bought)}份`,button('兑换',()=>mutate({type:'season-exchange',offer:o.id,count:1,expectedCount:o.bought,expectedOffer:o.token}),o.bought>=o.limit)));
}
