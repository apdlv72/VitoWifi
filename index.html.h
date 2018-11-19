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
.wid{background-color:rgba(100,100,120,0.5);margin:1ex;padding:1ex;
	box-shadow:
		inset #000 0px 0px 10px, 
		#555 4px 4px 10px;
	border-radius:1ex;}
.r{text-align:right;}
.c{text-align:center;}
.t{text-shadow: 
	 0px -1px rgba(0,0,0,0.5),
	 0px 1px 0px rgba(255,255,255,.5); 
	color:#666}
</style>

<script>
JSON_STR=
'{ ' + 
'		  "ok":1,' + 
'		  "build":"Mar 10 2016 12:51:08",' + 
'		  "reboots":"38",' + 
'		  "now":357430,' + 
'		  "uptime":"0d,0h,5m,57s",' + 
'		  "lastMsg":{"serial":0,"wifi":{"fed":0,"debug":0}},' + 
'		  "dbg":{"level":1},' + 
'		  "vent":{"set":1,"override":2,"ackn":-1,"reported":-1,"relative":55},' + 
'		  "temp":{"supply":-30000,"exhaust":-30000},' +
' "sensors":[20,23,24,25.8],'+
' "debug":2,'+
'		  "status":[-9,-4],' + 
'		  "faultFlagsCode":[-2,-1],' + 
'		  "configMemberId":[-3,-4],' + 
'		  "masterConfig":[-6,-5],' + 
'		  "remoteParamFlags":[-9,-2],' + 
'		  "filterCheck":-1,' + 
'		  "messages":{' + 
'		    "total":0,' + 
'		    "serial":0,' + 
'		    "wifi":{"fed":0,"debug":0},' + 
'		    "invalid":{"len":0,"format":0,"src":0},' + 
'		    "expected":{"T":0,"B":0,"R":0,"A":0},' + 
'		    "unexpected":{"T":0,"B":0,"R":0,"zero":0,"A":0}' + 
'		  },' + 
'		  "tsps":[' + 
'		    -1,-1,-1,-1,-1,-1,-1,-1,' + 
'		    -1,-1,-1,-1,-1,-1,-1,-1,' + 
'		    -1,-1,-1,-1,-1,-1,-1,-1,' + 
'		    -1,-1,-1,-1,-1,-1,-1,-1,' + 
'		    -1,-1,-1,-1,-1,-1,-1,-1,' + 
'		    -1,-1,-1,-1,-1,-1,-1,-1,' + 
'		    -1,-1,-1,-1,-1,-1,-1,-1,' + 
'		    -1,-1,-1,-1,-1,-1,-1,-1]' + 
'}';


function createRequest(){return window.XMLHttpRequest?new XMLHttpRequest():new ActiveXObject("Microsoft.XMLHTTP");}
function g(i){return document.getElementById(i);}
function tc(i,v){try{document.getElementById(i).textContent=v;}catch(e){}}
function l(x){try{console.log(x);}catch(e){}}
function ajax(url,succFunc,failFunc,data) {
	var r=window.XMLHttpRequest?new XMLHttpRequest():new ActiveXObject("Microsoft.XMLHTTP");
	r.onreadystatechange=function(){
		if(r.readyState==XMLHttpRequest.DONE){        	
			var j;
			l('resp: '+r.status+' '+r.responseText);
			if(200==r.status){
				if (!r.responseText)return failFunc(data,r.status,'Empty response from '+url,'');
				try{
					j=JSON.parse(r.responseText);
				}catch(e){
					l('INVALID:'+r.responseText);
					return failFunc(data,r.status,'Invalid json data from '+url, '');
				}
				if (j&&"1"==j['ok'])return succFunc(data,j);
			}else if (0==r.status){
				return failFunc(data,r.status,'Failed to receive ajax response from '+url, '');
			}
			return failFunc(data,r.status,r.responseText,j);
		}
	}
	l('ajax: '+url);
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
		}else if(k=='vent.set'||k=='vent.override'){
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

function parseJson(jsonstr) {
	l(JSON_STR);
	var j = JSON.parse(jsonstr);
}

function onLevelSetFailed(d,rc,rsp,j){console.log("Failed to set level: "+rc+" "+rsp+" "+j);}
function onLevelSet(d,j){assign('vent.override',d['level'])}
function setLevel(level){ajax('/api/set?level='+level,onLevelSet,onLevelSetFailed,{'level':level});}

function onStatusFailed(d,rc,rsp,j){console.log("Failed to read status: "+rc+" "+rsp+" "+j);}
function onStatus(d,j){
    for (k in j) assign(k,j[k]);
    try {
        // for TSPs below I figured out their values:
        var ts = j['tsps'];
        tc('relFanLow',ts[0]);
        tc('relFanNorm',ts[2]);
        tc('relFanHigh',ts[4]);
    }
    catch (e) {
        console.log(e);
    }
}

function readStatus(){
	l("<readStatus>");
	ajax('/api/status?n='+(new Date()).getTime(),onStatus,onStatusFailed,{});
	l("</readStatus>");
}

function onLoad(){
	setInterval(function(){readStatus()},5000);
}

function config(){
	document.location="/setup";
}
</script>

</head>

<body onload="onLoad()">
<div>
	<div style="position:relative;text-align:center">
		<span id="title" class="t">VitoWifi</span>
		<div style="position: absolute; right:1em; top:0;"><input type="button" onclick="readStatus()" value="Update"/></div>
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
		  	<td>Override</td>
		  	<td class="c" onclick="setLevel(0)" id="vent.override0">▉</td>
		  	<td class="c" onclick="setLevel(1)" id="vent.override1"></td>
		  	<td class="c" onclick="setLevel(2)" id="vent.override2"></td>
		  	<td class="c" onclick="setLevel(3)" id="vent.override3"></td>
		  </tr>
		  <tr>
		  	<td>Setpoint</td>
		  	<td class="c" id="vent.set0"></td>
		  	<td class="c" id="vent.set1">▉</td>
		  	<td class="c" id="vent.set2"></td>
		  	<td class="c" id="vent.set3"></td>
		  </tr>
		  <tr>
		  	<td>Relative</td><td colspan="4">
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
		  <tr><td class="t">Temperature</td><td colspan="2">Supply</td><td colspan="2">Exhaust</td></tr>
		  <tr><td>Inlet      </td><td id="temp.supply" ></td><td>&deg;C</td><td id="temp.exhaust" ></td><td>&deg;C</td></tr>
		  <tr><td>Outlet     </td><td id="sensors0"></td><td>&deg;C</td><td id="sensors1"></td><td>&deg;C</td></tr>
		  
		  <tr><td style="height:.5em"></td></tr>
		  
		  <tr><td></td><td  colspan="2">Basement</td><td colspan="2">Room</td></tr>
		  <tr><td></td><td id="sensors2"></td><td>&deg;C</td><td id="sensors3"></td><td>&deg;C</td></tr>
		</table>
	</div>

	<div class="wid">		
		<table style="width:100%">
			<tr><td class="t">Status:            </td></tr>
			<tr><td>Overall:           </td><td><span id="status0"          ></span></td><td><span id="status1"          ></span></td></tr>
			<tr><td>Fault flags:       </td><td><span id="faultFlagsCode0"  ></span></td><td><span id="faultFlagsCode1"  ></span></td></tr>
			<tr><td>Config member ID:  </td><td><span id="configMemberId0"  ></span></td><td><span id="configMemberId1"  ></span></td></tr>
			<tr><td>Master config:     </td><td><span id="masterConfig0"    ></span></td><td><span id="masterConfig1"    ></span></td></tr>
			<tr><td>Remote param flags:</td><td><span id="remoteParamFlags0"></span></td><td><span id="remoteParamFlags1"></span></td></tr>
		</table>
	</div>

	<div class="wid">		
		<table style="width:100%">
			<tr><td class="t">Messages:  </td><td class="r t">M</td><td class="r t">S</td><td class="r t">R</td><td class="r t">A</td></tr>		
			<tr><td>Total:     </td><td class="r" id="messages.serial"></td></tr>
			<tr>
				<td>Expected:  </td>
				<td class="r" id="messages.expected.T"></td>
				<td class="r" id="messages.expected.B"></td>
				<td class="r" id="messages.expected.R"></td>
				<td class="r" id="messages.expected.A"></td>
			</tr>
			<tr>
				<td>Unexpected:  </td>
				<td class="r" id="messages.unexpected.T"></td>
				<td class="r" id="messages.unexpected.B"></td>
				<td class="r" id="messages.unexpected.R"></td>
				<td class="r" id="messages.unexpected.A"></td>
			</tru>
			<tr>
				<td></td>
				<td class="r t">Len</td>
				<td class="r t">Fmt</td>
				<td class="r t">Src</td>
			</tr>
			<tr>
				<td>Invalid:  </td>
				<td class="r" id="messages.invalid.len"></td>
				<td class="r" id="messages.invalid.format"></td>
				<td class="r" id="messages.invalid.src"></td>
			</tru>
			<tr><td>Last:      </td><td class="r" colspan="5"><span id="lastMsg.serial"></span><span>s ago</span></td></tr>
		</table>
	</div>

	<div class="wid">		
		<table style="width:100%" border="1">
			<tr><td class="t" colspan="9">Transparent Slave Parameters</td></tr>
			<tr>
				<td colspan="9">Relative fan speed:</td>
			</tr>
			<tr>
				<td colspan="4">&nbsp;&nbsp;reduced:</td><td colspan="5" id="relFanLow"></td>
			</tr>
			<tr>
				<td colspan="4">&nbsp;&nbsp;normal:</td><td colspan="5" id="relFanNorm"></td>
			</tr>
			<tr>
				<td colspan="4">&nbsp;&nbsp;high:</td><td colspan="5" id="relFanHigh"></td>
			</tr>
			<tr>
				<td class="t">0-7</td>
				<td id="tsps0">.</td><td id="tsps1">.</td><td id="tsps2">.</td><td id="tsps3">.</td>
				<td id="tsps4">.</td><td id="tsps5">.</td><td id="tsps6">.</td><td id="tsps7">.</td>
			</tr>
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
			<tr><td class="t">Info:  </td></tr>		
			<tr><td>Build:     </td><td class="r" id="build"></td></tr>
			<tr><td>Uptime:    </td><td class="r" id="uptime"></td></tr>
			<tr><td>Reboots:   </td><td class="r" id="reboots"></td></tr>
<!--			
			<tr><td>NW:        </td><td class="r">Kamplumbaga</td></tr>
			<tr><td>           </td><td class="r">192.168.179.35</td></tr>
			<tr><td>AP:        </td><td class="r">thing-VitoWifi</td></tr>
			<tr><td>           </td><td class="r">192.168.4.1</td></tr>
-->		
			<tr><td>Debug level:</td><td class="r" id="debug"></td></tr>
			<tr><td>Sensors:</td><td class="r" id="sensors.len"></td></tr>
			<tr><td>Heap space:</td><td class="r" id="freeheap"></td></tr>
		</table>
	</div>

	
	<div>
		<div style="position: relative; left:1em; top:0;"><input type="button" onclick="config()" value="Setup"/></div>
	</div>

</body>
</html>


)====="
