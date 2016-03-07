/*
Copyright (C) Artur Pogoda de la Vega

Arduino library to ease the use of the uC's EEPROM. 
It does so by providing some functions and macros to write more complex data than just
single bytes. The "e" in eEEPROM stands for "easy" and the naming claims to differ as little
as possible form the "normal" Arduino EEPROM library such that one can switch back and forth
will little effort should this become necessary for some reasons, e.g. bugs or memory footprint

See README for more details.
*/

#ifndef eEEPROM_h
#define eEEPROM_h

#include <inttypes.h>
/*
#include <avr/pgmspace.h>
*/
#include <Arduino.h>

#ifdef OSX
	#define ADDR_T long
#else	
	#define ADDR_T int
#endif

/*
 * Structure to store integer values round robin.
 * The MSB bit defines which value is the current one and the remaining bits store the actual value.
 * Using an array of such structures and moving the current flag to the next element upon every
 * change will spread write cycles to the EEProm equally over the whole array.
 * This improves lifetime of the EEProm by N/2 where N is the number of array elements
 * (Not by N, because the former value needs to be invalidated).
 * Static member TYPE  is actually only a hint that allow the the template methods to derive
 * the correct type of the member "value". It should NOT eat up any valuable RAM nor EEProm
 * space since it is static, moreover its value never read.
 */
typedef struct s_rrint7   { int8_t  current : 1; int8_t  value :  7; static int8_t  TYPE; } s_rrint7;
typedef struct s_rrint15  { int16_t current : 1; int16_t value : 15; static int16_t TYPE; } s_rrint15;
typedef struct s_rrint31  { int32_t current : 1; int32_t value : 31; static int32_t TYPE; } s_rrint31;
typedef struct s_rrstruct { uint8_t current; uint8_t data; } s_rrstruct;

class eEEPROMClass
{
  public:
    uint8_t read(ADDR_T addr);
    void write(ADDR_T addr, uint8_t);
    
    // do not check target value but write through to the  EEPROM
    void doWrite(ADDR_T addr, uint8_t value);

    uint16_t readWord(ADDR_T addr);
    void writeWord(ADDR_T addr, uint16_t);
    
    uint32_t readLong(ADDR_T addr);
    void writeLong(ADDR_T addr, uint32_t);
    
    float readFloat(ADDR_T addr);
    void writeFloat(ADDR_T addr, float);

    double readDouble(ADDR_T addr);
    void writeDouble(ADDR_T addr, double);
    
    void readData(ADDR_T addr, void * buf, int len);
    void writeData(ADDR_T addr, const void * buf, int len);
    
    void memFill(ADDR_T addr, uint8_t data, uint16_t len);

    void rrsRead (s_rrstruct * rrstruct, uint16_t size, uint16_t count, void * user_struct);
    void rrsWrite(s_rrstruct * rrstruct, uint16_t size, uint16_t count, void * user_struct);

    template <class NUM> NUM numRead(NUM * piEE)
    {
      volatile NUM iRam = 0;
      ADDR_T addr = (ADDR_T)piEE;
      readData(addr, (void*)&iRam, sizeof(iRam));
      return iRam;
    }

    // const NUM to allow const definition of eeprom struct pointer: 
    //   const s_eeprom_data * EEPROM = 0;
    template <class NUM> void numWrite(const NUM * piEE, NUM iRam)
    {
      ADDR_T addr = (ADDR_T)piEE;
      writeData(addr, (void*)&iRam, sizeof(iRam));
    }

    // template RRINT will become one of s_rrint{7|15|31} through the macro eEE_RRREAD
    template <class RRINT> __decltype(RRINT::TYPE) rriRead(RRINT * rrint, uint16_t count)
    {
    	ADDR_T addr = (ADDR_T)rrint;
    	RRINT temp;
    	for (uint16_t i=0; i<count; i++, addr+=sizeof(s_rrint7))
    	{
    		readData(addr, &temp, sizeof(temp));
    		if (temp.current) return temp.value;
    	}
    	return 0;
    }

