#!/usr/bin/env python

import sys, re


datatypes = {
  0:"READ-DATA", 
  1:"WRITE-DATA", 
  2:"INVALID-DATA", 
  3:"RESERVED", 
  4:"READ-ACK", 
  5:"WRITE-ACK", 
  6:"DATA-INVALID", 
  7:"UNK-DATAID" }

dataids = {
    # TODO: What does HBmean and where did I get this from?
    #2:"HB: Master configuration",
    2:"Master configuration",
    6:"Remote parameter flags",
   70:"Status V/H",
   71:"Control setpoint V/H",
   # otgw matrix only. not (yet) seen on own device:
   72:"Fault flags/code V/H",
   74:"Configuration/memberid V/H",
   77:"Relative ventilation",
   80:"Supply inlet temperature",
   82:"Exhaust inlet temperature",
   
   # TSP: "transparent slave parameter"
   89:"TSP setting V/H",
  126:"Master product version",
  127:"Slave product version"
}

LEVEL_OFF=0
LEVEL_REDUCED=1
LEVEL_NORMAL=2
LEVEL_PARTY=3

def setVentilation(level):
  send(com, "GW=1\n") 		# set to gateway mode
  send(com, "VS=%s\n" % level)	# ventilation setpoint override


def setPointToVentilationLevel(sp):
  if 0==sp: return "OFF"
  if 1==sp: return "LOW"
  if 2==sp: return "NORMAL"
  if 3==sp: return "HIGH"

def relativeToVentilationLevel(r):
  if   0==r: return "OFF"
  if  27==r: return "LOW"
  if  55==r: return "NORMAL"
  if 100==r: return "HIGH"


def rawValues(hi, lo):
  return "%3i %3i  x%02x x%02x  %s %s" % (hi, lo, hi, lo, format(hi,'08b').replace("0","."), format(lo,'08b').replace("0","."))



def decodeMessage(msg):
  D = False
  
  sender = msg[0]
  dword  = int(msg[1:], 16)  
  if D:
    print("%s %08x" % (sender, dword))

  parity = (0x80000000 & dword)>>31
  mtype  = (0x70000000 & dword)>>28
  spare  = (0x0f000000 & dword)>>24
  dataid = (0x00ff0000 & dword)>>16
  value  = (0x0000ffff & dword)
  name   = dataids.get(dataid)
  if D:
    print("parity: %01x" % parity)
    print("mtype:  %01x" % mtype)
    print("spare:  %01x" % spare)
    print("dataid: %02x" % dataid)
    print("value:  %04x" % value)
  
  hi = (0xff00 & value) >> 8
  lo = (0x00ff & value)

  datatype = datatypes[mtype]
  if 'T'==sender and datatype=='READ-DATA':
    pass # ignore... READ-ACK on this is more useful
  elif 'T'==sender and datatype=='WRITE-DATA':
    pass # same ... wait for WRITE-ACK
  elif 'B'==sender and datatype=="UNK-DATAID" and dataid in [126,127]:
    #print("**********")
    #print("%s\t%-14s  [ID: %3i] %-30s %3i %3i (x%02x x%02x)" % (msg, datatype, dataid, name, hi, lo, hi, lo))
    pass # "vitovent" does not know these. never did. never will.
  
  elif dataid in [89]: # TSP
    print("%s\t%-14s  [ID: %3i] %-30s TSP(%s) = %s" % (msg, datatype, dataid, name, "%2i"%hi, "%3i"%lo)) 
  elif dataid in [80,82]:
    temp = float(hi)+float(lo)/255
    print("%s\t%-14s  [ID: %3i] %-30s %2.2fC" % (msg, datatype, dataid, name, temp))
  elif dataid in [71]:
    level = setPointToVentilationLevel(lo)
    print("%s\t%-14s  [ID: %3i] %-30s %s"     % (msg, datatype, dataid, name, level))
  elif dataid in [77]:
    level = relativeToVentilationLevel(lo)
    print("%s\t%-14s  [ID: %3i] %-30s %s"     % (msg, datatype, dataid, name, level))
  elif name is not None:
    print("%s\t%-14s  [ID: %3i] %-30s %s"     % (msg, datatype, dataid, name, rawValues(hi,lo)))
  else:
    print("%s\t%-14s  [ID: %3i] %-30  %s"     % (msg, datatype, dataid, "??", rawValues(hi,lo)))
  
  if 0!=spare:
    print("*WARNING*: spare: %s" % spare)


def main(args):
  with open(args[1],"r") as fd:
    lines = fd.readlines()
    for line in lines:
      line = line.strip().upper()
      if ""==line:
        pass
      elif (re.match('[0-9]{2}:[0-9]{2}:[\.0-9]{9}[\t ]*[TB][0-9A-F]{8}.*', line)):
        #print
        #print(line[16:])
        line = line[16:25]        
        #print("line: '%s'" % line)
        decodeMessage(line)
      elif (re.match('[TB][0-9A-F]{8}.*', line)):
        decodeMessage(line)
      else:
        print("INFO: %s" % line)
        
if __name__ == "__main__":
  if len(sys.argv)<2:
    sys.stderr.write("USAGE: opentherm.py <file>\n")
    sys.exit(1)
    
  main(sys.argv)

  
