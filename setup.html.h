R"=====(<?xml version="1.0" encoding="UTF-8" ?>
<!DOCTYPE html PUBLIC "-//WAPFORUM//DTD XHTML Mobile 1.0//EN" "http://www.wapforum.org/DTD/xhtml-mobile10.dtd">
<html>
<head>
<meta charset='utf-8'>
<title>Network Setup</title>
<style>
body{font-family:Arial;background-color:#AAF}
</style>
</head>
<body>
	<h1>Network setup</h1><br/>
	Please provide credentials of the WiFi network to connect to:<br/>
	<div style="color:red">$ERR$</div>
	<form method="get">
		<input type="hidden" name="stage" value="save" /><br/>
		Network:<br/>
		<input type="text" name="ssid" size="32" value="$NWSSID$" /><br/>
		Password:<br/>
		<input type="text" name="psk" size="32" value="$NWPSK$" /><br/>
		<br/>
		<input type="submit" name="submit" value="Save" />
	</form>
</body>
</html>
)====="