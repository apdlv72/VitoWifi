R"=====(<?xml version="1.0" encoding="UTF-8" ?>
<!DOCTYPE html PUBLIC "-//WAPFORUM//DTD XHTML Mobile 1.0//EN" "http://www.wapforum.org/DTD/xhtml-mobile10.dtd">
<html>
<head>
<meta charset='utf-8'>
<title>Network Setup</title>
<script>
function createRequest(){if (window.XMLHttpRequest){return new XMLHttpRequest();}else{return new ActiveXObject("Microsoft.XMLHTTP");}}
function g(id){return document.getElementById(id);}
function v(id){return g(id).value;}
function tc(id,v){g(id).textContent=v;}
function e(x){return encodeURIComponent(x);}
function l(x){try{console.log(x);}catch(e){}}
function showError(txt){tc('errmsg',txt);vi('error',1);setTimeout(function(){vi('error',0)},2000);}
function onFailure(msg) {
	h('connecting');
	showError("Failed to connect to network.\n"+msg);	
}

var TEST = false;

function onConnected(ssid,addr,token) {
	vi('connecting',0);
	tc('netw2a', ssid);
	tc('netw2b', ssid);
	tc('addr2',  addr);
	vi('connected',1);	
	var sLink="http://"+addr+"/setup?a=confirm";
	try {tc('link2',sLink);g('link2').href=sLink;} catch(e){}
}

function waitTillConnected(ssid,token) {
    var r=createRequest();
    r.onreadystatechange=function() {        
        if (r.readyState==XMLHttpRequest.DONE){        	
            if (200==r.status){
            	var j=JSON.parse(r.responseText);	
            	var s=j['status'];
            	if("1"==s){ 
            		g('progr1').textContent+='▉';
            		tc('time',j['left']);
            		setTimeout(function(){waitTillConnected(ssid,token);},1000); 
            	}
            	if("0"==s)onConnected(ssid,j['addr'],token);
            	if("2"==s)onFailure();
            }
	        else{
	        	onFailure("ERROR: waitTillConnected: status: "+r.status+", result:"+r.responseText);
	        }
        }
    }
    r.open("GET", "/ajax?a=wait", true);
    r.send();	
}

function vi(id,v){g(id).style.visibility=v?"visible":"hidden";}
function h(id){vi(id,0);}

function setNetwork() {
	var ssid=v('ssid');
	var psk=v('psk');		
	if (""==ssid||""==psk)return showError("Network or password empty");
	if (psk.length<8)return showError("Minimum password length: 8 characters");
	
    var token=Math.floor(1000000000*Math.random());
    var r=createRequest();
    r.onreadystatechange=function() {
        if (r.readyState==XMLHttpRequest.DONE){        	
            if (200==r.status) {
            	l('parsing json: ' + r.responseText);
            	var j=JSON.parse(r.responseText);
            	l('json: ' + j);
            	if (!j['ok'])return showError(j['msg']);
                tc('netw1',ssid);
            	vi('connecting',1);
            	g('progr1').textContent='▉';
            	waitTillConnected(ssid,token);
            }
	        else {
	        	showError("ERROR: setNetwork: status: "+r.status+", result:"+r.responseText);
	        }
        }
    }
    var url = "/ajax?a=set&ssid="+e(ssid)+"&psk="+e(psk);
    l('GET: ' + url);
    r.open("GET", url, true);
    r.send();
}


function getNetworkInfo() {
	vi('configap',1);
	tc('netw3', 'TODO'); // /ajax?a=status will caddr and saddr
	tc('addr3', 'TODO');
}

function s(){
	var confirm = document.URL.indexOf('a=confirm')>-1;	
	if (confirm)getNetworkInfo();
}

function checkKeep() {
	var checked = g('keepap').checked;
	g('apcredentials').style.display=checked?"block":"none";			
}

function waitTillRebooted() {
    var r=createRequest();
    r.onreadystatechange=function() {    	
        if (r.readyState==XMLHttpRequest.DONE){        	
        	var rc = r.status;  
        	if (TEST) {
	        	//r.responseText ='{"ok":false,"msg":"Password too short"}';
	        	r.responseText ='{"ok":true,"msg":"ok"}';
	        	rc=200;
        	}
            if (200==rc) {
            	var j=JSON.parse(r.responseText);
            	l(j);
            	if (!j['ok'])return showError(j['msg']);
            	setTimeout(function(){waitTillRebooted();},2000);
            	document.location="/";
            }
	        else {
	        	showError("ERROR: waitTillRebooted: status: "+r.status+", result:"+r.responseText);
	        }
        }
    }
    r.open("GET", "/ajax?a=status", true);
    r.send();	
}

function checkAPCredentials() {
	var keepap = g('keepap').checked;
	var apssid  = v('apssid');
	var appsk1  = v('appsk1');
	var appsk2  = v('appsk2');
	var url     = "/ajax?a=save&ap=0";
	if (keepap) {
		if (apssid=="")      return showError("AP name must not be empty");
		if (appsk1!=appsk2)  return showError("Passwords do not match");
		if (appsk1.length<8) return showError("Minimum password length is 8 characters");
		url = "/ajax?a=save&ap=1&apssid="+e(apssid)+"&appsk="+e(appsk1);
	}
	
    var r=createRequest();
    r.onreadystatechange=function() {    	
        if (r.readyState==XMLHttpRequest.DONE){        	
            if (200==r.status) {
            	var j=JSON.parse(r.responseText);
            	l(j);
            	if (!j['ok'])return showError(j['msg']);
            	vi('configap', 0);
            	vi('rebooting',1);            	
            	setTimeout(function(){waitTillRebooted();}, 2000);
            }
	        else {
	        	showError("ERROR: checkAPCredentials: status: "+r.status+", result:"+r.responseText);
	        }
        }
    }
    r.open("GET", url, true);
    r.send();
    setInterval(function(){g('progr4').textContent+='▉';},500);
}

