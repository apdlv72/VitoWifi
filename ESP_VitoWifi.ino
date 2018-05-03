/**
 * Arduino sketch for a ESP-8266 WiFi micro controller to control a Vitovent 300 
 * home ventilation unit via a OpenTherm Gateway and a WiFi connection.
 * See https://github.com/apdlv72/VitoWifi or README.md 
 * @author: apdlv72@gmail.com
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
#define WITH_PIN_TEST

// Uncomment to enable ID to text translation of all known OpenTherm data IDs.
// This cause a considerable overhead in RAM usage and is not enable by default since it is not used for normal operation. 
// However, should you use this sketch as a starting point to control anything else than a VitoVent 300, you might enable
// this to learn what your devices are talking to eachother. 
//#define WITH_ALL_DATAIDS

// Enable debug functions. 
//#define WITH_DEBUG

// Enable a help page on http://192.168.4.1/help
#define WITH_USAGE_HELP

//#define WITH_WEB_SETUP

const char DEFAULT_START_SSID[] = "🐂💩";
//const char DEFAULT_START_SSID[] = "I💜http://venista.com";

// Initial password used to connect to it and set up networking.
// The device will normally show up as SSID "http://192.168.4.1"
// Connect to this network using the intial password and navigate your
// browser to the above URL.
#define DEFAULT_PSK  "12345678"

#define HTTP_LISTEN_PORT 80

/****************************************/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <errno.h>

#ifdef WITH_DALLAS_TEMP_SENSORS
#include <OneWire.h>
#include <DallasTemperature.h>
#endif

// Using a copy of "easy EEPROM" library since needed to modify for ESP 
// to cope with watchdog timer resets (https://github.com/apdlv72/eEEPROM)
#include "eEEPROM.h"

const String buildNo   = __DATE__ " " __TIME__;

#define LED_HEART 	0 // heartbeat 
#define LED_ONBOARD 2 // blue onboard LED / temp. sensors

#define LEVEL_NOOVER -1 // do not overrride (monitor mode)
#define LEVEL_OFF   0 // standby, ventilation off
#define LEVEL_LOW   1 // "reduced" mode (27% for mine ... may vary)
#define LEVEL_NORM  2 // "normal"  mode (55% for mine) 
#define LEVEL_HIGH  3 // "party"   mode (100%)

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

const char HTML_INDEX[] PROGMEM =
#include "index.html.h"
		 // The following line makes buildNo accessible for javascript:
		 "<script>var buildNo = '" __DATE__ " " __TIME__ "';</script>";
;
#ifdef WITH_WEB_SETUP
const char HTML_SETUP[] PROGMEM =
#include "setup.html.h"
;
#endif

const char MANIFEST_JSON[] PROGMEM =
#include "manifest.json.h"
;

const char FAVICON_ICO[] PROGMEM =
#include "favicon.ico.h"
;

const char FAN_PNG[] PROGMEM =
#include "fan.png.h"
;

// Conveniently zero-terminates a C-string.
#define TZERO(STR) { (STR)[sizeof((STR))-1] = 0; }

class OTMessage {

  public:
	
	// OpenTherm message types
	const static uint8_t MT_READ_DATA    = 0;
	const static uint8_t MT_WRITE_DATA   = 1;
	const static uint8_t MT_INVALID_DATA = 2;
	const static uint8_t MT_RESERVED     = 3;
	const static uint8_t MT_READ_ACK     = 4;
	const static uint8_t MT_WRITE_ACK    = 5;
	const static uint8_t MT_DATA_INVALID = 6;
	const static uint8_t MT_UNKN_DATAID  = 7;
	
	// OpenTherm data IDs (common and vendor specific)
	// see https://forum.fhem.de/index.php?topic=29762.115;wap2
	const static uint8_t DI_MASTER_CONFIG		=   2;
	const static uint8_t DI_REMOTE_PARAM_FLAGS 	=   6;
	const static uint8_t DI_STATUS 				=  70;
	const static uint8_t DI_CONTROL_SETPOINT 	=  71;
	// According to http://otgw.tclcode.com/matrix.cgi#thermostats only:
	const static uint8_t DI_FAULT_FLAGS_CODE	=  72;
	const static uint8_t DI_CONFIG_MEMBERID 	=  74;
	const static uint8_t DI_REL_VENTILATION 	=  77;
	const static uint8_t DI_SUPPLY_INLET_TEMP 	=  80;
	const static uint8_t DI_EXHAUST_INLET_TEMP 	=  82;
	const static uint8_t DI_TSP_SETTING 		=  89; 
	const static uint8_t DI_MASTER_PROD_VERS 	= 126;
	const static uint8_t DI_SLAVE_PROD_VERS 	= 127;
      
    
    static String msgTypeToStr(uint8_t t) {
      switch(t) {
        case MT_READ_DATA:    return "READ_DATA";
        case MT_WRITE_DATA:   return "WRITE_DATA";
        case MT_INVALID_DATA: return "INVALID_DATA";
        case MT_RESERVED:     return "RESERVED";
        case MT_READ_ACK:     return "READ_ACK";
        case MT_WRITE_ACK:    return "WRITE_ACK";
        case MT_DATA_INVALID: return "DATA_INVALID";
        case MT_UNKN_DATAID:  return "UNKN_DATAID";
      }
    }

