

// Copy secrets.h to secrets_priv.h, same for index.html.h and comment in to 
// activate your own versions of this file. Add both files to .gitignore!!!
#define WITH_OWN_SECRETS

#ifdef WITH_OWN_SECRETS
#include "secrets_priv.h"
#else
#include "secrets.h"
#endif

/**
   Arduino sketch for a ESP-8266 WiFi micro controller to control a Vitovent 300
   home ventilation unit via a OpenTherm Gateway and a WiFi connection.
   See https://github.com/apdlv72/VitoWifi or README.md
   @author: apdlv72@gmail.com
*/

// Uncomment WITH_DALLAS_TEMP_SENSORS to enable support for DS18B290 temperature sensors connected on PIN_SENSOR.
// MAX_NUM_SENSORS defines the maximum number of sensors connected.
// Since this sketch is for a home ventilation system, actually 4 would be sufficient for inlet/outlet air supply
// and inlet/outlet exhaust. However, defining 8 here lets us add even more e.g. for monitoring the room temperature(s)
// in your basement and/or living room, monitoring the temperature inside the OTGW device and so forth.
// Every sensor requires just 2 bytes in RAM, so a large value is not too critical.
#define WITH_DALLAS_TEMP_SENSORS

#define MAX_NUM_SENSORS 6

// Uncomment to support a pin test on http://192.168.4.1/pintest?p=0.
// When called, the mode of the respective pin "p" will be set to OUTPUT and value toogled between hi/low a couple of times.
// This is useful to find out which pin number is assigned to what pin. Be careful with pins used by the ESP.
// Check https://github.com/esp8266/Arduino/issues/1219 for more details.
//#define WITH_PIN_TEST

// Uncomment to enable ID to text translation of all known OpenTherm data IDs.
// This cause a considerable overhead in RAM usage and is not enable by default since it is not used for normal operation.
// However, should you use this sketch as a starting point to control anything else than a VitoVent 300, you might enable
// this to learn what your devices are talking to eachother.
//#define WITH_ALL_DATAIDS

// enable OTA (over the air programming)
//#define WITH_OTA

// Enable serial debug functions
//#define WITH_DEBUG_SERIAL

// Enable sending debug messages on UDP port
#define WITH_DEBUG_UDP
#define UDP_PORT 4220

// Enable sending continuous stream of OT messages received on /dbg/tailf
// #define WITH_TAILF

// Enable a help page on http://192.168.4.1/help
//#define WITH_USAGE_HELP

// Enable html page to setup the device via browser.
//#define WITH_WEB_SETUP

// Enable upating ventilation level from an external web page on a regular basis.
// Also requied to send alerts using a web API.
#define WITH_WEB_CALLS

// Enable adding invalid lines (commands) being handles as errors
#define WITH_INVALID_LINES_AS_ERRORS false

const char DEFAULT_START_SSID[] = "AP_VitoWifi";

// Initial password used to connect to it and set up networking.
// The device will normally show up as SSID "http://192.168.4.1"
// Connect to this network using the intial password and navigate your
// browser to the above URL.
#define DEFAULT_PSK  "12345678"

#define HTTP_LISTEN_PORT 80

/****************************************/

#include <errno.h>
#include <EEPROM.h>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#elif ESP32
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#else
#error Unsupported platform
#endif

#ifdef WITH_OTA
#include <ArduinoOTA.h>
#endif

#include <WiFiUdp.h>
#ifdef WITH_WEB_CALLS
#include <UrlEncode.h>
#endif

#ifdef WITH_DALLAS_TEMP_SENSORS
#include <OneWire.h>
#include <DallasTemperature.h>
#endif

// Using a copy of "easy EEPROM" library since needed to modify for ESP
// to cope with watchdog timer resets (https://github.com/apdlv72/eEEPROM)
//#include "eEEPROM.h"

const String buildNo   = __DATE__ " " __TIME__;

#define LED_HEART 	  0 // heartbeat 
#define LED_ONBOARD   2 // blue onboard LED / temp. sensors

#define LEVEL_NOOVER -1 // do not overrride (monitor mode)
#define LEVEL_OFF     0 // standby, ventilation off
#define LEVEL_LOW     1 // "reduced" mode (27% for mine ... may vary)
#define LEVEL_NORM    2 // "normal"  mode (55% for mine) 
#define LEVEL_HIGH    3 // "party"   mode (100%)

// result codes of onOTMessage
#define RC_OK         0	// message handled
#define RC_INV_LENGTH 1 // invalid message length (not Sxxxxxxxx)
#define RC_INV_FORMAT 2 // xxxxxxxx was not a parsable hex value
#define RC_INV_SOURCE 3 // S is not a known source (T,B.R,A)
#define RC_UNEXP_T    4 // unexpected message from T(hermostat)/controller/master
#define RC_UNEXP_B    5 // unexpected message from B(oiler)/ventilation/slave
#define RC_UNEXP_R    6 // unexpected request (sent by OTG in favor of master)
#define RC_UNEXP_A    7 // unexpected answer  (sent by OTG in favor of slave)
#define RC_UNEXP_O    8 // unexpected message from unknown source (should never occur) 

#define CT_TEXT_PLAIN  "text/plain"
#define CT_TEXT_HTML   "text/html"
#define CT_APPL_JSON   "application/json"

// Context type sent along with content stored in PROGMEM:
const char PGM_CT_TEXT_HTML[] PROGMEM = "text/html";
const char PGM_CT_APPL_JSON[] PROGMEM = "application/json";

#ifdef WITH_OWN_SECRETS
#pragma message "index_priv.h is active"
const char HTML_INDEX[] PROGMEM =
#include "index_priv.html.h"
  // The following line makes buildNo accessible for javascript:
  "<script>var buildNo = '" __DATE__ " " __TIME__ "';</script>";
;
#else
const char HTML_INDEX[] PROGMEM =
#include "index.html.h"
  // The following line makes buildNo accessible for javascript:
  "<script>var buildNo = '" __DATE__ " " __TIME__ "';</script>";
;
#endif

#ifdef WITH_WEB_SETUP
const char HTML_SETUP[] PROGMEM =
#include "setup.html.h"
  ;
#endif

#ifdef WITH_ICONS
const char MANIFEST_JSON[] PROGMEM =
#include "manifest.json.h"
  ;

const char FAVICON_ICO[] PROGMEM =
#include "favicon.ico.h"
  ;

const char FAN_PNG[] PROGMEM =
#include "fan.png.h"
  ;
#endif  

// Conveniently zero-terminates a C-string.
#define TZERO(STR) { (STR)[sizeof((STR))-1] = 0; }

#include "OTMessage.h"

typedef struct {
  // Use signed 16 bits to allow -1 to indicate that
  // value was not yet assigned.
  int16_t hi;
  int16_t lo;
} Value;

// Global status
struct {
  struct {
    long serial; // time of last message via serial line
#ifdef WITH_DEBUG_SERIAL
    struct {
      long fed;		// via URL /feedmsg?msg=...&feed=1
      long debug; 	// via URL /feedmsg?msg=... (parsed only, not fed)
    } wifi;
#endif
  } lastMsg;         // time (millis()) when last message was received

  int16_t  ventSetpoint; 	   // as set by controller
  int16_t  ventOverride; 	   // as overridden by OTGW (sent to slave instead of ventSetpoint)
  int16_t  ventWeb;          // received from external web resource/page (see web_update.h)
  int16_t  ventAcknowledged; // by the slave, sent to OTGW
  unsigned long lastAcknChange;

  int16_t  ventReported;     // from OTGW to controller, pretending level is as requested (ventSetpoint)
  int16_t  ventRelative; 	   // as reported by ventilation
  int16_t  tempSupply;  	   // milli Celsius (inlet)
  int16_t  tempExhaust;  	   // milli Celsius (inlet)
  int16_t  tsps[64];  	 	   // transparent slave parameters

  struct {
    Value master;
    Value slave;
  }        version;			// values for DI_*_PROD_VERS

  Value    status;
  Value    faultFlagsCode;
  Value    configMemberId;
  Value    masterConfig;
  Value    remoteParamFlags;

  struct {
    long serial;	// total number of messages received on serial line

    struct {
      long fed; 	// via /feedmsg?feed=1...
      long debug; 	// (parsed only, not fed)
    }    wifi;

    // number of invalid messages received
    struct {
      long length;		// invalid length
      long format;		// invalid line (pasing failed)
      long source;		// invalid source (not T,B,R,A)
    } invalid;

    // expected messages of various types
    struct {
      long T; long B; long R; long A;
      long otgw; // otgw message response e.g. "GW: 1" replied on "GW=1"
    } expected;

    // unexpected messages of various types
    struct {
      long T; long B; long R; long A; long zero;
    } unexpected;

  } messages; // number of messages received

  unsigned long timeAdj = 0;
  String scriptVersion;
  String scriptName;
  String filterAction;
  String cartridgeAction;

#ifdef WITH_DALLAS_TEMP_SENSORS
  int sensorsFound;
  long lastMeasure;
  int16_t extraTemps[MAX_NUM_SENSORS];
#endif

} state;

