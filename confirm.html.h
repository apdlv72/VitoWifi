R"=====(<?xml version="1.0" encoding="UTF-8" ?>
<!DOCTYPE html PUBLIC "-//WAPFORUM//DTD XHTML Mobile 1.0//EN" "http://www.wapforum.org/DTD/xhtml-mobile10.dtd">
<html>
<head>
	<meta charset='utf-8'>
	<title>Confirm Network Settings</title>
<style>
body{font-family:Arial;background-color:#BFB}
</style>
</head>
<body>
    <h1>Confirm settings</h1>
	If you see this page, network setup should have been successful<br/> 
    <table>
      <tr><td>Your IP:</td><td>$YOURIP$</td><td>on network $YOURNW$</td></tr> 	
	  <tr><td>My   IP:</td><td>$MYIP$  </td><td>on network $MYNW$. </td></tr>
    </table>
    <div style="color:red">$ERR$</div>
	<br/>
	You can now chose to disable the access point '$APSSID$'.<br/> 
	If you want to keep it,	please make sure to change the least the default password.<br/>
    <br/>
	<form method="get">
		<div style="border: 1px solid black; padding: 10px; ">
	        <h2>Access point settings:</h2>
			<input type="checkbox" name="apenable"/> leave access point enabled<br/>
			Name (SSID):<br/>
			<input type="text" name="apssid" value="$APSSID$"><br/>
			Password (miminum 8 characters):<br/>
			<input type="text" name="appsk" value="$APPSK$"><br/>
			<input type="hidden" name="stage" value="save">
		</div>
		</br/>
		Press "Save" to save your settings permanently and reboot<br/>
		<input type="submit" name="submit" value="Save"><br/>			 
	</form>
</body>
</html>
)====="
