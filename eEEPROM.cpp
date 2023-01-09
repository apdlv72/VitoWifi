#include "Arduino.h"
#include "eEEPROM.h"

// does not work (any more? not with esp8266?)
//#include <avr/eeprom.h>
#include <EEPROM.h>

uint8_t eEEPROMClass::read(ADDR_T addr)
{
	//return eeprom_read_byte((unsigned char *) addr);
	return EEPROM.read((int)addr);
}

void eEEPROMClass::write(ADDR_T addr, uint8_t value)
{
	// write only if there is no change to avoid writy cycles
	uint8_t old = read(addr);
	if (old!=value)
	{
		//eeprom_write_byte((unsigned char *) addr, value);
		EEPROM.write((int)addr, value);
		EEPROM.commit();
	}
}

void eEEPROMClass::doWrite(ADDR_T addr, uint8_t value)
{
	//eeprom_write_byte((unsigned char *) addr, value);
	EEPROM.write((int)addr, value);
}

uint16_t eEEPROMClass::readWord(ADDR_T addr)
{
	uint16_t w;
	readData(addr, &w, sizeof(w));
	return w;
}

void eEEPROMClass::writeLong(ADDR_T addr, uint32_t l)
{
	writeData(addr, &l, sizeof(l));
}

uint32_t eEEPROMClass::readLong(ADDR_T addr)
{
	uint32_t l;
	readData(addr, &l, sizeof(l));
	return l;
}

void eEEPROMClass::writeWord(ADDR_T addr, uint16_t w)
{
	writeData(addr, &w, sizeof(w));
}

float eEEPROMClass::readFloat(ADDR_T addr)
{
	float f;
	readData(addr, &f, sizeof(f));
	return f;
}

void eEEPROMClass::writeFloat(ADDR_T addr, float f)
{
	writeData(addr, &f, sizeof(f));
}

double eEEPROMClass::readDouble(ADDR_T addr)
{
	double d;
	readData(addr, &d, sizeof(d));
	return d;
}

void eEEPROMClass::writeDouble(ADDR_T addr, double d)
{
	writeData(addr, &d, sizeof(d));
}

void eEEPROMClass::readData(ADDR_T addr, void * buf, int len)
{
	//Serial.print("ZZ=");
	for (uint8_t * b=(uint8_t*)buf; len>0; len--, addr++, b++)
	{		
		*b = read(addr);
		yield(); delay(1); //Serial.print("r");
	}
	//Serial.println();
}

void eEEPROMClass::writeData(ADDR_T addr, const void * buf, int len)
{
	//Serial.print("ZZ=");
	for (const uint8_t * b=(uint8_t*)buf; len>0; len--, addr++, b++)
	{
		write(addr, *b);
		yield(); delay(1); //Serial.print("w");
	}	
	//Serial.println();
}

void eEEPROMClass::memFill(ADDR_T addr, uint8_t data, uint16_t len)
{
	for (; len>0; len--, addr++)
	{
		write(addr, data);
		yield();
	}
}

/*
void eEEPROMClass::showPgmString (PGM_P s)
{
	char c;
	while ((c = pgm_read_byte(s++)) != 0)
	{
		Serial.print(c);
	}
}

void eEEPROMClass::showPgmStringLn(PGM_P s)
{
	showPgmString(s); 
	Serial.println();
}
*/

void eEEPROMClass::rrsWrite(s_rrstruct * rrstruct, uint16_t size, uint16_t count, void * user_struct)
{
	for (uint16_t i=0; i<count; i++)
	{
		s_rrstruct test;
		ADDR_T addr1 = (ADDR_T)&rrstruct[i];
		readData(addr1, &test, sizeof(test));
		if (test.current)
		{
			ADDR_T addr2 = (ADDR_T)&(rrstruct[(i+1)%count]);
			ADDR_T addr3 = (ADDR_T)&(rrstruct[(i+1)%count].data);

			writeData(addr2, &test, sizeof(test)); // mark next entry current
			writeData(addr3, user_struct, size);   // write new value

			test.current = 0;
			writeData(addr1, &test, sizeof(test)); // invalidate previous entry
			return;
		}
	}
}

void eEEPROMClass::rrsRead(s_rrstruct * rrstruct, uint16_t size, uint16_t count, void * user_struct)
{
	for (uint16_t i=0; i<count; i++)
	{
		s_rrstruct test;
		ADDR_T addr1 = (ADDR_T)&rrstruct[i];
		readData(addr1, &test, sizeof(test));
		if (test.current)
		{
			uint16_t addr2 = (ADDR_T)&(rrstruct[i].data);
			readData(addr2, user_struct, size);   // write new value
			return;
		}
	}
	memset(user_struct, 0, size);
}



eEEPROMClass eEEPROM;
