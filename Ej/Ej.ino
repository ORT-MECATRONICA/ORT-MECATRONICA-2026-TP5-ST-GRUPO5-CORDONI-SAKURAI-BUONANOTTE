// TP5 Firebase - Cordoni, Sakurai, Buonanotte

//defines e includes
#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP32Time.h>
#include <FirebaseClient.h>
#include "time.h"

#define SWITCH_1 35
#define SWITCH_2 34

#define LED 25
#define DHTPIN 23
#define DHTTYPE DHT11

#define SSID "MECA-IoT"
#define PASS "IoT$2026"

#define DATABASE_URL "https://st-esp-project-2ed88-default-rtdb.firebaseio.com"
#define WEB_API_KEY "AIzaSyCc5yeF_D4kaD5plZTnv71nW4IdOTTFw0w"

#define USER_MAIL "grupo5@gmail.com"
#define USER_PASS "argentina"

#define TREINTA_SEGUNDOS 30000


//tipos de variables
typedef enum {
  P1,
  P2,
  espera1,
  espera2,
  esperaAumentoCiclo,
  esperaDisminucionCiclo
} tipoEstado;

//funciones
void cicloLogica();
void medirTemperatura();
void maquinaDeEstados();
void imprimirTemperaturaYHora();
void imprimirCiclo();
void actualizarHora();
void subirDatos();
unsigned long getTime();
void processData(AsyncResult &aResult);

//config
DHT dht(DHTPIN, DHTTYPE);

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /*reset*/ U8X8_PIN_NONE);

ESP32Time rtc;

FirebaseApp app;

UserAuth user_auth(WEB_API_KEY, USER_MAIL, USER_PASS);  //autenticación de firebase (mail & pass)

//permiten que la comunicación se de de forma asincrónica
WiFiClientSecure ssl_client;  //el "canal" de comunicación: esp <--> friebase
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);

RealtimeDatabase Database;  //el servicio a conectarse donde escribir los datos

//objeto json con sus subpartes
JsonWriter writer;
object_t jsonData;        //
object_t objTemperatura;  //
object_t objTimestamp;    // para guardar la info del json
object_t objFecha;        //
object_t objHora;         //


const char *ntpServer = "pool.ntp.org";
int gmtOffset = -3;
int gmtOffsetSegundos = 0;

//variables
tipoEstado estado = P1;

int SW1;
int SW2;

int horaActual = 0;
int minutoActual = 0;

float hic;

unsigned long ciclo = TREINTA_SEGUNDOS;

unsigned long ultimaSubida = 0;
unsigned long timestamp = 0;

//paths (dónde se guarda la info)
String uid;
String databasePath;

String parentPath;  //path "padre"

String tempPath = "/temperature";  // "sub-
String timePath = "/timestamp";    //  paths"


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

  //https (conexión)
  ssl_client.setInsecure();
  ssl_client.setConnectionTimeout(1000);
  ssl_client.setHandshakeTimeout(5);

  //inicio de sesión
  initializeApp(
    aClient,             //cliente
    app,                 //objeto sesión firebase
    getAuth(user_auth),  //datos de sesión
    processData,         //función
    "Auth"               //tarea
  );

  app.getApp<RealtimeDatabase>(Database);  //conecta la app según la rtdb "database"
  Database.url(DATABASE_URL);              //a qué db conectarse
}

void loop() {
  app.loop();  //necesario

  actualizarHora();

  SW1 = digitalRead(SWITCH_1);
  SW2 = digitalRead(SWITCH_2);

  medirTemperatura();

  maquinaDeEstados();

  subirDatos();
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
        ciclo += TREINTA_SEGUNDOS;
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
        ciclo -= TREINTA_SEGUNDOS;
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
  if (ciclo <= TREINTA_SEGUNDOS) ciclo = TREINTA_SEGUNDOS;  //condicional corto
}

void imprimirTemperaturaYHora() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  u8g2.drawStr(0, 20, "VA: ");
  char stringTemp[10];
  sprintf(stringTemp, "%.1f C", hic);
  u8g2.drawStr(50, 20, stringTemp);

  u8g2.sendBuffer();
}

void imprimirCiclo() {

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  u8g2.drawStr(0, 20, "Ciclo: ");
  char stringCiclo[10];
  sprintf(stringCiclo, "%.1d :", ciclo);
  u8g2.drawStr(TREINTA_SEGUNDOS, 20, stringCiclo);

  u8g2.sendBuffer();
}

void actualizarHora() {
  horaActual = rtc.getHour();
  minutoActual = rtc.getMinute();
}

void subirDatos() {

  //comprobar si fb "está listo" (terminado auth user y ready para trabajar)
  if (!app.ready()) {
    return;
  }

  //comprobar que no haya algún valor de temp
  if (isnan(hic)) {
    return;
  }

  if (millis() - ultimaSubida >= ciclo) {
    timestamp = getTime();  //tiempo actual
    if (timestamp == 0) {   //si no puede devuelve cero
      return;
    }

    ultimaSubida = millis();

    uid = app.getUid().c_str();                        //uid
    databasePath = "/UsersData/" + uid + "/readings";  //db path que no estaba definido

    parentPath = databasePath + "/" + String(timestamp);  //base

    //crea el json (temp & timestamp; y los une)
    writer.create(objTemperatura, tempPath, hic);
    writer.create(objTimestamp, timePath, timestamp);
    writer.join(
      jsonData,  //dónde
      2,         //cuántos
      objTemperatura,
      objTimestamp);


    //envía la info
    Database.set<object_t>(
      aClient,          //cliente
      parentPath,       //ruta de guardado (main path)
      jsonData,         //dónde están guardados los datos (json) temp y timestamp
      processData,      //f() de errores, respuestas, etc.
      "RTDB_Send_Data"  //nombre de la tarea (para el serial monitor)
    );
  }
}

unsigned long getTime() {

  time_t now;
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    return 0;
  }

  time(&now);

  return now;
}

void processData(AsyncResult &aResult) {

  if (!aResult.isResult()) {  //ver si hay algún resultado nuevo
    return;
  }

  if (aResult.isError()) {
    Serial.print("Error: ");
    Serial.println(aResult.error().message());
  }

  if (aResult.available()) {
    Serial.println("Operacion completada");
  }
}
