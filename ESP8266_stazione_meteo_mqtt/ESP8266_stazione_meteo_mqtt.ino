#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <SPI.h>
#include <EEPROM.h> // funzionamento del deep sleep
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "config.h"

#define int_buffer_size 16 // dimensione del buffer di contenimento del payload
char buffer[int_buffer_size]; // byte di memoria del payload in buffer 

/* CREDENZIALI DI ACCESSO */
const char* mqtt_server = config_mqtt_server; // Mosquitto Server su VM PC aziendale
const int mqtt_port = config_mqtt_port;
const char* mqtt_username = config_mqtt_username; // client con accesso abilitato su Mosquitto Server  
const char* mqtt_password = config_mqtt_password; // password del client abilitato su Mosquitto Server
const char* ssid = config_ssid;
const char* password = config_password;

/* ANEMOMETRO */
struct wind_sensor_counter {
  const uint8_t PIN;
  uint32_t steed_count; //conteggio del numero di mezze rotazioni (switch magnetico)
  bool sensor_close_state; //stato dello switch/sensore
  bool previous_sensor_close_state; //stato precedente dello switch/sensore
  const float K_sensore; // 2*pigreco*raggio*fattore_correttivo  (raggio anemometro = 7.3 cm) // alcuni datasheet indicano 0.33 - NECESSARIA TARATURA
};

/*INIZIALIZZAZIONE DEI PARAMETRI*/
int intervallo_tempo_misura = 5; //secondi
float array_direzioni_vento[50];
uint32_t deep_sleep_time= (60000000); /*espresso in microsecondi (10^(-6)s*/
unsigned long delayTime = 55000;

/* INIZIALIZZAZIONE DEI CLIENT E SENSORI*/
Adafruit_BME280 bme; // I2C
WiFiClient espClient; // client ESP8266
PubSubClient client(mqtt_server, mqtt_port, espClient); //con CALLBACK ATTIVO: PubSubClient client(mqtt_server, mqtt_port, callback, espClient);
wind_sensor_counter ws1 = {D7, 0, false, false, 0.46};

/* FUNZIONI */

void start_deep_sleep() {
  ESP.deepSleep(deep_sleep_time);
}

void connect_to_wifi() {
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void mqtt_publish(const char* topic_mqtt){
  client.connect("esp8266_Client", mqtt_username, mqtt_password);
  client.publish(topic_mqtt, buffer); 
}

void connect_to_bme280(){
  unsigned status;
  status = bme.begin(0x76);
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
    while (1) delay(10);    
  }
}

void get_bme280_values() {
    dtostrf(bme.readTemperature(), 3, 1, buffer);
    Serial.print("Temperature = ");
    Serial.print(buffer);
    Serial.println(" °C");
    mqtt_publish("Temperature");
    
    dtostrf(bme.readPressure()/100.0F, 3, 2, buffer);
    Serial.print("Pressure = ");
    Serial.print(buffer);
    Serial.println(" hPa");
    mqtt_publish("Pressure");

    dtostrf(bme.readHumidity(), 3, 0, buffer);
    Serial.print("Humidity = ");
    Serial.print(buffer);
    Serial.println(" %");
    mqtt_publish("Humidity");
    Serial.println();
}

