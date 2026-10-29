export async function socialPanel84(body,{el,button,row,request,mutate,render}){
 const [data,party]=await Promise.all([request('/social'),request('/party')]);if(!body.isConnected||document.querySelector('#panelTitle')?.textContent!=='好友与私聊')return;
 body.replaceChildren();body.append(button('刷新联系人',()=>render('social')));
 body.append(el('h3','队伍'));if(party.team){for(const member of party.team.members)body.append(row(member.name+(member.id===party.team.leader?' · 队长':''),member.online?'在线':'离线',...(party.me===party.team.leader&&member.id!==party.me?[button('移交队长',()=>mutate({type:'party',operation:'transfer',target:member.id})),button('移出队伍',()=>mutate({type:'party',operation:'kick',target:member.id}))]:[])));body.append(button('离开队伍',()=>mutate({type:'party',operation:'leave'})));}else body.append(el('small','尚未加入队伍'));
 for(const r of party.requests)body.append(row(r.sender.name+' 邀请组队','30秒内有效',button('接受组队',()=>mutate({type:'party',operation:'answer',request:r.id,accepted:true})),button('拒绝组队',()=>mutate({type:'party',operation:'answer',request:r.id,accepted:false}))));
 for(const r of data.requests)body.append(row(r.sender.name+' 请求添加好友','申请30秒内有效',button('接受',()=>mutate({type:'social',operation:'answer',request:r.id,accepted:true})),button('拒绝',()=>mutate({type:'social',operation:'answer',request:r.id,accepted:false}))));
 for(const [kind,title]of [[6,'好友'],[3,'黑名单'],[2,'仇人'],[5,'临时好友']]){
  body.append(el('h3',title));const list=data.lists[kind];if(!list.length)body.append(el('small','暂无联系人'));
  for(const person of list)body.append(row(person.name,`${person.level}级 · ${person.online?'在线':'离线'}`,button('移除',()=>mutate({type:'social',operation:'remove',kind,target:person.id}))));
 }
 body.append(el('h3','在线玩家'));
 for(const person of data.online)body.append(row(person.name,`${person.level}级`,button('邀请组队',()=>mutate({type:'party',operation:'invite',target:person.id})),button('加好友',()=>mutate({type:'social',operation:'request',target:person.id})),button('拉黑',()=>mutate({type:'social',operation:'block',target:person.id})),button('私聊',()=>{
  const form=el('div',undefined,'toolbar'),input=el('input');input.placeholder='发送给'+person.name;input.maxLength=200;form.append(input,button('发送',()=>mutate({type:'social',operation:'whisper',target:person.id,text:input.value})));body.prepend(form);input.focus();
 })));
 body.append(el('h3','私聊记录'));for(const message of data.messages)body.append(row(`${message.sender.name} → ${message.recipient.name}`,message.text));
}
