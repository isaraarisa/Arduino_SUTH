//FirebaseESP8266.h must be included before ESP8266WiFi.h
#include <LiquidCrystal_PCF8574.h>
#include <Wire.h>
#include <OneWire.h>
#include "FirebaseESP8266.h"
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <TridentTD_LineNotify.h>
#include <WiFiUdp.h>
#include <time.h>


#define adressSensor 0x3F //0x27 หรือ 0x3F เป็น Address ใช้สำหรับจอ LCD
//SDA = D2 ,SCL = D1
LiquidCrystal_PCF8574 lcd(adressSensor);

#define ntp  "ntp.sut.ac.th" //NTP สำหรับเน็ต มทส.

WiFiUDP ntpUDP;//SET TIMEZONE
NTPClient timeClient(ntpUDP,ntp,7*3600);//SET TIMEZONE














//============SET FOR USE BOT================================= แก้ไขได้

#define FIREBASE_HOST "https://isara-prototype.firebaseio.com"
#define FIREBASE_AUTH "wFUPI7gfMcYaa6dX0ru6BGc2JrgoikZL8OVQRApo"
#define WIFI_SSID "SUTH-Mobile"//"SUTH-Mobile"
#define WIFI_PASSWORD ""

String botName = "prototype_00";//<<--Unique Per Bot

//============================================================











FirebaseData firebaseData;//ไม่ต้องแก้ไขก็ได้
FirebaseJson json; //ไม่ต้องแก้ไขก็ได้

OneWire  ds(2); //D4 or GPIO2 *สามารถแก้ไขได้

//จำไม่ได้
String Timestamp;
float maxTemp = 999 ,minTemp = -999;
//float humidity = 100;
float temperature = .2f;
float breakSave = 0;
int lost = 0;//for connect wifi

int delayLine = 0;
int delaySave = 0;//วินาที
int delayGetSetting = 0;
String statusRoom = "";
String location = "";
String messageOnLine = botName + " Online.\nMy location ";
bool lcdOnline = true;
int lcdError;
int years = 1900;
String savedate = "error"; 
String lcdStatus[5] = {"Success!",
                       "ERROR! data too long to fit in transmit buffer",
                       "ERROR!! received NACK on transmit of address",
                       "ERROR!!! received NACK on transmit of data",
                       "ERROR!!!! other"     };


//******************************************************************************










void setup()
{
  // LCD Zone
  Wire.begin();//
  Wire.beginTransmission(adressSensor);//
  //lcdError = Wire.endTransmission();
  lcd.begin(16, 2); // initialize the lcd
  
  lcd.setBacklight(255);//เปิดจอ
  lcd.home();
  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Now Loading...");
//-------------------------------------------

  Serial.begin(9600);
  Serial.println(LINE.getVersion());


//Mac ของบอร์ด ดูที่ Monitor
  Serial.print("MAC address: ");
  Serial.println(WiFi.macAddress());

  String thisBoard= ARDUINO_BOARD;
  Serial.println(thisBoard);

  connection();

  //function ภายใน loop for นี้ มีปัญหา getValue ที่ช้า จึง get ค่าเล่นๆ ไป 10 รอบ
  for(int i=0;i<10;i++){
  getSensors();
  date();
  times();
  }
  delay(1000*3);
  
}
//END setup