// Health monitoring and alert/report book keeping
struct {
  // time when requested and acknowleged override started to differ
  unsigned long ventDiffStarted = 0;
  // time when last report sent that there is a difference
  unsigned long lastDiffReported = 0;
  // time we sent a recovery message (device responsive again)
  unsigned long lastRecoveryReported = 0;
  // time of last OTGW reset attempt
  unsigned long lastGWReset = 0 ;
  // time of when GW reset was reported
  unsigned long lastGWResetReported = 0;

  boolean isResponsive  = true;
  boolean wasResponsive = true;

  // initial memory
  long initalFree = -1;

  // 1 hour before millis() will roll over
  unsigned long rebootTime = 4294967296 - 3600 * 1000;

  boolean startAnnounced = false;
  boolean rebootAnnounced = false;

  unsigned long lastWebCheck = 0;
  unsigned long lastWebCheckError = 0;

  unsigned long webFailCount = 0;
  unsigned long postOkCount = 0;

  String lastRebootDetails = "";

  long loopIterations = 0;
} health;


#define WEB_CHECK_INTERVAL (20*60*1000) // 20 minutes

// Credentials used to 1. set up ESP8266 as an access point and 2. define WiFI network to connect to.
typedef struct {
  uint8_t used;
  char    ssid[32 + 1];
  char    psk[32 + 1];
} Credentials;

// I'm from cologne ;)
const uint16_t MAGIC = 0x4712
;

// Everything that goes to EEPROM
struct {
  // Detect if first time setup of EEPROM was done.
  uint16_t  magic;
  boolean 	configured;
  // Start ESP in access point mode?
  Credentials accessPoint;
  // Network to connect to
  Credentials homeNetwork;
  // Count incremented on every reboot to enable monitoring of device crashes.
  uint32_t reboots;
  char     rebootDetails[128];
} EE;

// User committed first time setup (e.g. AP, network) and it was saved permanently.
boolean configured = false;

// A random token used during setup to authenticate committer.
char configToken[10 + 1];

// Time at which an attempt to connect to a network will be considered expired during setup procedure.
long connectAttemptExpires = -1;

// Remember whether temperatures have been already requested for the first time.
boolean temperaturesRequested = false;

// After setup, a reboot is necessary. However, cannot do this while handling client request,
// because this will abort the connection. Instead of this, finish sending the reponse to the
// client and schedule a reboot after millis() will exceed this value.
long rebootScheduledAfterSetup = -1;

// A string to hold incoming serial data from OTGW.
String inputString = "";
// wWhether the string is complete.
boolean stringComplete = false;

// set in setup()
unsigned long stopUdpLoggingAfter = 0;

#ifdef ESP8266
ESP8266WebServer server(HTTP_LISTEN_PORT);
#elif ESP32
WebServer server(HTTP_LISTEN_PORT);
#endif

/***************************************************************

   Heart beat LED

*/

boolean led_lit = false;
volatile boolean led_blink = false;
int timer_value = 250000;

//void ICACHE_RAM_ATTR timerRoutine() {
void IRAM_ATTR timerRoutine() {
  if (led_blink) {
    led_lit = (millis() / 50) % 2;
    digitalWrite(LED_HEART, led_lit ? LOW : HIGH);
  }
  timer1_write(timer_value);
}

void ledToggle() {
  led_lit = !led_lit;
  digitalWrite(LED_HEART, led_lit ? LOW : HIGH);
}

void ledFlash(int ms) {
  ledToggle();
  delay(ms);
  ledToggle();
}

/***************************************************************

   Dallas temperature sensors support

*/

#ifdef WITH_DALLAS_TEMP_SENSORS

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 2 // GPIO2
#define TEMPERATURE_PRECISION 9


// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

#endif // WITH_DALLAS_TEMP_SENSORS

/***************************************************************
   Debug support
*/

#define DBG_OFF 0
#define DBG_ON  1
#define DBG_UDP 2

#if defined(WITH_DEBUG_SERIAL)

int DEBUG_LEVEL = DBG_ON;
void dbg(String s)         {
  if (DBG_OFF < DEBUG_LEVEL) {
    Serial.print("");
    Serial.print(s);
  }
}
void dbgln(String s)       {
  if (DBG_OFF < DEBUG_LEVEL) {
    Serial.print("");
    Serial.println(s);
  }
}
void dbg(const char * s)   {
  if (DBG_OFF < DEBUG_LEVEL) {
    Serial.print("");
    Serial.print(s);
  }
}
void dbgln(const char * s) {
  if (DBG_OFF < DEBUG_LEVEL) {
    Serial.print("");
    Serial.println(s);
  }
}
void dbg(int i)            {
  if (DBG_OFF < DEBUG_LEVEL) {
    Serial.print("");
    Serial.print(i);
  }
}
void dbgln(int i)          {
  if (DBG_OFF < DEBUG_LEVEL) {
    Serial.print("");
    Serial.println(i);
  }
}

#elif defined(WITH_DEBUG_UDP)

WiFiUDP UDP;
boolean doLogUDP = false;

void logUDP(String msg) {
  if (doLogUDP) {
    UDP.beginPacket("255.255.255.255", UDP_PORT);
    UDP.print("  ");
    UDP.print(msg);
    UDP.endPacket();
  }
}

int DEBUG_LEVEL = DBG_OFF;
void dbg(String s)         {
  logUDP(s);
}
void dbgln(String s)       {
  logUDP(s);
}
void dbg(const char * s)   {
  logUDP(s);
}
void dbgln(const char * s) {
  logUDP(s);
}
void dbg(int i)            {
  logUDP(String(i));
}
void dbgln(int i)          {
  logUDP(String(i));
}

#else

int DEBUG_LEVEL = DBG_OFF;
#define dbg(WHATEVER) {}
#define dbgln(WHATEVER) {}

#endif

long leastHeap  = -1;
long maxStack = -1;

void checkMemory() {
  long curr = ESP.getFreeHeap();
  if (curr<leastHeap || leastHeap<0) {
    leastHeap = curr;
  }
  curr = getStackSize();
  if (curr>maxStack || maxStack<0) {
    maxStack = curr;
  }  
}


#ifdef WITH_WEB_CALLS
#include "web_update.h"
boolean doSendAlerts = true;
#endif


/***************************************************************

   OpenTherm message parsing

*/

int onUnexpectedMessage(char sender, OTMessage& m) {
  dbg(String("onUnexpectedMessage: sender=") + sender + ": " + m.toString());
  switch (sender) {
    case 'T': state.messages.unexpected.T++; return RC_UNEXP_T; // thermostat/master
    case 'B': state.messages.unexpected.B++; return RC_UNEXP_B; // boiler/ventilation/slave
    case 'R': state.messages.unexpected.R++; return RC_UNEXP_R; // otgw to thermostat
    case 'A': state.messages.unexpected.A++; return RC_UNEXP_A; // otgw to ventilation
  }
  return RC_UNEXP_O;
}

int toTemp(int8_t hi, int8_t lo) {
  // TODO: Need proof if temperatue conversion is correct for t<0
  // But wow? Even at winter time, temperature will never be <0 ... because of my setup.
  // Maybe someone else can help here?
  int sign = hi < 0 ?  -1 :  1;
  int abs  = hi < 0 ? -hi : hi;
  abs = 100 * abs + (100 * lo / 255);
  return sign * abs;
}

int onMasterMessage(OTMessage& m) {

  state.messages.expected.T++;
  {
    switch (m.type) {
      case OTMessage::MT_READ_DATA:
      case OTMessage::MT_WRITE_DATA:
        switch (m.dataid) {
          case OTMessage::DI_CONTROL_SETPOINT:
          case OTMessage::DI_CONTROL_SETPOINT_VH:
            state.ventSetpoint = m.lo;
            return RC_OK;
          case OTMessage::DI_MASTER_PROD_VERS:
            state.version.master.hi = m.hi;
            state.version.master.lo = m.lo;
            return RC_OK;
          case OTMessage::DI_SLAVE_PROD_VERS:
            state.version.slave.hi = m.hi;
            state.version.slave.lo = m.lo;
            return RC_OK;
        }

        state.messages.expected.T++;
        return RC_OK; // ACKs from slave will be more usefull than these
    }
  }
  state.messages.expected.T--; // revoke former increment

  return onUnexpectedMessage('T', m);
}

