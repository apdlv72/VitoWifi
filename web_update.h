
#include "secrets.h"

WiFiClientSecure client;

unsigned long lastConnectionSuccess = 0;

#define MAX_LENGTH 256

void hw_wdt_disable(){
  *((volatile uint32_t*) 0x60000900) &= ~(1); // Hardware WDT OFF
}

void hw_wdt_enable(){
  *((volatile uint32_t*) 0x60000900) |= 1; // Hardware WDT ON
}

boolean responseBodyComplete(const String& body) {
  dbgln(String("responseBodyComplete: '") + body + "'");
  if (body.length() > MAX_LENGTH) {
    // avoid memory overflow
    dbgln("COMPLETE (size)");
    return true;
  }
  // Google script
  if (body.indexOf("ResultCode:") > 0) {
    dbgln("COMPLETE (alert)");
    return true;
  }
  // callbot API:
  if (body.indexOf("Message to") >= 0) {
    dbgln("COMPLETE (callmebot)");
    return true;
  }
  if (body.indexOf("ventLevel=") >= 0 && body.indexOf("lastCheck=") >= 0) {
    dbgln("COMPLETE (gscript)");
    return true;
  }

  dbgln("NOT COMPLETE");
  checkMemory();
  return false;
}

String extractHost(String url) {
  String httpsProto = "https://";
  if (url.startsWith(httpsProto)) {
    url = url.substring(httpsProto.length());
    int slashPos = url.indexOf("/");
    if (slashPos >= 0) {

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
    if (slashPos >= 0) {
      return url.substring(slashPos);
    }
    return "/";
  }
  return "";
}

/*
   Starting with the URL as defined by "web_url", return the response body folling
   at most one redirection if necessary.
*/
String webGet(String url, String params, boolean ignoreBody) {

  long startTime = millis();
  dbgln(String("getWebPage: params=") + params);

  client.setInsecure();
  //client.setHandshakeTimeout(seconds);

  String host = extractHost(url);
  String path = extractPath(url);

#ifdef ESP8266
  ESP.wdtDisable() ;
#endif
  dbgln(String("Connecting to '") + host + "'");
  unsigned long t0 = millis();
  bool connected = client.connect(host, 443);
  unsigned long t1 = millis();
  checkMemory();
#ifdef ESP8266
  ESP.wdtEnable(5000);
#endif

  dbgln(String("Connected to '") + host + "' in " + (t1 - t0) + " ms");
  if (!connected) {
    dbgln("Connection failed!");
    return "";
  }
  dbgln("Connected!");
  lastConnectionSuccess = millis();
  client.setTimeout(30 * 1000);
  
  //String getLine  = String("GET /macros/s/") + google_key + "/exec" + (params.length()>0 ? "?" : "") + params + " HTTP/1.0\r\n";
  String pathLine = String("GET "  ) + path + (params.length() > 0 ? "?" : "") + params + " HTTP/1.0\r\n";
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
    if (line == "\r" || line == "") break; // End of response headers
    if (line.indexOf("Location") >= 0) { // Redirect location found
      redirURL = line.substring(line.indexOf(":") + 2) ;
      redirURL.trim();
    }
  }
  checkMemory();

  // When there was no redirection return this response body
  if (redirURL.length() < 1) {
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
  checkMemory();
  client.stop();

  //dbgln("Redirect: '" + redirURL + "'");
  host = extractHost(redirURL);
  path = extractPath(redirURL);
  pathLine = "GET "   + path + " HTTP/1.0\r\n";
  hostLine = "Host: " + host + "\r\n";

#ifdef ESP8266
  ESP.wdtDisable() ;
#endif
  dbgln(String("Connecting to '") + host + "'");
  t0 = millis();
  connected = client.connect(host, 443);
  t1 = millis();
#ifdef ESP8266
  ESP.wdtEnable(5000);
#endif

  //dbgln(String("Connected to '") + host + "' in " + (t1 - t0) + " ms");
  if (!connected) {
    dbgln("Redirection failed!");
    return "";
  }
  dbgln("Connected!");
  client.setTimeout(2 * 1000);
  checkMemory();

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
  checkMemory();

  if (ignoreBody) {
    //dbg("Ignoring (redirect) response body as requested");
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
        dbgln("** COMPLETE **");
        break;
      }
    }
  }
  checkMemory();
  client.stop();

  //unsigned long duration = millis() - startTime;
  //dbgln(String("getWebPage: responseBody: '") + responseBody + "'");
  //dbgln(String("getWebPage: duration: ") + duration + " ms");

  return responseBody;
}

