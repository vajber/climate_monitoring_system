#include <SPI.h>
#include <Wire.h>
#include <TFT_eSPI.h> 
#include "OpenFontRender.h"
#include "NotoSans_Bold.h"
#include <DHTesp.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include <PubSubClient.h>
#include <WiFi.h>

const char* ssid="Wi-FI_name";
const char* password="Wi-Fi_pass";
const char* mqtt_server="your_IP";

#define TTF_FONT NotoSans_Bold
const int DHT_PIN = 27;

TFT_eSPI tft = TFT_eSPI();            
TFT_eSprite spr = TFT_eSprite(&tft);  
OpenFontRender ofr;
DHTesp dhtSensor;
Adafruit_BME280 bme;
BH1750 lightmeter;
WiFiClient espClient;
PubSubClient client(espClient);

#define DARKER_GREY 0x18E3
#define LOOP_DELAY 1000
uint16_t backgroundColor = TFT_NAVY; 

uint32_t updateTime = 0;       
uint16_t angle0 = 0, angle1 = 0, angle2 = 0, angle3 = 0;
bool initMeters = true;

float temp;
float hum;
float press;
float lux;

enum state{LOW_STATE, NORMAL_STATE, HIGH_STATE};
state tempState = NORMAL_STATE;
state humState = NORMAL_STATE;
state pressState = NORMAL_STATE;
state lightState = NORMAL_STATE;
int lastTempVal = -999; 
int lastHumVal = -999;
int lastPressVal = -999; 
int lastLuxVal = -999;

void ringMeter(int x, int y, int r, int val, const char *units, uint16_t &last_angle, int v_min, int v_max);
void controlParams(int x, int y, int val,int val_min, int val_max, state &currentState,int &lastVal, const char *name);
void printValues();
void publishData();
void callback(char* topic, byte* payload, unsigned int length) {

}

void setup(void) {
  Serial.begin(115200);
  Wire.begin();
  tft.begin();
  tft.setRotation(3);

  tft.fillScreen(backgroundColor);

  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);
  lightmeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  bme.begin(0x76);

  if (ofr.loadFont(TTF_FONT, sizeof(TTF_FONT))) {
    Serial.println("Font error");
  }

  updateTime = millis();
  /*tft.fillCircle(50, 52, 52, DARKER_GREY);
  tft.fillCircle(160, 52, 52, DARKER_GREY);
  tft.fillCircle(267, 52, 52, DARKER_GREY);
  tft.fillCircle(50, 175, 52, DARKER_GREY);*/ //if have flicker on the display
  
  Serial.print("Connecting WiFi...");
  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" Connected!");
    client.setServer(mqtt_server, 1883);
  } else {
    Serial.println(" WiFi Timeout - working offline");
  }
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    client.loop();
    if (!client.connected()) client.connect("espClient");
  }

  if (millis() - updateTime >= LOOP_DELAY) {
    updateTime = millis();

    temp = dhtSensor.getTemperature();
    hum = dhtSensor.getHumidity();
    press = bme.readPressure() / 100.0F;
    lux = lightmeter.readLightLevel();

    if (isnan(temp)) temp = 0;
    if (isnan(hum)) hum = 0;
    if (lux < 0) lux = 0; 

    ringMeter(50, 52, 52, temp, "TEMP", angle0, 0, 100);
    ringMeter(160, 52, 52, hum, "HUM %", angle1, 0, 100);
    ringMeter(267, 52, 52, press, "hPa", angle2, 900, 1100);
    ringMeter(50, 175, 52, lux, "LUX", angle3, 0, 4000);

    controlParams(140, 140,temp,22, 25, tempState, lastTempVal, "Temperature ");
    controlParams(140, 160,hum,40, 60, humState, lastHumVal, "Humidity ");
    controlParams(140, 180,press,980, 1010, pressState, lastPressVal, "Pressure ");
    controlParams(140, 200,lux,300, 500, lightState, lastLuxVal, "Light ");
    initMeters = false;

    printValues();
   
 
    if (client.connected()) {
      publishData();
    }
  }
}