int onSlaveMessage(OTMessage& m) {

  state.messages.expected.B++; {

    if (OTMessage::MT_UNKN_DATAID == m.type) {
      switch (m.dataid) {
        // Vitovent 300 never replies to these, never did, never will?
        // Seems like controller was designed to speak also to devices
        // that DO support this.
        case OTMessage::DI_MASTER_PROD_VERS:
          // should not be assigne here (always 0)
          //state.version.master.hi = m.hi;
          //state.version.master.lo = m.lo;
          return RC_OK; // its ok if that fails
        case OTMessage::DI_SLAVE_PROD_VERS:
          // should not be assigne here (always 0)
          //state.version.slave.hi = m.hi;
          //state.version.slave.lo = m.lo;
          return RC_OK; // its ok if that fails
      }
    }

    switch (m.dataid) {

      case OTMessage::DI_CONTROL_SETPOINT:
      case OTMessage::DI_CONTROL_SETPOINT_VH:
        if (state.ventAcknowledged != m.lo) {
          state.lastAcknChange = millis();
          state.ventAcknowledged = m.lo;
        }
        return RC_OK;
      case OTMessage::DI_REL_VENTILATION:
        state.ventRelative = m.lo;
        return RC_OK;
      case OTMessage::DI_SUPPLY_INLET_TEMP:
        state.tempSupply = toTemp(m.hi, m.lo);
        return RC_OK;
      case OTMessage::DI_EXHAUST_INLET_TEMP:
        state.tempExhaust = toTemp(m.hi, m.lo);
        return RC_OK;
      case OTMessage::DI_TSP_SETTING:
        // Check filter:
        // "Die Filternachricht steckt aber in dem Telegramm mit den hex codes Bxx5917yy."
        // Sent from Boiler to Thermostat:
        // 0000 0000 0101 1001 00010111 00000000
        // PTTT SSSS DDDD DDDD HHHHHHHH LLLLLLLL # data ID: 0x59 = 89 = DI_TSP_SETTING
        // parameter no. 0x17 = 23
        if (/* always true: 0 <= m.hi && */ m.hi < 64) {
          state.tsps[m.hi] = m.lo;
        }
        return RC_OK;
      case OTMessage::DI_STATUS:
        state.status.hi = m.hi;
        state.status.lo = m.lo;
        return RC_OK;
      case OTMessage::DI_CONFIG_MEMBERID:
        state.configMemberId.hi = m.hi;
        state.configMemberId.lo = m.lo;
        return RC_OK;
      case OTMessage::DI_MASTER_CONFIG:
        state.masterConfig.hi = m.hi;
        state.masterConfig.lo = m.lo;
        return RC_OK;
      case OTMessage::DI_REMOTE_PARAM_FLAGS:
        state.remoteParamFlags.hi = m.hi;
        state.remoteParamFlags.lo = m.lo;
        return RC_OK;
      case OTMessage::DI_FAULT_FLAGS_CODE:
        state.faultFlagsCode.hi = m.hi;
        state.faultFlagsCode.lo = m.lo;
        return RC_OK;
    }
  }
  state.messages.expected.B--; // revoke former increment

  return onUnexpectedMessage('B', m); // B: boiler
}

int onRequestMessage(OTMessage& m) {

  state.messages.expected.R++;
  {
    switch (m.dataid) {
      // OTGW pretending towards the slave that controller wants to set this ventilation level,
      // e.g. R90470003	WRITE-DATA	CONTROL SETPOINT V/H: 3
      case OTMessage::DI_CONTROL_SETPOINT:
      case OTMessage::DI_CONTROL_SETPOINT_VH:
        // Hmm.. sometimes it does, sometime is does not.
        // Therefore saving this vakue now in overrideVentSetPoint()
        //state.ventOverride = m.lo;
        return RC_OK;
    }
  }
  state.messages.expected.R--; // revoke former increment

  return onUnexpectedMessage('R', m);
}

int onAnswerMessage(OTMessage& m) {

  state.messages.expected.A++; {
    switch (m.dataid) {
      case OTMessage::DI_CONTROL_SETPOINT:
      case OTMessage::DI_CONTROL_SETPOINT_VH:
        state.ventReported = m.lo;
        // OTGW answering to controller, pretending the current ventilation
        // level is the one that was requested by the controller before.
        return 0;
    }
  } state.messages.expected.A--; // revoke former increment

  return onUnexpectedMessage('A', m);
}

int onInvalidMessageSource(char sender, OTMessage& m) {
  onUnexpectedMessage(sender, m);
  return RC_INV_SOURCE;
}

int onInvalidLine(const char * line) {
  state.messages.invalid.format++;
  if (WITH_INVALID_LINES_AS_ERRORS) {
    handleError(line);
  }
  return RC_INV_FORMAT;
}

int onInvalidLength(const char * line) {
  dbg(String("onInvalidLength: '") + line + "'");
  // TODO Investigate why we are receivign wrong lengths, seems like we are missing \n when busy
  state.messages.invalid.length++;
  handleError(line);
  return RC_INV_LENGTH;
}

void handleError(const char * line) {
  line = line; // please the compiler (unused param)
}

String onOTGWMessage(String line, boolean feed) {

  const char * cstr   = line.c_str();
  char         sender = cstr[0];
  const char * hexstr = cstr + 1;

  #ifdef WITH_DEBUG_UDP
  logUDP(line);
  #endif

  // Indicate there is some activity:
  ledFlash(5);

  // ACK of OTGW to command 'VS=x' or 'GW=1' messages sent by us to control fan speed level.
  if (line.startsWith("VS: ") || line.startsWith("GW: ")) {
    if (feed) {
      state.messages.expected.otgw++;
    }
    return String("ACK:'") + line + "'\n"; // adding white space causes flush??
  }

  // This message sent by OTGW if there is no controller connected, probably
  // to fulfill OpneTherm specs (at least 1 msg/sec sent)
  if (line.startsWith("R00000000")) {
    state.messages.unexpected.zero++;
    return String("NOCONN:'") + line + "\n";
  }

  String rc;
  int len = strlen(hexstr);
  if (8 != len) { // 1: 'B'/'T'/'A'/'R' followed by 8 hex chars
    rc = String("INVLEN[") + len + "]:'" + cstr + "':";

    if (feed) {
      // add real result code if we should handle this message
      rc += onInvalidLength(cstr);
    }
    else {
      dbg(String("INVALID LENGTH: '" + line + "'"));
      // otherwise bail out after parsing, no rc applicable
      rc += "none";
    }
    // and newline, to ease outout to client in tailf
    rc += "\n";
    return rc;
  }

  //dbg("hexstr: "); dbg(hexstr); dbgln("'");
  char hs[5], ls[5];
  strncpy(hs, hexstr,   4); TZERO(hs);
  strncpy(ls, hexstr + 4, 4); TZERO(ls);

  uint32_t hi = strtol(hs, NULL, 16);
  uint32_t lo = strtol(ls, NULL, 16);
  uint32_t number = (hi << 16) | lo;
  if (0 == number && EINVAL == errno) {
    rc = "INVLINE:";
    rc += cstr;
    rc += ':';
    if (feed) {
      rc += onInvalidLine(cstr);
    }
    else {
      rc += "none";
    }
    rc += "\n";
    return rc;
  }

  OTMessage msg(number);
#ifdef WITH_DEBUG_UDP
  logUDP(msg.toString());
#endif

  rc = String(sender) + ":";
  rc += msg.toString();
  rc += ':';

  if (feed) {
    switch (sender) {
      case 'T': // from master: thermostat, controller
        rc += onMasterMessage(msg);
        break;
      case 'B': // from slave: boiler, ventilation
        rc += onSlaveMessage(msg);
        break;
      case 'A': // response (answer) replied by OTGW to the master
        rc += onAnswerMessage(msg);
        break;
      case 'R': // request send by OTGW to the slave
        rc += onRequestMessage(msg);
        break;
      default:
        state.messages.invalid.source++;
        rc += onInvalidMessageSource(sender, msg);
    }
  }
  else {
    rc += "none";
  }

  rc += "\n";
  return rc;
}

// TODO: Untested if OTGW firmware accep[ts this already
void resetOTGW() {
  dbgln("resetOTGW");
  for (int i = 0; i < 2; i++) {
    Serial.print("GW=R\r\n");
    delay(50);
  }
}

int overrideVentSetPoint(int level) {
  level = level < -1 ? -1 : level > LEVEL_HIGH ? LEVEL_HIGH : level;
  if (level < 0) {
    for (int i = 0; i < 2; i++) {
      // stop overriding, just monitor
      Serial.print("GW=0\r\n");
      delay(100);
    }
  }
  else {
    for (int i = 0; i < 2; i++) {
      Serial.print("GW=1\r\n");
      delay(10);
      Serial.print("VS="); Serial.print(level); Serial.print("\r\n");
      delay(20);
    }
  }
  dbg(String("overrideVentSetPoint, old:") + state.ventOverride + " new:" + level);
  int old = state.ventOverride;
  state.ventOverride = level;
  return old;
}


/***************************************************************

   EEPROM configuration and networking setup

*/

boolean readConfiguration() {
  boolean success = false;

  EEPROM.begin(sizeof(EE));
  EEPROM.get(0, EE);
  EEPROM.commit();
  EEPROM.end();

  if (MAGIC == EE.magic) {
    success = true;
    health.lastRebootDetails = EE.rebootDetails;
  }
  dbg(String("readConfiguration: success=") + success + ", lastRebootDetails=" + health.lastRebootDetails);
  return success;
}

void firstTimeSetup() {

  memset(&EE, 0, sizeof(EE));
  EE.magic = MAGIC;
  EE.reboots = 0;
  EE.accessPoint.used = 1;
  strcpy(EE.accessPoint.ssid, DEFAULT_START_SSID);
  EE.accessPoint.psk[0] = 0;

  EE.homeNetwork.used = 0;
  EE.homeNetwork.ssid[0] = 0;
  EE.homeNetwork.psk[0] = 0;
  EE.configured = 0;

  strncpy(EE.rebootDetails, "First time setup", sizeof(EE.rebootDetails) - 1);

  EEPROM.begin(sizeof(EE));
  EEPROM.put(0, EE);
  if (EEPROM.commit()) {
    dbg("EEPROM committed");
  } else {
    dbg("EEPROM commit FAILED!");
  }
  EEPROM.end();

  health.lastRebootDetails = EE.rebootDetails;
  dbg(String("firstTimeSetup: lastRebootDetails:=") + health.lastRebootDetails);
}

