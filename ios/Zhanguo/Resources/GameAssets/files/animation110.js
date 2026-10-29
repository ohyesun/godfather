export function restoreTimeline110(asset,timelines){
 if(asset.userData?.timeline110)return asset;
 for(const clip of asset.animations){const times=timelines?.[clip.name];if(!times)continue;
  for(const track of clip.tracks){if(track.times.length!==times.length)throw Error('动作时间轴与原始关键帧不匹配：'+clip.name);track.times=new Float32Array(times);}
  clip.resetDuration();
 }
 asset.userData={...asset.userData,timeline110:true};return asset;
}
