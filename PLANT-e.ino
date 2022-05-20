#include <DallasTemperature.h>
#include <OneWire.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFiNINA.h>
#include<NTPClient.h>
#include<WiFiUdp.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
//#include "arduino_secrets.h"
#define PIN_POMPA 7
//CONFIG DISPLAY OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//TIMERMILLIS
unsigned long t1, dt1;
unsigned long t2, dt2;
unsigned long t3, dt3;

//VAR SOIL/POMPA
int terra;
int th_terra = 20;
bool POMPA;
int t_on_pompa = 1000;
int statoPompa = LOW;

//VAR LUX
int valoreLux;
const float GAMMA = 0.7;
const float RL10 = 50;



//DATI WIFI
char ssid[] = "XXXXXXXXX";        // NOME SSID
char pass[] = "XXXXXXXXX";    // PASSWORD WPA WI-FI
int status = WL_IDLE_STATUS;     // STATO RADIO WI-FI

//TEMP INIT
OneWire oneWire(3);
DallasTemperature temp(&oneWire);

//WiFI SERVER
WiFiServer server(80);

//DATI ORARIO
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);




void setup() {

  Serial.begin(9600);

  //INIZIALIZZAZIONE DISPLAY OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (true);
  }
  display.display();
  display.clearDisplay();

  //CONTROLLO STATO WIFI
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.print("CONNESSIONE FALLITA!!");
    display.print("CONNESSIONE FALLITA!!");
    while (true);
  }

  //CONTROLLO FIRMWARE
  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("AGGIORNA IL FIRMWARE");
    display.println("AGGIORNA IL FIRMWARE");
  }

  //TENTATIVO CONNESSIONE
  while (status != WL_CONNECTED) {
    display.clearDisplay(); //clears display
    display.setTextColor(SSD1306_WHITE); //sets color to white
    display.setTextSize(1); //sets text size to 2
    display.setCursor(0, 0); //x, y starting coordinates
    display.cp437(true);
    Serial.print("CONNETTENDO A: ");
    display.println("CONNETTENDO A: ");
    Serial.println(ssid);
    display.println(ssid);
    display.display();
    delay(200);

    //CONNESSIONE
    status = WiFi.begin(ssid, pass);
    delay(10000);
  }
  server.begin();
  
  //CONNESSIONE OK
  display.clearDisplay(); //clears display
  display.setTextColor(SSD1306_WHITE); //sets color to white
  display.setTextSize(1); //sets text size to 2
  display.setCursor(0, 0); //x, y starting coordinates
  display.cp437(true);
  Serial.print("SEI CONNESSO ALLA RETE!!");
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("CONNESSO!!");
  display.display();
  delay(5000);
  display.clearDisplay();
  // printCurrentNet();
  timeClient.begin();
  printWifiStatus();

  //IMPOSTAZIONI POMPA
  pinMode(PIN_POMPA, OUTPUT);
  digitalWrite(PIN_POMPA, LOW);
}

void loop() {
  //VAR TEMP
float varTemp = readTemp();
int varSoil = readSoil();
int varAcqua = readWaterLevel();
float varLux = readLux();

  dt1 = millis() - t1;
  if (dt1 >= 10000) {
    t1 = millis();
    printTime();
    readTemp();
    readSoil();
    readLux();
    readWaterLevel();
  }

  if (POMPA) {
    dt3 = millis() - t3;
    if (dt3 > t_on_pompa) {
      t3 = millis();
      statoPompa = !statoPompa;
      if (statoPompa) {
        digitalWrite(PIN_POMPA, HIGH);
        t_on_pompa = 2000;
      }
      else {
        digitalWrite(PIN_POMPA, LOW);
        t_on_pompa = 10000;
      }
    }
  }
  printPlante();
  
  // listen for incoming clients
  WiFiClient client = server.available();
  if (client) {
    Serial.println("new client");
    // an HTTP request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the HTTP request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard HTTP response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println("Refresh: 5");  // refresh the page automatically every 5 sec
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          // output the value of each analog input pin
          client.print("valore temperatura: ");
          client.println(varTemp);
          client.println("<br />");
          client.print("valore umidita: ");
          client.println(varSoil);
          client.println(" %");
          client.println("<br />");
          client.print("Livello Acqua: ");
          client.println(varAcqua);
          client.println("<br />");
          client.print("Valore Lux: ");
          client.println(varLux);
          client.println("<br />");
          client.println("<br />");
          IPAddress ip = WiFi.localIP();
          Serial.print("IP Address: ");
          Serial.println(ip);
          client.println("</html>");
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);

    // close the connection:
    client.stop();
    Serial.println("client disconnected");
  }
}

