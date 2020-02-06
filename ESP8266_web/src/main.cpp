#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <HX711.h>
#include <index.h> 
#include <U8x8lib.h>
#include <time.h>
#include <locale.h>



//Wlan
#define LOCALE "de_DE.UTF-8"
#define WIFISSID "Ok-Home"
#define WIFIPASS "radiation2day"
#define MQTTSERVER "192.168.1.2"
#define MQTTPORT 1886
#define MQTTNAME "Filawaage"
#define MQTTUSER "Karim"
#define MQTTPASS "Octop4ka"
#define MQTTTOPIC "scale"
//Hardware
#define LED 2
// Sensor Settings
#define I2C_SDA D6
#define I2C_SCL D7
#define BME280 0x76
#define ALTITUDE 55
#define Containersize 10 // in liter
// Scale Settings
#define SCALE1_SDA D1
#define SCALE1_SCL D2
#define SCALE2_SDA D3
#define SCALE2_SCL D4
//display
#define DISPLAY_SDA TX
#define DISPLAY_SCL RX
	
ESP8266WebServer server(80);
U8X8_SSD1306_128X64_NONAME_SW_I2C display(DISPLAY_SCL, DISPLAY_SDA, U8X8_PIN_NONE); 
Adafruit_BME280 bme;
HX711 scale1;
HX711 scale2;
WiFiClient espClient;
PubSubClient client(espClient);

//Predefined Vars
float weight = 10000;
int reference = 0;
String webmessage ="";
unsigned long webmessagestart = 0;
int webmessageduration = 0;
int statuscal=0;
bool mqtton=false;
struct CAL {
  float factor1;
  float factor2;
  float offset1;
  float offset2;
};
CAL cal;
unsigned int coil = 0;
unsigned int maxspool = 0;
unsigned int maxmat = 0;
unsigned int maxsave = 0;
unsigned int active_spool = 0;
unsigned int active_mat = 0;
struct SPOOL {
  String name;
  int weight;
};
struct SPOOL spool[100];
struct MATERIAL {
  String name;
  float density;
};
struct MATERIAL mat[100];
struct SAVE {
  String color;
  int weight;
  unsigned int spool;
  unsigned int material;
  String updated;
};
struct SAVE save[100];

//===============================================================
// This routines loads persistent data
//===============================================================

bool loadConfig() {
  File configFile = SPIFFS.open("/config.txt", "r");
  if (!configFile) {
    webmessage="Konfiguration konnte nicht gefunden werden!";
    webmessagestart = millis();
    webmessageduration = 5000;
    return false;
  }
  size_t size = configFile.size();
  if (size > 1024) {
    webmessage="Konfigurationsdatei zu groß!";
    webmessagestart = millis();
    webmessageduration = 5000;
    return false;
  }
  mqtton = configFile.readStringUntil('\n').toInt();
  cal.factor1 = configFile.readStringUntil('\n').toFloat();
  cal.factor2 = configFile.readStringUntil('\n').toFloat();
  cal.offset1 = configFile.readStringUntil('\n').toFloat();
  cal.offset2 = configFile.readStringUntil('\n').toFloat();
  coil = configFile.readStringUntil('\n').toInt();
  maxspool = configFile.readStringUntil('\n').toInt();
  maxmat = configFile.readStringUntil('\n').toInt();
  maxsave = configFile.readStringUntil('\n').toInt();
  active_spool = configFile.readStringUntil('\n').toInt();
  active_mat = configFile.readStringUntil('\n').toInt();
  for (unsigned int i = 0;i<=maxspool;i++){
    spool[i].name = configFile.readStringUntil('\n');
    spool[i].name.trim();
    spool[i].weight = configFile.readStringUntil('\n').toInt();
  }    
  for (unsigned int i = 0;i<=maxmat;i++){
    mat[i].name = configFile.readStringUntil('\n');
    mat[i].name.trim();
    mat[i].density = configFile.readStringUntil('\n').toFloat();
  }    
  for (unsigned int i = 0;i<=maxsave;i++){
    save[i].color = configFile.readStringUntil('\n');
    save[i].color.trim();
    save[i].weight = configFile.readStringUntil('\n').toInt();
    save[i].spool = configFile.readStringUntil('\n').toInt();
    save[i].material = configFile.readStringUntil('\n').toInt();
    save[i].updated = configFile.readStringUntil('\n');
    save[i].updated.trim();
  }    
  configFile.close();
  webmessage="Konfiguration geladen!";
  webmessagestart = millis();
  webmessageduration = 5000; 
  return true;
}