//=========================================================================
void loop()
{
  Serial.println("******************************");

  // LCD มักพบปัญหาในการแสดงผลหาใช้ไปนาน ๆ จอจะดับหากไม่คอยตรวจเช็ค
  //Check LCD-----------------------------------------------
  Wire.beginTransmission(adressSensor);
  lcdError = Wire.endTransmission();
  Serial.println("Status LCD : " + lcdStatus[lcdError]);

  if(lcdError != 0){lcdOnline = false;}

  if (lcdOnline == false && lcdError == 0) {

  lcd.begin(16, 2); 
  lcd.setBacklight(255);
  lcd.home();

  lcdOnline = true;
  }
//--------------------------------------------------------------------- 

//ป้องกันการหลุดของ WiFi กับ Board
 if(WiFi.status() != WL_CONNECTED)
  {
    messageOnLine = botName + " in location " + location + " Back to Online again!!";
    connection();
  }

//ดึงการตั้งค่าทุก 1 นาที โดยประมาณ
  if(delayGetSetting <= 0){
  settingTemperatureBot();
  delayGetSetting = 60*1;
  
  delayGetSetting--;
  delaySave--;
  }
  //ดึงค่าวันที่ และ เวลา
  String timest = times();
  String datet = date();

//ดึงค่าอุณหภูมิมาแสดง
   newTemperature(datet,timest);
   
    if(lost>=4){//แจ้งการ Lose Connect บนจอ LCD
    lcd.setCursor(0,1);
    lcd.print("Connect LOST !!");
  }

  //Save ค่าไปที่ Firebase
  if(delaySave<=0){
    
  saveData("/dataTemperature/"+botName+"/"+datet+"/"+timest+"/");
  delaySave = 60*30;
  }

 //Save ค่าไปที่ Firebase เป็นค่าที่ไว้ดูผ่าน Web
  if(temperature<breakSave-.5 || temperature>breakSave+.5){
  saveCallData("/_Call/"+botName+"/",datet,timest);
  breakSave = temperature;
  }
  
  delaySave--;
  delayGetSetting--;


  Serial.print("delaySave = ~");
  Serial.print(delaySave/60);
  Serial.print(":");
  if(delaySave%60<10){Serial.print("0");}
  Serial.print(delaySave%60);
  Serial.println();
  
  Serial.print("delayGetSetting = ~");
  Serial.print(delayGetSetting/60);
  Serial.print(":");
  if(delayGetSetting%60<10){Serial.print("0");}
  Serial.print(delayGetSetting%60);
  Serial.println();

  
  Serial.println("******************************");
  
  delay(1000*1);//ผลจากการรัน ทุก 2 วิ ,Delay+1s
}
//END loop******************************************************************










//START connection===================================================
void connection(){//สำหรับ WiFi และ Line Notify
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  int cwifi = 0;
  lost = 0;
  while (WiFi.status() != WL_CONNECTED && lost<4)
  {
    cwifi++;
    Serial.print(".");
    if(cwifi>10){Serial.println();Serial.print("Connecting to Wi-Fi"); cwifi = 0; lost++;}
    delay(500);
  }

  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  timeClient.begin();

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);

  //Set database read timeout to 1 minute (max 15 minutes)
  Firebase.setReadTimeout(firebaseData, 1000 * 60);
  //tiny, small, medium, large and unlimited.
  //Size and its write timeout e.g. tiny (1s), small (10s), medium (30s) and large (60s).
  Firebase.setwriteSizeLimit(firebaseData, "tiny");

  
    configTime(7*3600,0, " time.navy.mi.th", " time.navy.mi.th");     //ดึงเวลาจาก Server
    Serial.println("\nWaiting for time");
    while (!time(nullptr)) {
      Serial.print(".");
      delay(1000);
    }
    
 
 // LINE.notify(messageOnLine+location); 
}
//END connection










//STRAT settingTemperatureBot=======================================

void settingTemperatureBot(){

 if (Firebase.get(firebaseData, "/settingBot/"+botName+"/location"))
    {
      Serial.println("GET location PASS");
      location = firebaseData.stringData();
      Serial.println(location);
    }
    else
    {
      Serial.println("FAIL");
      Serial.println("REASON: " + firebaseData.errorReason());
    }
    Serial.println("-------------------------------");
    delay(1000*.5);
    
    if (Firebase.get(firebaseData, "/settingBot/"+botName+"/maxTemp"))
    {
      Serial.println("GET maxTemp PASS");
      maxTemp = firebaseData.floatData();
      Serial.println(maxTemp);
    }
    else
    {
      Serial.println("FAIL");
      Serial.println("REASON: " + firebaseData.errorReason());
    }
    Serial.println("-------------------------------");
    
    if (Firebase.get(firebaseData, "/settingBot/"+botName+"/minTemp"))
    {
      Serial.println("GET minTemp PASS");
      minTemp = firebaseData.floatData();
      Serial.println(minTemp);
    }
    else
    {
      Serial.println("FAIL");
      Serial.println("REASON: " + firebaseData.errorReason());
    }
    Serial.println("-------------------------------");
   

    String lineToken;

  
  if (Firebase.get(firebaseData, "/settingBot/"+botName+"/tokenLine"))
    {
      Serial.println("GET tokenLine PASSED");
      
      lineToken = firebaseData.stringData();
      Serial.println(lineToken);
    }
    else
    {
      Serial.println("FAILED");
      Serial.println("REASON: " + firebaseData.errorReason());
    }

    if (Firebase.get(firebaseData, "/settingBot/"+botName+"/location"))
    {
      Serial.println("GET location PASS");
      location = firebaseData.stringData();
      Serial.println(location);
    }
    else
    {
      Serial.println("FAIL");
      Serial.println("REASON: " + firebaseData.errorReason());
    }
    
  LINE.setToken(lineToken);
 delay(1000*.5);
}
//END temperature