int readSoil() {
  terra = 0;
  for (int i = 0; i < 10; i++) {
    terra += analogRead(A2);
  }
  terra = terra / 10;
  terra = map(terra, 522, 1023, 100, 0);

  if (terra < 20) {
    POMPA = HIGH;
  }
  else {
    POMPA = LOW;
    digitalWrite(PIN_POMPA, LOW);
  }
  display.setTextColor(WHITE, BLACK);
  display.setTextSize(1);
  display.setCursor(0, 25);
  display.cp437(true);
  display.print("HUM: ");
  display.print(terra);
  display.print("%");
  return (terra);
}


float readTemp() {
  float tc, tf;
  temp.begin();
  temp.requestTemperatures();
  tc = temp.getTempCByIndex(0);
  display.setTextColor(WHITE, BLACK);
  display.setTextSize(1);
  display.setCursor(0, 17);
  display.cp437(true);
  display.print("TEMP: ");
  display.print(tc);
  display.print(" C");
  return(tc);
  }

int readWaterLevel(){
  int valoreAcqua;
  for (int i=0; i < 10; i++){
    valoreAcqua += analogRead(A3);
  }
  valoreAcqua = valoreAcqua / 10;
  return(valoreAcqua);
}

float readLux() {
  for (int i = 0; i < 10; i++) {
    valoreLux += analogRead(A1);
  }
  valoreLux = valoreLux / 10;
  float voltaggio = valoreLux / 1024.*5;
  float resistenza = 4057 * voltaggio / (1 - voltaggio / 5);
  float lux = pow(RL10 * 1e3 * pow(10, GAMMA) / resistenza, (1 / GAMMA));
  display.setTextColor(WHITE, BLACK);
  display.setTextSize(1);
  display.setCursor(0, 9);
  display.cp437(true);
  display.print("LUX: ");
  display.print(lux);
  return(lux);
}

void printPlante() {
  display.setCursor(0, 0); //x, y starting coordinate
  display.cp437(true);
  display.println("PIANTA");
  display.display();
}

void printTime() {

  timeClient.update();
  timeClient.getHours();
  if (timeClient.getHours() < 10) {
    display.setTextColor(WHITE, BLACK); //sets color to white
    display.setTextSize(1); //sets text size to 2
    display.setCursor(95, 0); //x, y starting coordinates
    display.cp437(true);
    display.print("0");
    display.print(timeClient.getHours());
    display.display();
  }
  else {
    display.setTextColor(WHITE, BLACK); //sets color to white
    display.setTextSize(1); //sets text size to 2
    display.setCursor(95, 0); //x, y starting coordinates
    display.cp437(true);
    display.print(timeClient.getHours());
    display.display();
  }
  display.setCursor(108, 0);
  display.print(":");
  display.display();
  timeClient.getMinutes() ;
  if (timeClient.getMinutes() < 10) {
    display.setTextColor(WHITE, BLACK); //sets color to white
    display.setTextSize(1); //sets text size to 2
    display.setCursor(114, 0); //x, y starting coordinates
    display.cp437(true);
    display.print("0");
    display.setCursor(120, 0);
    display.print(timeClient.getMinutes());
    display.display();
  }
  else {
    display.setTextColor(WHITE, BLACK); //sets color to white
    display.setTextSize(1); //sets text size to 2
    display.setCursor(114, 0);
    display.print(timeClient.getMinutes());
    display.display();
  }
  /*if ((timeClient.getHours() >= 7) && (timeClient.getHours() < 21)) {
    GIORNO = true;
  }
  else {
    GIORNO = false;
  }*/
}
void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}
