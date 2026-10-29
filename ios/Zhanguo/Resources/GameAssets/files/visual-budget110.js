// Cosmetic work only. Damage, rewards and collisions come from the server.
export const VISUAL_TASK_LIMIT110=512,VISUAL_ACTIVE_LIMIT110=128;
export function visualDuration110(item,data={}){return Math.max(.05,Math.min((item.life||data.life||(item.type===1&&item.speed>=1?30000:1000))/1000,30));}
// A newly spawned visual cannot inherit time spent with the app suspended.
// Expiry and projectile location still use elapsed wall time, not capped frames.
export function visualStep110(start,clock,elapsed){return Math.max(0,Math.min(elapsed,clock-start));}
export function taskExpired110(task,clock,data={}){
 if(task.child||task.sound)return clock-task.start>.25;
 return clock>=task.start+visualDuration110(task.item,data);
}
export function enqueueVisual110(tasks,task,owner){
 if(tasks.length>=VISUAL_TASK_LIMIT110){if(task.event.uid!==owner)return false;const at=tasks.findIndex(t=>t.event.uid!==owner);if(at<0)return false;tasks.splice(at,1);}
 tasks.push(task);return true;
}