//START dateTime ===========================================================
String date(){
  
  configTime(7*3600,0,ntp,ntp);    //ดีงเวลาปัจจุบันจาก Server อีกครั้ง
  time_t now = time(nullptr);
  struct tm* p_tm = localtime(&now);
  //Serial.print(p_tm->tm_mon);
  String save = savedate;
  
  String d = String((p_tm->tm_year)+1900)+":";//นับจาก ค.ศ. 1900 ผ่านมากี่ปี(หากไม่บวก 1900)
             if((p_tm->tm_mon)+1<10){d+="0";}
             d += String((p_tm->tm_mon)+1)+":";//0-11 เดือน(หากไม่บวก 1)
             if((p_tm->tm_mday)<10){d+="0";}
             d += String((p_tm->tm_mday));

  Serial.println(d);
  Serial.println("-------------------------------");
             
  return(d);
}
String times(){
  
  timeClient.update();
  String t = String(timeClient.getFormattedTime());
  Serial.println(t);
  Serial.println("-------------------------------");

  return(t);
}
//END dateTime












//START saveData=====================================================
void saveData(String part){

  
    Serial.println("------------------------------------");
    
    if (Firebase.set(firebaseData,part+"temperature",temperature))
    {
      Serial.println("SAVE temperature PASSED");
    }
    else
    {
      Serial.println("FAILED");
      Serial.println("REASON: " + firebaseData.errorReason());
    }
    Serial.println("------------------------------------");
    delay(1000*.5);

    if (Firebase.set(firebaseData,part+"statusRoom",statusRoom))
    {
      Serial.println("SAVE statusRoom PASSED");
    }
    else
    {
      Serial.println("FAILED");
      Serial.println("REASON: " + firebaseData.errorReason());
    }
    Serial.println("------------------------------------");
    delay(1000*.5);

    

    
}//END SAVE











//START saveCallData=====================================================
void saveCallData(String part,String date,String times){

  
    Serial.println("------------------------------------");
    
    if (Firebase.set(firebaseData,part+"temperature",temperature))
    {
      Serial.println("saveCallData temperature PASSED");
    }
    else
    {
      Serial.println("FAILED");
      Serial.println("REASON: " + firebaseData.errorReason());
    }
    Serial.println("------------------------------------");
    delay(1000*.5);

    if (Firebase.set(firebaseData,part+"statusRoom",statusRoom))
    {
      Serial.println("saveCallData statusRoom PASSED");
    }
    else
    {
      Serial.println("FAILED");
      Serial.println("REASON: " + firebaseData.errorReason());
    }
    Serial.println("------------------------------------");
    delay(1000*.5);

     Serial.println("------------------------------------");
    
    if (Firebase.set(firebaseData,part+"date",date))
    {
      Serial.println("saveCallData date PASSED");
    }
    else
    {
      Serial.println("FAILED");
      Serial.println("REASON: " + firebaseData.errorReason());
    }
    Serial.println("------------------------------------");
    delay(1000*.5);

     Serial.println("------------------------------------");
    
    if (Firebase.set(firebaseData,part+"time",times))
    {
      Serial.println("saveCallData times PASSED");
    }
    else
    {
      Serial.println("FAILED");
      Serial.println("REASON: " + firebaseData.errorReason());
    }
    Serial.println("------------------------------------");
    delay(1000*.5);

    
}
//END saveCallData











