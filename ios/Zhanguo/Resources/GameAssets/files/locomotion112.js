// Player locomotion: stamina changes the pose; sprint changes server-owned speed.
export function playerMovementAction112(player,moving){
 if(player.stalling||player.sitting)return 7;
 if(player.mounted)return moving?22:21;
 if(!moving)return player.weak?79:1;
 const endurance=typeof player.endurance==='number'?player.endurance:player.endurance?.current;
 return player.weak||endurance===0?4:5;
}
