#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <AWS_IOT.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include "RemoteDebug.h"
#include <string>
#include <stdio.h>
#include <NTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

int day,hours,minutes,seconds,year,month,date,minuteSave;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP,"europe.pool.ntp.org", 7200, 60000);
#define DHTPIN 23     // Digital pin connected to the DHT sensor
#define DHTTYPE    DHT22     // DHT 11
DHT dht(DHTPIN, DHTTYPE);
// ledPin refers to ESP32 GPIO 23
const int ledPin = 2;
AWS_IOT hornbill;   // AWS_IOT instance

char WIFI_SSID[]="Aswin";
char WIFI_PASSWORD[]="Pi@Riya*1";
char HOST_ADDRESS[]="a1gcytxbxjm16j-ats.iot.eu-west-1.amazonaws.com";
char CLIENT_ID[]= "ESP32_02";
//char TOPIC_PJ[] = "sdkTest/";
char TOPIC_NAME[] = "home/b001";

//#define HOST_NAME *CLIENT_ID;
int status = WL_IDLE_STATUS;
RemoteDebug Debug;
const char* host = "ESP32_H01";
int tick=0,msgCount=0,msgReceived = 0,publishMsg=0, sensorfail =0;
char payload[512];
char rcvdPayload[512];
//reset after fail publish
int publicfail = 5;
int publicfailCount = 5;
int i = 0;
int samplect = 200;
float ph,rawadc = 0.00;

float h,t,dallas_t;
String formattedTime;
String IP_adr;
//config ph meter

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 15
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

void initWifi();
void IOT_send_msg();
void initOTA();
void initAWS();
void initRemoteDebug();
void IOT_received_msg();

void mySubCallBackHandler (char *topicName, int payloadLen, char *payLoad)
{
    strncpy(rcvdPayload,payLoad,payloadLen);
    rcvdPayload[payloadLen] = 0;
    msgReceived = 1;
}

void setup()
{
  pinMode(ledPin, OUTPUT);
  Serial.begin(115200);
  initWifi();
  IP_adr = WiFi.localIP().toString();
	initRemoteDebug();
	initAWS();
  analogSetAttenuation(ADC_11db);
  adcAttachPin(34);
  analogSetCycles(255);
  analogSetClockDiv(255); // 1338mS

  sensors.begin();     //dallas start
	dht.begin();
	timeClient.begin();
  initOTA ();
  Debug.println("Setup Ok");
  delay(500);
}

void loop()
{
  Debug.handle();
  ArduinoOTA.handle();
  IOT_received_msg();
	timeClient.update();
	minutes = timeClient.getMinutes();
  formattedTime = timeClient.getTimeStampString();
  h = dht.readHumidity();
  t = dht.readTemperature();
  sensors.requestTemperatures();
  dallas_t = sensors.getTempCByIndex(0);

  for(i =0; i <samplect; i++)
  {
    rawadc = rawadc + analogRead(34);
  }
  // ph = (rawadc/samplect)*0.00233489;
    ph = ((rawadc/samplect)-4925)/-275;
  //https://www.mathportal.org/calculators/analytic-geometry/two-point-form-calculator.php
  // BNC to GND = 3000 (2,5V) & 7ph, Amonnique  1900 (1,65V) & 14ph

  rawadc =0;


  if ((minutes == 0)||(minutes == 10)||(minutes == 20)||(minutes == 30)||(minutes == 40)||(minutes == 50)||(sensorfail==1))
  {
    if (isnan(h) || isnan(t)) {
        Debug.println("Failed to read from DHT sensor!");
        delay(2000);
        publishMsg =0;
        sensorfail =1;
    }
    else
    {
        // Create the payload for publishing
        formattedTime = timeClient.getTimeStampString();
        snprintf(payload,sizeof(payload),"{\"thingid\": \"%X\", \"IP_adr\": \"%s\", \"DHT22_t\": %.2f, \"DHT22_h\": %.2f, \"Dallas_t\": %.2f, \"ph\": %.2f, \"datetime\": \"%s\"}" ,WiFi.macAddress(),IP_adr.c_str(),t,h,dallas_t,ph,formattedTime.c_str() );
  //    snprintf(payload,sizeof(payload),"{\n \"thingid\":\"%X\",  \"DHT22_t\":%.2f, \"DHT22_h\":%.2f, \"datetime\":\"%s\" \n}" ,WiFi.macAddress(),t,h,formattedTime.c_str() );
        publishMsg =1;
        sensorfail =0;
    }
  }
//  if (((minutes == 0)||(minutes == 20)||(minutes == 40))&&(publishMsg ==1))
  if (minutes != minuteSave)
  {
      if(publishMsg ==1)
      {
              Debug.println("Sending Response");
        IOT_send_msg();
        digitalWrite(ledPin,  HIGH);
        delay(10000);
      }
			Debug.println(formattedTime);
			Debug.print("dht22_t: ");
			Debug.println(t);
			Debug.print("dht22_h: ");
			Debug.println(h);
      Debug.print("dallas_t: ");
      Debug.println(dallas_t);
      Debug.print("ph: ");
      Debug.println(ph);

			minuteSave = timeClient.getMinutes();;
	}
  digitalWrite(ledPin,  LOW);    // turn the LED off by making the voltage LOW
  //delay(100);
}

