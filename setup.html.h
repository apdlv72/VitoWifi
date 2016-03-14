R"=====(<?xml version="1.0" encoding="UTF-8" ?>
<!DOCTYPE html PUBLIC "-//WAPFORUM//DTD XHTML Mobile 1.0//EN" "http://www.wapforum.org/DTD/xhtml-mobile10.dtd">
<html>
<head>
	<title>VitoWifi Setup</title>
	<meta charset="utf-8">
	<meta name="mobile-web-app-capable" content="yes">
    <meta name="viewport" content="width=device-width">
	<link rel="manifest" href="manifest.json">
	<link rel="icon" sizes="192x192" href="homeicon.png">
	<link rel="shortcut icon" href="data:image/x-icon;," type="image/x-icon"> 
<script>
function g(i){return document.getElementById(i);}
function v(i){return g(i).value;}
function tc(id,v){g(id).textContent=v;}
function e(x){return encodeURIComponent(x);}
function l(x){try{console.log(x);}catch(e){}}
function showError(txt,rc,rsp,j){tc('errmsg',txt);show('error');setTimeout(function(){hide('error')},2000);}
function vi(i,v){g(i).style.visibility=v?"visible":"hidden";}
function hide(i){vi(i,0);}
function show(i){vi(i,1);}
function ajax(qs,succFunc,failFunc,data) {
	var r=window.XMLHttpRequest?new XMLHttpRequest():new ActiveXObject("Microsoft.XMLHTTP");
	var u='/ajax?'+qs;
	r.onreadystatechange=function(){
		if(r.readyState==XMLHttpRequest.DONE){        	
			var j;
			l('resp: '+r.status+' '+r.responseText);
			if(200==r.status){
				try{j=JSON.parse(r.responseText);}catch(e){}
				if (j&&"1"==j['ok'])return succFunc(data,j);
			}
			return failFunc(data,r.status,r.responseText,j);
		}
	}
	l('ajax: '+u);
	try{r.open("GET",u,true);r.send();}catch(e){failFunc(0,'',''+e);}
}
function onConnected(d,j){
	hide('connecting');
	//l('onConnected: d='+d+',j='+j);
	var a=j['addr'];var s=d['ssid'];var t=d['token'];
	tc('addr2',a);tc('netw2a',s);tc('netw2b',s);
	var l="http://"+a+"/setup?a=confirm&t="+e(t);
	try{tc('link2',l);g('link2').href=l;}catch(e){}
	show('connected');	
}
function onWaitConnectedTimeout(){showError("Reboot timed out");}
function onWaitConnectedFailed(d,rc,rsp,j){showError("Error checking reboot status.",rc,rsp);}
function onWaitConnected(d,j){
	var s=j['status'];
	var ssid=d['ssid'];
	var token=d['token'];
	//l('onWaitConnected: ssid='+ssid+', token='+token);
	if("0"==s)onConnected(d,j);
	if("1"==s){
		g('progr1').textContent+='▉';
		tc('time',j['left']);
		setTimeout(function(){waitTillConnected(ssid,token);},1000); 		
	} 
	if("2"==s)onWaitConnectedTimeout();
}
function waitTillConnected(ssid,token){ajax("a=wait&t="+token,onWaitConnected,onWaitConnectedFailed,{'ssid':ssid,'token':token});}
function onSetNetworkFailed(d,rc,msg,j){showError('Failed to set network credentials. '+msg);}
function onSetNetwork(d,j){	
	var ssid=v('ssid');
    tc('netw1',ssid);
	show('connecting');	
	g('progr1').textContent='▉';
	waitTillConnected(ssid,j['token']);
}
function setNetwork(){
	var ssid=v('ssid');
	var psk=v('psk');		
	if (""==ssid||""==psk)return showError("Network or password empty");
	if (psk.length<8)return showError("Minimum password length: 8 characters");
	var qs="a=set&ssid="+e(ssid)+"&psk="+e(psk);
	ajax(qs,onSetNetwork,onSetNetworkFailed);
}

function onNetworkInfoFailed(d,rc,rsp,j){showError('Failed to get network/address info.',rc,rsp,j);}	
function onNetworkInfo(d,j){tc('nwssid3',j['nwssid']);tc('saddr3',j['saddr']);tc('caddr3',j['caddr']);show('configap');}
function getNetworkInfo(){ajax('a=status',onNetworkInfo,onNetworkInfoFailed,{});}

function onWaitRebootFailed(d,rc,rsp,j){showError("Failed to check status.",rc,rsp,j);}
function onWaitReboot(d,j){document.location="/";}
function waitTillRebooted(){ajax('a=status',onWaitReboot,onWaitRebootFailed,{});}