    static String dataIdToStr(uint8_t id) {
      switch(id) {
		case DI_MASTER_CONFIG:		return "Master configuration";
		case DI_REMOTE_PARAM_FLAGS:	return "Remote parameter flags";
		case DI_STATUS:				return "Status V/H";
		case DI_FAULT_FLAGS_CODE:	return "Fault flags/code V/H";
		case DI_CONTROL_SETPOINT:	return "Control setpoint V/H";
		case DI_CONFIG_MEMBERID:	return "Configuration/memberid V/H";
		case DI_REL_VENTILATION:	return "Relative ventilation";
		case DI_SUPPLY_INLET_TEMP:	return "Supply inlet temperature";
		case DI_EXHAUST_INLET_TEMP:	return "Exhaust inlet temperature";    
		// TODO: Find out meaning of various "transparent slave parameters"
		// These are requested by the master in a round robin fashion from slave in the range 0-63,
		// however not some indexes are skipped. 
		case DI_TSP_SETTING:		return "TSP setting V/H";
		case DI_MASTER_PROD_VERS:	return "Master product version";
		case DI_SLAVE_PROD_VERS:	return "Slave product version";
		
		#ifdef WITH_ALL_DATAIDS
		case  3: return "SLAVE CONFIG/MEMBERID",READ,FLAG,00000000,U8,0,255,0,Yes
		case  4: return "COMMAND";
		case  5: return "FAULT FLAGS/CODE";
		case  7: return "COOLING CONTROL";
		case  8: return "TsetCH2";
		case  9: return "REMOTE ROOM SETPOINT";
		case 10: return "TSP NUMBER";
		case 11: return "TSP ENTRY";
		case 11: return "TSP ENTRY";
		case 12: return "FAULT BUFFER SIZE";
		case 13: return "FAULT BUFFER ENTRY";
		case 14: return "CAPACITY SETTING";
		case 15: return "MAX CAPACITY / MIN-MOD-LEVEL";
		case 16: return "ROOM SETPOINT";
		case 17: return "RELATIVE MODULATION LEVEL";
		case 18: return "CH WATER PRESSURE";
		case 19: return "DHW FLOW RATE";
		case 20: return "DAY - TIME";
		case 20: return "DAY - TIME";
		case 21: return "DATE";
		case 21: return "DATE";
		case 22: return "YEAR";
		case 22: return "YEAR";
		case 23: return "SECOND ROOM SETPOINT";
		case 24: return "ROOM TEMPERATURE";
		case 25: return "BOILER WATER TEMP.";
		case 26: return "DHW TEMPERATURE";
		case 27: return "OUTSIDE TEMPERATURE";
		case 28: return "RETURN WATER TEMPERATURE";
		case 29: return "SOLAR STORAGE TEMPERATURE";
		case 30: return "SOLAR COLLECTOR TEMPERATURE";
		case 31: return "SECOND BOILER WATER TEMP.";
		case 32: return "SECOND DHW TEMPERATURE";
		case 32: return "EXHAUST TEMPERATURE";
		case 48: return "DHW SETPOINT BOUNDS";
		case 49: return "MAX CH SETPOINT BOUNDS";
		case 50: return "OTC HC-RATIO BOUNDS";
		case 56: return "DHW SETPOINT";
		case 56: return "DHW SETPOINT";
		case 57: return "MAX CH WATER SETPOINT";
		case 57: return "MAX CH WATER SETPOINT";
		case 58: return "OTC HEATCURVE RATIO";
		case 58: return "OTC HEATCURVE RATIO";
		case // New ID for ventilation/heat-recovery applications
		case 73: return "DIAGNOSTIC CODE V/H";
		case 75: return "OPENTHERM VERSION V/H";
		case 76: return "VERSION & TYPE V/H";
		case 78: return "RELATIVE HUMIDITY";
		case 78: return "RELATIVE HUMIDITY";
		case 79: return "CO2 LEVEL";
		case 79: return "CO2 LEVEL";
		case 81: return "SUPPLY OUTLET TEMPERATURE";
		case 83: return "EXHAUST OUTLET TEMPERATURE";
		case 84: return "ACTUAL EXHAUST FAN SPEED";
		case 85: return "ACTUAL INLET FAN SPEED";
		case 86: return "REMOTE PARAMETER SETTINGS V/H";
		case 87: return "NOMINAL VENTIALTION VALUE";
		case 87: return "NOMINAL VENTIALTION VALUE";
		case 88: return "TSP NUMBER V/H";
		case 89: return "TSP ENTRY V/H";
		case 90: return "FAULT BUFFER SIZE V/H";
		case 91: return "FAULT BUFFER ENTRY V/H";
		case 115: return "OEM DIAGNOSTIC CODE";
		case 116: return "BURNER STARTS";
		case 116: return "BURNER STARTS";
		case 117: return "CH PUMP STATRS";
		case 117: return "CH PUMP STATRS";
		case 118: return "DHW PUMP/VALVE STARTS";
		case 118: return "DHW PUMP/VALVE STARTS";
		case 119: return "DHW BURNER STARTS";
		case 119: return "DHW BURNER STARTS";
		case 120: return "BURNER OPERATION HOURS";
		case 120: return "BURNER OPERATION HOURS";
		case 121: return "CH PUMP OPERATION HOURS";
		case 121: return "CH PUMP OPERATION HOURS";
		case 122: return "DHW PUMP/VALVE OPERATION HOURS";
		case 122: return "DHW PUMP/VALVE OPERATION HOURS";
		case 123: return "DHW BURNER HOURS";
		case 123: return "DHW BURNER HOURS";
		case 124: return "OPENTHERM VERSION MASTER";
		case 125: return "OPENTHERM VERSION SLAVE";
		#endif      
     }

     return String("?");
   }
   
    uint32_t 
		parity : 1, 
		type   : 3, 
		spare  : 8, 
		dataid : 8, 
		hi     : 8, 
		lo     : 8;
    
    // Construcor: takes raw 32 bit message and splits the double word into its components.
   OTMessage(int32_t dword) {
     parity = (0x80000000 & dword)>>31;
     type   = (0x70000000 & dword)>>28;
     spare  = (0x0f000000 & dword)>>24;
     dataid = (0x00ff0000 & dword)>>16;
     hi     = (0x0000ff00 & dword)>>8;
     lo     = (0x000000ff & dword);
    }

    String toString() {
      return String("OTMessage[")    
    		  + msgTypeToStr(type)  
			  + ",id:" + dataid + ",hi:" + hi + ",lo:" + lo  
			  + "," + dataIdToStr(dataid)
			  + "]";
    }  
};

