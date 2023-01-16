#ifndef OTMESSAGE_H
#define OTMESSAGE_H

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
  const static uint8_t DI_STATUS_FLAGS        =   0;
  const static uint8_t DI_CONTROL_SETPOINT    =   1;
  const static uint8_t DI_MASTER_CONFIG       =   2;
  const static uint8_t DI_REMOTE_PARAM_FLAGS  =   6;
  const static uint8_t DI_STATUS              =  70;
  const static uint8_t DI_CONTROL_SETPOINT_VH =  71;
  // According to http://otgw.tclcode.com/matrix.cgi#thermostats only:
  const static uint8_t DI_FAULT_FLAGS_CODE    =  72;
  const static uint8_t DI_CONFIG_MEMBERID     =  74;
  const static uint8_t DI_REL_VENTILATION     =  77;
  const static uint8_t DI_SUPPLY_INLET_TEMP   =  80;
  const static uint8_t DI_EXHAUST_INLET_TEMP  =  82;
  const static uint8_t DI_TSP_SETTING         =  89; 
  const static uint8_t DI_MASTER_PROD_VERS    = 126;
  const static uint8_t DI_SLAVE_PROD_VERS     = 127;
      
    
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
    case DI_STATUS_FLAGS:        return "Master/slave Status flags";
    case DI_CONTROL_SETPOINT_VH: return "Control setpoint V/H";
    case DI_MASTER_CONFIG:       return "Master configuration";
    case DI_REMOTE_PARAM_FLAGS:  return "Remote parameter flags";
    case DI_STATUS:              return "Status V/H";
    case DI_FAULT_FLAGS_CODE:    return "Fault flags/code V/H";
    case DI_CONTROL_SETPOINT:    return "Control setpoint";
    case DI_CONFIG_MEMBERID:     return "Configuration/memberid V/H";
    case DI_REL_VENTILATION:     return "Relative ventilation";
    case DI_SUPPLY_INLET_TEMP:   return "Supply inlet temperature";
    case DI_EXHAUST_INLET_TEMP:  return "Exhaust inlet temperature";

    // TODO: Find out meaning of various "transparent slave parameters"
    // These are requested by the master in a round robin fashion from slave in the range 0-63,
    // however some indexes are skipped.
        // TSPs in at index 0, 2 and 4 seem to hold values for relative fan speeds for
        // reduced, normal, high (party) level.
    case DI_TSP_SETTING:    return "TSP setting V/H";
    case DI_MASTER_PROD_VERS: return "Master product version";
    case DI_SLAVE_PROD_VERS:  return "Slave product version";
    
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
    case 57: return "MAX CH WATER SETPOINT";
    case 58: return "OTC HEATCURVE RATIO";

    case 73: return "DIAGNOSTIC CODE V/H";
    case 75: return "OPENTHERM VERSION V/H";
    case 76: return "VERSION & TYPE V/H";
    case 78: return "RELATIVE HUMIDITY V/H";
    case 79: return "CO2 LEVEL V/H";
    case 80: return "SUPPLY INLET TEMPERATURE V/H";
    case 81: return "SUPPLY OUTLET TEMPERATURE V/H";
    case 83: return "EXHAUST OUTLET TEMPERATURE V/H";
    case 84: return "ACTUAL EXHAUST FAN SPEED V/H";
    case 85: return "ACTUAL INLET FAN SPEED V/H";
    case 86: return "REMOTE PARAMETER SETTINGS V/H";
    case 87: return "NOMINAL VENTIALTION VALUE V/H";
    case 88: return "TSP NUMBER V/H";
    case 89: return "TSP ENTRY V/H";
    case 90: return "FAULT BUFFER SIZE V/H";
    case 91: return "FAULT BUFFER ENTRY V/H";

    case 100: return "REMOTE OVERRIDE FUNCTION";

    case 115: return "OEM DIAGNOSTIC CODE";
    case 116: return "BURNER STARTS";
    case 117: return "CH PUMP STATRS";
    case 118: return "DHW PUMP/VALVE STARTS";
    case 119: return "DHW BURNER STARTS";
    case 120: return "BURNER OPERATION HOURS";
    case 121: return "CH PUMP OPERATION HOURS";
    case 122: return "DHW PUMP/VALVE OPERATION HOURS";
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

#endif