void setRebootReason(String reason) {
  const char * msg = reason.c_str();
  strncpy(EE.rebootDetails, msg, sizeof(EE.rebootDetails) - 1);
  EEPROM.begin(sizeof(EE));
  EEPROM.put(0, EE);
  bool rc = EEPROM.commit();
  dbg(String("setRebootReason: rc=") + rc);
  EEPROM.end();
}

void updateReboots() {
  EE.reboots++;
  EEPROM.begin(sizeof(EE));
  EEPROM.put(0, EE);
  EEPROM.commit();
  EEPROM.end();
}

String toStr(IPAddress a) {
  return String("") + a[0] + '.' + a[1] + '.' + a[2] + '.' + a[3];
}

String getAPIP() {
  return toStr(WiFi.softAPIP());
}

void setupNetwork() {

  dbgln("setupNetwork");
  // if configured, try to connect to network first
  if (EE.homeNetwork.used) {

    int retries = 40; //*250ms=10s

    dbg("connecting to "); dbgln(EE.homeNetwork.ssid);
    WiFi.mode(EE.accessPoint.used ? WIFI_AP_STA : WIFI_STA);
    WiFi.begin(EE.homeNetwork.ssid, EE.homeNetwork.psk);

    while (WiFi.status() != WL_CONNECTED && 0 < retries--) {
      digitalWrite(LED_ONBOARD, retries % 2);
      delay(250);
    }
  }
  else {
    dbgln("no home network set up");
  }

  boolean connected = WiFi.status() == WL_CONNECTED;
  dbg("setupNetwork:"); dbg(connected ? " " : " not "); dbgln("connected");

  // if that fails (or requested through configuration), start access point (as well)
  if (EE.accessPoint.used || !connected) {

    if (!configured) {
      //dbgln("not configured => WIFI_AP_STA");
      // will need to connect to network during setup
      WiFi.mode(WIFI_AP_STA);
    }
    else if (EE.homeNetwork.used) {
      // was configured to connect to a network
      //dbgln("homeNetwork used => WIFI_AP_STA");
      WiFi.mode(WIFI_AP_STA);
    }
    else {
      // was configured to stay access point only, no network
      //dbgln("configure, no home network => WIFI_AP");
      WiFi.mode(WIFI_AP);
    }

    String sid = EE.accessPoint.ssid;
    String psk = EE.accessPoint.psk;
    dbg("accessPoint.ssid: "); dbgln(sid);
    dbg("accessPoint.psk:  "); dbgln(psk);
    if (!EE.accessPoint.used) {
#ifdef DEFAULT_START_SSID
      sid = DEFAULT_START_SSID;
#else
      sid = String("http://") + getAPIP();
#endif
      psk = DEFAULT_PSK;
    }

    dbg("ssid: "); dbgln(sid);
    dbg("psk:  "); dbgln(psk);

    WiFi.softAP(sid.c_str(), psk.c_str());
  }
}

/***************************************************************

   Web server support
*/
// Send headers with HTTP response that disable browser caching
void avoidCaching() {
  server.sendHeader(String("Pragma"),  String("no-cache"));
  server.sendHeader(String("Expires"), String("0"));
  server.sendHeader(String("Cache-Control"), String("no-cache, no-store, must-revalidate"));
}

// Send 320 redirect HTTP response
void sendRedirect(String path) {
  String body = String("<htm><body>Proceed to <a href=\"") + path + "\">" + path + "</a></body></html>";
  server.sendHeader("Location", path);
  server.send(302, CT_TEXT_HTML, body);
}

void handleIndex() {
  server.send_P(200, PGM_CT_TEXT_HTML, HTML_INDEX);
}

// handle requests to /: if not yet configured, redirect to /setup, otherwise show default page
void handleRoot() {
#ifdef WITH_WEB_SETUP
  if (!configured) {
    sendRedirect("/setup");
    return;
  }
#endif
  handleIndex();
}

void handleNotFound() {
  server.send(404, CT_TEXT_HTML, String("Not found: ") + server.uri());
  //handleIndex();
}

/***************************************************************

   Web server support (first time network setup)

*/

#ifdef WITH_WEB_SETUP
void handleSetup() {
  server.send_P(200, PGM_CT_TEXT_HTML, HTML_SETUP);
}
#endif

String getLocalAddr() {
  IPAddress local = WiFi.localIP();
  String addr = String(local[0]) + "." + local[1] + "." + local[2] + "." + local[3];
  return addr;
}

void createConfigToken() {
  unsigned int i;
  for (i = 0; i < sizeof(configToken) - 1; i++) {
    configToken[i] = random('A', 'Z' + 1);
  }
  configToken[i] = 0;
}

void handleAjax() {
  String action = server.arg("a");

  if (action == "set") { // set network name and password, start to connect
    String ssid = server.arg("ssid");
    String psk  = server.arg("psk");
    if (ssid.length() < 1 || psk.length() < 8) {
      server.send(200, CT_APPL_JSON, "{\"ok\":0,\"msg\":\"Name or password to short\"}\n");
      return;
    }
    else {
      strncpy(EE.homeNetwork.ssid, ssid.c_str(), sizeof(EE.homeNetwork.ssid)); TZERO(EE.homeNetwork.ssid);
      strncpy(EE.homeNetwork.psk,  psk.c_str(),  sizeof(EE.homeNetwork.psk )); TZERO(EE.homeNetwork.psk );
      EE.homeNetwork.used = true;

      WiFi.mode(WIFI_AP_STA);
      WiFi.begin(EE.homeNetwork.ssid, EE.homeNetwork.psk);
      long timeout = 20; // secs
      connectAttemptExpires = millis() + timeout * 1000L;
      createConfigToken();
      dbg("created configToken "); dbgln(configToken);
      server.send(200, CT_APPL_JSON,
                  String("{\"ok\":1,\"msg\":\"Ok\",\"token\":\"") + configToken + "\",\"timeout\":\"" + timeout + "\"}\n");
      return;
    }
  }
  else if (action == "status") { // sent when setup.html is polling for device coming up after reboot
    String saddr = getLocalAddr();
    String caddr = toStr(server.client().remoteIP());
    server.send(200, CT_APPL_JSON,
                String("{\"ok\":1,\"nwssid\":\"") + EE.homeNetwork.ssid + "\",\"saddr\":\"" + saddr + "\",\"caddr\":\"" + caddr + "\"}\n");
    return;
  }

  String token = server.arg("t");
  if (token == "MaGiC") {
    // okay (for debug purposes only)
    dbg("debug token: "); dbgln(token);
  }
  else if (token != configToken) {
    dbg("token exp: "); dbgln(configToken);
    dbg("token got: "); dbgln(token);
    server.send(200, CT_APPL_JSON, "{\"ok\":0,\"msg\":\"Token mismatch\"}\n");
    return;
  }

  if (action == "wait") { // setup.html is checking if device connected
    long now = millis();
    if (WiFi.status() == WL_CONNECTED) {
      String addr = getLocalAddr();
      if (80 != HTTP_LISTEN_PORT) {
        addr += ":";
        addr += HTTP_LISTEN_PORT;
      }
      server.send(200, CT_APPL_JSON, String("{\"ok\":1,\"status\":0,\"addr\":\"") + addr + "\"}\n");
      return;
    }
    else if (connectAttemptExpires < 0 || now > connectAttemptExpires) {
      server.send(200, CT_APPL_JSON, "{\"ok\":1,\"status\":2,\"msg\":\"Timeout\"}\n");
      return;
    }
    else {
      int left = (connectAttemptExpires - now) / 1000;
      server.send(200, CT_APPL_JSON, String("{\"ok\":1,\"status\":1,\"msg\":\"Connecting\",\"left\":") + left + "}\n");
      return;
    }
  }
  else if (action == "save") { // setup.html connected via network (not AP), ready to commit settings
    String ap = server.arg("ap");
    if (ap == "1") { // keep ap?
      String apssid = server.arg("apssid");
      String appsk  = server.arg("appsk");
      strncpy(EE.accessPoint.ssid, apssid.c_str(), sizeof(EE.accessPoint.ssid)); TZERO(EE.accessPoint.ssid);
      strncpy(EE.accessPoint.psk,  appsk.c_str(),  sizeof(EE.accessPoint.psk )); TZERO(EE.accessPoint.psk );
      EE.accessPoint.used = true;
    }
    else if (ap == "0") {
      EE.accessPoint.used = false;
    }
    else {
      server.send(200, CT_APPL_JSON, String("{\"ok\":0,\"msg\":\"parameter 'ap' missing\"}"));
      return;
    }

    EE.homeNetwork.used = true;
    EEPROM.begin(sizeof(EE));
    EEPROM.put(0, EE);
    EEPROM.commit();
    EEPROM.end();

    String body = "{\"ok\":1}\n\n";
    server.send(200, CT_APPL_JSON, body);
    delay(200);
    server.client().stop();
    delay(200);
    dbgln("reboot scheduled");
    rebootScheduledAfterSetup = millis() + 1 * 1000;
    return;
  }
  server.send(404, CT_APPL_JSON, String("{\"ok\":0,\"msg\":\"unknown resource\"}"));
}