//START newTemperature==================================================

void newTemperature(String d,String t) {

  getSensors();
  Serial.println("°C°C°C°C°C°C°C°C°C°C°C°C°C°C°C°C°C°C°C");
  Serial.println();
  Serial.print("Temperature = ");
  Serial.print(temperature);
  Serial.println("°C");
  Serial.println();
  Serial.println("°C°C°C°C°C°C°C°C°C°C°C°C°C°C°C°C°C°C°C");

 // lcd.setBacklight(255);//เปิดจอ
 // lcd.home();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Temp = "+String(temperature)+" C");
  
  //LINE--------------------------------------------------
  
  line(d,t);

      
}

//END newTemperature












//getSensors====================================================================
 void getSensors(){//การทำงานของ DS18B20

  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  float celsius;
  
  if ( !ds.search(addr)) {
    Serial.println("search address...");
    ds.reset_search();
    delay(250);
    return;//return itself
  }
  

  if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("get CRC...");
      return;//return itself
  }
  Serial.println();
 
  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      type_s = 1;
       Serial.println("indicates Chip = DS18S20");  // or old DS1820
      break;
    case 0x28:
      type_s = 0;
      Serial.println("indicates Chip = DS18B20");
      break;
    case 0x22:
      type_s = 0;
      Serial.println("indicates Chip = DS1822");
      break;
    default:
     Serial.println("Device is not a DS18x20 family device.");
     return;//return itself
  } 

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);Serial.println("start conversion.");// start conversion, with parasite power on at the end
  
     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);Serial.println("Read Scratchpad.");  // Read Scratchpad

  Serial.print("Read Data byte: ");
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
    Serial.print(data[i],HEX);
    if(i<8){Serial.print(":");}
    }
    Serial.println();
  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  Serial.print("Convert data.");
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    Serial.print(".");
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      Serial.print(".");
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    Serial.print(".");
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) {raw = raw & ~7;Serial.print(".");}  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) {raw = raw & ~3;Serial.print(".");} // 10 bit res, 187.5 ms
    else if (cfg == 0x40) {raw = raw & ~1;Serial.print(".");} // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  Serial.println();
  celsius = (float)raw / 16.0;

  temperature = celsius;

  delay(1000*.1);
  }
  //=======================================================================










//line==========================================================================
  void line(String d,String t){//แจ้งเตือนไปที่ไลน์

    statusRoom = "Good";
  if(temperature>maxTemp){
    if(delayLine==0){
    LINE.notify("\nณ "+String(location)+"\nอุณหภูมิห้อง "+String(temperature)+"°C \nอุณหภูมิเกินกว่ากำหนด");
    saveData("/dataTemperature/"+botName+"/"+date()+"/"+times()+"/");
    delayLine = 60*5;
    }
    statusRoom = "Bad";
    }
  else if(temperature<minTemp){
     if(delayLine==0){
    LINE.notify("\nณ "+String(location)+"\nอุณหภูมิห้อง "+String(temperature)+"°C \nอุณหภูมิต่ำกว่ากำหนด");
    saveData("/dataTemperature/"+botName+"/"+date()+"/"+times()+"/");
    delayLine = 60*5;
     }
    statusRoom = "Bad";
    }
    
    if(delayLine>0){
      if(delayLine%7==0){delayLine--;}
      delayLine--;
      Serial.print("delayLine = ");
      Serial.print(delayLine/60);
      Serial.print(":");
      if(delayLine%60<10){Serial.print("0");}
      Serial.print(delayLine%60);
      Serial.println();
      }

    if(statusRoom == "Good" && delayLine > 0 && (temperature<(maxTemp-.5)||temperature>(minTemp+.5))){
       LINE.notify("\nณ "+String(location)+"\nอุณหภูมิห้องได้กลับมาปกติแล้ว \nที่อุณหภูมิ "+String(temperature)+"°C");
       saveData("/dataTemperature/"+botName+"/"+date()+"/"+times()+"/");
       delayLine = 0;
      }
    
    }//==========================================================================