typedef struct {
   // Use signed 16 bits to allow -1 to indicate that
   // value was not yet assigned.
   int16_t hi;  
   int16_t lo;
} Value;

// Global status 
struct {
  struct {
	  long serial; // time of last messe via serial line
      #ifdef WITH_DEBUG
	  struct { 
		  long fed;		// via URL /feedmsg?msg=...&feed=1 
		  long debug; 	// via URL /feedmsg?msg=... (parsed only, not fed)
	  } wifi;
      #endif
  }        lastMsg;         // time (millis()) when last message was received
  
  int16_t  ventSetpoint; 	 // as set by controller
  int16_t  ventOverride; 	 // as overridden by OTGW (sent to slave instead of ventSetpoint)
  int16_t  ventAcknowledged; // by the slave, sent to OTGW 
  int16_t  ventReported;     // from OTGW to controller, pretending level is as requested (ventSetpoint)
  int16_t  ventRelative; 	 // as reported by ventilation
  int16_t  tempSupply;  	 // milli Celsius (inlet)
  int16_t  tempExhaust;  	 // milli Celsius (inlet)
  int16_t  tsps[64];  	 	 // transparent slave parameters
  
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
  
  #ifdef WITH_DALLAS_TEMP_SENSORS
  int sensorsFound;
  long lastMeasure;
  int16_t extraTemps[MAX_NUM_SENSORS];
  #endif
  
} state;


// Credentials used to 1. set up ESP8266 as an access point and 2. define WiFI network to conenct to.
typedef struct {
	uint8_t used;
	char    ssid[32+1];
	char    psk[32+1];	
} Credentials;


// I'm from cologne ;)
#define MAGIC 4711+3

// Everything that goes to EEPROM
struct {	
	// Detect if first time setup of EEPROM was done. 
	uint16_t    magic;
	boolean 	configured;
	// Start ESP in access point mode?
	Credentials accessPoint;
	// Network to connect to
	Credentials homeNetwork;
	// Count incremented on every reboot to enablee monitoring of device crashes.
	uint32_t reboots;
} * EE = 0;


// User committed first time setup (e.g. AP, network) and it was saved permanently.
boolean configured = false;

// A random token used during setup to authenticate committer.
char configToken[10+1];

// Time at which an attempt to connect to a network will be considered expired during setup procedure.
long connectAttemptExpires = -1;

// Remember whether temperatures have been already requested for the first time. 
boolean temperaturesRequested = false;

Credentials accessPoint;
Credentials homeNetwork;

// After setup, a reboot is necessary. However, cannot do this while handling client request,
// because this will abort the connection. Instead of this, finish sending the reponse to the
// client and schedule a reboot after millis() will exceed this value.
long rebootScheduledAfter = -1;

// A string to hold incoming serial data from OTGW.
String inputString = "";         
// wWhether the string is complete.
boolean stringComplete = false;  

ESP8266WebServer server(HTTP_LISTEN_PORT);


/***************************************************************
 * 
 * Heart beat LED 
 * 
 */

boolean led_lit=false;

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
 * 
 * Dallas temperature sensors support
 * 
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
 * 
 * Error logging
 * 
 */

#ifdef WITH_ERROR_LOG

#define MAX_ERR_LEN		32
#define MAX_ERR_COUNT	10

typedef struct {
	long time; // millis
	char text[MAX_ERR_LEN] ;
} LogEntry;


//LogEntry errorLog[MAX_ERR_COUNT];
//uint8_t  errorLogIndex = 0;

// round robin buffer with last N errors
void addError(const char * text) {
/*	
	LogEntry * e = &errorLog[errorLogIndex];
	e->time = millis();
	strncpy(e->text, text, sizeof(e->text));
	e->text[sizeof(e->text)-1] = 0;
	errorLogIndex = (errorLogIndex+1)%MAX_ERR_COUNT;
	*/
} 
#else

void addError(const char * text) {}

#endif // WITH_ERROR_LOG

/***************************************************************
 * 
 * Debug support
 * 
 */

#define DBG_OFF 0 
#define DBG_ON  1
		
#ifdef WITH_DEBUG
	int DEBUG_LEVEL = DBG_ON;
	void dbg(String s)         { if (DBG_OFF<DEBUG_LEVEL) Serial.print(s);   } 
	void dbgln(String s)       { if (DBG_OFF<DEBUG_LEVEL) Serial.println(s); }
	void dbg(const char * s)   { if (DBG_OFF<DEBUG_LEVEL) Serial.print(s);   } 
	void dbgln(const char * s) { if (DBG_OFF<DEBUG_LEVEL) Serial.println(s); }
	void dbg(int i)            { if (DBG_OFF<DEBUG_LEVEL) Serial.print(i);   } 
	void dbgln(int i)          { if (DBG_OFF<DEBUG_LEVEL) Serial.println(i); }
#else
	int DEBUG_LEVEL = DBG_OFF;
	#define dbg(WHATEVER) {}
	#define dbgln(WHATEVER) {}
#endif

	
/***************************************************************
 * 
 * OpenTherm message parsing
 * 
 */

int onUnexpectedMessage(char sender, OTMessage& m) {
  //dbg("ZZ=onUnexpectedMessage:"); dbg(sender); dbg(": "); dbgln(m.toString());
  switch (sender) {
	  case 'T': state.messages.unexpected.T++; return RC_UNEXP_T;
	  case 'B': state.messages.unexpected.B++; return RC_UNEXP_B;
	  case 'R': state.messages.unexpected.R++; return RC_UNEXP_R;
	  case 'A': state.messages.unexpected.A++; return RC_UNEXP_A;
  }
  return RC_UNEXP_O;
}

int toTemp(int8_t hi, int8_t lo) {
	// TODO: Need proof if temperatue conversion is correct for t<0 
	// But wow? Even at winter time, temperature will never be <0 ... because of my setup.
	// Maybe someone else can help here?
	int sign = hi<0 ?  -1 :  1;
	int abs  = hi<0 ? -hi : hi; 
    abs = 100*abs + (100*lo/255);
    return sign*abs;
}

