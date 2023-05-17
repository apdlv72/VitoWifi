R"=====(<?xml version="1.0" encoding="UTF-8" ?>
<!DOCTYPE html PUBLIC "-//WAPFORUM//DTD XHTML Mobile 1.0//EN" "http://www.wapforum.org/DTD/xhtml-mobile10.dtd">
<html>
<head>
  <link rel="shortcut icon" href="data:image/x-icon;," type="image/x-icon"> 
  <!-- https://developer.chrome.com/multidevice/android/installtohomescreen -->
  <meta charset='utf-8'>
  <!-- link rel="manifest" href="manifest.json" -->
    <meta name="viewport" content="width=device-width">
  <meta name="mobile-web-app-capable" content="yes">
  <!-- link rel="icon" sizes="192x192" href="homeicon.png" -->
  <title>Ventilation Control</title>
<style>
body{font-family:Arial;
background:-webkit-radial-gradient(center,circle,rgba(255,255,255,.35),rgba(255,255,255,0) 20%,rgba(255,255,255,0) 21%),-webkit-radial-gradient(center,circle,rgba(0,0,0,.2),rgba(0,0,0,0) 20%,rgba(0,0,0,0) 21%),-webkit-radial-gradient(center,circle farthest-corner,#f0f0f0,#c0c0c0);
background-size:10px 10px,10px 10px,100% 100%;
background-position:1px 1px,0px 0px,centercenter;}
.wid{
background-color:rgba(160,160,180,1);margin:1ex;padding:1ex;
//  box-shadow:
//    inset #000 0px 0px 10px, 
//    #555 4px 4px 10px;
  //border-radius:1ex;
  }
.r{text-align:right;}
.c{text-align:center;}
.rd{color: red;}
//.t{text-shadow: 
//   0px -1px rgba(0,0,0,0.5),
//   0px 1px 0px rgba(255,255,255,.5); 
//  color:#666}
</style>

<script>
// relative fan level to power consumption
function fl2pwatts(l) { return l<0 ? '?' : (9.1510341510342E-06*l*l*l + 0.001189874939875*l*l + 0.200440115440116*l + 5.99999999999999).toFixed(1); }
function createRequest(){return window.XMLHttpRequest?new XMLHttpRequest():new ActiveXObject("Microsoft.XMLHTTP");}
function g(i){return document.getElementById(i);}
function tc(i,v){try{document.getElementById(i).textContent=v;}catch(e){}}
function l(x){try{console.log(x);}catch(e){}}
function ajax(url,succFunc,failFunc,data) {
  var r=window.XMLHttpRequest?new XMLHttpRequest():new ActiveXObject("Microsoft.XMLHTTP");
  r.onreadystatechange=function(){
    if(r.readyState==XMLHttpRequest.DONE){          
      var j;
      //l('resp: '+r.status+' '+r.responseText);
      if(200==r.status){
        if (!r.responseText) {
          hide();
          return failFunc(data,r.status,'Empty response from '+url,'');
        }
        try {
          j=JSON.parse(r.responseText);
        } catch(e){
          l('INVALID:'+r.responseText);
          hide();
          return failFunc(data,r.status,'Invalid json data from '+url, '');
        }
        if (j&&"1"==j['ok']) {
          hide();
          return succFunc(data,j);
        }
      }else if (0==r.status){
        hide();
        return failFunc(data,r.status,'Failed to receive ajax response from '+url, '');
      }
      hide();
      return failFunc(data,r.status,r.responseText,j);
    }
  }
  //l('ajax: '+url);
  try{r.open("GET",url,true);r.send();}catch(e){
    //var j=JSON.parse(JSON_STR);
    //succFunc(data,j);
    failFunc(data,-1,'Unable to send ajax request to'+url, e);
  } 
}

function assign(k,v){
  if(v.constructor==String||v.constructor==Number){
    //log(k+'='+v);
    if (k=='vent.relative') {
      g(k).style.width=v+'%';
      tc(k,v+'%');
    }else if(k=='vent.set'||k=='vent.override'||k=='vent.web'){
      for (var i=0;i<4;i++) {
        tc(k+i,i==v?"▉":"");  
      }           
    }else if(k.startsWith("tsps")){
      try {
        //log("setting color on " + k);
        g(k).style.color=(v==-1)?"#999":"#000";
      }
      catch (e) {
        //log(e);
      }
      tc(k,v);
    }
    else {
      tc(k,v);
    }
  }else if(v.constructor==Array){
    assign(k+'.len',v.length);
    for(var i=0;i<v.length;i++) assign(k+i,v[i]);
  }else{
    for(var l in v) assign(k+'.'+l,v[l]);
  }
}

function onLevelSetFailed(d,rc,rsp,j){console.log("Failed to set level: "+rc+" "+rsp+" "+j);}
function onLevelSet(d,j){assign('vent.override',d['level'])}
function setLevel(level){ajax('/api/set?level='+level,onLevelSet,onLevelSetFailed,{'level':level});}
function onStatusFailed(d,rc,rsp,j){console.log("Failed to read status: "+rc+" "+rsp+" "+j);}
function toC(hC){var c=0.01*hC; return c.toFixed(2);} // hecto-C -> C
function onStatus(d,j){
    try {
      var ss = j['sensors'];
      var sg = ss[2]-ss[1];
      var el = ss[3]-ss[4];
      tc('supply.gain',toC(sg));
      tc('exhaust.gain',toC(el));
    }
    catch (e) {
        console.log(e);
    }
    try {
      var ss = j['sensors'];
      for (i in ss) ss[i]=toC(ss[i]);
      j['sensors'] = ss;
      j['temp']['supply']=toC(j['temp']['supply']);
      j['temp']['exhaust']=toC(j['temp']['exhaust']);
    }
    catch (e) {
        console.log(e);
    }
    
    for (k in j) assign(k,j[k]);
    try {
        // for TSPs below I figured out their values:
        var ts = j['tsps'];
        tc('relFanOff',0);
        tc('relFanLow',ts[0]);
        tc('relFanNorm',ts[2]);
        tc('relFanHigh',ts[4]);

        tc('pwrOff', fl2pwatts(0));
        tc('pwrLow', fl2pwatts(ts[0]));
        tc('pwrNorm',fl2pwatts(ts[2]));
        tc('pwrHigh',fl2pwatts(ts[4]));
        
    }
    catch (e) {
        console.log(e);
    }

    try {
      var ffs = j['faultFlagsCode'];
      g('faultFlags').style.display = (ffs[0]<0 && ffs[1]<0) ? 'none' : '';
    }
    catch (e) {
        console.log(e);
    }

    try {
      var adj = j['timeAdj']; // s
      var loc = "";
      if (adj>0) {
        var now = j['now']; // ms
        var sum = now+1000*adj;        
        loc = localdate(new Date(sum));
      }
      tc('time', loc);
    } catch (e) {
        console.log(e);
    }

    try {
      g('service.filter').style.color=s2c(j.service.filter);
      g('service.cartridge').style.color=s2c(j.service.cartridge);
    } catch (e) {
        console.log(e);
    }

    try {
      var e = g('gateway');
      if (j.unresponsiveness.isResp==1) {
        e.textContent='OK';
        e.style.color='#006000';
      } else {
        e.textContent = 'Reset/replug required!';      
        e.style.color='red';
      }    
    } catch (e) {
        console.log(e);
    }

    try {
      tc('build', localdate(new Date(j.build)));
    } catch (e) {
        console.log(e);
    }
}

function s2c(val) {
  if (val=='OK') return '#006000';
  if (val.indexOf('Waiting')>-1) return '#A08000';
  return 'red';
}

function post() {
  show();
  ajax('/dbg/post');
}

function refresh() {
  show(); 
  readStatus(); 
}

function readStatus(){
  ajax('/api/status?n='+(new Date()).getTime(),onStatus,onStatusFailed,{});
}

function setUdp(v){
  show();
  ajax('/dbg/udp/' + v + '?n='+(new Date()).getTime(),onStatus,onStatusFailed,{});
}

function onLoad(){
  setInterval(function(){readStatus()},5000);
  readStatus();
}

function config(){
  document.location="/setup";
}

function localdate(d) {
  const offset = d.getTimezoneOffset()
  d = new Date(d.getTime() - (offset*60*1000))
  var parts = d.toISOString().split('T');
  return parts[0] + ' ' + parts[1].replace(/\..*/, '');
}

function show() { g('progress').style.visibility='visible'; }
function hide() { g('progress').style.visibility='hidden'; }
</script>

</head>
<body style="margin:0" onload="onLoad()">

<div id="progress" style="visibility:'hidden'; background:rgba(50,50,50,.5); position:fixed; width:100%; height:100%; line-height:100px;" onclick="hide()">
  <h1 style="text-align:center; font-size:96px"><br>⌛<br><br></h1>
</div>

<div>
  <div style="position:relative;text-align:center">
    <span id="title" class="t">VitoWifi</span>
    <div style="position: absolute; left:1em; top:0;">
      <input type="button" onclick="setUdp(1)" value="UDP on"/>
      <input type="button" onclick="setUdp(0)" value="UDP off"/>
    </div>
    <div style="position: absolute; right:1em; top:0;">
      <input type="button" onclick="refresh()" value="Refresh"/>
      <input type="button" onclick="post()" value="Post"/>
    </div>
  </div>
  
  <div class="wid">   
    <table style="width:100%">
      <tr>
        <td class="t">Ventilation</td>
        <td class="c" onclick="setLevel(0)" >OFF</td>
        <td class="c" onclick="setLevel(1)" >LOW</td>
        <td class="c" onclick="setLevel(2)" >NORM</td>
        <td class="c" onclick="setLevel(3)" >HIGH</td>
      </tr>
      <tr>
        <td>Manual</td>
        <td class="c rd" onclick="setLevel(0)" id="vent.override0"></td>
        <td class="c rd" onclick="setLevel(1)" id="vent.override1"></td>
        <td class="c rd" onclick="setLevel(2)" id="vent.override2"></td>
        <td class="c rd" onclick="setLevel(3)" id="vent.override3"></td>
      </tr>
      <tr>
        <td>Automatic</td>
        <td class="c" id="vent.web0"></td>
        <td class="c" id="vent.web1"></td>
        <td class="c" id="vent.web2"></td>
        <td class="c" id="vent.web3"></td>
      </tr>
      <tr style="foreground:red">
        <td>Setpoint</td>
        <td class="c" id="vent.set0"></td>
        <td class="c" id="vent.set1"></td>
        <td class="c" id="vent.set2"></td>
        <td class="c" id="vent.set3"></td>
      </tr>
      <tr>
        <td>Fan speed</td><td colspan="4">
          <div style="border: 0px solid black; box-shadow: inset #222 1px 1px 2px, inset #222 -1px -1px 2px; padding:2px;">
            <div id="vent.relative" style="width:55%; background-color:#ccc; text-align: center;" >
              55%
            </div>
          </div>
        </td>
      </tr>
    </table>
  </div>
  
  <div class="wid">   
    <table style="width:100%">
      <tr><td class="t">Service actions</td></tr>
      <tr><td>Gateway  </td><td id="gateway" ></td></tr>
      <tr><td>Filter   </td><td id="service.filter" ></td></tr>
      <tr><td>Cartridge</td><td id="service.cartridge" ></td></tr>
    </table>
  </div>

  <div class="wid">   
    <table style="width:100%" border="0">
      <tr><td class="t">Temperature (&deg;C)</td><td class="r">Supply</td><td class="r">Exhaust</td></tr>
      <tr><td>Built-in   </td><td class="r" id="temp.supply" ></td><td class="r" id="temp.exhaust" ></td></tr>
      <tr><td></td></tr>
      <tr><td>Inlet </td><td class="r" id="sensors1"   ></td><td class="r" id="sensors3"    ></td></tr>
      <tr><td>Outlet</td><td class="r" id="sensors2"   ></td><td class="r" id="sensors4"    ></td></tr>
      <tr><td>Gain  </td><td class="r" id="supply.gain"></td><td class="r" id="exhaust.gain"></td></tr>
      <tr><td></td></tr>
      <tr><td>Room  </td><td class="r" id="sensors0"></td></tr>
    </table>
  </div>

  <div class="wid">   
    <table style="width:100%" border="0">
      <tr></tr>
      <tr><td>Level</td><td class="r">Throughput (m³/h)</td><td colspan="2" class="r">Power (Watt)</td></tr>
      <tr><td>&nbsp;&nbsp;OFF</td><td id="relFanOff" class="r"></td><td id="pwrOff" class="r"></td></tr>
      <tr><td>&nbsp;&nbsp;LOW</td><td id="relFanLow" class="r"></td><td id="pwrLow" class="r"></td></tr>
      <tr><td>&nbsp;&nbsp;NORM</td><td id="relFanNorm" class="r"><td id="pwrNorm" class="r"></td></tr>
      <tr><td>&nbsp;&nbsp;HIGH</td><td id="relFanHigh" class="r"><td id="pwrHigh" class="r"></td></tr>
    </table>
  </div>

  <div class="wid">   
    <table style="width:100%">
      <tr><td>Filter check      </td><td class="r"><span id="filterCheck"      ></span></td><td/></tr>
      <tr><td>Overall status   </td><td class="r"><span id="status0"          ></span></td><td class="r"><span id="status1"          ></span></td></tr>
      <tr id='faultFlags'><td>Fault flags       </td><td class="r"><span id="faultFlagsCode0"  ></span></td><td class="r"><span id="faultFlagsCode1"  ></span></td></tr>
      <tr><td>Config member ID  </td><td class="r"><span id="configMemberId0"  ></span></td><td class="r"><span id="configMemberId1"  ></span></td></tr>
      <tr><td>Master config     </td><td class="r"><span id="masterConfig0"    ></span></td><td class="r"><span id="masterConfig1"    ></span></td></tr>
      <tr><td>Remote param flags</td><td class="r"><span id="remoteParamFlags0"></span></td><td class="r"><span id="remoteParamFlags1"></span></td></tr>
    </table>
  </div>

  <div class="wid">   
    <table style="width:100%">
      <tr>
        <td class="t">Messages  </td><td class="r">Total</td><td class="r t">M</td><td class="r t">S</td><td class="r t">R</td><td class="r t">A</td>
      </tr>    
      <tr>
        <td>Expected  </td>
        <td class="r" id="messages.serial">
        <td class="r" id="messages.expected.T"></td>
        <td class="r" id="messages.expected.B"></td>
        <td class="r" id="messages.expected.R"></td>
        <td class="r" id="messages.expected.A"></td>
      </tr>
      <tr>
        <td>Unexpected  </td>
        <td></td>
        <td class="r" id="messages.unexpected.T"></td>
        <td class="r" id="messages.unexpected.B"></td>
        <td class="r" id="messages.unexpected.R"></td>
        <td class="r" id="messages.unexpected.A"></td>
      </tru>
      <tr>
        <td>Invalid due to</td>
        <td></td>
        <td class="r t">Len</td>
        <td class="r t">Fmt</td>
        <td class="r t">Src</td>
      </tr>
      <tr>
        <td></td>
        <td></td>
        <td class="r" id="messages.invalid.len"></td>
        <td class="r" id="messages.invalid.format"></td>
        <td class="r" id="messages.invalid.src"></td>
      </tru>
      <tr><td>Last      </td><td class="r" colspan="5"><span id="lastMsg.serial"></span><span> ms ago</span></td></tr>
    </table>
  </div>

  <div class="wid">   
    <table style="width:100%" border="0">
      <tr><td class="t" colspan="9">Transparent Slave Parameters</td></tr>
      <tr>
        <td class="t">0-7</td>
        <td id="tsps0">.</td><td id="tsps1">.</td><td id="tsps2">.</td><td id="tsps3">.</td>
        <td id="tsps4">.</td><td id="tsps5">.</td><td id="tsps6">.</td><td id="tsps7">.</td>
      </tr>
      <tr>
        <td class="t">8-15</td>
        <td id="tsps8">.</td><td id="tsps9">.</td><td id="tsps10">.</td><td id="tsps11">.</td>
        <td id="tsps12">.</td><td id="tsps13">.</td><td id="tsps14">.</td><td id="tsps15">.</td>
      </tr>
      <tr>
        <td class="t">16-23</td>
        <td id="tsps16">.</td><td id="tsps17">.</td><td id="tsps18">.</td><td id="tsps19">.</td>
        <td id="tsps20">.</td><td id="tsps21">.</td><td id="tsps22">.</td><td id="tsps23">.</td>
      </tr>
      <tr>
        <td class="t">24-31</td>
        <td id="tsps24">.</td><td id="tsps25">.</td><td id="tsps26">.</td><td id="tsps27">.</td>
        <td id="tsps28">.</td><td id="tsps29">.</td><td id="tsps30">.</td><td id="tsps31">.</td>
      </tr>
      <tr>
        <td class="t">32-39</td>
        <td id="tsps32">.</td><td id="tsps33">.</td><td id="tsps34">.</td><td id="tsps35">.</td>
        <td id="tsps36">.</td><td id="tsps37">.</td><td id="tsps38">.</td><td id="tsps39">.</td>
      </tr>
      <tr>
        <td class="t">40-15</td>
        <td id="tsps40">.</td><td id="tsps41">.</td><td id="tsps42">.</td><td id="tsps43">.</td>
        <td id="tsps44">.</td><td id="tsps45">.</td><td id="tsps46">.</td><td id="tsps47">.</td>
      </tr>
      <tr>
        <td class="t">48-55</td>
        <td id="tsps48">.</td><td id="tsps49">.</td><td id="tsps50">.</td><td id="tsps51">.</td>
        <td id="tsps52">.</td><td id="tsps53">.</td><td id="tsps54">.</td><td id="tsps55">.</td>
      </tr>
      <tr>
        <td class="t">56-63</td>
        <td id="tsps56">.</td><td id="tsps57">.</td><td id="tsps58">.</td><td id="tsps59">.</td>
        <td id="tsps60">.</td><td id="tsps61">.</td><td id="tsps62">.</td><td id="tsps63">.</td>
      </tr>
    </table>
  </div>
  
  <div class="wid">   
    <table style="width:100%">
      <tr><td>Health</td></tr>      
      <tr><td>Build      </td><td class="r" id="build"></td></tr>
      <tr><td>Uptime     </td><td class="r" id="uptime"></td></tr>
      <tr style="display:none"><td>Time adjustment </td><td class="r"><span id="timeAdj"></span>s</td></tr>
      <tr><td>Time  </td><td class="r"><span id="time"></span></td></tr>
      <tr><td>Reboots       </td><td class="r" id="reboots"></td></tr>
      <tr><td>Last reason   </td><td class="r" id="esp.reboot"></td></tr>
      <tr><td></td><td class="r" id="esp.details"></td></tr>
      <tr><td>Next reboot   </td><td class="r" id="nextReboot"></td></tr>
    </table>
  </div>

  <div class="wid">   
    <table style="width:100%">
      <tr><td>Web posts</td></tr>      
      <tr><td>Next post</td><td class="r" id="web.next"></td></tr>
      <tr><td>Last post</td><td class="r" id="web.last"></td></tr>
      <tr><td>Last error</td><td class="r" id="web.lastError"></td></tr>
      <tr><td>Number of posts</td><td class="r" id="web.posts"></td></tr>
      <tr><td>Server script</td><td class="r"><span id="script.n"></span> Vers. </span><span id="script.v"></span></td></tr>
    </table>
  </div>

  <div class="wid">   
    <table style="width:100%">
      <tr><td>WiFi</td></tr>
      <tr><td>Hostname</td><td class="r" id="wifi.host"></td></tr>
      <tr><td>IP addr.</td><td class="r"><span id="wifi.ip"></span></td></tr>
      <tr><td>MAC addr.</td><td class="r" id="wifi.mac"></td></tr>
      <tr><td>BSSID</td><td class="r"><span id="wifi.bssid"></span></td></tr>
      <tr><td>RSSI</td><td class="r"><span id="wifi.rssi"></span> dBm</td></tr>
    </table>
  </div>

  <div class="wid">   
    <table style="width:100%">
      <tr><td>Access point</td></tr>
      <tr><td>SSID</td><td class="r"><span id="ap.ssid"></span></td></tr>
      <tr><td>IP addr.</td><td class="r"><span id="ap.ip"></span></td></tr>
      <tr><td>MAC addr.</td><td class="r"><span id="ap.mac"></span></td></tr>
      <tr><td>Number of clients</td><td class="r"><span id="ap.cl"></span></td></tr>
    </table>
  </div>

  <div class="wid">   
    <table style="width:100%">
      <tr><td>Hardware</td></tr>
      <tr><td>Chip ID</td><td class="r" id="esp.id"></td></tr>
      <tr><td>CPU </td><td class="r"><span id="esp.freq"></span> MHz </td></tr>
      <tr><td>Core version</td><td class="r" id="esp.core"></td></tr>
      <tr><td>SDK version</td><td class="r" id="esp.sdk"></td></tr>
      <tr><td>Sensors    </td><td class="r" id="sensorsFound"></td></tr>
    </table>
  </div>

  <div class="wid">   
    <table style="width:100%">
      <tr><td>Memory</td></tr>
      <tr><td>Sketch size </td><td class="r"><span id="esp.sketch"></span> Bytes </td></tr>
      <tr><td>Least heap space</td><td class="r"><span id="mem.heap.least"></span> Bytes </td></tr>
      <tr><td>Highest stack usage</td><td class="r"><span id="mem.stack.max"></span> Bytes </td></tr>
    </table>
  </div>

<!--
<div style="padding: 1ex">      
<iframe width="100%" height="260" style="border: 1px solid #cccccc;" src="https://thingspeak.com/channels/your_id/charts/your_channel1?"></iframe>
<div style="height:1ex;"></div>
<iframe width="100%" height="260" style="border: 1px solid #cccccc;" src="https://thingspeak.com/channels/your_id/charts/your_channel2?"></iframe>
<div style="height:1ex;"></div>
<iframe width="100%" height="260" style="border: 1px solid #cccccc;" src="https://thingspeak.com/channels/your_id/charts/your_channel3?"></iframe>
</div>
-->

<!--
  <div>
    <div style="position: relative; left:1em; top:0;"><input type="button" onclick="config()" value="Setup"/></div>
  </div>
-->

</body>
</html>


)====="