//===============================================================
// This routine saves persistent data
//===============================================================

bool saveConfig() {
  File configFile = SPIFFS.open("/config.txt", "w");
  if (!configFile) {
    webmessage="Konfiguration konnte nicht gespeichert werden!";
    webmessagestart = millis();
    webmessageduration = 5000;
    return false;
  }
  configFile.println(mqtton);
  configFile.println(cal.factor1);
  configFile.println(cal.factor2);
  configFile.println(cal.offset1);
  configFile.println(cal.offset2);
  configFile.println(coil);
  configFile.println(maxspool);
  configFile.println(maxmat);
  configFile.println(maxsave);
  configFile.println(active_spool);
  configFile.println(active_mat);
  for (unsigned int i = 0;i<=maxspool;i++){
    configFile.println(spool[i].name);
    configFile.println(spool[i].weight);
  }    
  for (unsigned int i = 0;i<=maxmat;i++){
    configFile.println(mat[i].name);
    configFile.println(mat[i].density);
  }    
  for (unsigned int i = 0;i<=maxsave;i++){
    configFile.println(save[i].color);
    configFile.println(save[i].weight);
    configFile.println(save[i].spool);
    configFile.println(save[i].material);
    configFile.println(save[i].updated);
  }    
  configFile.close();
  webmessage="Konfiguration wurde gespeichert!";
  webmessagestart = millis();
  webmessageduration = 5000;
  return true;
}

//===============================================================
// This routines displays Webpage
//===============================================================
//Default Page
void handleRoot() {
 String s = MAIN_page; //Read HTML contents
 server.send(200, "text/html", s); //Send web page
}

//Display aktualisieren
void handleupdateDisplay() {
  float newweight = scale1.get_units(1) + scale2.get_units(1);
  if (weight < newweight - 10 || weight > newweight + 10) {
      weight = weight*0.2 + newweight*0.8;  
  }
  else {
    if (weight > newweight) {
      weight = weight*0.8 + newweight*0.2;  
    }
  }  
  if (weight<coil+spool[active_spool].weight) {
    weight=coil+spool[active_spool].weight;
  }
  float temperature = bme.readTemperature();
  float humidity = bme.readHumidity();
  float pressure = bme.readPressure();
  pressure = bme.seaLevelForAltitude(ALTITUDE,pressure);
  pressure = pressure/100.0F;
  float water = (humidity * Containersize * 13.24 * exp((17.62 * temperature) / (temperature + 243.12))) / (273.15 + temperature);
  float length=(weight-coil-spool[active_spool].weight) / mat[active_mat].density / 2.41;
  String str1 = "Fila:         ";
  String str=String(weight-coil-spool[active_spool].weight,2);
  str1.remove(7,str.length());
  str1 +=str;
  str1 += " g";
  display.drawString(0, 0, str1.c_str());
  str1 = "Laenge:       ";
  str=String(length,2);
  str1.remove(7,str.length());
  str1 +=str;
  str1 += " m";
  display.drawString(0, 2, str1.c_str());
  str1 = "Temp:         ";
  str=String(temperature,2);
  str1.remove(7,str.length());
  str1 +=str;
  str1 += " C";
  display.drawString(0, 3, str1.c_str());
  str1 = "Luft%:        ";
  str=String(humidity,2);
  str1.remove(7,str.length());
  str1 +=str;
  str1 += " %";
  display.drawString(0, 4, str1.c_str());
  str1 = "Wasser:       ";
  str=String(water,2);
  str1.remove(7,str.length());
  str1 +=str;
  str1 += "mg";
  display.drawString(0, 5, str1.c_str());
  if (mqtton) {
    char temp[10];
    dtostrf(weight-coil-spool[active_spool].weight, 10, 4, temp);
    client.publish(MQTTTOPIC, temp);
  }
}