int onMasterMessage(OTMessage& m) {
	
	state.messages.expected.T++; {
		switch (m.type) {
		case OTMessage::MT_READ_DATA:
		case OTMessage::MT_WRITE_DATA:
			
			switch (m.dataid) {
			case OTMessage::DI_CONTROL_SETPOINT:   
				state.ventSetpoint = m.lo;
				break;
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
	
		if (OTMessage::MT_UNKN_DATAID==m.type) {
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
			state.ventAcknowledged = m.lo;
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
			if (0<=m.hi && m.hi<64) {
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
	
	state.messages.expected.R++; {
		switch (m.dataid) {
		// OTGW pretending towards the slave that controller wants to set this ventilation level,
		// e.g. R90470003	WRITE-DATA	CONTROL SETPOINT V/H: 3
		case OTMessage::DI_CONTROL_SETPOINT:
			// Hmm.. sometimes it does, sometime is does not.
			// Therefore saving this vakue now in overrideVentSetPoint() 
			//state.ventOverride = m.lo;
			return RC_OK;
		}
	} state.messages.expected.R--; // revoke former increment
	
	return onUnexpectedMessage('R', m);
}

int onAnswerMessage(OTMessage& m) {
	
	state.messages.expected.A++; {
		switch (m.dataid) {
			case OTMessage::DI_CONTROL_SETPOINT:
				state.ventReported=m.lo;
				// OTGW answering to controller, pretending the current ventilation
				// level is the one that was requested by the controller before.
				return 0;
		}
	} state.messages.expected.A--; // revoke former increment
	
	return onUnexpectedMessage('A', m);
}

int onInvalidMessageSource(char sender, OTMessage& m) {
  onUnexpectedMessage('?', m);
  return RC_INV_SOURCE;
}

int onInvalidLine(const char * line) {
  state.messages.invalid.format++;
  addError(line);
  return RC_INV_FORMAT;
}

int onInvalidLength(const char * line) {
  state.messages.invalid.length++;
  addError(line);
  return RC_INV_LENGTH;
}

String onOTGWMessage(String line, boolean feed) {
	
  const char * cstr   = line.c_str();
  char         sender = cstr[0];
  const char * hexstr = cstr+1;

  // Indicate there is some activity:
  ledFlash(5);
  
  // ACK of OTGW to command 'VS=x' or 'GW=1' messages sent by us to control fan speed level.
  if (line.startsWith("VS: ") || line.startsWith("GW: ")) {
	  if (feed) {
		  state.messages.expected.otgw++;
	  }
	  return String("ACK:'")+line + "'\n"; // adding white space causes flush??
  }
  
  // This message sent by OTGW if there is no controller connected, probably
  // to fulfill OpneTherm specs (at least 1 msg/sec sent)
  if (line.startsWith("R00000000")) {
	  state.messages.unexpected.zero++;
	  return String("NOCONN:'")+line+"\n";
  }
  
  String rc;
  int len = strlen(hexstr);
  if (8!=len) { // 1: 'B'/'T'/'A'/'R' followed by 8 hex chars
	  rc = String("INVLEN[") + len + "]:'" + cstr + "':";
	  
	  if (feed) {
		  // add real resut code if we should handle this message
		  rc += onInvalidLength(cstr);
	  }
	  else {
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
  strncpy(ls, hexstr+4, 4); TZERO(ls);
  
  uint32_t hi = strtol(hs, NULL, 16);
  uint32_t lo = strtol(ls, NULL, 16);  
  uint32_t number = (hi<<16)|lo;
  if (0==number && EINVAL==errno) {
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
  rc = String(sender)+":";
  rc += msg.toString();
  rc += ':';
  
  if (feed) {	  
	  switch(sender) {
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


int overrideVentSetPoint(int level) {
  level = level<-1 ? -1 : level>LEVEL_HIGH ? LEVEL_HIGH : level;
  if (level<0) {
	  for (int i=0; i<3; i++) {
		  // stop overriding, just monitor
		  Serial.print("GW=0\r\n"); 
		  delay(100);
	  } 
  }
  else {
	  for (int i=0; i<3; i++) {
		  Serial.print("GW=1\r\n");
		  delay(10);
		  Serial.print("VS="); Serial.print(level); Serial.print("\r\n");
		  delay(20);
	  }
  }
  dbg("ZZ=overrideVentSetPoint, old:"); dbg(state.ventOverride); dbg(" new:"); dbgln(level);
  int old = state.ventOverride; 
  state.ventOverride = level;
  return old;
}


/***************************************************************
 * 
 * EEPROM configuration and networking setup
 * 
 */

boolean readConfiguration() {
    //dbgln("ZZ=readConfiguration");
	boolean success = false;
	uint16_t magic;

	ESP.wdtDisable() ;
	EEPROM.begin(sizeof(*EE));
	eEE_READ(EE->magic, magic);
	
	//dbg("ZZ=magic="); dbgln(magic); 
	if (MAGIC==magic) {
		
		eEE_READ(EE->configured, configured);
		//dbg("ZZ=configured="); dbgln(configured); 
		
		eEE_READ(EE->accessPoint.used, accessPoint.used);
		if (accessPoint.used) {
			eEE_READ(EE->accessPoint, accessPoint);
		}
		eEE_READ(EE->homeNetwork.used, homeNetwork.used);
		if (homeNetwork.used) {
			eEE_READ(EE->homeNetwork, homeNetwork);
		}
		
		//dbgln("readConfiguration: success=true");
		success = true;
	}
	EEPROM.end();
	ESP.wdtEnable(5000);
	
	return success;
}

void firstTimeSetup() {
    dbgln("ZZ=firstTimeSetup");
	
	ESP.wdtDisable() ;
	EEPROM.begin(sizeof(*EE));
	
    dbgln("ZZ=writing credentials");
	Credentials creds = { 0, "", "" }; 
	memset(&creds, 0, sizeof(creds));
	
	eEE_WRITE(creds, EE->accessPoint.used);	
	eEE_WRITE(creds, EE->homeNetwork);
    dbgln("ZZ=credentials written");

    dbgln("ZZ=writing config flag");
	eEE_WRITE(configured, EE->configured);
    dbgln("ZZ=config flag written");
	
	// write magic to state that eeprom content is valid now
	uint16_t magic = MAGIC;
	eEE_WRITE(magic, EE->magic);

	EEPROM.commit();
	EEPROM.end();
	ESP.wdtEnable(5000); 
    dbgln("ZZ=firstTimeSetup done");
}

uint32_t reboots;

void updateReboots() {
    //dbgln("ZZ=updateReboots");

	ESP.wdtDisable() ;
	EEPROM.begin(sizeof(*EE));

	eEE_READ(EE->reboots, reboots);
	reboots++;
	//dbg("ZZ=reboots="); dbgln(reboots); 
	eEE_WRITE(reboots, EE->reboots);
	
	EEPROM.commit();
	EEPROM.end();
	ESP.wdtEnable(5000) ;
    //dbgln("ZZ=updateReboots done");
}

String toStr(IPAddress a) {
  return String("") + a[0] + '.' + a[1] + '.' + a[2] + '.' + a[3];
}

String getAPIP() {
  return toStr(WiFi.softAPIP());
}
  
void setupNetwork() {
	
	//dbgln("ZZ=setupNetwork");
	// if configured, try to connect to network first
	if (homeNetwork.used) {
		
	    int retries = 40; //*250ms=10s
	    
	    dbg("ZZ=connecting to "); dbgln(homeNetwork.ssid);
	    WiFi.mode(accessPoint.used ? WIFI_AP_STA : WIFI_STA);
		WiFi.begin(homeNetwork.ssid,  homeNetwork.psk);

	    while (WiFi.status() != WL_CONNECTED && 0<retries--) {
	      digitalWrite(LED_ONBOARD, retries % 2);
	      delay(250);
	      //dbgln("ZZ=.");
	    }
	}
	else {
		//dbgln("ZZ=no network set up");
	}
	
	boolean connected = WiFi.status()==WL_CONNECTED;	
	//dbg("setupNetwork:"); dbg(connected ? " " : " not "); dbgln("connected");
	
	// if that fails (or requested through configuration), start access point (as well)
	if (accessPoint.used || !connected) {
		
		if (!configured) {
			//dbgln("not configured => WIFI_AP_STA");
			// will need to connect to network during setup
		    WiFi.mode(WIFI_AP_STA);
		}
		else if (homeNetwork.used) {
			// was configured to connect to a network		
			//dbgln("homeNetwork used => WIFI_AP_STA");
		    WiFi.mode(WIFI_AP_STA);			
		}
		else {
			// was configured to stay access point only, no network		
			//dbgln("configure, no home network => WIFI_AP");
			WiFi.mode(WIFI_AP);
		}
	    
	    String sid = accessPoint.ssid;
	    String psk = accessPoint.psk;
	    //dbg("ZZ=accessPoint.ssid: "); dbgln(sid);
	    //dbg("ZZ=accessPoint.psk:  "); dbgln(psk);
	    if (!accessPoint.used) {
			#ifdef DEFAULT_START_SSID
	    	sid = DEFAULT_START_SSID;
			#else
	    	sid = String("http://") + getAPIP();
			#endif	    	
	    	psk = DEFAULT_PSK;
	    }
	    
	    // Prefix debug messages with (invalid) command prefix "ZZ=" in case we are really conneted to a OTGW.
	    // This way it will just print a "invalid command" message and we do not risk to confuse/crash it.
	    dbg("ZZ=ssid: "); dbgln(sid);
	    dbg("ZZ=psk:  "); dbgln(psk);
		WiFi.softAP(sid.c_str(), psk.c_str());
	}
}

/***************************************************************
 * 
 * Web server support  
 * 
 */

// Send headers with HTTP response that disable browser caching
void avoidCaching() {
	server.sendHeader(String("Pragma"),  String("no-cache"));
	server.sendHeader(String("Expires"), String("0"));
	server.sendHeader(String("Cache-Control"), String("no-cache, no-store, must-revalidate"));
}

// Send 320 redirect HTTP response
void sendRedirect(String path) {
	String body = String("<htm><body>Proceed to <a href=\"")+path+"\">"+path+"</a></body></html>";
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
 * 
 * Web server support (first time network setup)  
 * 
 */

#ifdef WITH_WEB_SETUP
void handleSetup() {
	server.send_P(200, PGM_CT_TEXT_HTML, HTML_SETUP);		
}
#endif

String getLocalAddr() {
	IPAddress local = WiFi.localIP();
	String addr = String(local[0])+"."+local[1]+"."+local[2]+"."+local[3];	
	return addr;
}

void createConfigToken() {
	int i;
	for (i=0; i<sizeof(configToken)-1; i++) {
		configToken[i]=random('A', 'Z' + 1);
	}
	configToken[i]=0;
}

void handleAjax() {
	String action = server.arg("a");
	
	if (action=="set") { // set network name and password, start to connect
		String ssid = server.arg("ssid");
		String psk  = server.arg("psk");
		if (ssid.length()<1 || psk.length()<8) {
			server.send(200, CT_APPL_JSON, "{\"ok\":0,\"msg\":\"Name or password to short\"}\n");
			return;
		}
		else {		
			strncpy(homeNetwork.ssid, ssid.c_str(), sizeof(homeNetwork.ssid)); TZERO(homeNetwork.ssid); 
			strncpy(homeNetwork.psk,  psk.c_str(),  sizeof(homeNetwork.psk )); TZERO(homeNetwork.psk ); 
			homeNetwork.used = true;
			
			WiFi.mode(WIFI_AP_STA);
			WiFi.begin(homeNetwork.ssid, homeNetwork.psk);
			long timeout=20; // secs
			connectAttemptExpires = millis()+timeout*1000L;
			createConfigToken();
			dbg("ZZ=created configToken "); dbgln(configToken);
			server.send(200, CT_APPL_JSON, 
				String("{\"ok\":1,\"msg\":\"Ok\",\"token\":\"")+configToken+"\",\"timeout\":\""+timeout+"\"}\n");
			return;
		}
	}
	else if (action=="status") { // sent when setup.html is polling for device coming up after reboot
		String saddr = getLocalAddr();
		String caddr = toStr(server.client().remoteIP());
		server.send(200, CT_APPL_JSON, 
			String("{\"ok\":1,\"nwssid\":\"")+homeNetwork.ssid+"\",\"saddr\":\""+saddr+"\",\"caddr\":\""+caddr+"\"}\n");
		return;	
	}
	
	String token = server.arg("t");
	if (token=="MaGiC") {
		// okay (for debug purposes only)
		dbg("ZZ=debug token: "); dbgln(token);
	}
	else if (token!=configToken) {
		dbg("ZZ=token exp: "); dbgln(configToken);
		dbg("ZZ=token got: "); dbgln(token);
		server.send(200, CT_APPL_JSON, "{\"ok\":0,\"msg\":\"Token mismatch\"}\n");
		return;
	}
	
	if (action=="wait") { // setup.html is checking if device connected
		long now = millis();
		if (WiFi.status() == WL_CONNECTED) {
			String addr = getLocalAddr();
			if (80!=HTTP_LISTEN_PORT) { 
				addr+=":"; 
				addr+=HTTP_LISTEN_PORT; 
			}
			server.send(200, CT_APPL_JSON, String("{\"ok\":1,\"status\":0,\"addr\":\"")+addr+"\"}\n");
			return;
		}
		else if (connectAttemptExpires<0 || now>connectAttemptExpires) {
			server.send(200, CT_APPL_JSON, "{\"ok\":1,\"status\":2,\"msg\":\"Timeout\"}\n");
			return;
		}
		else {
			int left = (connectAttemptExpires-now)/1000;
			server.send(200, CT_APPL_JSON, String("{\"ok\":1,\"status\":1,\"msg\":\"Connecting\",\"left\":")+left+"}\n");
			return;
		}
	}
	else if (action=="save") { // setup.html connected via network (not AP), ready to commit settings 
		String ap = server.arg("ap");
		if (ap=="1") { // keep ap?
			String apssid = server.arg("apssid");
			String appsk  = server.arg("appsk");
			strncpy(accessPoint.ssid, apssid.c_str(), sizeof(accessPoint.ssid)); TZERO(accessPoint.ssid); 
			strncpy(accessPoint.psk,  appsk.c_str(),  sizeof(accessPoint.psk )); TZERO(accessPoint.psk ); 
			accessPoint.used = true;
		}
		else if (ap=="0") {
			accessPoint.used = false;			
		}
		else {
			server.send(200, CT_APPL_JSON, String("{\"ok\":0,\"msg\":\"parameter 'ap' missing\"}"));
			return;
		}
		
		homeNetwork.used = true;
		ESP.wdtDisable() ;
		EEPROM.begin(sizeof(*EE));
		dbg("ZZ=saving accessPoint: "); dbg(accessPoint.used); dbg(" "); dbg(accessPoint.ssid); dbg(" "); dbgln(accessPoint.psk);
		eEE_WRITE(accessPoint, EE->accessPoint);
		dbg("ZZ=saving homeNetwork: "); dbg(homeNetwork.used); dbg(" "); dbg(homeNetwork.ssid); dbg(" "); dbgln(homeNetwork.psk);
		eEE_WRITE(homeNetwork, EE->homeNetwork);
		boolean configured = true;
		eEE_WRITE(configured, EE->configured);
		EEPROM.commit();
		EEPROM.end();
		ESP.wdtEnable(5000) ;

		
		String body = "{\"ok\":1}\n\n";
		server.send(200, CT_APPL_JSON, body);
		delay(200);		
		server.client().stop();
		delay(200);		
		dbgln("ZZ=reboot scheduled");
		rebootScheduledAfter = millis()+1*1000;
		return;
	}
	server.send(404, CT_APPL_JSON, String("{\"ok\":0,\"msg\":\"unknown resource\"}"));
}


/***************************************************************
 * 
 * Web server support (normal operation)  
 * 
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

// TODO: makes ESP crash:
/*
void handleHomeIcon() {
	sendBinary(FAN_PNG, sizeof(FAN_PNG), "image/png");
}
*/
void handleManifest() {
	server.send_P(200, PGM_CT_APPL_JSON, MANIFEST_JSON);		
}

String upTime() {
	long secs = millis()/1000;
	long mins = secs / 60; secs %= 60;
	long hrs  = mins / 60; mins %= 60;
	long days = hrs  / 24; hrs  %= 24; 	
	String t = String(days) + "d," + hrs + "h," + mins + "m," + secs + "s";
	return t;
}

void handleApiStatus() {
	  String refresh = server.arg("refresh");
	  //String filterCheck = String(state.tsps[23], 16);
	  String filterCheck = (state.status.lo & 32) ? "1" : "0";
	  String json = String() +
		"{\n"
		"  \"ok\":1,\n"
		"  \"build\":\"" + buildNo + "\",\n"
		"  \"reboots\":\"" + reboots + "\",\n"
	    "  \"now\":" + millis() + ",\n"
		"  \"uptime\":\"" + upTime()+ "\",\n"
	    "  \"lastMsg\":{" + 
				"\"serial\":" + state.lastMsg.serial + ""
				#ifdef WITH_DEBUG
				",\"wifi\":{\"fed\":" + state.lastMsg.wifi.fed  + ",\"debug\":" + state.lastMsg.wifi.debug + "}" 
				#endif
		   "},\n"	
	    "  \"dbg\":{\"level\":" + DEBUG_LEVEL + "},\n"
		"  \"vent\":{\"set\":" + state.ventSetpoint + ",\"override\":" + state.ventOverride + ",\"ackn\":" + state.ventAcknowledged + ",\"reported\":" + state.ventReported + ",\"relative\":" + state.ventRelative + "},\n"
		"  \"temp\":{\"supply\":"   + state.tempSupply   + ",\"exhaust\":" + state.tempExhaust + "},\n"
		"  \"status\":["            + state.status.hi           + "," + state.status.lo           + "],\n"
		"  \"faultFlagsCode\":["    + state.faultFlagsCode.hi   + "," + state.faultFlagsCode.lo   + "],\n"
		"  \"configMemberId\":["    + state.configMemberId.hi   + "," + state.configMemberId.lo   + "],\n"
		"  \"masterConfig\":["      + state.masterConfig.hi     + "," + state.masterConfig.lo     + "],\n"
		"  \"remoteParamFlags\":["  + state.remoteParamFlags.hi + "," + state.remoteParamFlags.lo + "],\n"
		"  \"filterCheck\":" + filterCheck + ",\n"
		"  \"messages\":{\n" + 
		"    \"total\":"   + (state.messages.serial+state.messages.wifi.fed+state.messages.wifi.debug) + ",\n" 
		"    \"serial\":"  + state.messages.serial + ",\n"
		#ifdef WITH_DEBUG
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
        "  \"freeheap\":"  + ESP.getFreeHeap() + ",\n"				
        "  \"debug\":"    + DEBUG_LEVEL + ",\n";

	  #ifdef WITH_DALLAS_TEMP_SENSORS
	  json +=					
		String(
		"  \"sensorsFound\":")+state.sensorsFound+",\n"
		"  \"lastMeasure\":"+(millis()-state.lastMeasure)+",\n"
	    "  \"sensors\":[";
	  for (int i=0; i<sizeof(state.extraTemps)/sizeof(state.extraTemps[0]); i++) {
		  if (i>0) json += ",";
		  json += state.extraTemps[i];
	  }	  
	  json += "],\n";
	  #endif
				  
	  json +=
	    "  \"tsps\":[";	  
	  // print "transparent slave parameters" as json array
	  for (int i=0; i<sizeof(state.tsps)/sizeof(state.tsps[0]); i++) {
	    if (i>0    ) json += ",";
		if (0==i%8) json+="\n    ";
	    json += state.tsps[i];
	  }
	  json += "]\n";
	  
	  json += "}\n";
	  
	  avoidCaching();
	  if (refresh!="") server.sendHeader("Refresh",  refresh);
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
	String json = String("{\"ok\":1,\"now\":") + getDate() + ",\"level\":"+l+"}\n";
	avoidCaching();
	server.send(200, CT_APPL_JSON, json); 						
}

#ifdef WITH_DEBUG
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
	if (msg!="") {
		if (feed=="1") {
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

// Use this like: "curl http://192.168.4.1/tailf".
// It will however not receive a valid HTTP response but dump things to stdout.
void handleDbgTailF() {
	
	String cmd = server.arg("cmd");
	
	WiFiClient client = server.client();
	// Send at least a very basic HTTP header. 
	client.write("HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n\r\n");
	
	if (cmd=="") {
		client.print("No cmd sent\n");
	}
	else {		
		Serial.print(cmd); Serial.print("\r\n");
		Serial.print(cmd); Serial.print("\n");
		client.print(String("Cmd sent: '") + cmd + "'");
	}

	inputString = "";
	uint32_t maxWait = 60*1000; // 20s
	do {
		//client.print("waiting for serial data\n");
		// Wait for data on serial line to become available
		while (client.connected() && !Serial.available() && maxWait--){
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
#endif // WITH_DEBUG


#ifdef WITH_PIN_TEST
void handlePinTest() {
	String str = server.arg("p");
	if (str=="") {
		server.send(404, CT_TEXT_PLAIN, "No pin specified\n");
		return;
	}

	int pin = atoi(str.c_str());
	server.send(200, CT_TEXT_PLAIN, String("Toggle pin ") + pin + "\n");
	server.client().stop();
	
	dbg("toggle start pin"); dbgln(pin);
	pinMode(pin, OUTPUT);	
	for (int i=0; i<10; i++) {
		dbg("digitalWrite("); dbg(pin); dbg(","); dbgln(i%2);   
		digitalWrite(pin, i%2);
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
	if      (s=="high")   l=LEVEL_HIGH;
	// -------
	else if (s=="norm")   l=LEVEL_NORM;  
	else if (s=="normal") l=LEVEL_NORM;  
	// -------
	else if (s=="low")    l=LEVEL_LOW;  
	// -------
	else if (s=="off")    l=LEVEL_OFF;  
	else { 
	  l = atoi(s.c_str());
	  if (0==l && EINVAL==errno) {
		  l=-1; // inval
	  }
	  else if (l<LEVEL_OFF || l>LEVEL_HIGH) {
		  l=-1; // inval		  
	  }
	}
		
	String json;
	if (-1==l) {
		json = String("{\"ok\":0,\"now\":")+getDate()+",,\"msg\":\"invalid value\",\"value\":\""+s+"\"}\n";
	}
	else {
		int old = overrideVentSetPoint(l);
		json = String("{\"ok\":1,\"now\":")+getDate()+",\"level\":"+l+",\"value\":\""+s+"\",\"old\":" + old + "}\n";
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
  
  while (Serial.available()) {
	
    // get the new byte:
    char c = (char)Serial.read();
    //dbg("read: "); dbgln(String(inChar));
    
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
	switch (c) {
		// ignore CR, assume next is a newline
		case '\r': break;
		case '\n': stringComplete = true; break;
		default:   inputString += c;	    				
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
			
		#ifdef WITH_DEBUG
		"/dbg/tailf?cmd=<str>\n"
		"/dbg/set?level=[0-1]\n"
		"/dbg/msg?msg=X00000000&feed=[0-1]\n"
		#endif // WITH_DEBUG
			
		#ifdef WITH_PIN_TEST
		"/pintest?pin=X\n"
		#endif // WITH_PIN_TEST
			
		"/help\n";			
	server.send(200, CT_TEXT_PLAIN, body); 						
}
#endif


void setup() {
	
  Serial.begin(9600); // as used by OTGW
  overrideVentSetPoint(LEVEL_NOOVER);  
  
  inputString.reserve(200);
  
  if (!readConfiguration()) {
	  // when EEPROM was not staring with magic,need to set up first:
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
  state.ventAcknowledged = -1;
  state.ventReported = -1;
  state.ventRelative = -1;
  state.tempSupply   = -30000; // -300C 
  state.tempExhaust  = -30000;
  state.status.hi           = state.status.lo           = -1; 		 
  state.faultFlagsCode.hi   = state.faultFlagsCode.lo   = -1;	
  state.configMemberId.hi   = state.configMemberId.lo   = -1; 
  state.masterConfig.hi     = state.masterConfig.lo     = -1;
  state.remoteParamFlags.hi = state.remoteParamFlags.lo = -1;
  for (int i=0; i<sizeof(state.tsps)/sizeof(state.tsps[0]); i++) {
	  state.tsps[i] = -1;
  }
  
  #ifdef WITH_DALLAS_TEMP_SENSORS
  setupSensors();    
  #endif
    
  setupNetwork();

  server.on("/",              handleRoot);
  server.on("/index",         handleIndex);
  server.on("/index.html",    handleIndex);
  server.on("/manifest.json", handleManifest);
  server.on("/favicon.ico",   handleFavicon);
  // TODO: makes ESP crash
  //server.on("/fan.png",       handleHomeIcon);
  server.on("/api/status",    handleApiStatus);
  server.on("/api/set",       handleApiSet);
  server.on("/api/level",     handleApiLevel);
  #ifdef WITH_WEB_SETUP
  server.on("/setup",    	  handleSetup);
  #endif
  server.on("/ajax",          handleAjax);

  #ifdef WITH_DEBUG
  server.on("/dbg/tailf",     handleDbgTailF);
  server.on("/dbg/set",       handleDbgSet);
  server.on("/dbg/msg",       handleDbgMsg);
  #endif
  
  #ifdef WITH_PIN_TEST
  server.on("/dbg/pin",       handlePinTest);
  #endif

  server.on("/help",          handleHelp);
  server.onNotFound(          handleNotFound);  
  server.begin();
}


void handleHeartbeat() {
	
	int now = millis();
	// Start blinking after things stable.
	// Seems like fiddeling with GPIO0 too early resets the device sporadically.
	if (now<5000) return;
	
	boolean light = false;
	int level     = state.ventOverride; // 0=off,...,3=high
	if (now>30000 && WiFi.status() != WL_CONNECTED) {
		light = (now/100)%2;
	}
	else if (level<0) {
		light = (now/2000)%2;
	}
	else {
		int sequence  = now%2500;           // 0,...,2500-1
		int prefix    = 500*(level+1);      // 500, 1000, 1500, 2000
		if (sequence < prefix) {
			sequence %= 500; 				// runs 1x,2x,3x or 4x through 0,..,500-1
			light = sequence<250;
		}
	}
	
	if (light != led_lit) {
		pinMode(LED_HEART, OUTPUT);
		digitalWrite(LED_HEART, light ? LOW : HIGH);
		led_lit = light;
	}
}

long loopIterations = 0;

void loop() {
	serialEvent();	
	server.handleClient();
	
	#ifdef WITH_DALLAS_TEMP_SENSORS
	if (!temperaturesRequested || 0==loopIterations%(1000*1000))  {
		handleSensors();
	}
	#endif

	if (stringComplete) {
		state.lastMsg.serial = millis();
		state.messages.serial++;
		onOTGWMessage(inputString, true);
		inputString = "";
		stringComplete = false;
	}
	
	handleHeartbeat();
	
	if (rebootScheduledAfter>-1 && rebootScheduledAfter>millis()) {
		rebootScheduledAfter = -1;
		dbgln("*********** REBOOT **********");
		delay(100);
		ESP.reset();
		delay(10000);
	}
	
	loopIterations++;
}


#ifdef WITH_DALLAS_TEMP_SENSORS
String toString(DeviceAddress& a) {
	String rtv;
	for (int i=0; i<sizeof(a)/sizeof(a[0]); i++) {
		if (i) rtv+=":";
		if (a[i]<0x10) rtv+="0";
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
void printTemperature(DeviceAddress deviceAddress)
{
  float tempC = sensors.getTempC(deviceAddress);
  dbg("Temp C: ");
  dbg(tempC);
}


void handleSensors() {
	//dbgln("ZZ=handleSensors");
	if (!temperaturesRequested) {
		sensors.requestTemperatures();
		temperaturesRequested=true;
	}
	
	state.lastMeasure = millis();	
	//dbgln("ZZ=handleSensors: reading ts");
    for (int i=0; i<state.sensorsFound; i++) {
    	//dbg("ZZ=sensors.getTempCByIndex("); dbg(i); dbgln(")");
    	float t = sensors.getTempCByIndex(i);
    	//dbg("ZZ=t="); dbgln((int)(100*t));
    	state.extraTemps[i] = 100*t;
	}
    sensors.requestTemperatures();
    temperaturesRequested=true;
	//dbgln("ZZ=handleSensors done");
}

void setupSensors() {
	//dbgln("ZZ=setupSensors");

	// Start up the library
	sensors.begin();
	sensors.setResolution(11);

	// locate devices on the bus
	//dbg("ZZ=locating devices...");
	state.sensorsFound = sensors.getDeviceCount();
	//dbg("ZZ=found "); dbg(state.sensorsFound); dbgln(" sensors.");
	
	// report parasite power requirements
	//dbg("parasite power: "); 
	//if (sensors.isParasitePowerMode()) dbgln("ON"); else dbgln("OFF");
}
#endif
