R"=====(<?xml version="1.0" encoding="UTF-8" ?>
<!DOCTYPE html PUBLIC "-//WAPFORUM//DTD XHTML Mobile 1.0//EN" "http://www.wapforum.org/DTD/xhtml-mobile10.dtd">
<html>
<head>
	<meta charset='utf-8'>
	<title>Network Settings Saved</title>
<style>
body{font-family:Arial;background-color:#888}
</style>
<script>
function createRequest(){if(window.XMLHttpRequest){return new XMLHttpRequest();}else{return new ActiveXObject("Microsoft.XMLHTTP");}}
function poll() {
  document.getElementById('pr').textContent += ".";      		
  var r = createRequest();
  r.onreadystatechange = function() {        
    if (r.readyState == XMLHttpRequest.DONE && r.status == 200) { document.location = "/"; }  
  }
  try {  
    r.open("GET", "/level", true);
    r.send();
  } catch (e){}
  
  window.setTimeout(function(){r.abort();}, 2000);
}
function start() { window.setInterval(poll, 1000); }
</script>
</head>

<body onload="start()">
	<h1 id="pr">Rebooting ...</h1>
    Click <a href="/">here</a> when not redirected within 10 seconds.
</body>
</html>
)====="