//AJAX Webpage aktualisieren
void handleupdatePage() {
  float newweight = scale1.get_units(1) + scale2.get_units(1);
  if (weight < newweight - 10 || weight > newweight + 10) {
      weight = weight*0.2 + newweight*0.8;  
  }
  else {
    if (weight > newweight) {
      weight = weight*0.8 + newweight*0.2;  
    }
  }  
  if (weight<coil+spool[active_spool].weight) {
    weight=coil+spool[active_spool].weight;
  }
  float temperature = bme.readTemperature();
  float humidity = bme.readHumidity();
  float pressure = bme.readPressure();
  pressure = bme.seaLevelForAltitude(ALTITUDE,pressure);
  pressure = pressure/100.0F;
  float water = (humidity * Containersize * 13.24 * exp((17.62 * temperature) / (temperature + 243.12))) / (273.15 + temperature);
  float length=(weight-coil-spool[active_spool].weight) / mat[active_mat].density / 2.41;
  String data = "<table border='0'><tr><td>Aktuell genutzt:</td><td align='right'><b> ";
  data += mat[active_mat].name;
  data += " auf </b></td><td><b>";
  data += spool[active_spool].name;
  data += "</b></td></tr><tr><td>Gewicht: </td><td align='right'>";
  data += newweight;
  data += "</td><td>gramm</td></tr><tr><td>Filament: </td><td align='right'><b>";
  data += weight-coil-spool[active_spool].weight;
  data += "</b></td><td>gramm</td></tr><tr><td>Länge: </td><td align='right'><b>";
  data += length;
  data += "</b></td><td>m</td></tr><tr><td>Temperatur: </td><td align='right'>";
  data += temperature;
  data += "</td><td>&deg;C</td></tr><tr><td>Luftfeuchtigkeit: </td><td align='right'>";
  data += humidity;
  data += "</td><td>%</td></tr><tr><td>Wasser: </td><td align='right'>";
  data += water;
  data += "</td><td>milligramm</td></tr><tr><td>Luftdruck: </td><td align='right'>";
  data += pressure;
  data += "</td><td>hPa</td></tr></table>";
  if (webmessagestart) {
    if (millis()> webmessagestart + webmessageduration) {
      webmessage="&#160;";
      webmessagestart=0;
    }
    if (statuscal) {
      float a = scale1.get_units(10);
      float b = scale2.get_units(10);
      scale1.set_scale(a/(reference/2));
      scale2.set_scale(b/(reference/2));
      statuscal = 0;
      webmessage = "Waage wurde kalibriert.";
      webmessagestart = millis();
      webmessageduration = 5000;  
    }
  }
  data += "<span><font color=red>";
  data += webmessage;
  data += "</font></span>";
  server.send(200, "text/plane", data); 
}