/***************************************************************

   Web server support (normal operation)

*/
void sendBinary(PGM_P src, size_t contentLength, const char *contentType) {
  // For some reason, this blocks for about one second and the client
  // receives no content. Not implemented for binary data ?????
  WiFiClient client = server.client();
  String head
    = String("HTTP/1.0 200 OK\r\n") +
      "Content-Type: "   + contentType + "\r\n"
      "Content-Length: " + contentLength + "\r\n"
      "Connection: close\r\n"
      "\r\n";
  // TODO: Send in chunks. Avoid to allocate buffer for whole content.
  // This is probably the reason why fan.png crashes.
  char body[contentLength];
  memcpy_P(body, src, contentLength);

  client.write(head.c_str(), head.length());
  client.write(body, contentLength);
  client.flush();
  delay(2);
  client.stop();
}

#ifdef WITH_ICONS
void handleFavicon() {
  sendBinary(FAVICON_ICO, sizeof(FAVICON_ICO), "image/x-icon");
  /*
    // For some reason, this blocks for about one second and the client
    // receives no content. Not implemented for binary data ?????
    WiFiClient client = server.client();
    int size = sizeof(FAVICON_ICO);
    String head
      = String("HTTP/1.0 200 OK\r\n"
               "Content-Type: image/x-icon\r\n"
               "Content-Length: ") + size + "\r\n"
               "Connection: close\r\n"
               "\r\n";

    char buffer[size];
    memcpy_P(buffer, (char*)FAVICON_ICO, size);

    client.write(head.c_str(), head.length());
    client.write(buffer, size);
    client.flush();
    delay(2);
    client.stop();
  */
}
#endif

// TODO: makes ESP crash:
/*
  void handleHomeIcon() {
	sendBinary(FAN_PNG, sizeof(FAN_PNG), "image/png");
  }
*/
#ifdef WITH_ICONS
void handleManifest() {
  server.send_P(200, PGM_CT_APPL_JSON, MANIFEST_JSON);
}
#endif

String upTime() {
  return toHumanReadableTime(millis() / 1000);
}

String toHumanReadableTime(int secs) {
  long mins = secs / 60; secs %= 60;
  long hrs  = mins / 60; mins %= 60;
  long days = hrs  / 24; hrs  %= 24;
  String t;
  if (days > 0) {
    t += String(days) + "d";
  }
  if (t.length() > 0 || hrs > 0) {
    if (t.length() > 0) t += ", ";
    t += String(hrs ) + "h";
  }
  if (t.length() > 0 || mins > 0) {
    if (t.length() > 0) t += ", ";
    t += String(mins) + "m";
  }
  if (t.length() > 0) t += ", ";
  t += String(secs) + "s";
  return t;
}

String createStatusJson() {

  unsigned long now = millis();

  unsigned long left1 = health.rebootTime - now;

  unsigned long span2 = (now - health.lastWebCheck);
  long long left2 = WEB_CHECK_INTERVAL - span2;

  unsigned long span3 = (now - health.lastWebCheckError);
  //unsigned long left3 = WEB_CHECK_INTERVAL - span3;

  String nextReboot = toHumanReadableTime(left1 / 1000);
  String lastCheck  = toHumanReadableTime(span2 / 1000);
  String nextCheck;
  if (left2 < 0) {
    nextCheck = "overdue";
  }
  else {
    nextCheck = toHumanReadableTime(left2 / 1000);
  }
  String lastError;
  if (health.lastWebCheckError > 0) {
    lastError = toHumanReadableTime(span3 / 1000);
  }

  unsigned long delta = health.ventDiffStarted > 0 ? now - health.ventDiffStarted : 0;
  String refresh = server.arg("refresh");

  // https://github.com/apdlv72/VitoWifi/issues/1
  // (wobei ich jetzt hier gelesen hab daß das TSP (Id 89) Nummer 0x17 sein soll,
  // 1 ist offenbar "filter ok", mal sehen was meiner grad sagt, bei dem ist grad die Filteranzeige an.

  //String filterCheck = String(state.tsps[23], 16);
  String filterCheck =  state.status.lo < 0 ? "-1" : (state.status.lo & 32) ? "1" : "0";
  String json = String() +
                "{\n"
                "  \"ok\":1,\n"
                "  \"build\":\"" + buildNo + "\",\n"
                "  \"reboots\":\"" + EE.reboots + "\",\n"
                "  \"now\":" + now + ",\n"
                "  \"uptime\":\"" + upTime() + "\",\n"
                "  \"timeAdj\":\"" + state.timeAdj + "\",\n"
                "  \"script\":{\"v\":\"" + state.scriptVersion + "\",\"n\":\"" + state.scriptName + "\"},\n"
                "  \"lastMsg\":{" +
                "\"serial\":" + (now - state.lastMsg.serial) + ""
#ifdef WITH_DEBUG_SERIAL
                ",\"wifi\":{\"fed\":" + (now - state.lastMsg.wifi.fed)  + ",\"debug\":" + (now - state.lastMsg.wifi.debug) + "}"
#endif
                "},\n"
                "  \"dbg\":{\"level\":" + DEBUG_LEVEL + "},\n"
                "  \"vent\":{\"set\":" + state.ventSetpoint +
                ",\"override\":" + state.ventOverride +
                ",\"web\":" + state.ventWeb +
                ",\"ackn\":" + state.ventAcknowledged +
                ",\"reported\":" + state.ventReported +
                ",\"relative\":" + state.ventRelative + "},\n"
                "  \"lastAcknCh\":" + (now - state.lastAcknChange) + ",\n"
                "  \"unresponsiveness\":{"
                "\"isResp\":"  + health.isResponsive  + ","
                "\"wasResp\":" + health.wasResponsive + ","
                "\"start\":" + health.ventDiffStarted +
                ",\"delta\":" + delta +
                ",\"reported\":" + health.lastDiffReported +
                "},\n"
                "  \"web\":{\"last\":\"" + lastCheck + "\",\"next\":\"" + nextCheck + "\",\"lastErr\":\"" + lastError + "\",\"posts\":" + health.postOkCount + ",\"fail\":" + health.webFailCount + "},\n"
                "  \"nextReboot\":\"" + nextReboot + "\",\n"
                "  \"temp\":{\"supply\":"   + state.tempSupply   + ",\"exhaust\":" + state.tempExhaust + "},\n"
                "  \"status\":["            + state.status.hi           + "," + state.status.lo           + "],\n"
                "  \"faultFlagsCode\":["    + state.faultFlagsCode.hi   + "," + state.faultFlagsCode.lo   + "],\n"
                "  \"configMemberId\":["    + state.configMemberId.hi   + "," + state.configMemberId.lo   + "],\n"
                "  \"masterConfig\":["      + state.masterConfig.hi     + "," + state.masterConfig.lo     + "],\n"
                "  \"remoteParamFlags\":["  + state.remoteParamFlags.hi + "," + state.remoteParamFlags.lo + "],\n"
                "  \"filterCheck\":" + filterCheck + ",\n"
                "  \"service\":{\"filter\":\"" + state.filterAction + "\",\"cartridge\":\"" + state.cartridgeAction + "\"},\n"
                "  \"messages\":{\n" +
                "    \"total\":"   + (state.messages.serial + state.messages.wifi.fed + state.messages.wifi.debug) + ",\n"
                "    \"serial\":"  + state.messages.serial + ",\n"
                #ifdef WITH_DEBUG_SERIAL
                "    \"wifi\":{\"fed\":" + state.messages.wifi.fed + ",\"debug\":" + state.messages.wifi.debug + "},\n"
                #endif
                "    \"invalid\":{\"len\":"  + state.messages.invalid.length + ",\"format\":" + state.messages.invalid.format + ",\"src\":" + state.messages.invalid.source + "},\n"
                "    \"expected\":{"  +
                "\"T\":"  + state.messages.expected.T + ","
                "\"B\":"  + state.messages.expected.B + ","
                "\"R\":"  + state.messages.expected.R + ","
                "\"A\":"  + state.messages.expected.A + "},\n"
                "    \"unexpected\":{"  +
                "\"T\":" + state.messages.unexpected.T + ","
                "\"B\":" + state.messages.unexpected.B + ","
                "\"R\":" + state.messages.unexpected.R + ","
                "\"A\":" + state.messages.unexpected.A + ","
                "\"zero\":" + state.messages.unexpected.zero + "}\n"
                "  },\n"
                "  \"mem\":{\"heap\":{\"least\":" + leastHeap + "},\"stack\":{\"max\":" + maxStack + "}},\n"                
                "  \"debug\":" + DEBUG_LEVEL + ",\n"
                ;

#ifdef WITH_DALLAS_TEMP_SENSORS
  json +=
    String(
      "  \"sensorsFound\":") + state.sensorsFound + ",\n"
    "  \"lastMeasure\":" + (now - state.lastMeasure) + ",\n"
    "  \"sensors\":[";
  for (unsigned int i = 0; i < sizeof(state.extraTemps) / sizeof(state.extraTemps[0]); i++) {
    if (i > 0) json += ",";
    json += state.extraTemps[i];
  }
  json += "],\n";
#endif

  json +=
    "  \"tsps\":[";
  // print "transparent slave parameters" as json array
  for (unsigned int i = 0; i < sizeof(state.tsps) / sizeof(state.tsps[0]); i++) {
    if (i > 0    ) json += ",";
    if (0 == i % 8) json += "\n    ";
    json += state.tsps[i];
  }

  json += "],\n";


  String bssid = WiFi.BSSIDstr();

  json +=
    "  \"wifi\": {"
      "\"host\":\"" + WiFi.hostname() + "\","
      "\"ip\":\""   + getLocalAddr() + "\","
      "\"mac\":\""  + getMac() + "\","
      "\"rssi\":"   + WiFi.RSSI() + ","
      "\"bssid\":\"" + bssid 
    + "\"},\n" 
    "  \"ap\":{"
      "\"ssid\":\"" + WiFi.softAPSSID() + "\","
      "\"ip\":\""   + getAPIP() + "\","
      "\"mac\":\""  + WiFi.softAPmacAddress() + "\","
      "\"cl\":"     + WiFi.softAPgetStationNum() + 
      "},\n"
    ;

  String details = health.lastRebootDetails;
  json +=
    "  \"esp\":{\"id\":\"" + String(ESP.getChipId(), 16) + "\",\"core\":\"" + ESP.getCoreVersion() + "\",\"sdk\":\""
    + ESP.getSdkVersion() + "\",\"reboot\":\"" + ESP.getResetReason() + "\",\"details\":\"" + details + "\","
    "\"freq\":" + ESP.getCpuFreqMHz() + ",\"sketch\":" + ESP.getSketchSize() + "}\n"
    ;

  json += "}\n";
  return json;
}