</script>
<style>
body{
font-family:Arial;
background: -webkit-radial-gradient(center, circle, rgba(255,255,255,.35), rgba(255,255,255,0) 20%, rgba(255,255,255,0) 21%), -webkit-radial-gradient(center, circle, rgba(0,0,0,.2), rgba(0,0,0,0) 20%, rgba(0,0,0,0) 21%), -webkit-radial-gradient(center, circle farthest-corner, #f0f0f0, #c0c0c0);
background-size: 10px 10px, 10px 10px, 100% 100%;
/*background-repeat: repeat, repeat, no-repeat;*/
background-position: 1px 1px, 0px 0px, center center;
}
.modal{visibility:hidden;position:absolute;top:0;left:0;width:100%;height:100%;background-color:rgba(0,0,40,0.6);}
.dia{position:relative;top:10%;left:10%;width:80%;background-color:#fff;border-radius:1em; box-shadow: black 4px 4px 10px;}
.close{position:absolute;right:2ex;top:1ex;}
.title{color:#888;text-shadow:#fff -1px -1px 1px,#000 1px 1px 1px;}
.c{text-align:center;}
.butt{box-shadow: rgba(0,0,0,0.5) 0px 0px 20px;}
</style>

</head>
<body onload="s()"'>
	<div id="connecting" class="modal">
		<div class="dia">
			<div style="padding:1em">
				<div><h2 class="title" id="title1">Connecting network</h2></div>
				<div>
					<span id="netw1"></span>
					<br/>
					<br/>
				</div>
				<div style="text-overflow:ellipsis;white-space:nowrap;overflow:hidden;">
					<span class="title" id="progr1" ></span>
				</div>
				<div style="margin-top:.5em"><span id="time"></span> seconds left ...</div>
			</div>
		</div>
	</div>
	<div id="connected" class="modal">
		<div class="dia">
			<div style="padding:1em">
				<div><h2 class="title" id="title2">Network connected</h2></div>
				<table>
					<tr><td>Network:</td><td id="netw2a"></td></tr>
					<tr><td>Address:</td><td id="addr2"></td></tr>
				</table>
				<ol>
					<li>Disconnect from this access point.</li>
					<li>Connect to <b><span id="netw2b"></span></b>.</li>
					<li>Navigate to <a id="link2"></a></li>
				</ol>
				If you cancel now, the settings will not be saved permanently
				and therefore discarded after next reboot.<br/></br/>
				<div class="c">
					<input class="butt" type="button" onclick="h('connected')" value="Cancel"/>
				</div>
			</div>
		</div>
	</div>
	<div id="configap" class="modal">
		<div class="dia">
			<div style="padding:1em">
				<div><h2 class="title" id="title3">Device found</h2></div>
				You are now connected via the network<span id="netw3"></span>.
				with IP address <span id="addr3"></span>
				<br/>
				You can now switch off the access point of the device. If you do so,
				you can only connect to it using the WiFi network configured.<br/>
				<br/>
				If leave it enabled, you can still connect to the device directly. 
				You need to provide secure credentials in this case.<br/><br/>
				<input id="keepap" onchange="checkKeep()" type="checkbox"/>&nbsp;Leave AP active<br/><br/> 
				<div id="apcredentials" style="display:none">
					<table>
					<tr><td>AP name: </td><td><input id="apssid"/></td></tr>
					<tr><td>Password:</td><td><input id="appsk1"/></td></tr>
					<tr><td>Repeat:  </td><td><input id="appsk2"/></td></tr>
					</table>
				<br/><br/>	
				</div>
				<div class="c" style="text-align: center"">
					<input id="save" class="butt" onclick="checkAPCredentials()" type="button" value="Save and reboot"/>
					<input id="save" class="butt" onclick="h('configap')" type="button" value="Cancel"/>
				</div>
			</div>
		</div>
	</div>
	<div id="rebooting" class="modal">
		<div class="dia">
			<div style="padding:1em">
				<div><h2 class="title" id="title4">Rebooting</h2></div>
				<div><span id="progr4"></span></div>
			</div>
		</div>
	</div>
	<div id="error" class="modal">
		<div class="dia" style="background-color:rgba(255,200,200,0.8)">
			<div style="padding:1em 2em 2em 2em;">
				<div class="close" onclick="h('error')">❌</div>
				<div><h1 class="title" id="title5">Error</h1></div>
				<span id="errmsg"></span>.
				<br/>
				<br/>
			</div>
		</div>
	</div>
	<div style="margin:auto; display: table;">
		<h1 id="title" class="title">VitoWifi</h1>
		<div style="margin:auto; display: table;">
		<span class="title">Network:</span><br/>
		<input type="text" id="ssid" maxlength="32"/><br/>
		<br/>
		<span class="title">Password:</span><br/>
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