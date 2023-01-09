
#include "secrets.h"

WiFiClientSecure client;

boolean responseBodyComplete(const String& body) {
  dbgln(String("responseBodyComplete: '") + body + "'");
  if (body.length()>1000) {
    // avoid memory overflow
    dbgln("COMPLETE (size)");
    return true;
  }
  // Google script
  if (body.indexOf("ResultCode:")>0) {
    dbgln("COMPLETE (alert)");
    return true;
  }
  // callbot API:
  if (body.indexOf("Message to")>=0) {
    dbgln("COMPLETE (callmebot)");
    return true;
  }
  if (body.indexOf("ventLevel=")>=0 && body.indexOf("lastCheck=")>=0) {
    dbgln("COMPLETE (gscript)");
    return true;
  }

  dbgln("NOT COMPLETE");
  return false;
}

String extractHost(String url) {
  String httpsProto = "https://";
  if (url.startsWith(httpsProto)) {
    url = url.substring(httpsProto.length());
    int slashPos = url.indexOf("/");
    if (slashPos>=0) {
      
      return url.substring(0, slashPos);
    } 
  }
  return ""; 
}

String extractPath(String url) {
  String httpsProto = "https://";
  if (url.startsWith(httpsProto)) {
    url = url.substring(httpsProto.length());
    int slashPos = url.indexOf("/");
    if (slashPos>=0) {
      return url.substring(slashPos);    
    } 
    return "/";
  }
  return "";
}


/*
 * Starting with the URL as defined by "web_url", return the response body folling
 * at most one redirection if necessary.
 */
String getWebPage2(String url, String params, boolean ignoreBody) {

  long startTime = millis();
  dbgln(String("getWebPage: params=") + params);

  client.setInsecure();

  String host = extractHost(url);
  String path = extractPath(url);
  
  dbgln(String("Connect to '") + host + "'");
  if (!client.connect(host, 443)) {
    dbgln("Connection failed!");
    return "";
  }
  dbgln("Connected!");
  client.setTimeout(30 * 1000);

  //String getLine  = String("GET /macros/s/") + google_key + "/exec" + (params.length()>0 ? "?" : "") + params + " HTTP/1.0\r\n";
  String pathLine = String("GET "  ) + path + (params.length()>0 ? "?" : "") + params + " HTTP/1.0\r\n";
  String hostLine = String("Host: ") + host + "\r\n";
  dbg(pathLine);
  dbg(hostLine);
  client.print(pathLine);
  client.print(hostLine);
  client.print("Connection: close\r\n");
  client.print("Accept: */*\r\n\r\n");
  
  String redirURL;

  // While reading response headers, check for "Location:" 
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    dbgln(line);
    if (line=="\r" || line=="") break; // End of response headers
    if (line.indexOf("Location") >= 0) { // Redirect location found
      redirURL = line.substring(line.indexOf(":") + 2) ;
      redirURL.trim();
    }
  }

  // When there was no redirection return this response body
  if (redirURL.length()<1) { 
    if (ignoreBody) {      
      dbg("Ignoring response body as requested");
      client.stop();
      return "";      
    }
    String responseBody = "";
    while (client.connected()) {
      if (client.available()) {
        String line = client.readStringUntil('\n');
        dbgln(line);
        responseBody += line;
        responseBody += '\n';
        if (responseBodyComplete(responseBody)) {
          break;
        }
      }
    }
    client.stop();
    return responseBody;    
  }  
  client.stop();

  //dbgln("Redirect: '" + redirURL + "'");
  host = extractHost(redirURL);
  path = extractPath(redirURL);
  pathLine = "GET "   + path + " HTTP/1.0\r\n";
  hostLine = "Host: " + host + "\r\n";

  dbgln(String("Connect to '") + host + "'");
  if (!client.connect(host, 443)) {
    dbgln("Redirection failed!");
    return "";
  }
  dbgln("Connected!");
  client.setTimeout(2 * 1000);

  dbg(pathLine);
  dbg(hostLine);
  client.print(pathLine);
  client.print(hostLine);
  client.print("Connection: close\r\n");
  client.print("Accept: */*\r\n\r\n");

  while (client.connected()) { // receive headers
    String line = client.readStringUntil('\n');
    dbgln(line);
    if (line == "\r" || line == "") break;
  }

  if (ignoreBody) {      
    dbg("Ignoring (redirect) response body as requested");
    client.stop();
    return "";      
  }
  String responseBody = "";
  while (client.connected()) { // read reponse body
    if (client.available()) {
      String line = client.readStringUntil('\n');
      responseBody += line;
      responseBody += '\n';
      if (responseBodyComplete(responseBody)) {
        dbgln("************ COMPLETE ************");
        break;
      }
    }
  }
  client.stop();

  unsigned long duration = millis()-startTime;
  dbgln(String("getWebPage: responseBody: '") + responseBody + "'");
  dbgln(String("getWebPage: duration: ") + duration + " ms");

  return responseBody;
}

String getWebPage(String params) {
  return getWebPage2(UPDATE_URL, params, false);
}
