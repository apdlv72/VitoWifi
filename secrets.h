#ifndef SECRETS_H
#define SECRETS_H

#define NETWORK  "your_ssid"
#define PASSWORD "your_password"

// This is just an example of a URL that is supposed to return the desired ventilation level.
// In my case, this is a google script that connects to Netato API anc computes this level.
// When requested, the script will return a plain text document with a "ventLevl=X" on a single line.
// When your requirements differ, make sure to change the conditions in the function "responseBodyComplete".
const char * UPDATE_URL = "https://script.google.com/macros/s/your_google_key/exec";

// An URL that gets called in case of alerts. 
const char * ALERT_URL  = "https://api.callmebot.com/whatsapp.php?phone=your_number&apikey=aabbcc&text=";

// Send maintenance messages (e.g. check filter, reset gateway) also to this number
const char * MAINTENANCE_URL  = "https://api.callmebot.com/whatsapp.php?phone=your_wifes_number&apikey=xxyyzz&text=";

#endif
