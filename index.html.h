R"=====(<?xml version="1.0" encoding="UTF-8" ?>
<!DOCTYPE html PUBLIC "-//WAPFORUM//DTD XHTML Mobile 1.0//EN" "http://www.wapforum.org/DTD/xhtml-mobile10.dtd">
<html>
<head>
	<!-- https://developer.chrome.com/multidevice/android/installtohomescreen -->
	<meta charset='utf-8'>
	<!-- link rel="manifest" href="manifest.json" -->
    <meta name="viewport" content="width=device-width">
	<meta name="mobile-web-app-capable" content="yes">
	<!-- link rel="icon" sizes="192x192" href="homeicon.png" -->
	<title>Ventilation Control</title>
<style>
body{margin:0px;background-color:#888;font-family:Arial;color:white;}
</style>

<script>
function createRequest() {
	if (window.XMLHttpRequest){return new XMLHttpRequest();}else{return new ActiveXObject("Microsoft.XMLHTTP");}
}

function g(id, val) { return document.getElementById(id).textContent = val; }

function parseJson(jsonstr) {
	
	var j = JSON.parse(jsonstr);	
	g('lastMsgReceived', j['lastRcv']);
	
	g('status',           j['status']);
	g('faultFlagsCode',   j['faultFlagsCode']);
	g('configMemberId',   j['configMemberId']);
	g('masterConfig',     j['masterConfig']);
	g('remoteParamFlags', j['remoteParamFlags']);

	var v = j['vent'];
	g('ventSetpoint', v['setpoint']); 
	g('ventOverride', v['override']); 
	g('ventRelative', v['setpoint']);
	
	var t = j['temp'];
	g('tempSupply',  t['supply']); 
	g('tempExhaust', t['exhaust']); 
}

TEST_JSON = '{\
  "lastRcv":0,\
  "vent":{"setpoint":0,"override":0,"relative":0},\
  "temp":{"supply":0,"exhaust":0},\
  "status":[0,0],\
  "faultFlagsCode":[0,0],\
  "configMemberId":[0,0],\
  "masterConfig":[0,0],\
  "remoteParamFlags":[0,0],\
  "tsps":[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]\
}';

function readStatus() {
    var xmlhttp = createRequest();
    xmlhttp.onreadystatechange = function() {        
        if (xmlhttp.readyState == XMLHttpRequest.DONE ) {        	
            if (xmlhttp.status == 200) {
            	parseJson(xmlhttp.result);
            }
	        else {
            	parseJson(TEST_JSON);
	        }
        }
    }

    xmlhttp.open("GET", "/status.json", true);
  	xmlhttp.send();
}

function s() {
	parseJson(TEST_JSON);
}
</script>

</head>

<body onload="s()">
<div>
	<input type="button" onclick="readStatus()" value="Update"/>
	<hr noshade="noshade" />
	<table>
	<tr><td>lastMsgReceived:  </td><td><span id="lastMsgReceived"></span></td></tr>
	<tr><td>status:           </td><td><span id="status"></span></td></tr>
	<tr><td>faultFlagsCode:   </td><td><span id="faultFlagsCode"></span></td></tr>
	<tr><td>configMemberId:   </td><td><span id="configMemberId"></span></td></tr>
	<tr><td>masterConfig:     </td><td><span id="masterConfig"></span></td></tr>
	<tr><td>remoteParamFlags: </td><td><span id="remoteParamFlags"></span></td></tr>
	<tr><td>ventSetpoint:     </td><td><span id="ventSetpoint"></span></td></tr>
	<tr><td>ventOverride:     </td><td><span id="ventOverride"></span></td></tr>
	<tr><td>ventRelative:     </td><td><span id="ventRelative"></span></td></tr>
	<tr><td>tempSupply:       </td><td><span id="tempSupply"></span></td></tr>
	<tr><td>tempExhaust:      </td><td><span id="tempExhaust"></span></td></tr>
	</table>
</div>	
</body>
</html>
)====="