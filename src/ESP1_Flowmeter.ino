#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <Ticker.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

Ticker interrupcaoSegundo;

//PULSO
#define portaPulso 3    // Flow sensor 

//LED
#define pinoLed 1       // Building LED (1 or 2 depends on ESP-1)

// WiFi AP Credentials
const char* ssid = "CezarNet";
const char* password = "nze-4206-1549";

// WiFi Client Static IP
IPAddress ip(192, 168, 0, 16);    
IPAddress gateway(192,168,0,1); 
IPAddress subnet(255,255,255,0);

// Server IP and Port numbers
WiFiUDP Udp;
IPAddress serverIP1(192, 168, 0, 2); 
IPAddress serverIP2(192, 168, 0, 4); 
unsigned int porta = 5678;  

// Global variables
int ntry = 0;
bool bitLED = 0;
volatile int  pulsosContados = 0;
bool tempo = 0;
uint32_t timeoutLeitura = 0;

// Interrupts
void IRAM_ATTR interrupcaoTempo(){
  tempo = 1;
  timeoutLeitura++;
}

void IRAM_ATTR interrupcaoExterna (){ 
   pulsosContados++;
} 

void setup()
{
  Serial.begin(115200,SERIAL_8N1,SERIAL_TX_ONLY); 
  Serial.println();

  delay(100);
  
  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.config(ip, gateway, subnet);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (ntry > 60) {
      ESP.restart();
      ntry = 0;       
    }
    ntry++;
  }
  
  // Listening to UDP port
  Udp.begin(porta);
  
  Serial.println("");
  Serial.print("Connected to WiFi as: ");
  Serial.println(ip);
  Serial.println("");
  Serial.print("UDP Servers IP are ");
  Serial.print(serverIP1);
  Serial.print(" and ");
  Serial.print(serverIP2);

  delay(1000);
  
  // OTA settings
  ArduinoOTA.setHostname("esp8266-Flowmeter");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();

  // Signaling Wi-Fi Setup completion
  pinMode(pinoLed, OUTPUT);
  for (int i = 1; i <= 10; i++) {
    digitalWrite(pinoLed,HIGH);
    delay(500); 
    digitalWrite(pinoLed,LOW);
    delay(500);   
  }

  // Interrupt Setup
   pinMode(portaPulso, INPUT_PULLUP);
   attachInterrupt(portaPulso, interrupcaoExterna, RISING);
   sei();
   //interrupcaoSegundo.attach_ms(200, interrupcaoTempo);
   digitalWrite(pinoLed,LOW);//Deixo o LED aceso  
}

void loop()
{
  ArduinoOTA.handle();
  if(pulsosContados)
  {
    tempo = 0;
    timeoutLeitura = 0;
    interrupcaoSegundo.attach_ms(200, interrupcaoTempo);
    while(timeoutLeitura < 300)//60 segundos
    { 
      yield();       //Alimenta/reseta o Software Watchdog(3.2 segundos).
      ESP.wdtFeed(); //Alimenta/reseta o Hardware Watchdog(8 segundos)
      if(tempo)
      { 
        if(pulsosContados)
        {
          timeoutLeitura = 0;//Zera o tempo de tolerância caso perceba alguma vazão em 1 minito
        }
        tempo = 0;
        Udp.beginPacket(serverIP1, porta);
        Udp.write(int(pulsosContados));
        Udp.endPacket();
        Udp.beginPacket(serverIP2, porta);
        Udp.write(int(pulsosContados));
        Udp.endPacket();
        pulsosContados = 0;
        //delay(5);
      
        if(bitLED)
        {
          digitalWrite(pinoLed,HIGH);
        }
        else
        {
          digitalWrite(pinoLed,LOW);
        }
        bitLED = !bitLED;
      }
    }
    
    Udp.beginPacket(serverIP1, porta);
    Udp.write(int(250));//Envio sinalizador de fim de leitura
    Udp.endPacket();
    Udp.beginPacket(serverIP2, porta);
    Udp.write(int(250));//Envio sinalizador de fim de leitura
    Udp.endPacket();
    digitalWrite(pinoLed,LOW);//Deixo o LED aceso
    interrupcaoSegundo.detach();//Desligo a interupção de tempo
  }

  yield();       //Alimenta/reseta o Software Watchdog(3.2 segundos).
  ESP.wdtFeed(); //Alimenta/reseta o Hardware Watchdog(8 segundos)

}
