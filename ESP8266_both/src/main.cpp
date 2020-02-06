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
//Hardware
#define LED 2
//Pin Settings
#define I2C_SDA D1
#define I2C_SCL D2
#define SCALE_SDA D3
#define SCALE_SCL D4
#define HEATERPIN D5
// Sensor Settings
#define BME280 0x76

// Serial Read
const byte numChars = 32;
char receivedChars[numChars];
char tempChars[numChars];
boolean newData = false;
	
ESP8266WebServer server(80);
U8X8_SSD1306_128X64_NONAME_SW_I2C display(I2C_SCL, I2C_SDA, U8X8_PIN_NONE); 
Adafruit_BME280 bme;
HX711 scale;
WiFiClient espClient;
PubSubClient client(espClient);

//Predefined Vars
float weight = 10000;
float reference = 0;
float spoolweight = 100;
String webmessage ="";
unsigned long webmessagestart = 0;
int webmessageduration = 0;
int statuscal=0;
float factor;
float offset;
float tempfloat;

float coil = 100;
float tara = 200;
float density = 0;
float containersize = 10.6; // in liter
float altitude = 59; //in metern
bool wifi = false;
float heatertemp = 0;

String wifissid = "";
String wifipasswd = "";
String mqttserver = "";
unsigned int mqttport = 1886;
String mqttname = "Filamentscale";
String mqttuser =  "";
String mqttpass = "";
String mqtttopic = "Weight";

bool mqtton=false;
int maxspool = 0;
int active_spool = 0;
struct SPOOL {
  String name;
  int weight;
};
struct SPOOL spool[100];
int maxmat = 0;
int active_mat = 0;
struct MATERIAL {
  String name;
  float density;
};
struct MATERIAL mat[100];
unsigned int maxsave = 0;
struct SAVE {
  String color;
  int weight;
  int spool;
  int material;
  String updated;
};
struct SAVE save[100];

//===============================================================
// This routines loads persistent data
//===============================================================

bool loadConfig() {
  File configFile = SPIFFS.open("/config.txt", "r");
  if (!configFile) {
    return false;
  }
  factor = configFile.readStringUntil('\n').toFloat();
  offset = configFile.readStringUntil('\n').toFloat();
  containersize = configFile.readStringUntil('\n').toFloat();
  altitude = configFile.readStringUntil('\n').toFloat();
  coil = configFile.readStringUntil('\n').toFloat();
  spoolweight = configFile.readStringUntil('\n').toFloat();
  density = configFile.readStringUntil('\n').toInt();
  wifi = configFile.readStringUntil('\n').toInt();
  wifissid = configFile.readStringUntil('\n');
  wifissid.trim();
  wifipasswd = configFile.readStringUntil('\n');
  wifipasswd.trim();
  mqtton = configFile.readStringUntil('\n').toInt();
  active_spool = configFile.readStringUntil('\n').toInt();
  active_mat = configFile.readStringUntil('\n').toInt();
  configFile.close();
  if (wifi) {
    File spoolFile = SPIFFS.open("/spool.txt", "r");
    if (!spoolFile) {
      return false;
    }
    maxspool = spoolFile.readStringUntil('\n').toInt();
    for (int i = 0;i<=maxspool;i++){
      spool[i].name = spoolFile.readStringUntil('\n');
      spool[i].name.trim();
      spool[i].weight = spoolFile.readStringUntil('\n').toInt();
    }    
    spoolFile.close();
    File matFile = SPIFFS.open("/mat.txt", "r");
    if (!matFile) {
      return false;
    }
    maxmat = matFile.readStringUntil('\n').toInt();
    for (int i = 0;i<=maxmat;i++){
      mat[i].name = matFile.readStringUntil('\n');
      mat[i].name.trim();
      mat[i].density = matFile.readStringUntil('\n').toFloat();
    }    
    matFile.close();
    File saveFile = SPIFFS.open("/save.txt", "r");
    if (!saveFile) {
      return false;
    }
    maxsave = saveFile.readStringUntil('\n').toInt();
    for (unsigned int i = 0;i<=maxsave;i++){
      save[i].color = saveFile.readStringUntil('\n');
      save[i].color.trim();
      save[i].weight = saveFile.readStringUntil('\n').toInt();
      save[i].spool = saveFile.readStringUntil('\n').toInt();
      save[i].material = saveFile.readStringUntil('\n').toInt();
      save[i].updated = saveFile.readStringUntil('\n');
      save[i].updated.trim();
    }    
    saveFile.close();
    File mqttFile = SPIFFS.open("/mqtt.txt", "r");
    if (!mqttFile) {
      return false;
    }
    mqttserver = mqttFile.readStringUntil('\n');
    mqttserver.trim();
    mqttport = mqttFile.readStringUntil('\n').toInt();
    mqttname = mqttFile.readStringUntil('\n');
    mqttname.trim();
    mqttuser = mqttFile.readStringUntil('\n');
    mqttuser.trim();
    mqttpass = mqttFile.readStringUntil('\n');
    mqttpass.trim();
    mqtttopic = mqttFile.readStringUntil('\n');
    mqtttopic.trim();
    mqttFile.close();
    webmessage="Config loaded!";
    webmessagestart = millis();
    webmessageduration = 5000; 
    if (active_spool!=-1 and active_mat!=-1) {
      tara=coil+spool[active_spool].weight;
      density=mat[active_mat].density;
    }
    else {
      tara=coil+spoolweight;
    }
  }
  return true;
}