void ringMeter(int x, int y, int r, int val, const char *units, uint16_t &last_angle, int v_min, int v_max)
{
  if (val < v_min) val = v_min;
  if (val > v_max) val = v_max;

  /***************meter background******************/
  if (initMeters) {
    tft.fillCircle(x, y, r, DARKER_GREY);
    tft.drawSmoothCircle(x, y, r, TFT_SILVER, DARKER_GREY);
    uint16_t tmp = r - 3;
    tft.drawArc(x, y, tmp, tmp - tmp / 5, 30, 330, TFT_BLACK, DARKER_GREY);
    last_angle = 30;
  }
  /***************meter background******************/

  int r_inner = r - 3;// radius of circle inside

  int val_angle = map(val, v_min, v_max, 30, 330);// val->angle/grads

  /************************sprite***************************/
//picture creating in memory and after show in display//

  if (last_angle != val_angle) {
    ofr.setDrawer(spr);
    ofr.setFontSize(r_inner * 1.1);
    ofr.setFontColor(TFT_WHITE, DARKER_GREY);

    char str_buf[8];
    itoa(val, str_buf, 10);

    uint16_t spriteW = ofr.getTextWidth("8888") + 10; 
    uint16_t textH = ofr.getTextHeight("4");
    uint16_t spriteH = textH + 4;

    spr.createSprite(spriteW, spriteH);
    spr.fillSprite(DARKER_GREY);

    uint16_t currentTextW = ofr.getTextWidth(str_buf);
    int16_t cursorX = (spriteW - currentTextW) / 2; 

    ofr.setCursor(cursorX, -spriteH / 4); 
    ofr.printf(str_buf);

    spr.pushSprite(x - spriteW / 2, y - spriteH / 2);
    spr.deleteSprite();
    /************************sprite***************************/

    /***********signatures**************/
    ofr.setDrawer(tft);
    ofr.setFontColor(TFT_GOLD, DARKER_GREY);
    ofr.setFontSize(r_inner / 2.5);
    ofr.setCursor(x, y + (r_inner * 0.45));
    ofr.cprintf(units);

    /***********signatures**************/

    /********draw ark*******************/
    uint8_t thickness = r_inner / 5;
    if (val_angle > last_angle) {
      tft.drawArc(x, y, r_inner, r_inner - thickness, last_angle, val_angle, TFT_SKYBLUE, TFT_BLACK);
    } else {
      tft.drawArc(x, y, r_inner, r_inner - thickness, val_angle, last_angle, TFT_BLACK, DARKER_GREY);
    }
    
    last_angle = val_angle;
  }
/********draw ark*******************/
}

void printValues() {
  Serial.print("Temp: "); 
  Serial.print(dhtSensor.getTemperature());
  Serial.print(" Hum: "); 
  Serial.print(dhtSensor.getHumidity());
  Serial.print(" Press: "); 
  Serial.println(bme.readPressure() / 100.0F);
  Serial.print(" Light: "); 
  Serial.println(lightmeter.readLightLevel());
}

void publishData() {
  if (!client.connected()) return;

  char msg[10];

  dtostrf(temp, 1, 1, msg); 
  client.publish("temp", msg);

  dtostrf(hum, 1, 1, msg);
  client.publish("hum", msg);

  dtostrf(press, 1, 1, msg);
  client.publish("press", msg);

  itoa(lux, msg, 10);
  client.publish("lux", msg);

  Serial.println("MQTT Publish OK");
}

void controlParams(int x, int y, int val,int val_min, int val_max, state &currentState,int &lastVal, const char *name){
    state newstate;

    if(val<val_min){
      newstate=LOW_STATE;
    }else if(val>val_max){
      newstate=HIGH_STATE;
    }else newstate=NORMAL_STATE;

    if (newstate != currentState || (newstate != NORMAL_STATE && val != lastVal)||initMeters) {
      currentState=newstate;

      tft.setCursor(x,y);
      tft.setTextSize(1);
      tft.setTextColor(TFT_GOLD,backgroundColor);
      tft.print(name);

      switch(currentState){
        case LOW_STATE:{
        int diff=val_min-val;
        tft.print("lower on ");
        tft.print(diff);
        break;}

        case NORMAL_STATE:{
        tft.print("in normal     ");
        break;}

        case HIGH_STATE:{
        int diff=val-val_max;
        tft.print("higher on ");
        tft.print(diff);}
      }
    }
}