//Settings
String loadsettings(){
  String settings = "<div id='cal'>Referenzgewicht: <input type='number' id='kali' value=''><br><button type='button' onclick='Scale(\x22kali\x22)'>Kalibrieren</button><button type='button' id='tare' onclick='Scale(\x22tare\x22)'>Auf Null</button><button type='button' id='save' onclick='Scale(\x22save\x22)'>Speichern</button><button type='button' id='load' onclick='Scale(\x22load\x22)'>Laden</button></div><br><br><div id='divcoil'><table border='0'><tr><td>Spindelgewicht: </td><td><input type='number' id='koil' value='";
  settings += coil;
  settings += "' style='background-color:#CCFF99;'><button type='button' onclick='Scale(\x22koil\x22)'>Speichern</button></td></tr></table></div><br><br><div id='divspool'><table border='0'><tr><td>Leerrolle</td><td colspan='2'>Gewicht</td><td><button type='button' onclick='manSpool(-1)'>+</button></td></tr>";
  for (unsigned int i = 0; i<=maxspool; i++){
    settings += "<tr><td><input type='text' id='spoolname";
    settings += i;
    if (i==active_spool) settings += "' style='background-color:#CCFF99;";
    settings += "' value='";
    settings += spool[i].name;
    settings += "'></td><td><input type='number' id='spoolweight";
    settings += i;
    settings += "' value='";
    settings += spool[i].weight;
    if (i==active_spool) settings += "' style='background-color:#CCFF99;";
    settings += "'></td><td><button type='button' onclick='saveSpool(";
    settings += i;
    settings += ")'>Speichern</button></td><td>";
    if (maxspool>0) {
      settings += "<button type='button' onclick='manSpool(";
      settings += i;
      settings += ")'>-</button>";
    }
    settings += "</td></tr>";
  }
  settings += "</table></div><br><br><div id='divmat'><table border='0'><tr><td>Material</td><td colspan='2'>Dichte (g/cm^3)</td><td><button type='button' onclick='manMat(-1)'>+</button></td></tr>";
  for (unsigned int i = 0; i<=maxmat; i++){
    settings += "<tr><td><input type='text' id='matname";
    settings += i;
    if (i==active_mat) settings += "' style='background-color:#CCFF99;";
    settings += "' value='";
    settings += mat[i].name;
    settings += "'></td><td><input type='number' pattern='[0-9]+([.,][0-9]+)?' step='0.01' id='matdensity";
    settings += i;
    settings += "' value='";
    settings += mat[i].density;
    if (i==active_mat) settings += "' style='background-color:#CCFF99;";
    settings += "'></td><td><button type='button' onclick='saveMat(";
    settings += i;
    settings += ")'>Speichern</button></td><td>";
    if (maxmat>0) {
      settings += "<button type='button' onclick='manMat(";
      settings += i;
      settings += ")'>-</button>";
    }
    settings += "</td></tr>";
  }
  settings += "</table></div><br><br><div id='divsav'><table border='0'><tr><td>Farbe</td><td colspan='8'>Info</td><td><button type='button' onclick='manSave(-1)'>+</button></td></tr>";
  for (unsigned int i = 0; i<=maxsave; i++){
    settings += "<tr><td><input type='text' id='savecolor";
    settings += i;
    settings += "' value='";
    settings += save[i].color;
    settings += "'></td><td align='right'>";
    settings += save[i].weight;
    settings += "</td><td>Gramm</td><td>";
    if (save[i].material>maxmat) {
      settings += "unbekanntes Material";
    }
    else {
      settings += mat[save[i].material].name;
    }
    settings += "</td><td>auf</td><td>";
    if (save[i].spool>maxspool) {
      settings += "unbekannter Spule";
    }
    else {
      settings += spool[save[i].spool].name;
    }
    settings += ".</td><td> Erstellt am: </td><td>";
    settings += save[i].updated;
    settings += "</td><td><button type='button' onclick='saveSave(";
    settings += i;
    settings += ")'>Speichern</button></td><td>";
    if (maxsave>0) {
      settings += "<button type='button' onclick='manSave(";
      settings += i;
      settings += ")'>-</button>";
    }
    settings += "</td></tr>";
  }
  settings += "</table></div><br><br><div id='mqtt'><button type='button' onclick='settings(\x22";
  if (mqtton) {
    settings += "mqtton\x22)' id='mqtton'>MQTT an";
  }
  else {
    settings += "mqttoff\x22)' id='mqttoff'>MQTT aus";
  }
  settings += "</button></div>";
  return settings;
}

//Show Settings
void handleshowSettings(){
  String stat = server.arg("stat").c_str(); 
  if (stat=="show") {
    server.send(200, "text/plane", loadsettings()); 
  }
  else if (stat=="mqtton"){
    mqtton=false;
    saveConfig();
    server.send(200, "text/plane", loadsettings()); 
  }
  else if (stat=="mqttoff"){
    mqtton=true;
    saveConfig();
    server.send(200, "text/plane", loadsettings()); 
  }
}

//===============================================================
// This routines manages defined empty spools
//===============================================================

void handlesaveSpool() {
  unsigned int spoolnr = atoi(server.arg("spoolnr").c_str()); 
  spool[spoolnr].name = server.arg("spoolname").c_str(); 
  spool[spoolnr].weight = atoi(server.arg("spoolweight").c_str()); 
  saveConfig();
  active_spool=spoolnr;
  weight=10000;
  server.send(200, "text/plane", loadsettings()); 
}

void handlemanSpool(){
  int spoolnr = atoi(server.arg("spoolnr").c_str()); 
  if (spoolnr<0){
    maxspool++;
    spool[maxspool].name="Default";
    spool[maxspool].weight=100;  
  }
  else { 
    for (unsigned int i = spoolnr; i < maxspool; i++){
      spool[i].name=spool[i+1].name;
      spool[i].weight=spool[i+1].weight;  
    }
    for (unsigned int i = 0; i < maxsave; i++){
      if (save[i].spool==std::abs(spoolnr)) {
        save[i].spool=1000;
      }
      else {
        if (save[i].spool>std::abs(spoolnr)) {
          save[i].spool=save[i].spool-1;
        }
      }
    }
    maxspool--;
    if (active_spool>maxspool) active_spool=0;
  }
  saveConfig();
  server.send(200, "text/plane", loadsettings()); 
}