//===============================================================
// This routine saves persistent data
//===============================================================

bool saveConfig() {
  File configFile = SPIFFS.open("/config.txt", "w");
  if (!configFile) {
    return false;
  }
  configFile.println(factor);
  configFile.println(offset);
  configFile.println(containersize);
  configFile.println(altitude);
  configFile.println(coil);
  configFile.println(spoolweight);
  configFile.println(density);
  configFile.println(wifi);  
  configFile.println(wifissid);
  configFile.println(wifipasswd);
  configFile.println(mqtton);
  configFile.println(active_spool);
  configFile.println(active_mat);
  configFile.close();
  if (wifi) {
    File spoolFile = SPIFFS.open("/spool.txt", "w");
    if (!spoolFile) {
      return false;
    }
    spoolFile.println(maxspool);
    for (int i = 0;i<=maxspool;i++){
      spoolFile.println(spool[i].name);
      spoolFile.println(spool[i].weight);
    }    
    spoolFile.close();
    File matFile = SPIFFS.open("/mat.txt", "w");
    if (!matFile) {
      return false;
    }
    matFile.println(maxmat);
    for (int i = 0;i<=maxmat;i++){
      matFile.println(mat[i].name);
      matFile.println(mat[i].density);
    }    
    matFile.close();
    File saveFile = SPIFFS.open("/save.txt", "w");
    if (!saveFile) {
      return false;
    }
    saveFile.println(maxsave);
    for (unsigned int i = 0;i<=maxsave;i++){
      saveFile.println(save[i].color);
      saveFile.println(save[i].weight);
      saveFile.println(save[i].spool);
      saveFile.println(save[i].material);
      saveFile.println(save[i].updated);
    }    
    saveFile.close();
    File mqttFile = SPIFFS.open("/mqtt.txt", "w");
    if (!mqttFile) {
      return false;
    }
    mqttFile.println(mqttserver);
    mqttFile.println(mqttport);
    mqttFile.println(mqttname);
    mqttFile.println(mqttuser);
    mqttFile.println(mqttpass);
    mqttFile.println(mqtttopic);
    mqttFile.close();
    webmessage="Config saved!";
    webmessagestart = millis();
    webmessageduration = 5000;
  }
  if (active_spool!=-1 and active_mat!=-1) {
    tara=coil+spool[active_spool].weight;
    density=mat[active_mat].density;
  }
  else {
    tara=coil+spoolweight;
  }
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

//Displayupdate
void handleupdateDisplay() {
  float newweight = 0;
  if (scale.is_ready()) {
    newweight = scale.get_units(1);
  }
  if (weight < newweight - 10 || weight > newweight + 10) {
      weight = weight*0.2 + newweight*0.8;  
  }
  else {
    if (weight > newweight) {
      weight = weight*0.8 + newweight*0.2;  
    }
  }  
  if (weight<tara) {
    weight=tara;
  }
  float temperature = bme.readTemperature();
  float humidity = bme.readHumidity();
  float pressure = bme.readPressure();
  pressure = bme.seaLevelForAltitude(altitude,pressure);
  pressure = pressure/100.0F;
  float water = (humidity * containersize * 13.24 * exp((17.62 * temperature) / (temperature + 243.12))) / (273.15 + temperature);
  float length=(weight-tara) / density / 2.41;
  String str1 = "Fila:         ";
  String str2 = "[U:";
  str2 += String(newweight,2);
  str2 += ";W:";
  String str=String(weight-tara,2);
  str1.remove(7,str.length());
  str1 +=str;
  str1 += " g";
  str2 +=str;
  str2 += ";L:";
  display.drawString(0, 0, str1.c_str());
  str1 = "Length:       ";
  str=String(length,2);
  str1.remove(7,str.length());
  str1 +=str;
  str1 += " m";
  str2 +=str;
  str2 += ";T:";
  display.drawString(0, 2, str1.c_str());
  str1 = "Temp:         ";
  str=String(temperature,2);
  str1.remove(7,str.length());
  str1 +=str;
  str1 += " C";
  str2 +=str;
  str2 += ";R:";
  display.drawString(0, 3, str1.c_str());
  str1 = "Humi%:        ";
  str=String(humidity,2);
  str1.remove(7,str.length());
  str1 +=str;
  str1 += " %";
  str2 +=str;
  str2 += ";A:";
  display.drawString(0, 4, str1.c_str());
  str1 = "Water:        ";
  str=String(water,2);
  str1.remove(7,str.length());
  str1 +=str;
  str1 += "mg";
  display.drawString(0, 5, str1.c_str());
  str2 +=str;
  str2 += ";P:";
  str2 += String(pressure,2);
  str2 += "]";
  Serial.println(str2);
  if (heatertemp>0){
    if (heatertemp>temperature) {
        digitalWrite(HEATERPIN, LOW);
    }
    else {
        digitalWrite(HEATERPIN, HIGH);
    }
  }
  if (mqtton) {
    char temp[10];
    dtostrf(weight-tara, 10, 4, temp);
    client.publish(mqtttopic.c_str(), temp);
  }
}

//AJAX Webpageupdate
void handleupdatePage() {
  float newweight = 0;
  if (scale.is_ready()) {
    newweight = scale.get_units(1);
  }
  if (weight < newweight - 10 || weight > newweight + 10) {
      weight = weight*0.2 + newweight*0.8;  
  }
  else {
    if (weight > newweight) {
      weight = weight*0.8 + newweight*0.2;  
    }
  }  
  if (weight<tara) {
    weight=tara;
  }
  float temperature = bme.readTemperature();
  float humidity = bme.readHumidity();
  float pressure = bme.readPressure();
  pressure = bme.seaLevelForAltitude(altitude,pressure);
  pressure = pressure/100.0F;
  float water = (humidity * containersize * 13.24 * exp((17.62 * temperature) / (temperature + 243.12))) / (273.15 + temperature);
  float length=(weight-tara) / density / 2.41;
  String data = "<table border='0'><tr><td>Current Spool:</td><td align='right'><b> ";
  if (active_mat<0) {
    data += "Unknown material";
  }
  else {
    data += mat[active_mat].name;
  }
  data += " on </b></td><td><b>";
  if (active_spool<0) {
    data += "unknown spool";
  }
  else {
    data += spool[active_spool].name;
  }
  data += "</b></td></tr><tr><td>Weight: </td><td align='right'>";
  data += newweight;
  data += "</td><td>gram</td></tr><tr><td>Filament: </td><td align='right'><b>";
  data += weight-tara;
  data += "</b></td><td>gram</td></tr><tr><td>Length: </td><td align='right'><b>";
  data += length;
  data += "</b></td><td>m</td></tr><tr><td>Temperature: </td><td align='right'>";
  data += temperature;
  data += "</td><td>&deg;C</td></tr><tr><td>Humidity: </td><td align='right'>";
  data += humidity;
  data += "</td><td>%</td></tr><tr><td>Water: </td><td align='right'>";
  data += water;
  data += "</td><td>milligram</td></tr><tr><td>Pressure: </td><td align='right'>";
  data += pressure;
  data += "</td><td>hPa</td></tr></table>";
  if (webmessagestart) {
    if (millis()> webmessagestart + webmessageduration) {
      webmessage="&#160;";
      webmessagestart=0;
    }
    if (statuscal) {
      float a = scale.get_units(10);
      scale.set_scale(a/reference);
      statuscal = 0;
      webmessage = "Scale calibrated.";
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
  String settings = "<div id='cal'Referenceweight: <input type='number' id='kali' value=''><br><button type='button' onclick='Scale(\"kali\")'>Calibrate</button><button type='button' id='tare' onclick='Scale(\"tare\")'>Set to Zero</button><button type='button' id='save' onclick='Scale(\"save\")'>Save</button><button type='button' id='load' onclick='Scale(\"load\")'>Load</button></div><br><br><div id='divcoil'><table border='0'><tr><td>Coilweight: </td><td><input type='number' id='koil' value='";
  settings += coil;
  settings += "' style='background-color:#CCFF99;'><button type='button' onclick='Scale(\"koil\")'>Save</button></td></tr><tr><td>Containersize in l: </td><td><input type='number' id='cont' value='";
  settings += containersize;
  settings += "' style='background-color:#CCFF99;'><button type='button' onclick='Scale(\"cont\")'>Save</button></td></tr><tr><td>Altitude above zero: </td><td><input type='number' id='alt' value='";
  settings += altitude;
  settings += "' style='background-color:#CCFF99;'><button type='button' onclick='Scale(\"alt\")'>Save</button></td></tr><tr><td>Spoolweight: </td><td><input type='number' value='";
  settings += tara-coil;
  if (active_mat!=-1 and active_spool!=-1) {
    settings += "' style='background-color:#CCFF99;";  
  }
  settings += "'></td></tr><tr><td>Density: </td><td><input type='number' value='";
  settings += density;
  if (active_mat!=-1 and active_spool!=-1) {
    settings += "' style='background-color:#CCFF99;";  
  }
  settings += "'></td></tr></table></div><br><br><div id='divspool'><table border='0'><tr><td>Spool Name</td><td colspan='2'>Empty Spool Weight</td><td><button type='button' onclick='manSpool(-1)'>+</button></td></tr>";
  for (int i = 0; i<=maxspool; i++){
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
    settings += ")'>Save</button></td><td>";
    if (maxspool>0) {
      settings += "<button type='button' onclick='manSpool(";
      settings += i;
      settings += ")'>-</button>";
    }
    settings += "</td></tr>";
  }
  settings += "</table></div><br><br><div id='divmat'><table border='0'><tr><td>Material Name</td><td colspan='2'>Density (g/cm^3)</td><td><button type='button' onclick='manMat(-1)'>+</button></td></tr>";
  for (int i = 0; i<=maxmat; i++){
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
    settings += ")'>Save</button></td><td>";
    if (maxmat>0) {
      settings += "<button type='button' onclick='manMat(";
      settings += i;
      settings += ")'>-</button>";
    }
    settings += "</td></tr>";
  }
  settings += "</table></div><br><br><div id='divsav'><table border='0'><tr><td>Color</td><td colspan='8'>Info</td><td><button type='button' onclick='manSave(-1)'>+</button></td></tr>";
  for (unsigned int i = 0; i<=maxsave; i++){
    settings += "<tr><td><input type='text' id='savecolor";
    settings += i;
    settings += "' value='";
    settings += save[i].color;
    settings += "'></td><td align='right'>";
    settings += save[i].weight;
    settings += "</td><td>gram</td><td>";
    if (save[i].material>maxmat) {
      settings += "unknown material";
    }
    else {
      settings += mat[save[i].material].name;
    }
    settings += "</td><td>on</td><td>";
    if (save[i].spool>maxspool) {
      settings += "unknown spool";
    }
    else {
      settings += spool[save[i].spool].name;
    }
    settings += ".</td><td> Saved on: </td><td>";
    settings += save[i].updated;
    settings += "</td><td><button type='button' onclick='saveSave(";
    settings += i;
    settings += ")'>Save</button></td><td>";
    if (maxsave>0) {
      settings += "<button type='button' onclick='manSave(";
      settings += i;
      settings += ")'>-</button>";
    }
    settings += "</td></tr>";
  }
  settings += "</table></div><br><br><div id='mqtt'><button type='button' onclick='settings(\"";
  if (mqtton) {
    settings += "mqtton\")' id='mqtton'>MQTT on</button></div><div><table border='0'><tr><td>Mqtt Server IP</td><td><input type='text' id='mqttserver' value='";
    settings += mqttserver;
    settings += "'></td></tr><tr><td>Mqtt Port</td><td><input type='text' id='mqttport' value='";
    settings += mqttport;
    settings += "'></td></tr><tr><td>Mqtt name</td><td><input type='text' id='mqttname' value='";
    settings += mqttname;
    settings += "'></td></tr><tr><td>Mqtt user</td><td><input type='text' id='mqttuser' value='";
    settings += mqttuser;
    settings += "'></td></tr><tr><td>Mqtt Password</td><td><input type='password' id='mqttpass' value='";
    settings += mqttpass;
    settings += "'></td></tr><tr><td>Mqtt Topic</td><td><input type='text' id='mqtttopic' value='";
    settings += mqtttopic;
    settings += "'></td></tr><tr><td colspan='2'><button type='botton' onclick='saveMqtt()'>Save</button></td></tr></table></div>";
  }
  else {
    settings += "mqttoff\")' id='mqttoff'>MQTT off</button></div>";
  }
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
    for (int i = spoolnr; i < maxspool; i++){
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
    for (int i = matnr; i < maxmat; i++){
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
    save[maxsave].weight=weight-tara; 
    save[maxsave].spool=active_spool;  
    save[maxsave].material=active_mat; 
    setlocale(LC_ALL, LOCALE);
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
// This routines manages Mqtt settings
//===============================================================

void handlesaveMqtt() {
  mqttserver = server.arg("mqttserver").c_str(); 
  mqttport = atoi(server.arg("mqttport").c_str());
  mqttname = server.arg("mqttname").c_str(); 
  mqttuser =  server.arg("mqttuser").c_str(); 
  mqttpass = server.arg("mqttpass").c_str(); 
  mqtttopic = server.arg("mqtttopic").c_str(); 
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
    factor = scale.get_scale();
    offset = scale.get_offset();
    saveConfig();
  }
  else if (varname=="load") {
    scale.set_scale(factor);
    scale.set_offset(offset);
    webmessage="Config loaded!";
    webmessagestart = millis();
    webmessageduration = 5000; 
  }
  else if (varname =="kali") {
    reference = atof(varval.c_str());
    if (reference!=0) {
      statuscal=1;
      scale.set_scale();
      webmessage = "Calibrating ... please install referenceweight!";
      webmessagestart = millis();
      webmessageduration = 5000;
    }
    else {
      webmessage = "Bevor calibration: Set empty scale to zero. Then install and enter known reference weight!";
      webmessagestart = millis();
      webmessageduration = 5000;     
    }
  }
  else if (varname=="tare") {
    scale.tare(10);
    webmessage="Scale set to zero!";
    webmessagestart = millis();
    webmessageduration = 5000;
  }
  else if (varname=="koil"){
    coil=atof(varval.c_str());
    saveConfig();
    weight=10000;
  }
  else if (varname=="cont"){
    containersize=atof(varval.c_str());
    saveConfig();
  }
  else if (varname=="alt"){
    altitude=atof(varval.c_str());
    saveConfig();
  }
}

//===============================================================
// This routines read from Serial Connection
//===============================================================

void recvWithStartEndMarkers() {
    static boolean recvInProgress = false;
    static byte ndx = 0;
    char startMarker = '<';
    char endMarker = '>';
    char rc;
    while (Serial.available() > 0 && newData == false) {
        rc = Serial.read();
        if (recvInProgress == true) {
            if (rc != endMarker) {
                receivedChars[ndx] = rc;
                ndx++;
                if (ndx >= numChars) {
                    ndx = numChars - 1;
                }
            }
            else {
                receivedChars[ndx] = '\0'; // terminate the string
                recvInProgress = false;
                ndx = 0;
                newData = true;
            }
        }
        else if (rc == startMarker) {
            recvInProgress = true;
        }
    }
}

void parseData() {      // split the data into its parts
    char * strtokIndx; // this is used by strtok() as an index
    String command = strtok(tempChars,":");      // get the first part - the string
    if (command == "tara"){
      scale.tare(10);
      offset = scale.get_offset();
      Serial.println("Tara");
    }
    else if(command == "cali") {
      strtokIndx = strtok(NULL, ";");
      reference = atof(strtokIndx);     // convert this part to a float
      scale.set_scale();
      tempfloat = scale.get_units(10);
      scale.set_scale(tempfloat/reference);
      factor = scale.get_scale();
      Serial.printf("Reference weight set to %.2f\n", reference);
    }
    else if(command == "coil") {
      strtokIndx = strtok(NULL, ";");
      coil = atof(strtokIndx);     // convert this part to a float
      Serial.printf("Coilweight set to %.2f\n", tara);
    }
    else if(command == "spow") {
      strtokIndx = strtok(NULL, ";");
      spoolweight = atof(strtokIndx);     // convert this part to a float
      Serial.printf("Spoolweight set to %.2f\n", spoolweight);
      active_mat=-1;
      active_spool=-1;
    }
    else if(command == "dens") {
      strtokIndx = strtok(NULL, ";");
      density = atof(strtokIndx);     // convert this part to a float
      Serial.printf("Density set to %.2f\n", density);
      active_mat=-1;
      active_spool=-1;
    }
    else if(command == "wifi") {
      strtokIndx = strtok(NULL, ";");
      wifi = atoi(strtokIndx);     // convert this part to a float
      Serial.println("Turning ");
      if (wifi) {
        Serial.println("on");
      }
      else {
        Serial.println("off");
      }
        Serial.println(" wifi");
      saveConfig();
      ESP.restart();
    }
    else if(command == "ssid") {
      wifissid = strtok(NULL,";"); 
      Serial.println("Settings ssid to " + wifissid);
    }
    else if(command == "pass") {
      wifipasswd = strtok(NULL,";"); 
      Serial.println("Changing Wifi Passwd");
    } 
    else if(command == "cont") {
      strtokIndx = strtok(NULL, ";");
      containersize = atof(strtokIndx);     // convert this part to a float
      Serial.printf("Containersize set to %.2f\n", containersize);
    } 
    else if(command == "alti") {
      strtokIndx = strtok(NULL, ";");
      altitude = atof(strtokIndx);     // convert this part to a float
      Serial.printf("Altitude set to %.2f\n", altitude);
    }
    else if(command == "heat") {
      strtokIndx = strtok(NULL, ";");
      heatertemp = atof(strtokIndx);     // convert this part to a float
      Serial.printf("Heatertemp set to %.2f\n", heatertemp);
    } 
    else if(command == "dele") {
      Serial.printf("Restore Factorysettings");
      strtokIndx = strtok(NULL, ";");    //delete all configfiles
      File configFile = SPIFFS.open("/config.txt", "w");
      configFile.close();
      File spoolFile = SPIFFS.open("/spool.txt", "w");
      spoolFile.close();
      File matFile = SPIFFS.open("/mat.txt", "w");
      matFile.close();
      File saveFile = SPIFFS.open("/save.txt", "w");
      saveFile.close();
      File mqttFile = SPIFFS.open("/mqtt.txt", "w");
      mqttFile.close();
      ESP.restart();
    } 
    else {
        Serial.println("Unknown Command. Please Check Manual");
    }
    saveConfig();
}

//===============================================================
// This routines opens Wifi and MQTT Connections
//===============================================================

void connectToWifi()
{
  WiFi.enableSTA(true);
  WiFi.begin(wifissid, wifipasswd);
  int i=3;
  while (WiFi.status() != WL_CONNECTED && i<30) {
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
  server.on("/saveMqtt", handlesaveMqtt);
  server.on("/Scale", handleScale);
  server.on("/showSettings", handleshowSettings);
  server.on("/updatePage", handleupdatePage);
  server.begin();
  //SET TIMEZONE
  configTime(0, 0, "0.de.pool.ntp.org", "0.europe.pool.ntp.org");
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  if (mqtton) {
    client.setServer(mqttserver.c_str(), mqttport);
     if (client.connect(mqttname.c_str(), mqttuser.c_str(), mqttpass.c_str() )) {
       display.drawString(0, 6, "MQTT connected.");  
     } else {
      display.drawString(0, 6, "MQTT Error!");
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
  scale.begin(SCALE_SDA, SCALE_SCL, 128);
  if (scale.wait_ready_timeout(1000)) {
//    scale.get_units(1);
    display.drawString(0, 3, "Scale found.");
    scale.set_scale(factor);
    scale.set_offset(offset);
  }
  else {
    display.drawString(0, 3, "Scale missing!");
    delay(1000);
  }
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);
  bool status = bme.begin(BME280);
  display.drawString(2, 0, ".");
  if (status) {
    display.drawString(0, 5, "Sensor found.");
  }
  else {
    display.drawString(0, 5, "Sensor missing!");
    delay(1000);
  }
}

//==============================================================
//                  SETUP
//==============================================================
void setup(void){
  Serial.begin(115200);
  display.begin();
  display.setFont(u8x8_font_chroma48medium8_r);
  display.drawString(0, 0, ".");
  if (SPIFFS.begin()) {
    if (loadConfig()) {
      display.drawString(0, 2, "Config loaded.");
    }
    else {
      display.drawString(0, 2, "Loadingerror!");
      delay(1000);
    }
  }
  else {
    display.drawString(0, 2, "Dataerror!");
    delay(1000);
  }
  initSensor();

  if (wifi) {
    connectToWifi();
    handleRoot();
  }
  else {
      display.clear();
  }
  pinMode(LED,OUTPUT); 
  pinMode(HEATERPIN, OUTPUT);
  digitalWrite(HEATERPIN,HIGH);
}
//==============================================================
//                     LOOP
//==============================================================
void loop(void){
  handleupdateDisplay();
  if (wifi) {
    server.handleClient();          //Handle client requests
  }
  recvWithStartEndMarkers();
  if (newData == true) {
      strcpy(tempChars, receivedChars);
      parseData();
      newData = false;
  }
}
