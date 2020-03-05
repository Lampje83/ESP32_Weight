#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

#include <M5Stack.h>
#include <HX711.h>
#include <U8g2lib.h>
#include <U8x8lib.h>

const char* ssid = "MACHINEWPL";
const char* password = "19Lampje83";

typedef struct {
  const char* ssid;
  const char* password;
} wifiAp;

#define NUM_AP 3
byte apnum = 0;

wifiAp ap[NUM_AP] = { 
  { "MACHINEWPL",  "19Lampje83" },
  { "Lampje",      "19pentarick83" },
  { "Linksys",     "lg1892pdhxrg" } 
};

AsyncWebServer server(80);
String ledState;

U8G2_SH1107_64X128_F_4W_HW_SPI u8g2(U8G2_R0, 14, /* dc=*/ 27, /* reset=*/ 33);
U8G2LOG u8g2log;

HX711 hx711;

//U8X8_SH1107_64X128_4W_HW_SPI u8x8(14, /* dc=*/ 27, /* reset=*/ 33);
//U8X8LOG u8x8log;

#define LedPin 19
#define DOUTPin 13
#define SCKPin  25
#define U8LOG_WIDTH 16
#define U8LOG_HEIGHT 21

uint8_t u8log_buffer[U8LOG_WIDTH*U8LOG_HEIGHT];

String processor(const String& var) {
  Serial.println(var);
  if (var == "STATE") {
    if (digitalRead (LedPin)) {
      ledState = "ON";  
    } else {
      ledState = "OFF";
    }
    Serial.print (ledState);
    return ledState;
  }
  return String ();
}

void setup() {
  //u8x8.begin();
  //u8x8.setFont (u8x8_font_chroma48medium8_r);
  u8g2.begin();
  u8g2.setFont (u8g2_font_tom_thumb_4x6_mf);
  
  //u8x8log.begin (u8x8, U8LOG_WIDTH, U8LOG_HEIGHT, u8log_buffer);
  //u8x8log.setRedrawMode (0);
  u8g2log.begin (u8g2, U8LOG_WIDTH, U8LOG_HEIGHT, u8log_buffer);
  u8g2log.setLineHeightOffset (0);
  u8g2log.setRedrawMode (0);

  Serial.begin (115200);
  Serial.println ("Hello!");
  
  pinMode (LedPin, OUTPUT);

  hx711.begin (DOUTPin, SCKPin, 64);
  delay (100);
  hx711.tare ();
  Serial.println ("HX711 initialized");
  
  if (!SPIFFS.begin (true)) {
    u8g2.println ("Failed mounting SPIFFS!");
    Serial.println ("Failed mounting SPIFFS!");
    return;
  }
  
  //WiFi.begin (ssid, password);
  WiFi.begin (ap[apnum].ssid, ap[apnum].password);
  
  u8g2log.printf ("Connecting to %s...\n", ap[apnum].ssid);
  Serial.printf ("Connecting to %s...\n", ap[apnum].ssid);

  int count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    count++;
    if (count >= 5) {
      WiFi.disconnect();
      count = 0;
      apnum = (apnum + 1) % NUM_AP;

      WiFi.begin (ap[apnum].ssid, ap[apnum].password);  
      u8g2log.printf ("Connecting to %s...\n", ap[apnum].ssid);
      Serial.printf ("Connecting to %s...\n", ap[apnum].ssid);

    }
    delay (1000);
  }

  Serial.println (WiFi.localIP());
  u8g2log.print ("IP address: ");
  u8g2log.println (WiFi.localIP());

  server.on ("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send (SPIFFS, "/index.html", String(), false, processor);
    u8g2log.println ("serve index.html");
  });

  server.on ("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send (SPIFFS, "/style.css", "text/css");
    u8g2log.println ("serve style.css");
  });

  server.on ("/on", HTTP_GET, [](AsyncWebServerRequest *request) {
    digitalWrite (LedPin, HIGH);
    u8g2log.println ("Led ON");
    request->send (SPIFFS, "/index.html", String(), false, processor);
  });

  server.on ("/off", HTTP_GET, [](AsyncWebServerRequest *request) {
    digitalWrite (LedPin, LOW);
    u8g2log.println ("Led OFF");
    request->send (SPIFFS, "/index.html", String(), false, processor);
  });

  server.on ("/weight", HTTP_GET, [](AsyncWebServerRequest *request) {
    float value;
    
    if (hx711.wait_ready_timeout (1000)) {
      value = hx711.get_units (10);
      u8g2log.printf ("r: %f\n", value);
      request->send (200, "text/plain", String(value));
    } else {
      u8g2log.println ("HX711 not found");
      request->send (200, "text/plain", "-------");
    }
    
  });

  server.on ("/tare", HTTP_GET, [](AsyncWebServerRequest *request) {
    float value;
    
    if (hx711.wait_ready_timeout (1000)) {
      hx711.tare ();
      u8g2log.println ("Tare");
      request->send (200);
    } else {
      u8g2log.println ("HX711 not found");
      request->send (404);
    }
    
  });

  server.on ("/scale", HTTP_GET, [](AsyncWebServerRequest *request) {
    float value, knownmass;
    AsyncWebParameter *param;
    if (request->hasParam("mass")) {
      // calculate scale according to known mass
      value = hx711.get_value (10);
      u8g2log.printf ("v: %.0f\n", value);
      Serial.printf ("v: %.0f\n", value);
      
      param = request->getParam("mass");
      knownmass = param->value().toFloat();
      u8g2log.printf ("mass: %.1f\n", knownmass);
      Serial.printf ("Mass: %f\n", knownmass);

      hx711.set_scale (value / knownmass);
      u8g2log.printf ("scale: %.3f\n", hx711.get_scale());
      Serial.printf ("scale: %.3f\n", hx711.get_scale());
    } else if (request->hasParam("value")) {
      // set scale directly
      param = request->getParam("value");
      hx711.set_scale (param->value().toFloat());
      u8g2log.printf ("scale: %.3f\n", hx711.get_scale());
      Serial.printf ("scale: %.3f\n", hx711.get_scale());      
    }
    request->send (200, "text/plain", String(hx711.get_scale()));
  });
  server.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  //analogReadResolution (10);
  //u8x8log.print(int(analogRead(SensorPin)));
  //u8x8log.print("Hallo");
  //u8x8log.print("\n");
  //u8g2log.print("Hallo");
  //u8g2log.print("\n");
  //delay (100);
}
