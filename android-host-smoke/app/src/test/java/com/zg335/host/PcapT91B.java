// Build: javac -encoding UTF-8 ...; Windows default GBK can cause a false-negative SELF_OK.
package com.zg335.host;

import java.io.*;import java.nio.file.*;import java.util.*;import javax.crypto.*;import javax.crypto.spec.SecretKeySpec;
public class PcapT91B{
 public static void main(String[] a)throws Exception{
  byte[] all=Files.readAllBytes(Paths.get(a[0])); byte[] key=hex(a[1]);
  TreeMap<Long,byte[]> segs=new TreeMap<>();
  int off=24;
  while(off+16<=all.length){
   int incl=(all[off+8]&255)|((all[off+9]&255)<<8)|((all[off+10]&255)<<16)|((all[off+11]&255)<<24); off+=16;
   if(off+incl>all.length)break; if(incl<36){off+=incl;continue;}
   int ip=16; if((all[off+ip]>>4)!=4){off+=incl;continue;} int ihl=(all[off+ip]&15)*4;
   if((all[off+ip+9]&255)!=6){off+=incl;continue;} int tcp=off+ip+ihl;
   if(tcp+20>off+incl){off+=incl;continue;}
   int sp=((all[tcp]&255)<<8)|(all[tcp+1]&255); int dp=((all[tcp+2]&255)<<8)|(all[tcp+3]&255);
   if(sp!=6020){off+=incl;continue;}
   long seq=((long)(all[tcp+4]&255)<<24)|((long)(all[tcp+5]&255)<<16)|((long)(all[tcp+6]&255)<<8)|(all[tcp+7]&255);
   int dof=((all[tcp+12]>>4)&15)*4; int plen=incl-(tcp-off)-dof;
   if(plen>0){byte[] p=Arrays.copyOfRange(all,tcp+dof,tcp+dof+plen); segs.put(seq,p);}
   off+=incl;
  }
  System.out.println("SEGMENTS="+segs.size());
  ByteArrayOutputStream s2c=new ByteArrayOutputStream(); long cur=-1;
  for(Map.Entry<Long,byte[]> e:segs.entrySet()){long s=e.getKey();byte[] p=e.getValue(); if(cur<0)cur=s; if(s>cur){System.out.println("GAP "+(s-cur));cur=s;} if(s+p.length<=cur)continue; int from=(int)Math.max(0,cur-s); s2c.write(p,from,p.length-from);cur=s+p.length;}
  byte[] enc=s2c.toByteArray(); int usable=enc.length&~7; System.out.println("S2C="+enc.length);
  Cipher c=Cipher.getInstance("DES/ECB/NoPadding");c.init(Cipher.DECRYPT_MODE,new SecretKeySpec(key,"DES"));byte[] plain=c.doFinal(Arrays.copyOf(enc,usable));
  int o=0,frames=0,npc=0;Set<Integer>ids=new HashSet<>();boolean map=false,self=false;String selfName="";
  while(o+4<=plain.length){
   int len=(plain[o]&255)|((plain[o+1]&255)<<8)|((plain[o+2]&255)<<16);int flags=plain[o+3]&255;
   if(len==0){if(flags==0&&o+6<=plain.length&&plain[o+4]==0&&plain[o+5]==0){o+=6;continue;}break;}
   if(len>0xfffffb)break;int total=4+len;if(o+total>plain.length){System.out.println("TRUNC off="+o+" need="+len+" rem="+(plain.length-o));break;}
   byte[] body=Arrays.copyOfRange(plain,o+4,o+4+len);
   if((flags&0x40)!=0){byte[] inf=inflate(body);if(inf==null){System.out.println("INFLATE_FAIL off="+o+" len="+len+" hex="+hex(body,0,Math.min(32,body.length)));o+=total;continue;}body=inf;}
   if(body.length<2){o+=total;continue;}int cmd=((body[0]&255)<<8)|(body[1]&255);frames++;
   if(cmd==0x050c&&body.length>=55){int mapId=u16(body,12);String name=gbk(body,22,33);System.out.println("MAP_FRAME map="+mapId+" file="+name+" len="+body.length);map=true;}
   if(cmd==0x0501&&body.length>=405){String n=gbk(body,38,32);System.out.println("SELF_FRAME uid="+u32(body,6)+" name="+n+" len="+body.length);if(!n.isEmpty()){selfName=n;self=true;}}
   if(cmd==0x0507&&body.length>=177){ids.add(u32(body,6));npc=ids.size();}
   o+=total;
  }
  System.out.println("FRAMES="+frames);
  System.out.println("SCENE-MAP "+(map?"map=2 filename=xianyangcheng":"MISSING"));
  boolean selfOk = "\u70c8\u706b\u6559\u7236".equals(selfName);
  System.out.println("SELF_OK="+selfOk);
  System.out.println("SCENE-OK "+((map&&selfOk&&npc==10)?"map=2 self=\u70c8\u706b\u6559\u7236 npc="+npc:"NOT_READY map="+map+" self="+self+" name_ok="+selfOk+" npc="+npc));
 }
 static byte[] inflate(byte[]b)throws Exception{try(java.util.zip.InflaterInputStream in=new java.util.zip.InflaterInputStream(new ByteArrayInputStream(b))){ByteArrayOutputStream o=new ByteArrayOutputStream();byte[]x=new byte[4096];int n;while((n=in.read(x))>=0)o.write(x,0,n);return o.toByteArray();}catch(Exception e){return null;}}
 static int u16(byte[]b,int o){return(b[o]&255)|((b[o+1]&255)<<8);}static int u32(byte[]b,int o){return(b[o]&255)|((b[o+1]&255)<<8)|((b[o+2]&255)<<16)|((b[o+3]&255)<<24);}
 static String gbk(byte[]b,int o,int n)throws Exception{String s=new String(b,o,n,"GBK");int z=s.indexOf('\0');return(z>=0?s.substring(0,z):s).trim();}
 static byte[] hex(String s){byte[]b=new byte[s.length()/2];for(int i=0;i<b.length;i++)b[i]=(byte)Integer.parseInt(s.substring(2*i,2*i+2),16);return b;}
 static String hex(byte[]b,int o,int n){StringBuilder s=new StringBuilder();for(int i=0;i<n;i++)s.append(String.format("%02x",b[o+i]));return s.toString();}
}
