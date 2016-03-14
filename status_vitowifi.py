#!/usr/bin/env python

import datetime, httplib, urllib, urllib2, json, os

HOST="localhost"
PORT=1111
HOST="thing-vitowifi"
PORT=80

with open('thingspeak.vitowifi.key','r') as fd:
    APIKEY=fd.readline()
    APIKEY=APIKEY.strip()

response = urllib2.urlopen('http://%s:%s/api/status' % (HOST, PORT))
str    = response.read()

status = json.loads(str) 
uptime   = status.get("now", -1)
freeheap = status.get('freeheap',-1)
sensors  = status.get('sensors', [])
vent     = status.get('vent',{})
relative = vent.get('relative', -1)

sensorsFound = int(status.get('sensorsFound',0))
sensors = sensors[:sensorsFound]

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