String getMac() {
  uint8_t macAddr[6];
  WiFi.macAddress(macAddr);
  String rtv =
    String(macAddr[0], 16) + ":" +
    String(macAddr[1], 16) + ":" +
    String(macAddr[2], 16) + ":" +
    String(macAddr[3], 16) + ":" +
    String(macAddr[4], 16) + ":" +
    String(macAddr[5], 16);
  rtv.toUpperCase();
  return rtv;
}

void handleApiStatus() {
  String refresh = server.arg("refresh");
  String json = createStatusJson();
  avoidCaching();
  if (refresh != "") server.sendHeader("Refresh",  refresh);
  server.send(200, CT_APPL_JSON, json);
}

boolean timeWasSet = false;

String getDate() {

  if (timeWasSet) {
    // TODO: Set time later via API call and use a millis() based RTC or add NTP support (time server)
    // Use time to fallback to basic level control e.g. if no command received via WiFi for more than
    // a couple of hours/minutes.
  }
  return String(millis());
}

void handleApiLevel() {
  int l = state.ventOverride;
  String json = String("{\"ok\":1,\"now\":") + getDate() + ",\"level\":" + l + "}\n";
  avoidCaching();
  server.send(200, CT_APPL_JSON, json);
}

#ifdef WITH_DEBUG_SERIAL
void handleDbgSet() {
  String level = server.arg("level");
  int old = DEBUG_LEVEL;
  DEBUG_LEVEL = atoi(level.c_str());
  String body = String("debug level set from ") + old + " to " + DEBUG_LEVEL;
  avoidCaching();
  server.send(200, CT_TEXT_PLAIN, body);
}

void handleDbgMsg() {
  String msg  = server.arg("msg");
  String feed = server.arg("feed");
  String rc = "no message";
  if (msg != "") {
    if (feed == "1") {
      state.lastMsg.wifi.fed = millis();
      state.messages.wifi.fed++;
      rc = onOTGWMessage(msg, true);
    }
    else {
      state.lastMsg.wifi.debug = millis();
      state.messages.wifi.debug++;
      rc = onOTGWMessage(msg, false);
    }
  }
  avoidCaching();
  server.send(200, CT_TEXT_PLAIN, rc);
}

// Use this like: "curl http://192.168.4.1/dbg/tailf".
// It will however not receive a valid HTTP response but dump things to stdout.
#endif // WITH_DEBUG_SERIAL

#ifdef WITH_DEBUG_UDP
void handleUdpOn() {
  stopUdpLoggingAfter = 0;
  doLogUDP = true;
  logUDP("**** UDP LOGGING ENABLED ****");
  avoidCaching();
  server.send(200, CT_TEXT_PLAIN, "UDP logging enabled");
}

void handleUdpOff() {
  doLogUDP = true;
  logUDP("**** UDP LOGGING DISABLED ****");
  doLogUDP = false;
  avoidCaching();
  server.send(200, CT_TEXT_PLAIN, "UDP logging disabled");
}
#endif

#ifdef WITH_WEB_CALLS
void handlePost() {
  String response = handleWebUpdate(true);
  avoidCaching();
  server.send(200, CT_TEXT_PLAIN, response);
}
#endif

void handleReboot() {
  String ip = server.client().remoteIP().toString();
  String reason = String("API request from " + ip);
  setRebootReason(reason);

  avoidCaching();
  server.send(200, CT_TEXT_PLAIN, "OK\n");
  delay(100);
  #ifdef WITH_WEB_CALLS
  sendAlert(String("μController rebooting: ") + reason, true, false);
  #endif
  ESP.reset();
}

void handleResetOTGW() {
  dbgln("handleResetOTGW");
  resetOTGW();
  avoidCaching();
  server.send(200, CT_TEXT_PLAIN, "OK\n");
}

void handleResetReboots() {
  dbgln("handleResetReboots");
  EE.reboots = 0;
  EEPROM.begin(sizeof(EE));
  EEPROM.put(0, EE);
  EEPROM.commit();
  EEPROM.end();
  avoidCaching();
  server.send(200, CT_TEXT_PLAIN, "OK\n");
}

#ifdef WITH_TAILF
void handleDbgTailF() {

  String cmd = server.arg("cmd");

  WiFiClient client = server.client();
  // Send at least a very basic HTTP header.
  client.write("HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n\r\n");

  if (cmd == "") {
    client.print("No cmd sent\n");
  }
  else {
    Serial.print(cmd); Serial.print("\r\n");
    Serial.print(cmd); Serial.print("\n");
    client.print(String("Cmd sent: '") + cmd + "'");
  }

  inputString = "";
  uint32_t maxWait = 60 * 1000; // 20s
  do {
    //client.print("waiting for serial data\n");
    // Wait for data on serial line to become available
    while (client.connected() && !Serial.available() && maxWait--) {
      delay(1);
    }

    if (Serial.available()) {
      //client.print("Serial.available\n");
      char inChar = (char)Serial.read();

      //client.print("Serial.available: 0x");
      //client.print(String((int)inChar, 16)); client.print(" ");
      //client.print(String((int)inChar, 10)); client.print("\n");

      switch (inChar) {
        case '\r':
          // ignore CR, assume next is newline
          break;
        case '\n':
          stringComplete = true;
          break;
        default:
          inputString += inChar;
      }
    }

    if (stringComplete) {
      //client.print("stringComplete: '");
      //client.print(inputString);
      //client.print("'\n");

      state.lastMsg.serial = millis();
      state.messages.serial++;
      client.print(inputString); client.print("\n");
      String rc = onOTGWMessage(inputString, true);
      client.print(rc);

      stringComplete = false;
      inputString = "";

      ledToggle();
    }

  } while (client.connected());
  //dbgln("client disconnected");

  client.stop();
  delay(10);
}
#endif


#ifdef WITH_PIN_TEST
void handlePinTest() {
  String str = server.arg("p");
  if (str == "") {
    server.send(404, CT_TEXT_PLAIN, "No pin specified\n");
    return;
  }

  int pin = atoi(str.c_str());
  server.send(200, CT_TEXT_PLAIN, String("Toggle pin ") + pin + "\n");
  server.client().stop();

  dbg("toggle start pin"); dbgln(pin);
  pinMode(pin, OUTPUT);
  for (int i = 0; i < 10; i++) {
    dbg("digitalWrite("); dbg(pin); dbg(","); dbgln(i % 2);
    digitalWrite(pin, i % 2);
    delay(200);
  }
  dbg("toggle stop pin"); dbgln(pin);
}
#endif // WITH_PIN_TEST


void handleApiSet() {
  String s = server.arg("level");
  s.trim();
  s.toLowerCase();

  int l = -1;
  if      (s == "high")   l = LEVEL_HIGH;
  // -------
  else if (s == "norm")   l = LEVEL_NORM;
  else if (s == "normal") l = LEVEL_NORM;
  // -------
  else if (s == "low")    l = LEVEL_LOW;
  // -------
  else if (s == "off")    l = LEVEL_OFF;
  else {
    l = atoi(s.c_str());
    if (0 == l && EINVAL == errno) {
      l = -1; // inval
    }
    else if (l < LEVEL_OFF || l > LEVEL_HIGH) {
      l = -1; // inval
    }
  }

  String json;
  if (-1 == l) {
    json = String("{\"ok\":0,\"now\":") + getDate() + ",,\"msg\":\"invalid value\",\"value\":\"" + s + "\"}\n";
  }
  else {
    int old = overrideVentSetPoint(l);
    json = String("{\"ok\":1,\"now\":") + getDate() + ",\"level\":" + l + ",\"value\":\"" + s + "\",\"old\":" + old + "}\n";
  }
  avoidCaching();
  server.send(200, CT_APPL_JSON, json);
}