//===============================================================
// This routines manages defined materials
//===============================================================

void handlesaveMat() {
  unsigned int matnr = atoi(server.arg("matnr").c_str()); 
  mat[matnr].name = server.arg("matname").c_str(); 
  mat[matnr].density = atof(server.arg("matdensity").c_str()); 
  saveConfig();
  active_mat=matnr;
  weight=10000;
  server.send(200, "text/plane", loadsettings()); 
}

void handlemanMat(){
  int matnr = atoi(server.arg("matnr").c_str()); 
  if (matnr<0){
    maxmat++;
    mat[maxmat].name="Default";
    mat[maxmat].density=1.3;  
  }
  else { 
    for (unsigned int i = matnr; i < maxmat; i++){
      mat[i].name=mat[i+1].name;
      mat[i].density=mat[i+1].density;  
    }
    for (unsigned int i = 0; i < maxsave; i++){
      if (save[i].material==std::abs(matnr)) {
        save[i].material=1000;
      }
      else {
        if (save[i].material>std::abs(matnr)) {
          save[i].material=save[i].material-1;
        }
      }
    }
    maxmat--;
    if (active_mat>maxmat) active_mat=0;
  }
  saveConfig();
  server.send(200, "text/plane", loadsettings()); 
}

//===============================================================
// This routines manages saved spools
//===============================================================

void handlesaveSave() {
  unsigned int savenr = atoi(server.arg("savenr").c_str()); 
  save[savenr].color = server.arg("savecolor").c_str(); 
  save[savenr].weight = atoi(server.arg("saveweight").c_str()); 
  save[savenr].spool = atoi(server.arg("savespool").c_str()); 
  save[savenr].material = atoi(server.arg("savematerial").c_str()); 
  saveConfig();
  weight=10000;
  server.send(200, "text/plane", loadsettings()); 
}

void handlemanSave(){
  int savenr = atoi(server.arg("savenr").c_str()); 
  if (savenr<0){
    maxsave++;
    save[maxsave].color="Default";
    save[maxsave].weight=weight-coil-spool[active_spool].weight; 
    save[maxsave].spool=active_spool;  
    save[maxsave].material=active_mat; 
    setlocale(LC_ALL, "de_DE.utf8");
    char chardate[80]; 
    time_t now;
    time (&now);
    strftime (chardate, sizeof(chardate), "%F %T", localtime(&now));  
    save[maxsave].updated=chardate;
  }
  else { 
    for (unsigned int i = savenr; i < maxsave; i++){
      save[i].color=save[i+1].color;
      save[i].weight=save[i+1].weight;  
      save[i].spool=save[i+1].spool;
      save[i].material=save[i+1].material;
      save[i].updated=save[i+1].updated;
    }
    maxsave--;
  }
  saveConfig();
  server.send(200, "text/plane", loadsettings()); 
}

//===============================================================
// This routines handels Scalefunktions
//===============================================================

void handleScale() {
  String varname = server.arg("varname").c_str(); 
  String varval = server.arg("varval").c_str(); 

  if (varname=="save") {
    cal.factor1 = scale1.get_scale();
    cal.factor2 = scale2.get_scale();
    cal.offset1 = scale1.get_offset();
    cal.offset2 = scale2.get_offset();
    saveConfig();
  }
  else if (varname=="load") {
    scale1.set_scale(cal.factor1);
    scale2.set_scale(cal.factor2);
    scale1.set_offset(cal.offset1);
    scale2.set_offset(cal.offset2);
    webmessage="Konfiguration geladen!";
    webmessagestart = millis();
    webmessageduration = 5000; 
  }
  else if (varname =="kali") {
    reference = atoi(varval.c_str());
    if (reference!=0) {
      statuscal=1;
      scale1.set_scale();
      scale2.set_scale();
      webmessage = "Kalibriere ... bitte Referenzgewicht auflegen!";
      webmessagestart = millis();
      webmessageduration = 5000;
    }
    else {
      webmessage = "Vor dem Kalibrieren: Waage leeren, auf Null setzen sowie ein güliges Referenzgewicht angeben und auflegen!";
      webmessagestart = millis();
      webmessageduration = 5000;     
    }
  }
  else if (varname=="tare") {
    scale1.tare(10);
    scale2.tare(10);
    webmessage="Waage auf Null gesetzt!";
    webmessagestart = millis();
    webmessageduration = 5000;
  }
  else if (varname=="koil"){
    coil=atoi(varval.c_str());
    saveConfig();
    weight=10000;
  }
}

