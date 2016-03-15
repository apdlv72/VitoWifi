#!/usr/bin/env python

import datetime, httplib, urllib, urllib2, json, os

# Host name (or IP address) and port of the ESP device with the VitiWifi sketch. 
VITOWIFI_HOST="thing-vitowifi"
VITOWIFI_PORT=80

# Write thingspeak API key to a file called 'thingspeak.vitowifi.key'
# in the same directory. 
with open('thingspeak.vitowifi.key','r') as fd:
    APIKEY=fd.readline()
    APIKEY=APIKEY.strip()

# Get current status as a json document and extract information we need. 
response = urllib2.urlopen('http://%s:%s/api/status' % (VITOWIFI_HOST, VITOWIFI_PORT))
str    = response.read()

status = json.loads(str) 
uptime   = status.get("now",      -1) # uptime in millis
freeheap = status.get('freeheap', -1) # free heap bytes
sensors  = status.get('sensors',  []) # sensor temperatures mCelsius
vent     = status.get('vent',     {}) # ventilation status
relative = vent.get('relative', -1)   # relative fan speed percentile: 0, 27, 55, 100%

# get number of sensors found and truncate array of measures 
sensorsFound = int(status.get('sensorsFound',0))
sensors = sensors[:sensorsFound]

# scale mCelsius to Celsius
uptime /= 60000 # millis->minutes
for i in range(len(sensors)): sensors[i]=0.01*sensors[i]

params = urllib.urlencode({
            'field1':relative, 
            'field2':uptime, 
            'field3':freeheap, 
            'field4':sensors[1],
            'field5':sensors[2],
            'field6':sensors[3],
            'field7':sensors[4],
            'field8':sensors[0]
        })

headers = {
           "Content-type": "application/x-www-form-urlencoded",
           "Accept": "text/plain",
           "X-THINGSPEAKAPIKEY": APIKEY
           }

conn = httplib.HTTPConnection("api.thingspeak.com:80")
conn.request("POST", "/update", params, headers)
response = conn.getresponse()
print datetime.datetime.now(), response.status, response.reason

