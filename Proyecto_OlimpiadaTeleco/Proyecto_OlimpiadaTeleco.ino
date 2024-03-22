#include <WiFi.h>
#include "ThingSpeak.h"

#define TdsSensorPin A0
#define VREF 5.0              // analog reference voltage(Volt) of the ADC
#define SCOUNT  30            // sum of sample point

int analogBuffer[SCOUNT];     // store the analog value in the array, read from ADC
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0;
int copyIndex = 0;

float averageVoltage = 0;
float tdsValue = 0;
float temperature = 16;       // current temperature for compensation

const int MQ_PIN = A0;    // Pin del sensor
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

const char* ssid = "REPLACE_WITH_YOUR_SSID";   // your network SSID (name) 
const char* password = "REPLACE_WITH_YOUR_PASSWORD";   // your network password

WiFiClient  client;

unsigned long myChannelNumber = 2479298;
const char * myWriteAPIKey = "XXXXXXXXXXXXXXXX";

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 30000;

int getMedianNum(int bArray[], int iFilterLen){
  int bTab[iFilterLen];
  for (byte i = 0; i<iFilterLen; i++)
  bTab[i] = bArray[i];
  int i, j, bTemp;
  for (j = 0; j < iFilterLen - 1; j++) {
    for (i = 0; i < iFilterLen - j - 1; i++) {
      if (bTab[i] > bTab[i + 1]) {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  if ((iFilterLen & 1) > 0){
    bTemp = bTab[(iFilterLen - 1) / 2];
  }
  else {
    bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  }
  return bTemp;
}

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

int airQuality(){
  float rs_med = readMQ(MQ_PIN);    // Obtener la Rs promedio
  float concentration = getConcentration(rs_med/R0);  // Obtener la concentración
  
  // Mostrar el valor de la concentración por serial
  Serial.println("Concentración: ");
  Serial.println(concentration);
}

int waterQuality() {
  static unsigned long analogSampleTimepoint = millis();
  if(millis()-analogSampleTimepoint > 40U){     //every 40 milliseconds,read the analog value from the ADC
    analogSampleTimepoint = millis();
    analogBuffer[analogBufferIndex] = analogRead(TdsSensorPin);    //read the analog value and store into the buffer
    analogBufferIndex++;
    if(analogBufferIndex == SCOUNT){ 
      analogBufferIndex = 0;
    }
  }   
  
  static unsigned long printTimepoint = millis();
  if(millis()-printTimepoint > 800U){
    printTimepoint = millis();
    for(copyIndex=0; copyIndex<SCOUNT; copyIndex++){
      analogBufferTemp[copyIndex] = analogBuffer[copyIndex];
      
      // read the analog value more stable by the median filtering algorithm, and convert to voltage value
      averageVoltage = getMedianNum(analogBufferTemp,SCOUNT) * (float)VREF / 1024.0;
      
      //temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0)); 
      float compensationCoefficient = 1.0+0.02*(temperature-25.0);
      //temperature compensation
      float compensationVoltage=averageVoltage/compensationCoefficient;
      
      //convert voltage value to tds value
      tdsValue=(133.42*compensationVoltage*compensationVoltage*compensationVoltage - 255.86*compensationVoltage*compensationVoltage + 857.39*compensationVoltage)*0.5;
      
      //Serial.print("voltage:");
      //Serial.print(averageVoltage,2);
      //Serial.print("V   ");
      Serial.print("TDS Value:");
      Serial.print(tdsValue,0);
      Serial.println("ppm");
    }
  }
}

void setup() {
  Serial.begin(115200);  //Initialize serial

  pinMode(TdsSensorPin,INPUT);
  
  WiFi.mode(WIFI_STA);   
  
  ThingSpeak.begin(client);  // Initialize ThingSpeak
}

void loop() {
  if ((millis() - lastTime) > timerDelay) {
    
    // Connect or reconnect to WiFi
    if(WiFi.status() != WL_CONNECTED){
      Serial.print("Attempting to connect");
      while(WiFi.status() != WL_CONNECTED){
        WiFi.begin(ssid, password); 
        delay(5000);     
      } 
      Serial.println("\nConnected.");
    }

    int airQ = airQuality();
    int waterQ = waterQuality();
    
    int x = ThingSpeak.writeField(myChannelNumber, 1, waterQ, myWriteAPIKey);
    int y = ThingSpeak.writeField(myChannelNumber, 2, airQ, myWriteAPIKey);

    if(x == 200){
      Serial.println("Channel update successful.");
    }
    else{
      Serial.println("Problem updating channel. HTTP error code " + String(x));
    }
    lastTime = millis();
  }
}
