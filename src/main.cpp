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
NTPClient timeClient(ntpUDP,"europe.pool.ntp.org", 3600, 60000);
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
char TOPIC_PJ[] = "sdkTest/";
char TOPIC_NAME[] = "sdkTest/ESP32_02";

//#define HOST_NAME *CLIENT_ID;
int status = WL_IDLE_STATUS;
RemoteDebug Debug;
const char* host = "ESP32_H01";
int tick=0,msgCount=0,msgReceived = 0,publishMsg=0;;
char payload[512];
char rcvdPayload[512];
//reset after fail publish
int publicfail = 5;
int publicfailCount = 5;

float h,t;
String formattedTime;

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
	initRemoteDebug();
	initAWS();
  analogSetPinAttenuation (34, ADC_11db);
  sensors.begin();     //dallas start
	initOTA ();
	dht.begin();
	timeClient.begin();
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

	h = dht.readHumidity();
	t = dht.readTemperature();


	if (isnan(h) || isnan(t)) {
			Debug.println("Failed to read from DHT sensor!");
			delay(2000);
	}
	else
	{
			// Create the payload for publishing
			formattedTime = timeClient.getTimeStampString();
			snprintf(payload,sizeof(payload),"{\n \"thingid\":\"ESP32_02\", \"Temperature\":%.2f, \"Humidity\":%.2f, \"datetime\":\"%s\" \n}" ,t,h,formattedTime.c_str() );
			publishMsg =1;

	}



  if ((minutes != minuteSave)&&(publishMsg ==1))
  {
			Debug.println("Sending Response");
			Debug.println(formattedTime);
			Debug.print("temperature : ");
			Debug.println(t);
			Debug.print("Hum : ");
			Debug.println(h);
			IOT_send_msg();
			digitalWrite(ledPin,  HIGH);
			minuteSave = timeClient.getMinutes();;

      publishMsg =1;
      sprintf(payload,"Hi ESSILOR demo IOT %s", formattedTime.c_str());
      if(hornbill.publish(TOPIC_NAME,payload) == 0)
      {
        Debug.print("Publish Message:");
        Debug.println(payload);
      }
	}
  digitalWrite(ledPin,  LOW);    // turn the LED off by making the voltage LOW
  delay(100);
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
			delay(10000);
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
