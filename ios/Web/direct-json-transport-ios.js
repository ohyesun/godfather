/** iOS-only replacement of the compiled DirectJsonConnection class. Wire
 * crypto/framing/GBK remain the original frozen r119 implementations. */
class DirectJsonConnection extends Connection_1.Connection {
 request;pollMs;onPacket;decoder=new DirectJsonCodec_1.DirectJsonCodec();
 endpointHost='';endpointPort=0;pollTimer=null;pollInFlight=false;closedByUser=false;
 iosConnectionId=null;iosGeneration=0;iosDecodeQueue=Promise.resolve();
 constructor(options){super();this.request=options.request;this.pollMs=Math.max(10,options.pollMs??40);this.onPacket=options.onPacket;}
 iosCurrent(generation,id){return !this.closedByUser&&this.iosGeneration===generation&&this.iosConnectionId===id;}
 async connect(host,port){
  if(this.isConnected||this.state===Connection_1.ConnState.Connecting)throw Error('direct connection already active');
  if(!host||!Number.isInteger(port)||port<1||port>65535)throw Error('invalid direct game address');
  const generation=++this.iosGeneration;this.endpointHost=host;this.endpointPort=port;
  this.closedByUser=false;this.iosConnectionId=null;this.pollInFlight=false;this.decoder=new DirectJsonCodec_1.DirectJsonCodec();this.iosDecodeQueue=Promise.resolve();this.setState(Connection_1.ConnState.Connecting);
  try{
   const result=await this.request('/direct/open',{host,port});
   if(result.status!==undefined&&result.status!==200)throw Error('direct game connect failed');
   const body=resultBody(result),id=body.connectionId;
   if(body.connected!==true||typeof id!=='string'||!id||id.length>128)throw Error('iOS direct connection identity missing');
   if(this.closedByUser||generation!==this.iosGeneration){
    void this.request('/direct/close',{connectionId:id}).catch(()=>{});
    throw Object.assign(Error('direct connection opening cancelled'),{code:'connection_cancelled'});
   }
   this.iosConnectionId=id;this.setState(Connection_1.ConnState.Connected);this.schedulePoll(0);
  }catch(error){if(!this.closedByUser&&generation===this.iosGeneration)this.setState(Connection_1.ConnState.Error,error instanceof Error?error.message:'direct game connect failed');throw error;}
 }
 useSessionKey(key){this.decoder.useSessionKey(key);}
 send(data){
  if(!this.isConnected||!this.iosConnectionId)throw Error('direct game connection is not active');
  const generation=this.iosGeneration,id=this.iosConnectionId;
  this.request('/direct/send',{connectionId:id,packetHex:toHex(data)}).then(result=>{
   if(this.iosCurrent(generation,id)&&resultBody(result).connectionId!==id)throw Error('iOS direct send returned another connection');
  }).catch(error=>this.fail(error,generation,id));
 }
 close(){
  const id=this.iosConnectionId;++this.iosGeneration;this.closedByUser=true;this.iosConnectionId=null;this.pollInFlight=false;
  if(this.pollTimer!==null)clearTimeout(this.pollTimer);this.pollTimer=null;
  if(id)void this.request('/direct/close',{connectionId:id}).catch(()=>{});
  this.setState(Connection_1.ConnState.Closed);
 }
 schedulePoll(delay){if(this.closedByUser||!this.isConnected||!this.iosConnectionId||this.pollTimer!==null)return;
  this.pollTimer=setTimeout(()=>{this.pollTimer=null;void this.poll();},delay);}
 async poll(){
  if(this.closedByUser||!this.isConnected||!this.iosConnectionId||this.pollInFlight)return;
  const generation=this.iosGeneration,id=this.iosConnectionId;this.pollInFlight=true;
  try{const result=await this.request('/direct/receive',{connectionId:id});if(this.iosCurrent(generation,id))this.consume(result,generation,id);}
  catch(error){this.fail(error,generation,id);}
  finally{if(this.iosCurrent(generation,id)){this.pollInFlight=false;this.schedulePoll(this.pollMs);}}
 }
 consume(result,generation=this.iosGeneration,id=this.iosConnectionId){
  if(!this.iosCurrent(generation,id))return;const body=resultBody(result);
  if(body.connectionId!==id)throw Error('iOS direct receive returned another connection');
  const value=body.dataHex;if(typeof value!=='string')throw Error('iOS direct receive returned invalid bytes');if(!value)return;
  const bytes=fromHex(value);
  for(const listener of this.listeners){if(!this.iosCurrent(generation,id))return;listener.onData?.(bytes.slice());}
  void this.decode(bytes,generation,id,this.decoder);
 }
 decode(bytes,generation,id,decoder){
  // FrameCodec.push may yield while inflating. The same stream must never be
  // pushed concurrently, including when another native poll arrives first.
  const work=this.iosDecodeQueue.then(async()=>{
   if(!this.iosCurrent(generation,id))return;
   try{const packets=await decoder.push(bytes);for(const packet of packets){if(!this.iosCurrent(generation,id))return;this.onPacket?.(packet);}}
   catch(error){this.fail(error,generation,id);}
  });
  this.iosDecodeQueue=work;return work;
 }
 fail(error,generation=this.iosGeneration,id=this.iosConnectionId){
  if(!this.iosCurrent(generation,id))return;this.closedByUser=true;++this.iosGeneration;this.iosConnectionId=null;this.pollInFlight=false;
  if(this.pollTimer!==null)clearTimeout(this.pollTimer);this.pollTimer=null;
  if(id)void this.request('/direct/close',{connectionId:id}).catch(()=>{});
  this.setState(Connection_1.ConnState.Error,error instanceof Error?error.message:'direct game connection failed');
 }
}
exports.DirectJsonConnection=DirectJsonConnection;
