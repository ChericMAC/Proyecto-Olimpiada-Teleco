#include "heltec.h"
#include "ThingSpeak.h"
#include <WiFi.h>

// Paràmetres Wi-Fi que s'han de modificar
char ssid[] = "Cheric64";   // SSID de la Wi-Fi  
char pass[] = "Supermario64*";   // Password (clau) de la Wi-Fi
// Creació del client (connexió) Wi-Fi
WiFiClient  client;

// Paràmetres del canal de ThingSpeak que s'han de modificar
unsigned long myChannelNumber = 2479298;
const char * myWriteAPIKey = "RSQ73UHBR5ABCYK3";

//TDS sensor variables
#define SERIAL Serial
#define sensorPin 32

int sensorValue = 0;
float tdsValue = 0;
float Voltage = 0;

//Air Quality (MQ-2) variables

const int MQ_PIN = 12;    // Pin del sensor
const int RL_VALUE = 5;    // Resistencia RL del modulo en Kilo ohms
const int R0 = 10;          // Resistencia R0 del sensor en Kilo ohms

// Datos para lectura multiple
const int READ_SAMPLE_INTERVAL = 100;    // Tiempo entre muestras
const int READ_SAMPLE_TIMES = 5;     // Numero muestras

// Ajustar estos valores para vuestro sensor según el Datasheet
// (opcionalmente, según la calibración que hayáis realizado)
const float X0 = 200;
const float Y0 = 1.7;
const float X1 = 10000;
const float Y1 = 0.28;

// Puntos de la curva de concentración {X, Y}
const float punto0[] = { log10(X0), log10(Y0) };
const float punto1[] = { log10(X1), log10(Y1) };

// Calcular pendiente y coordenada abscisas
const float scope = (punto1[1] - punto0[1]) / (punto1[0] - punto0[0]);
const float coord = punto0[1] - punto0[0] * scope;

//Main code

void setup()
{
  Serial.begin(115200);
	Heltec.begin(true /*DisplayEnable Enable*/, false /*LoRa disable */,true /*Serial enable*/);
  Serial.println("Heltec begin fet");
  Heltec.display->init();
  Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT); 
  Heltec.display->setFont(ArialMT_Plain_10);
  Heltec.display->clear();
  Serial.println("Display preparat");
  ThingSpeak.begin(client);  // Inicialitza el client de ThingSpeak
  Serial.println("Client thingspeak creat");
  // Activem Wi-Fi i conectem a la xarxa indicada
  // Si abans estava connectat a una altra xarxa, el desconnectem
  WiFi.mode(WIFI_STA); 
  if(WiFi.status() != WL_CONNECTED){
    Heltec.display -> drawString(0, 20, "Connectant Wi-Fi "+String(ssid));
    Heltec.display -> display();
    Serial.println("Connectant a la Wi-Fi...");
    while(WiFi.status() != WL_CONNECTED){
      WiFi.begin(ssid, pass);  // Connecta a una xarxa amb seguretat del tipus WPA/WPA2
      delay(5000);     
    } 
    // Feedback per pantalla
    Heltec.display -> drawString(0, 30, "Conectado Wi-Fi "+String(ssid));
    Heltec.display -> display();
    // Feedback pel monitor sèrie
    Serial.println("Connectat a la Wi-Fi "+String(ssid));
    Serial.print("Adr. IP: ");
    Serial.println(WiFi.localIP());
  }
}

void loop()
{
  ThingSpeak.setField(1, waterQ());
  ThingSpeak.setField(2, airQ());

  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

  if(x == 200){
    Heltec.display->drawString(0, 40, "Canal actualizat!");
    Serial.println("Canal actualizat");
  }
  else{
    // S'hi ha algun problema s'indica 
    Heltec.display->drawString(0, 40, "Error actualizant el canal");
    Heltec.display->drawString(0, 50, "Error HTTP n. " + String(x));
    Serial.println("Problema actualizant el canal. Error HTTP n. " + String(x));
  }

  Heltec.display->display(); 

  delay(30000);
}

//Air Quality functions

float readMQ(int mq_pin)
{
  float rs = 0;
  for (int i = 0;i<READ_SAMPLE_TIMES;i++) {
    rs += getMQResistance(analogRead(mq_pin));
    delay(READ_SAMPLE_INTERVAL);
  }
  return rs / READ_SAMPLE_TIMES;
}

// Obtener resistencia a partir de la lectura analogica
float getMQResistance(int raw_adc)
{
  return (((float)RL_VALUE / 1000.0*(1023 - raw_adc) / raw_adc));
}

// Obtener concentracion 10^(coord + scope * log (rs/r0)
float getConcentration(float rs_ro_ratio)
{
  return pow(10, coord + scope * log(rs_ro_ratio));
}

int airQ()
{
  float rs_med = readMQ(MQ_PIN);    // Obtener la Rs promedio
  float concentration = getConcentration(rs_med/R0);  // Obtener la concentración
  return concentration;
}

//Water Quality functions

float waterQ()
{
  sensorValue = analogRead(sensorPin);
  Voltage = sensorValue*5/1024.0; //Convert analog reading to Voltage
  tdsValue=(133.42/Voltage*Voltage*Voltage - 255.86*Voltage*Voltage + 857.39*Voltage)*0.5; //Convert voltage value to TDS value
  return tdsValue;
}