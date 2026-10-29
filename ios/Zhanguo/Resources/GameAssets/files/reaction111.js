// Convert the authoritative server deadline once; never extend it on each snapshot.
export function reactionAnimation111(actor,n,now){
 const hit=n.hitReaction111;
 if(hit&&hit.at!==actor.reactionAt111&&n.hp!==0){
  actor.reactionAt111=hit.at;
  const remaining=Math.max(0,Math.min(801,hit.until-(n.serverTime111??hit.at)));
  actor.reactionUntil111=now+remaining;
  if(remaining){actor.attackUntil=0;actor.play(hit.animation,true);}
 }
 return n.hp!==0&&now<(actor.reactionUntil111??0);
}
