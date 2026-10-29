// Native scene pixels and navigation cells share this exact 64 x 32 mapping.
export const CELL_X=64,CELL_Y=32;
export const CAMERA_SIN=.5,CAMERA_COS=Math.sqrt(3)/2;
export function nativePoint(x,z){return {x:x*CELL_X,y:z*CELL_Y};}
export function spriteVertex(x,y,depth,object=false){
 if(!object)return [x/CELL_X,0,y/CELL_Y];
 const up=(depth-y)/CELL_X;
 return [x/CELL_X,up*CAMERA_COS,depth/CELL_Y-up*CAMERA_SIN];
}
export function decodeChunk(buffer,spriteCount){
 const v=new DataView(buffer);
 if(v.byteLength<8||v.getUint32(0,true)!==0x314e4353)throw Error('场景分块格式无效');
 const n=v.getUint32(4,true);
 if(n>1000000||v.byteLength!==8+n*24)throw Error('场景分块长度无效');
 const rows=[];
 for(let i=0;i<n;i++){
  const row=Array.from({length:6},(_,j)=>v.getInt32(8+i*24+j*4,true));
  if(row[0]<0||row[0]>=spriteCount||row[4]<0||row[4]>2||row[5]<1)throw Error('场景图片索引无效');
  rows.push(row);
 }
 return rows;
}