/*
  From Arduino docs:
  SerialEvent occurs whenever a new data comes in the
  hardware serial RX.  This routine is run between each
  time loop() runs, so using delay inside loop can delay
  response.  Multiple bytes of data may be available.

  For some reason, this does not work on ESP (never called).
  Therefor calling it at the beginning of the main loop()
*/
void serialEvent() {
  if (!Serial.available()) return;

  while (Serial.available() && !stringComplete) {
    // get the new byte:
    char c = (char)Serial.read();
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    switch (c) {
      // ignore CR, assume next is a newline
      case '\r': 
        break;
      case '\n': 
        stringComplete = true; // -> break ouf the while loop
        break;
      default:   
        inputString += c;
    }
  }
}


#ifdef WITH_DALLAS_TEMP_SENSORS
// forward declarations
void setupSensors();
void handleSensors();
#endif

#ifdef WITH_USAGE_HELP
void handleHelp() {
  String body =
    "/index\n"
    "/setup\n"
    "/api/status?refresh=<int>\n"
    "/api/set?level=[0-3]\n"
    "/api/level\n"

#ifdef WITH_DEBUG_SERIAL
    "/dbg/tailf?cmd=<str>\n"
    "/dbg/set?level=[0-1]\n"
    "/dbg/msg?msg=X00000000&feed=[0-1]\n"
#endif // WITH_DEBUG_SERIAL

#ifdef WITH_PIN_TEST
    "/pintest?pin=X\n"
#endif // WITH_PIN_TEST

    "/help\n";
  server.send(200, CT_TEXT_PLAIN, body);
}
#endif

// initial stack
char *stack_start;

int getStackSize() {
  char stack;
  return stack_start - &stack;
}

void setup() {

  // init record of stack
  char stack;
  stack_start = &stack;

  Serial.begin(9600); // as used by OTGW
  Serial.println(String("ZZ=REASON=") + ESP.getResetReason());

  pinMode(LED_HEART, OUTPUT);
  //pinMode(LED_BUILTIN, OUTPUT);

  timer1_attachInterrupt(timerRoutine);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
  timer1_write(timer_value);

  overrideVentSetPoint(LEVEL_NOOVER);

  WiFi.begin(NETWORK, PASSWORD);
  dbgln("Connecting to " NETWORK);

  int i = 0;
  while (WiFi.status() != WL_CONNECTED) { // Wait for the Wi-Fi to connect
    digitalWrite(LED_HEART, HIGH); delay(100);
    digitalWrite(LED_HEART, LOW);  delay(100);
    digitalWrite(LED_HEART, HIGH); delay(100);
    digitalWrite(LED_HEART, LOW);  delay(100);
    dbg(String(" " + i));
    ++i;
  }

#ifdef WITH_DEBUG_UDP
  UDP.begin(UDP_PORT);
  doLogUDP = true;
#endif

  dbgln(String("\nZZ=Connected, IP ") + WiFi.localIP().toString());
  overrideVentSetPoint(LEVEL_NOOVER);

  inputString.reserve(200);

  if (!readConfiguration()) {
    // when EEPROM was not starting with magic, need to set up first:
    firstTimeSetup();
    // read again, this time it should succeed:
    readConfiguration();
  }
  else {
    // increment reboot counter in EEPROM
    updateReboots();
  }

  // init global state
  state.ventSetpoint = -1;
  // set by overrideVentSetPoint above
  //state.ventOverride = -1;
  state.ventWeb = -1;
  state.ventAcknowledged = -1;
  state.lastAcknChange = 0;
  state.ventReported = -1;
  state.ventRelative = -1;
  state.tempSupply   = -12700; // -127C (as dallas when no value read)
  state.tempExhaust  = -12700;
  state.status.hi           = state.status.lo           = -1;
  state.faultFlagsCode.hi   = state.faultFlagsCode.lo   = -1;
  state.configMemberId.hi   = state.configMemberId.lo   = -1;
  state.masterConfig.hi     = state.masterConfig.lo     = -1;
  state.remoteParamFlags.hi = state.remoteParamFlags.lo = -1;
  for (unsigned int i = 0; i < sizeof(state.tsps) / sizeof(state.tsps[0]); i++) {
    state.tsps[i] = -1;
  }
  for (unsigned int i = 0; i < sizeof(state.extraTemps) / sizeof(state.extraTemps[0]); i++) {
    state.extraTemps[i] = -12700; // hecto-celsius
  }

#ifdef WITH_DALLAS_TEMP_SENSORS
  setupSensors();
#endif

  health.isResponsive = true;
  health.wasResponsive = true;

  setupNetwork();

  server.on("/",              handleRoot);
  server.on("/index",         handleIndex);
  server.on("/index.html",    handleIndex);
  #ifdef WITH_ICONS
  server.on("/manifest.json", handleManifest);
  server.on("/favicon.ico",   handleFavicon);
  // TODO: makes ESP crash
  //server.on("/fan.png",       handleHomeIcon);
  #endif
  server.on("/api/status",    handleApiStatus);
  server.on("/api/set",       handleApiSet);
  server.on("/api/level",     handleApiLevel);
#ifdef WITH_WEB_SETUP
  server.on("/setup",    	  handleSetup);
#endif
  server.on("/ajax",          handleAjax);

#ifdef WITH_TAILF
  server.on("/dbg/tailf",     handleDbgTailF);
#endif

#ifdef WITH_DEBUG_UDP
  server.on("/dbg/udp/0",      handleUdpOff);
  server.on("/dbg/udp/1",      handleUdpOn);
#endif

#ifdef WITH_WEB_CALLS
  server.on("/dbg/post",      handlePost);
#endif
  server.on("/dbg/reboot",      handleReboot);
  server.on("/dbg/reset/otgw",  handleResetOTGW);
  server.on("/dbg/reset/reboots", handleResetReboots);

#ifdef WITH_DEBUG_SERIAL
  server.on("/dbg/set",       handleDbgSet);
  server.on("/dbg/msg",       handleDbgMsg);
#endif

#ifdef WITH_PIN_TEST
  server.on("/dbg/pin",       handlePinTest);
#endif

#ifdef WITH_USAGE_HELP
  server.on("/help",          handleHelp);
#endif

  server.onNotFound(          handleNotFound);
  server.begin();

  #ifdef WITH_OTA
  String otaName = "VitoWifi-" + String(ESP.getFlashChipId(), 16);  
  ArduinoOTA.setHostname(otaName.c_str());
  ArduinoOTA.begin();
  #endif
  
  //stopUdpLoggingAfter = millis() + 10*60*1000; // stop spamming 10 minutes after start
  stopUdpLoggingAfter = -1; // never stop
  dbg(String("UDP logging will stop ") + stopUdpLoggingAfter);
}

void handleHeartbeat() {

  int now = millis();
  // Start blinking after things stable.
  // Seems like fiddeling with GPIO0 too early resets the device sporadically.
  if (now < 5000) return;

  boolean light = false;
  int level     = state.ventOverride; // 0=off,...,3=high
  if (now > 30000 && WiFi.status() != WL_CONNECTED) {
    light = (now / 100) % 2;
  }
  else if (level < 0) {
    light = (now / 2000) % 2;
  }
  else {
    int sequence  = now % 2500;         // 0,...,2500-1
    int prefix    = 500 * (level + 1);  // 500, 1000, 1500, 2000
    if (sequence < prefix) {
      sequence %= 500; 				// runs 1x,2x,3x or 4x through 0,..,500-1
      light = sequence < 250;
    }
  }

  if (light != led_lit) {
    pinMode(LED_HEART, OUTPUT);
    digitalWrite(LED_HEART, light ? LOW : HIGH);
    led_lit = light;
  }
}


#ifdef WITH_WEB_CALLS

void sendAlert(String text, bool isDebug, bool onlyDirectly) {
  
  if (!doSendAlerts) {
    return;
  }
  // controller might crash here, save last action:
  setRebootReason("Before sending alert");
  led_blink = true;
  {
    webGet(String(ALERT_URL) + urlEncode(text), "", true);
    if (!isDebug) {
      webGet(String(MAINTENANCE_URL) + urlEncode(text), "", true);
    }
  
    if (!onlyDirectly) {
      String msg = text
                 + " [VitoWifi-" + String(ESP.getFlashChipId(), 16) + "]"
                 + " http:// " + WiFi.localIP().toString();
      webGet(UPDATE_URL, "alert=" + urlEncode(msg), true /* ignore body */);
    }
  }
  led_blink = false;
  setRebootReason("Normal operation after sending alert");  
}