float get_actual_wind_direction(){ /* CONVERSIONE DEI VALORI LETTI DAL SENSORE GONIOANEMOMETRICO */
  unsigned int read_analog_value;
  read_analog_value = analogRead(A0); 
  
/* I VALORI ANALOGICI SONO LETTI AI CAPI DI UN PARTITORE DI TENSIONE RESISTIVO. 
 *  IL VALORE DI RIFERIMENTO E' QUINDI SOGGETTO A VARIAZIONI A CAUSA DI:
 *  - VARIAZIONI DELLA TENSIONE DI ALIMENTAZIONE
 *  - VARIAZIONI DI TEMPERATURA
 *  - ASSORBIMENTO DI CORRENTE E MAGGIORI CDT SULLA LINEA
 *  E' NECESSARIO CORREGGERE MANUALMENTE LE SOGLIE
 *  
 *  Serial.println("valore direzione letta");
 *  Serial.println(__actual_val__);
 */
 
  if (read_analog_value <= 41) return (-1); //ERRORE
  if (read_analog_value <= 91) return (247.5); //E-SE
  if (read_analog_value <= 99) return (292.5); //E-NE
  if (read_analog_value <= 128) return (270); //EST
  if (read_analog_value <= 177) return (202.5); //S-SE
  if (read_analog_value <= 238) return (225); //SE
  if (read_analog_value <= 291) return (157.5);//S-SW
  if (read_analog_value <= 376) return (180);//S
  if (read_analog_value <= 467) return (337.5);//N-NE
  if (read_analog_value <= 564) return (315);//NE
  if (read_analog_value <= 648) return (112.5);//W-SW
  if (read_analog_value <= 701) return (135);//SW
  if (read_analog_value <= 780) return (22.5);//N-NW
  if (read_analog_value <= 841) return (0); //N
  if (read_analog_value <= 891) return (67.5);//W-NW
  if (read_analog_value <= 950) return (45);//NW
  if (read_analog_value <= 1002) return (90);//W
  return (-1); // ERRORE
}
/* OTTENIMENTO DELLE INFORMAZIONI MEDIE SULLA DIREZIONE DEL VENTO */
void calcolate_mean_wind_direction(){
  int i = 0;
  double seno_totale = 0;
  double coseno_totale = 0;
    
  for (i; i<ws1.steed_count; i++) {
    double angolo_radianti = array_direzioni_vento[i]*2*PI/360;
    double seno_radianti = sin(angolo_radianti);
    seno_totale = seno_totale+seno_radianti;
    double coseno_radianti = cos(angolo_radianti);
    coseno_totale = coseno_totale+coseno_radianti;
  }
  
  double seno_medio = seno_totale/ws1.steed_count;
  double coseno_medio = coseno_totale/ws1.steed_count;
  double angolo_medio_radianti = atan2(seno_medio,coseno_medio);
  double angolo_medio_gradi = angolo_medio_radianti*360/(2*PI);
  if (angolo_medio_gradi < 0){
    angolo_medio_gradi = 360+angolo_medio_gradi;
    }
    dtostrf(angolo_medio_gradi, 3, 2, buffer);
    Serial.print("Wind Direction = ");
    Serial.print(buffer);
    Serial.println(" °");
    mqtt_publish("Wind Direction");
}

void get_wind_values() { /* OTTENIMENTO DI TUTTE LE INFORMAZIONI SUL VENTO */
  //Serial.println("Inizio lettura anemometro");
  unsigned long istante_iniziale = millis();
  unsigned long actual_time = 0;
  int array_index = 0;
  ws1.steed_count = 0;
  
  while (actual_time <= 1000*intervallo_tempo_misura) {
    actual_time = millis() - istante_iniziale;
    ws1.sensor_close_state = digitalRead(ws1.PIN);
    if (ws1.sensor_close_state != ws1.previous_sensor_close_state) {
      if (ws1.sensor_close_state == LOW){
        float wind_direction = get_actual_wind_direction();
        array_direzioni_vento[ws1.steed_count] = wind_direction;
        ws1.steed_count++;
      }
    }
    ws1.previous_sensor_close_state = ws1.sensor_close_state;
    yield(); // NECESSARIO PER STABILITA' DEL MODULO ESP8266 E DEL WDT
  }
  float num_rotazioni = ws1.steed_count/2; // 2 impulsi per singola rotazione
  double intervallo_temporale = actual_time/1000; //secondi
  double wind_speed_m_s = ws1.K_sensore*num_rotazioni/intervallo_temporale; // metri/secondo // Vt = 2*pigreco*r (m/s)
  double wind_speed_km_h = wind_speed_m_s*3.6;
//  Serial.println("N. di impulsi conteggiati: ");
//  Serial.println(ws1.steed_count);

  dtostrf(wind_speed_km_h, 3, 2, buffer);
  Serial.print("Wind Speed = ");
  Serial.print(buffer);
  Serial.println(" km/h");
  mqtt_publish("Wind Speed");
  if ( wind_speed_km_h !=0){
    calcolate_mean_wind_direction();
   }
}

void setup() {
/* ATTIVAZIONE DELLA EEPROM PER UTILIZZO DEL DEEP SLEEP - MODULO ESP8266*/
  EEPROM.begin(512);

/* ATTIVAZIONE DEL REGOLATORE DI TENSIONE */
  pinMode(D8, OUTPUT);
  digitalWrite(D8, HIGH); 
  
/* ATTIVAZIONE DEL PIN DI LETTURA DELLA VELOCITA' DEL VENTO*/
  pinMode(ws1.PIN, INPUT_PULLUP); 
  Serial.begin(9600);
  
/* INIZIALIZZAZIONE CONNESSIONI */
  connect_to_wifi();
  Serial.println();
  connect_to_bme280();
  client.setServer(mqtt_server, 1883);
  
/* ROUTINE DI LETTURA DEI PARAMETRI */
  get_bme280_values();
  get_wind_values();
  delay(10); /* STABILITA' */
  Serial.println();
  start_deep_sleep();
}

void loop() {
  delay(1);
}