void IOT_send_msg()
{
  if(hornbill.publish(TOPIC_NAME,payload) == 0)   // Publish the message(Temp and humidity)
  {
    Debug.print("Publish Message:");
    Debug.println(payload);
    publishMsg =0;
  }
  else
  {
      publicfail = publicfail -1;
      Debug.print("Publish Fail:");
      Debug.println(publicfail);
      publishMsg =1;
			delay(1000);
      if (publicfail == 0)
      {
          publicfail = publicfailCount;
					ESP.restart();
			}
  }
}

void initRemoteDebug()
{
	String hostNameWifi = CLIENT_ID;
	hostNameWifi.concat(".local");
	if (MDNS.begin(CLIENT_ID))
  {
			Serial.print("* MDNS responder started. Hostname -> ");
		  Serial.println(CLIENT_ID);
	}
	MDNS.addService("telnet", "tcp", 23);// Telnet server RemoteDebug
	Debug.setSerialEnabled(true);
	Debug.begin(CLIENT_ID);
	Debug.setResetCmdEnabled(true);
}

void initOTA ()
{
	ArduinoOTA
		.onStart([]() {
			String type;
			if (ArduinoOTA.getCommand() == U_FLASH)
				type = "sketch";
			else // U_SPIFFS
				type = "filesystem";

			// NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
			// //Serial.println("Start updating " + type);
			Debug.println("Start updating " + type);
		})
		.onEnd([]() {
			Debug.println("\nEnd");
		})
		.onProgress([](unsigned int progress, unsigned int total) {
			Debug.printf("Progress: %u%%\r", (progress / (total / 100)));

		})
		.onError([](ota_error_t error) {
			Debug.printf("Error[%u]: ", error);
			if (error == OTA_AUTH_ERROR) Debug.println("Auth Failed");
			else if (error == OTA_BEGIN_ERROR) Debug.println("Begin Failed");
			else if (error == OTA_CONNECT_ERROR) Debug.println("Connect Failed");
			else if (error == OTA_RECEIVE_ERROR) Debug.println("Receive Failed");
			else if (error == OTA_END_ERROR) Debug.println("End Failed");
		});
	ArduinoOTA.begin();
}

void initAWS()
{
  if(hornbill.connect(HOST_ADDRESS,CLIENT_ID)== 0) // Connect to AWS using Host Address and Cliend ID
  {
      Debug.println("Connected to AWS");
      delay(1000);
      if(0==hornbill.subscribe(TOPIC_NAME,mySubCallBackHandler))
      {
          Debug.println("Subscribe Successfull");
      }
      else
      {
          Debug.println("Subscribe Failed, Check the Thing Name and Certificates");
          publicfail = publicfail -1;
					delay(10000);
          if (publicfail == 0)
          {
              publicfail = publicfailCount;
              ESP.restart();
          }
      }
  }
  else
  {
      Debug.println("AWS connection failed, Check the HOST Address");
      publicfail = publicfail -1;
      if (publicfail == 0)
      {
          publicfail = publicfailCount;
          ESP.restart();
      }
  }
}

void initWifi()
{
  while (status != WL_CONNECTED)
  {
      Serial.print("Attempting to connect to SSID: ");
      Serial.println(WIFI_SSID);
      // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
      status = WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      // wait 5 seconds for connection:
      delay(5000);
      Serial.println("Connected to wifi");
  }
}

void IOT_received_msg()
{
  if(msgReceived == 1)
  {
      msgReceived = 0;
      Debug.print("Received Message:");
      Debug.println(rcvdPayload);
  }
}