function onSetAPFailed(d,rc,rsp,j){showError('Failed to save changes. '+j['msg'],rc,rsp,j);}
function onSetAP(d,j){
	hide('configap');
	setTimeout(function(){waitTillRebooted();},2000);	
	show('rebooting');            	
}
function setAP(){
	var keepap=g('keepap').checked;
	var token=v('_token_');
	var apssid=v('apssid');
	var appsk1=v('appsk1');
	var appsk2=v('appsk2');
	var qs="a=save&t="+e(token);
	if (keepap){
		if (apssid=="")return showError("AP name must not be empty");
		if (appsk1!=appsk2)return showError("Passwords do not match");
		if (appsk1.length<8)return showError("Minimum password length is 8 characters");
		qs+="&ap=1&apssid="+e(apssid)+"&appsk="+e(appsk1);
	}else{
		qs+="&ap=0";
	}
	ajax(qs,onSetAP,onSetAPFailed,{});
    setInterval(function(){g('progr4').textContent+='▉';},500);	
}
function checkKeep(){g('apcreds').style.display=g('keepap').checked?"block":"none";}
function onLoad(){
	var u=document.URL;
	if(u.indexOf('a=confirm')>-1){
		g('_token_').value=u.replace(/.*&t=/,'').replace(/&.*/,'');
		getNetworkInfo();
	}
}
</script>
<style>
body{font-family:Arial;
background:-webkit-radial-gradient(center,circle,rgba(255,255,255,.35),rgba(255,255,255,0) 20%,rgba(255,255,255,0) 21%),-webkit-radial-gradient(center,circle,rgba(0,0,0,.2),rgba(0,0,0,0) 20%,rgba(0,0,0,0) 21%),-webkit-radial-gradient(center,circle farthest-corner,#f0f0f0,#c0c0c0);
background-size:10px 10px,10px 10px,100% 100%;
background-position:1px 1px,0px 0px,centercenter;}
.modal{visibility:hidden;position:absolute;top:0;left:0;width:100%;height:100%;background-color:rgba(0,0,40,0.6);}
.dia{position:relative;top:10%;left:10%;width:80%;background-color:rgba(255,255,255,0.9);border-radius:1em;box-shadow:black 4px 4px 10px;}
.close{position:absolute;right:2ex;top:1ex;}
.t{color:#888;text-shadow:#fff -1px -1px 1px,#000 1px 1px 1px;}
.c{text-align:center;}
.butt{box-shadow:rgba(0,0,0,0.5) 0px 0px 20px;}
</style>
</head>
<body onload="onLoad()">
	<div id="connecting" class="modal">
		<div class="dia">
			<div style="padding:1em">
				<div><h2 class="t" id="title1">Connecting network</h2></div>
				<div>
					<span id="netw1"></span>
					<br/>
					<br/>
				</div>
				<div style="text-overflow:ellipsis;white-space:nowrap;overflow:hidden;">
					<span class="t" id="progr1" ></span>
				</div>
				<div style="margin-top:.5em"><span id="time"></span> seconds left ...</div>
				<div class="c">
					<input class="butt" type="button" onclick="hide('connecting')" value="Cancel"/>
				</div>
			</div>
		</div>
	</div>
	<div id="connected" class="modal">
		<div class="dia">
			<div style="padding:1em">
				<div><h2 class="t" id="title2">Network connected</h2></div>
				<table>
					<tr><td>Network:</td><td id="netw2a"></td></tr>
					<tr><td>Address:</td><td id="addr2"></td></tr>
				</table>
				<ol>
					<li>Disconnect from this access point.</li>
					<li>Connect to <b><span id="netw2b"></span></b>.</li>
					<li>Navigate to <a id="link2"></a></li>
				</ol>
				If you cancel now, the settings will not be saved permanently and therefore discarded after next reboot.<br/></br/>
				<div class="c">
					<input class="butt" type="button" onclick="hide('connected')" value="Cancel"/>
				</div>
			</div>
		</div>
	</div>	
	<div id="configap" class="modal">
		<div class="dia">
			<div style="padding:1em">
				<div><h2 class="t" id="title3">Device found</h2></div>
				You are now connected via the network <span id="nwssid3"></span> to IP address <span id="saddr3"></span>.<br/>
				<br/>
				Your own IP address is <span id="caddr3"></span>).<br/>
				<br/>
				You can now switch off the access point of the device. If you do so, you can only connect to it using the WiFi network configured.<br/>
				<br/>
				If leave it enabled, you can still connect to the device directly. You need to provide secure credentials in this case.<br/><br/>
				<input id="keepap" onchange="checkKeep()" type="checkbox"/>&nbsp;Leave AP active<br/><br/> 
				<div id="apcreds" style="display:none">
					<table>
					<tr><td>AP name: </td><td><input id="apssid"/></td></tr>
					<tr><td>Password:</td><td><input id="appsk1"/></td></tr>
					<tr><td>Repeat:  </td><td><input id="appsk2"/></td></tr>
					</table>
				<br/><br/>	
				</div>
				<div class="c" style="text-align: center"">
					<input id="_token_" type="hidden" />
					<input id="save" class="butt" onclick="setAP()" type="button" value="Save and reboot"/>
					<input id="cancel" class="butt" onclick="hide('configap')" type="button" value="Cancel"/>
				</div>
			</div>
		</div>
	</div>
	<div id="rebooting" class="modal">
		<div class="dia">
			<div style="padding:1em">
				<div><h2 class="t" id="title4">Rebooting</h2></div>
				<div><span id="progr4"></span></div>
			</div>
		</div>
	</div>
	<div id="error" class="modal">
		<div class="dia" style="background-color:rgba(255,200,200,0.8)">
			<div style="padding:1em 2em 2em 2em;">
				<div class="close" onclick="hide('error')">❌</div>
				<div><h1 class="t" id="title5">Error</h1></div>
				<span id="errmsg"></span>.
				<br/>
				<br/>
			</div>
		</div>
	</div>
	<div style="margin:auto; display: table;">
		<h1 id="title" class="t">VitoWifi</h1>
		<div style="margin:auto; display: table;">
		<span class="t">Network:</span><br/>
		<input type="text" id="ssid" maxlength="32"/><br/>
		<br/>
		<span class="t">Password:</span><br/>
		<input type="text" id="psk"  maxlength="32"/><br/>
		<br/>
		<div class="c">
			<input type="button" class="butt" onclick="setNetwork()" name="connect" value="Connect"/>
		</div>
		</div>
	</div>
</body>
</html>
)====="
