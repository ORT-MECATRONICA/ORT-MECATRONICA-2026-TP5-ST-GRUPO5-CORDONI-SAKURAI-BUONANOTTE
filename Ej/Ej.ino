// TP5 Firebase - Cordoni, Sakurai, Buonanotte

//defines e includes
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <ESP32Time.h>


#define SWITCH_1 35
#define SWITCH_2 34

#define LED 25
#define DHTPIN 23
#define DHTTYPE DHT11

#define SSID "MECA-IoT"
#define PASS "IoT$2026"

hw_timer_t* timer = NULL;  //declaración de timer de hardware

//tipos de variables
typedef enum {
  P1,
  P2,
  espera1,
  espera2,
  esperaAumentoCilco,
  esperaDisminucionCiclo
} tipoEstado;

//funciones
void cicloLogica();
void gmtConfigurar(int offset);
void medirTemperatura();
void maquinaDeEstados();
void imprimirTemperaturaYHora();
void imprimirCiclo();
void actualizarHora();

//config
DHT dht(DHTPIN, DHTTYPE);

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /*reset*/ U8X8_PIN_NONE);

ESP32Time rtc;

const char* ntpServer = "pool.ntp.org";
int gmtOffset = 4;
int gmtOffsetSegundos = 0;

//variables
tipoEstado estado = P1;

int SW1;
int SW2;

int horaActual = 0;
int minutoActual = 0;

float umbral = 28.0;
float hic;

int ciclo = 30;

void setup() {
  //inicio
  Serial.begin(9600);
  Serial.println();
  Serial.println("Inicio \n________");
  Serial.println();

  //setup
  pinMode(SWITCH_1, INPUT);
  pinMode(SWITCH_2, INPUT);

  pinMode(LED, OUTPUT);

  //setup sensor, pantalla y wifi
  dht.begin();

  u8g2.begin();
  u8g2.clearBuffer();

  //máquina de estados
  estado = P1;

  //máquina de estados
  Serial.print("Connecting to ");
  WiFi.begin(SSID, PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");

  gmtOffsetSegundos = gmtOffset * 3600;

  configTime(gmtOffsetSegundos, 0, ntpServer);

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    rtc.setTimeStruct(timeinfo);
  }

  rtc.offset = gmtOffsetSegundos;
}

void loop() {
  actualizarHora();

  SW1 = digitalRead(SWITCH_1);
  SW2 = digitalRead(SWITCH_2);

  medirTemperatura();

  maquinaDeEstados();
}

void medirTemperatura() {  //mide la temperatura en una función para que no se trabe el
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t) || isnan(h)) {
    return;
  } else {
    hic = dht.computeHeatIndex(t, h, false);  //mide la sensación térmica
  }
}

void maquinaDeEstados() {
  switch (estado) {
    case P1:
      imprimirTemperaturaYHora();
      //
      if (SW1 == LOW && SW2 == LOW) {
        estado = espera1;
      }
      break;
    case espera1:
      imprimirTemperaturaYHora();
      //
      if (SW1 == HIGH && SW2 == HIGH) {
        estado = P2;
      }
      break;
    case P2:
      imprimirCiclo();
      //
      if (SW1 == LOW && SW2 == HIGH) {
        estado = esperaAumentoCiclo;
      }
      if (SW1 == HIGH && SW2 == LOW) {
        estado = esperaDisminucionCiclo;
      }
      if (SW1 == LOW && SW2 == LOW) {
        estado = espera2;
      }
      break;
    case espera2:
      imprimirCiclo();
      //
      if (SW1 == HIGH && SW2 == HIGH) {
        estado = P1;
      }
      break;
    case esperaAumentoCiclo:
      imprimirCiclo();
      //
      if (SW1 == HIGH && SW2 == HIGH) {
        ciclo += 30;
        cicloLogica();
        Serial.println(ciclo);
        estado = P2;
      }
      if (SW1 == LOW && SW2 == LOW) {
        estado = espera2;
      }
      break;
    case esperaDisminucionCiclo:
      imprimirCiclo();
      //
      if (SW1 == HIGH && SW2 == HIGH) {
        ciclo -= 30;
        cicloLogica();
        Serial.println(gmtOffset);
        estado = P2;
      }
      if (SW1 == LOW && SW2 == LOW) {
        estado = espera2;
      }
      break;
  }
}

void cicloLogica() {
  if (ciclo <= 30) ciclo = 30;  //condicional corto
}

void imprimirTemperaturaYHora() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  u8g2.drawStr(0, 20, "VA: ");
  char stringTemp[10];
  sprintf(stringTemp, "%.1f C", hic);
  u8g2.drawStr(50, 20, stringTemp);

  u8g2.drawStr(0, 20, "VU: ");
  char stringUmb[10];
  sprintf(stringUmb, "%.1f C", umbral);
  u8g2.drawStr(50, 20, stringUmb);

  u8g2.sendBuffer();
}

void imprimirCiclo() {

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  u8g2.drawStr(0, 20, "Ciclo: ");
  char stringCiclo[10];
  sprintf(stringCiclo, "%.1d :", ciclo);
  u8g2.drawStr(30, 20, stringCiclo);

  u8g2.sendBuffer();
}

void actualizarHora() {
  horaActual = rtc.getHour();
  minutoActual = rtc.getMinute();
}