String createStatusJson();

String webPost(String url, boolean ignoreBody) {

  //long startTime = millis();
  //dbgln(String("webPost: startTime=") + startTime);
  client.setInsecure();  
  //client.setHandshakeTimeout(seconds);

  String pathLine = extractPath(url);
  String hostLine = extractHost(url);

  dbgln(String("Connect to '") + hostLine + "'");
  #ifdef ESP8266
    ESP.wdtDisable() ;
    //hw_wdt_disable();
  #endif
  bool connected = client.connect(hostLine, 443);
  #ifdef ESP8266
    ESP.wdtEnable(5000);
    //hw_wdt_enable();
  #endif
  if (!connected) {
    dbgln("Connection failed!");
    return "";
  }
  //dbgln("Connected!");
  checkMemory();

  lastConnectionSuccess = millis();
  client.setTimeout(30 * 1000);

  String postData = createStatusJson();  
  pathLine = String("POST "  ) + pathLine + " HTTP/1.0\r\n";
  hostLine = String("Host: ") + hostLine + "\r\n";
  dbg(pathLine);
  dbg(hostLine);
  client.print(pathLine);
  client.print(hostLine);
  client.print(String("Content-Length: ") + postData.length() + "\r\n");
  client.print("Content-Type: application/json\r\n");
  client.print("Connection: close\r\n");
  client.print("Accept: */*\r\n\r\n");
  client.print(postData);
  postData.clear(); // free heap asap
  pathLine.clear();
  hostLine.clear();
  checkMemory();

  String redirURL;

  // While reading response headers, check for "Location:"
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    dbgln(line);
    if (line == "\r" || line == "") break; // End of response headers
    if (line.indexOf("Location") >= 0) { // Redirect location found
      redirURL = line.substring(line.indexOf(":") + 2) ;
      redirURL.trim();
    }
  }
  checkMemory();

  // When there was no redirection return this response body
  if (redirURL.length() < 1) {
    if (ignoreBody) {
      dbg("Ignore resp body");
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
    checkMemory();
    client.stop();
    return responseBody;
  }
  checkMemory();
  client.stop();

  //dbgln("Redirect: '" + redirURL + "'");
  hostLine = extractHost(redirURL);
  pathLine = extractPath(redirURL);

  dbgln(String("Connect to '") + hostLine + "'");
  #ifdef ESP8266
    ESP.wdtDisable() ;
    //hw_wdt_disable();
  #endif  
  connected = client.connect(hostLine, 443);
  #ifdef ESP8266
    ESP.wdtEnable(5000);
    //hw_wdt_enable();
  #endif

  if (!connected) {
    dbgln("Redirection failed!");
    return "";
  }
  //dbgln("Connected!");
  client.setTimeout(2 * 1000);
  checkMemory();

  pathLine = "GET "   + pathLine + " HTTP/1.0\r\n";
  hostLine = "Host: " + hostLine + "\r\n";
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
  checkMemory();

  if (ignoreBody) {
    //dbg("Ignoring (redirect) response body as requested");
    checkMemory();
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
        dbgln("** COMPLETE **");
        break;
      }
    }
  }
  checkMemory();
  client.stop();

  //unsigned long duration = millis() - startTime;
  //dbgln(String("webPostJson: responseBody: '") + responseBody + "'");
  //dbgln(String("webPostJson: duration: ") + duration + " ms");

  return responseBody;
}