    // template RRINT will become one of s_rrint{7|15|31} through the macro eEE_RRWRITE
    template <class RRINT> void rriWrite(RRINT * rrint, uint16_t count, __decltype(RRINT::TYPE) value)
    {
    	RRINT temp, next;
		next.current = 1;
		next.value   = value;

    	for (ADDR_T i=0; i<count; i++)
    	{
        	ADDR_T addr1 = (ADDR_T)&rrint[i];
    		readData(addr1, &temp, sizeof(temp));
    		if (temp.current)
    		{
    			if (temp.value==value) return; // no change

    			ADDR_T addr2 = (ADDR_T)&rrint[(i+1)%count];
    			temp.current = 0;

    			writeData(addr2, &next, sizeof(next));
    			writeData(addr1, &temp, sizeof(temp));
    			return;
    		}
    	}

    	ADDR_T addr = (ADDR_T)rrint;
    	writeData(addr, &next, sizeof(next));
    }

    /* moved now to ePGM module since was mixing thing up: eeprom and flash
    void showPgmString(PGM_P s);
    void showPgmStringLn(PGM_P s);
    */
};

extern eEEPROMClass eEEPROM;

#define eEE_ADDR(EETOKEN) ((ADDR_T)(void*)&(EETOKEN))
#define eEE_WRITE(SRC, EETOKEN) { eEEPROM.writeData(eEE_ADDR(EETOKEN), &(SRC),  sizeof(EETOKEN)); }
#define eEE_READ(EETOKEN, DEST) { eEEPROM.readData( eEE_ADDR(EETOKEN), &(DEST), sizeof(DEST));    }
#define eEE_ZERO(EETOKEN)       { eEEPROM.memFill(eEE_ADDR(EETOKEN), 0, sizeof(EETOKEN)); }

#define eEE_CHECKSIZE(DATA) struct FailOnEEPromExceess { int c[(E2END)-sizeof((DATA))]; }

#define PPRINT(TEXT)   eEEPROM.showPgmString(PSTR(TEXT))
#define PPRINTLN(TEXT) eEEPROM.showPgmStringLn(PSTR(TEXT))

/*
 * Macros that define round robin integer arrays with user defined size;
 */
#define eEE_rrint7_t(NAME,COUNT)  s_rrint7  NAME[COUNT]
#define eEE_rrint15_t(NAME,COUNT) s_rrint15 NAME[COUNT]
#define eEE_rrint31_t(NAME,COUNT) s_rrint31 NAME[COUNT]

/*
 * Same for arbitrary user structures.
 */
#define eEE_rrstruct(NAME,COUNT,DEF) struct NAME { uint8_t current; DEF data; } NAME[COUNT]

/*
 * Read  write umbers (integers, float, double) (maps to template methods intRead/intWrite)
 */
#define eEE_READN(EETOKEN)        eEEPROM.numRead(&(EETOKEN))
#define eEE_WRITEN(EETOKEN,VALUE) eEEPROM.numWrite(&(EETOKEN), (VALUE))

/*
 * Macros to transparently init/read/write such round robin arrays with integeres.
 */
#define eEE_READRRI(EETOKEN)        eEEPROM.rriRead( EETOKEN, sizeof(EETOKEN)/sizeof(EETOKEN[0]))
#define eEE_WRITERRI(VALUE,EETOKEN) eEEPROM.rriWrite(EETOKEN, sizeof(EETOKEN)/sizeof(EETOKEN[0]), VALUE)

/*
 * Macros to write round robin arrays of arbitrary, user-defined structures
 */
#define eEE_READRRS(EETOKEN,TARGET)  eEEPROM.rrsRead ((s_rrstruct*)&(EETOKEN), sizeof(EETOKEN), sizeof(EETOKEN)/sizeof(EETOKEN[0]), &(TARGET));
#define eEE_WRITERRS(SOURCE,EETOKEN) eEEPROM.rrsWrite((s_rrstruct*)&(EETOKEN), sizeof(EETOKEN), sizeof(EETOKEN)/sizeof(EETOKEN[0]), &(SOURCE));


// eEEPROM_h
#endif