//===============================================================
// This routines opens Wifi and MQTT Connections
//===============================================================

void connectToWifi()
{
  WiFi.enableSTA(true);
  WiFi.begin(WIFISSID, WIFIPASS);
  int i=4;
  while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        display.drawString(i++, 0, ".");
    }
  server.on("/", handleRoot);
  server.on("/manMat", handlemanMat);
  server.on("/manSpool", handlemanSpool);
  server.on("/manSave", handlemanSave);
  server.on("/saveSpool", handlesaveSpool);
  server.on("/saveMat", handlesaveMat);
  server.on("/saveSave", handlesaveSave);
  server.on("/Scale", handleScale);
  server.on("/showSettings", handleshowSettings);
  server.on("/updatePage", handleupdatePage);
  server.begin();
  pinMode(LED,OUTPUT); 
  //SET TIMEZONE
  configTime(0, 0, "0.de.pool.ntp.org", "0.europe.pool.ntp.org");
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  client.setServer(MQTTSERVER, MQTTPORT);
  if (mqtton) {
     if (client.connect(MQTTNAME, MQTTUSER, MQTTPASS )) {
       display.drawString(0, 6, "MQTT verbunden.");  
     } else {
      display.drawString(0, 6, "MQTT Fehler!");
     }
  }
  delay(2000);
  String localip = String(WiFi.localIP()[0])+"."+String(WiFi.localIP()[1])+"."+String(WiFi.localIP()[2])+"."+String(WiFi.localIP()[3]);
  display.clear();
  display.drawString(0, 7, localip.c_str());
}

//===============================================================
// This routines initialises Sensors
//===============================================================

void initSensor()
{
  display.drawString(1, 0, ".");
  scale1.begin(SCALE1_SDA, SCALE1_SCL, 128);
  if (scale1.wait_ready_timeout(1000)) {
    scale1.get_units(1);
    display.drawString(0, 3, "Waage1 gefunden.");
  }
  else {
    display.drawString(0, 3, "Waage 1 fehlt!");
    delay(1000);
  }
  display.drawString(2, 0, ".");
  scale2.begin(SCALE2_SDA, SCALE2_SCL, 128);
  if (scale2.wait_ready_timeout(1000)) {
    scale2.get_units(1);
    display.drawString(0, 4, "Waage2 gefunden.");
  }
  else {
    display.drawString(0, 4, "Waage 2 fehlt!");
    delay(1000);
  }
  scale1.set_scale(cal.factor1);
  scale2.set_scale(cal.factor2);
  scale1.set_offset(cal.offset1);
  scale2.set_offset(cal.offset2);  
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);
  bool status = bme.begin(BME280);
  display.drawString(3, 0, ".");
  if (status) {
    display.drawString(0, 5, "Sensor gefunden.");
  }
  else {
    display.drawString(0, 5, "Sensor fehlt!");
    delay(1000);
  }
}

//==============================================================
//                  SETUP
//==============================================================
void setup(void){
  pinMode(1, FUNCTION_3);
  pinMode(3, FUNCTION_3);
  pinMode(1, INPUT);  
  pinMode(3, INPUT);  
  display.begin();
  display.setFont(u8x8_font_chroma48medium8_r);
  display.drawString(0, 0, ".");
  if (!SPIFFS.begin()) {
    display.drawString(0, 2, "Ladefehler!");
    delay(1000);
  }
  else display.drawString(0, 2, "Daten geladen.");
  loadConfig();
  initSensor();
  connectToWifi();
  handleRoot();
}
//==============================================================
//                     LOOP
//==============================================================
void loop(void){
  server.handleClient();          //Handle client requests
  handleupdateDisplay();
}
