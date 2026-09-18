/*
  ARCHIVED PROTOTYPE — not part of the final system.
  Early draft of the STM32 wearable sketch that explored GPS ($GPRMC parsing)
  and an MQ gas sensor pin. Both were dropped from the final design in favor
  of the flex / heart-rate / ADXL345 + Firebase pipeline documented in the
  project report. Kept here for reference only.
*/
#include <LiquidCrystal.h>
const int rs = PB12, en = PB13, d4 = PB14, d5 = PB15, d6 = PA11, d7 = PA12;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
char res[130];
#include "dht.h"
#define dht_apin PA3 // Analog Pin sensor is connected to
dht DHT;
      #include <Wire.h>
      #include <Adafruit_Sensor.h>
      #include <Adafruit_ADXL345_U.h>
      Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);
int flex=PA4;
int mq=PB6;
int v1=PB5;
int v2=PB4;
int hbpin = PA2;
int hcount = 0,hbval=0;
void serialFlush(){
  while(Serial1.available() > 0) {
    char t = Serial1.read();
  }
} 
char gps_location[40];
void gps()
{

  lcd.clear();
  lcd.setCursor(0, 1);lcd.print("READING GPS");delay(1000);
   while(1)
   {
    if(Serial1.find("$GPRMC"))
    {
      for(int i=0;i<14;i++)
      {
        while(!Serial1.available());
        char ch = Serial1.read();
      }
      for(int i=0;i<24;i++)
      {
        while(!Serial1.available());
        gps_location[i] = Serial1.read();
        Serial1.println(gps_location[i]);
      }
       break;      
    }
   }
   lcd.setCursor(0, 1);lcd.print("READING GPS DONE");
     delay(2000);
   
}
void setup() {

  pinMode(v1,OUTPUT);
    pinMode(v2,OUTPUT);
 pinMode(dht_apin,INPUT);
  pinMode(flex,INPUT);
  digitalWrite(v1,HIGH);
  digitalWrite(v2,HIGH);
    pinMode(hbpin, INPUT);
delay(1000);
  Serial.begin(9600);
  Serial1.begin(9600);
     Serial1.println("Accelerometer Test"); Serial1.println("");
      if(!accel.begin())
      {
    Serial1.println("Ooops, no ADXL345 detected ... Check your wiring!");
    while(1);
    }
  lcd.begin(16,2);
  lcd.clear();lcd.setCursor(0, 0);lcd.print("epelitic");
  delay(2000);
}
 unsigned long int duration = 0;
int hbeat=0;
void loop() {
  int flexd = digitalRead(flex);
DHT.read11(dht_apin);

int hd = DHT.humidity;
 
int td = DHT.temperature;
lcd.clear();
  lcd.setCursor(0, 0);lcd.print("H:");lcd.print(hd);
  lcd.setCursor(6, 0);lcd.print("T:");lcd.print(td);
lcd.setCursor(11,0);lcd.print("F:");lcd.print(flexd); 
    lcd.setCursor(0, 1);lcd.print("HB:");lcd.print(hbval);
             
               delay(1000);
     sensors_event_t event; //to activate
      accel.getEvent(&event);//get x,y,z data from sensor
    Serial1.print("X: "); Serial1.print(event.acceleration.x); 
    Serial1.print("  ");
    Serial1.print("Y: "); Serial1.print(event.acceleration.y); 
    Serial1.print("  ");
    Serial1.print("Z: "); Serial1.print(event.acceleration.z); 
    Serial1.print("  ");Serial1.println("m/s^2 ");
        Serial1.print("H: "); Serial1.print(hbval); 
    Serial1.print("  ");
    Serial1.print("HD: "); Serial1.print(hd); 
    Serial1.print("  ");
    Serial1.print("T: "); Serial1.print(td); 
    Serial1.print("  ");
    Serial1.print("F: "); Serial1.print(flexd); 
    Serial1.print("  ");
    delay(500);
     duration = pulseIn(hbpin,LOW,5000000)/1000;
    if(duration == 0)
        hbeat =0;
    else
        hbeat = 64 + duration%18;
        delay(50);
         hbval = hbeat;
     
    delay(500);

          if(hbval > 75)
     {
      lcd.clear();
      lcd.setCursor(0, 1);lcd.print("High Heart Rate");
       digitalWrite(v2,LOW);
       delay(1000);
       digitalWrite(v2,HIGH);
       delay(1000);
      }
    if(event.acceleration.x < -4 )
{
    Serial.println("FELT DUE TO EPILITIC");
  }

if(event.acceleration.x > 4)
{
    Serial.println("POSITION SIT");
  }
if(event.acceleration.y > 4)
{
    Serial.println("SLEPT");
  }   
if(event.acceleration.y < -4)
{
    Serial.println("FALLEN");
  }
  if(event.acceleration.y > 0 && event.acceleration.z > 5)
{ 
    Serial.println("sTABLE");
  }            
         if(flexd == 0)
        {
            
     lcd.clear();lcd.setCursor(0,0);lcd.print("EPLITIC");delay(1000);
          lcd.setCursor(0,0);lcd.print("OCCURED");delay(1000);              
   digitalWrite(v1,LOW); delay(1000);
  digitalWrite(v1,HIGH);delay(1000);
   Serial.println("EPILITIC OCCURED");
        }
        if(hd > 85 )
{
  lcd.clear();
  lcd.setCursor(0, 0);lcd.print("high HUMIDITY"); 
  Serial.println("high HUMIDITY");           
  delay(2000);
  }
  if(td > 35 )
{
  lcd.clear();
  lcd.setCursor(0, 0);lcd.print("high temperature"); 
    Serial.println("high temperature");            
  delay(500);
  }
  else
  {
   digitalWrite(v1,HIGH);
  digitalWrite(v2,HIGH);delay(100);
  }
  
 
}