String handleWebUpdate(boolean force) {
  
  unsigned long now = millis();

  if (now<20*1000) {
    //dbg(String("handleWebUpdate: lo=") + state.status.lo + ", hi=" + state.status.hi);
    if (state.status.lo<0 || state.status.hi<0) {
      // its better to wait until status info is available so 
      // the remote side can determine the filter change status.
      //dbg("Defer push until status available");
      delay(250);
      return "deferred";
    }
    //dbg("No status within 20 seconds ... push anyway");
  }    
  
  if (0 == health.lastWebCheck || now < health.lastWebCheck || now - health.lastWebCheck > WEB_CHECK_INTERVAL || force) {

    // controller might crash here, save last action:
    setRebootReason("Web post");
    led_blink = true;

    String response;
    String needle = "ventLevel=";

    String postData = createStatusJson();
    response = webPost(UPDATE_URL, false);
    setRebootReason("Normal operation after web post");
    
    led_blink = false;
    dbg(String("webPost: response='") + response + "'");

    // TODO Use findNeedle()
    int pos = response.indexOf(needle);
    if (pos > -1) {
      health.postOkCount++;
      int level = response.substring(pos + needle.length()).toInt();
      if (level < LEVEL_OFF || level > LEVEL_HIGH) {
        level = LEVEL_NORM;
      }
      state.ventWeb = level;
      overrideVentSetPoint(level);
      health.lastWebCheckError = 0;
    }
    else {
      health.webFailCount++;
      if (0 == health.lastWebCheckError) {
        health.lastWebCheckError = millis();
      }
      unsigned long delta = millis() - health.lastWebCheckError;
      if (delta > 60 * 60 * 1000) {
        sendAlert("Failed to fetch ventilation level from web page for more than 1 hour.", true, true);
      }
    }

    needle = "timeAdj=";
    pos = response.indexOf(needle);
    if (pos > -1) {
      state.timeAdj = response.substring(pos + needle.length()).toInt();
    }

    state.scriptName      = findNeedle(response, "script=");
    state.scriptVersion   = findNeedle(response, "version=");
    state.filterAction    = findNeedle(response, "filter=");
    state.cartridgeAction = findNeedle(response, "cartridge=");

    health.lastWebCheck = millis();
    
    return response;
  }
  return "0";
}
#endif

String findNeedle(String haystack, String needle) {  
  String found = "";
  int pos = haystack.indexOf(needle);
  if (pos>-1) {
    found = haystack.substring(pos + needle.length());
    pos = found.indexOf("\n");
    if (pos>-1) {
      found = found.substring(0, pos);
    }
    found.trim();
  }
  return found;
}

void loop() {

  // memory leak prevention: restart if free memory drops below 25% of initialy available
  if (health.initalFree < 0) {
    health.initalFree = ESP.getFreeHeap();
  } else {
    long freeNow = ESP.getFreeHeap();
    if ( (100 * freeNow) / health.initalFree < 25) {
#ifdef WITH_WEB_CALLS
      if (!health.rebootAnnounced) {
        String reason = String("Low memory ") + freeNow + " of " + health.initalFree + "B";
        sendAlert(String("μController rebooting: ") + reason, true, false);
        setRebootReason(reason);
      }
      health.rebootAnnounced = true;
#endif
      ESP.restart();
    }
  }

#ifdef WITH_DEBUG_UDP
  if (stopUdpLoggingAfter > 0 && millis() > stopUdpLoggingAfter) {
    dbg("*** UDP LOGGING STOPS ***");
    doLogUDP = false;
    stopUdpLoggingAfter = 0;
  }
#endif

  unsigned long now = millis();

  if (now > health.rebootTime) {
    if (!health.rebootAnnounced) {
      String reason = String("Uptime > ") + upTime();
      #ifdef WITH_WEB_CALLS
      sendAlert(String("μController rebooting: ") + reason, true, false);
      #endif
      setRebootReason(reason);
    }
    health.rebootAnnounced = true;
    ESP.reset(); // might fall through here?
  }

  #ifdef WITH_WEB_CALLS  
  if (now - lastConnectionSuccess > 60 * 60 * 1000) {
    if (!health.rebootAnnounced) {
      #ifdef WITH_WEB_CALLS      
      String reason = "No connectivity > 1h";
      sendAlert(String("μController rebooting: ") + reason, true, false);
      #endif
      setRebootReason(reason);
    }
    health.rebootAnnounced = true;
    ESP.reset(); // might fall through here?
  }
  #endif

  if (!health.startAnnounced) {

    health.startAnnounced = true;
    String reason = health.lastRebootDetails;
    String msg = String("μController started: reason: '") + reason + "', " + health.initalFree + " bytes free (" + buildNo + ")";
    dbg(msg);

    reason = "While in normal operation";
    setRebootReason(reason);
    dbg(String("new reason: ") + reason);

#ifdef WITH_WEB_CALLS
    sendAlert(msg, true, false);
#endif
  }

  if (state.ventOverride != state.ventAcknowledged) {
    unsigned long now = millis();
    if (health.ventDiffStarted == 0) {
      health.ventDiffStarted = now;
    }

    // allow the slave up to 30 seconds to accept/acknowledge a change of override value
    unsigned long delta = now - health.ventDiffStarted;
    if (delta > 60 * 1000) {

      health.isResponsive = false;

#ifdef WITH_WEB_CALLS
      delta = now - health.lastDiffReported;
      // report unresponsivness at most once per hour
      if (0 == health.lastDiffReported || delta > 3600 * 1000) {
        String msg;
        if (state.ventAcknowledged < 0) {
          msg = "Ventilation is not responding.";
        } else {
          msg = "Ventilation stopped responding.";
        }
        msg += " Please reset/replug the gateway!";
        sendAlert(msg, false, false);
        health.lastDiffReported = now;
      }
#endif

      // try to reset the OTGW, however at most once per 5 minutes
      delta = now - health.lastGWReset;
      if (delta > 5 * 60 * 1000) {
        resetOTGW();
        health.lastGWReset = now;

#ifdef WITH_WEB_CALLS
        // report at most once per hour
        delta = now - health.lastGWResetReported;
        if (0 == health.lastGWResetReported || delta > 3600 * 1000) {
          sendAlert("Tried to restart the OpenTherm Gateway", true, false);
          health.lastGWResetReported = now;
        }
#endif
      }
    }
  } else {
    health.ventDiffStarted = 0;
    health.isResponsive = true;
  }

  if (health.isResponsive && !health.wasResponsive) {
    unsigned long delta = millis() - health.lastRecoveryReported;
    if (delta > 5 * 60 * 1000) {
      #ifdef WITH_WEB_CALLS
      sendAlert("Ventilation recovered and seems to be responsive again.", false, false);
      #endif
      health.lastRecoveryReported = millis();
    }
  }

  //serialEvent();
  server.handleClient();
  #ifdef WITH_OTA
  ArduinoOTA.handle();
  #endif

#ifdef WITH_DALLAS_TEMP_SENSORS
  if (!temperaturesRequested || 0 == health.loopIterations % (500 * 1000))  {
    dbg("CALLING handleSensors()");
    handleSensors();
  }
#endif

  if (stringComplete) {
    //dbg(String("loop: stringComplete, inputString='") + inputString + "'"); 
    inputString.trim();
    state.lastMsg.serial = millis();
    state.messages.serial++;
    //String rc = 
    onOTGWMessage(inputString, true);
    //dbg(String("loop: onOTGWMessage: rc: ") + rc);    
    inputString = "";
    stringComplete = false;
  }

  handleHeartbeat();

#ifdef WITH_WEB_CALLS
  handleWebUpdate(false);
#endif

#ifdef ESP8266
  if (rebootScheduledAfterSetup > -1 && (unsigned long)rebootScheduledAfterSetup > millis()) {
    rebootScheduledAfterSetup = -1;
    dbgln("*********** REBOOT **********");
    delay(100);
    ESP.reset();
    delay(10000);
  }
#endif

  health.loopIterations++;
}


#ifdef WITH_DALLAS_TEMP_SENSORS
String toString(DeviceAddress& a) {
  String rtv;
  for (unsigned int i = 0; i < sizeof(a) / sizeof(a[0]); i++) {
    if (i) rtv += ":";
    if (a[i] < 0x10) rtv += "0";
    rtv += String(a[i], HEX);
  }
  return rtv;
}

/*
  // function to print a device's resolution
  void printResolution(DeviceAddress deviceAddress)
  {
  Serial.print("Resolution: ");
  Serial.print(sensors.getResolution(deviceAddress));
  Serial.println();
  }
*/

// function to print the temperature for a device
void printTemperature(DeviceAddress deviceAddress) {
  float tempC = sensors.getTempC(deviceAddress);
  dbg("Temp C: ");
  dbg(tempC);
}

void handleSensors() {

  //dbgln("handleSensors");
  if (!temperaturesRequested) {
    sensors.requestTemperatures();
    temperaturesRequested = true;
  }

  state.lastMeasure = millis();
  //dbgln("handleSensors: reading ts");
  for (int i = 0; i < state.sensorsFound; i++) {
    //dbg("sensors.getTempCByIndex("); dbg(i); dbgln(")");
    float t = sensors.getTempCByIndex(i);
    //dbg("t="); dbgln((int)(100*t));
    state.extraTemps[i] = 100 * t;
  }

  sensors.requestTemperatures();
  temperaturesRequested = true;
  //dbgln("handleSensors done");
}

void setupSensors() {
  //dbgln("setupSensors");

  // Start up the library
  sensors.begin();
  sensors.setResolution(11);

  // locate devices on the bus
  //dbg("locating devices...");
  state.sensorsFound = sensors.getDeviceCount();
  //dbg("found "); dbg(state.sensorsFound); dbgln(" sensors.");

  // report parasite power requirements
  //dbg("parasite power: ");
  //if (sensors.isParasitePowerMode()) dbgln("ON"); else dbgln("OFF");
}
#endif
