#include <WiFi.h>
#include <FirebaseESP32.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <EmonLib.h>

#define WIFI_SSID "AdrianoLuz" // "WIFI-ADRIANO" rede de casa
#define WIFI_PASSWORD "12345678" // "WIFI@ADRIANO" senha 
#define FIREBASE_HOST "https://esp-solar-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "AIzaSyCS7vTqiIYfI4EkfDpvNdKSoVDW9tcDsUU"

FirebaseConfig config;
FirebaseAuth auth;
FirebaseData firebaseData;
WiFiUDP udp;
NTPClient timeClient(udp, "pool.ntp.org",-10800); // 10800 segundos = 3 hora de offset (UTC+1 por exemplo)

EnergyMonitor emon1; 

//DEFINIÇÃO DAS CONSTANTES DO CÓDIGO 
const int SensTensPlaca = 34; 
const int SensNivelBat = 35; 
const int relePlaca = 12; 
const int releRede = 13; 

//DEFINIÇÃO DAS VARIÁVEIS DO CÓDIGO 
String StatusRede ="Desconectado";  
float potPlacaDia = 0.0; 
float potPlacaMes = 0.0; 
float corrPlaca = 0.0;
float tensPlaca = 0.0;

//==============================================================//=============================================================//=============

void setup() {
  Serial.begin(115200);
  
  emon1.current(33, 111.1);            
  //ENTRADAS E SAIDAS
  pinMode(SensTensPlaca, INPUT);
  pinMode(SensNivelBat, INPUT);
  pinMode(relePlaca, OUTPUT);
  pinMode(releRede, OUTPUT);

  // CONECTAR-SE À REDE WI-FI
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  //DEFINIR HORÁRIO
  timeClient.begin();
  timeClient.update();

  // Configure o Firebase
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;

  // Inicialize o Firebase
  Firebase.begin(&config, &auth);
}

//==============================================================//=============================================================//=============


void loop() {

  //DEFINIR HORÁRIO
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  struct tm *ptm = localtime((time_t *)&epochTime);
  int day = ptm->tm_mday;
  int month = ptm->tm_mon + 1; // Janeiro é 0
  int year = ptm->tm_year + 1900; // Anos desde 1900
  int hours = ptm->tm_hour;  // Hora atual
  int min = ptm->tm_min;    // Minuto atual
  int seconds = ptm->tm_sec;    // Segundo atual
  
  Serial.print("Date: ");
  Serial.print(day);
  Serial.print("/");
  Serial.print(month);
  Serial.print("/");
  Serial.println(year);
  Serial.print("/");
  Serial.print("Time: ");
  Serial.print(hours);
  Serial.print(":");
  Serial.print(min);
  Serial.print(":");
  Serial.println(seconds); 


  // CALCULO RMS PARA O SENSOR DE CORRENTE
  float Irms1 = emon1.calcIrms(1480); 
  
  // CÓDIGO CONTROLE DOS RELES E COMSUMO 
  float leitTensPlaca = analogRead(SensTensPlaca);
  float leitNivelBat = analogRead(SensNivelBat);
  float tensPlaca_mV = map(leitTensPlaca, 0, 4095, 0, 134000);
  float tensBat_mV = map(leitNivelBat, 0, 4095, 0, 27240);
  float potInstPlaca = ((Irms1/100) * (tensPlaca_mV / 1000));

  // CONTROLE DOS RELES
  if (tensBat_mV < 12300) {
    digitalWrite(relePlaca, LOW);
    delay(500);
    digitalWrite(releRede, HIGH);
    StatusRede = "REDE";
  } 
  else if (tensBat_mV >= 12800) {
    digitalWrite(releRede, LOW);
    delay(500);
    digitalWrite(relePlaca, HIGH);
    StatusRede = "PLACA";
  } 
  else {
    if (relePlaca == LOW) {
      digitalWrite(relePlaca, LOW);
      delay(500);
      digitalWrite(releRede, HIGH);
      StatusRede = "REDE";
    } 
    if (releRede == LOW) {
      digitalWrite(releRede, LOW);
      delay(500);
      digitalWrite(relePlaca, HIGH);
      StatusRede = "PLACA";
    }   
  }

  
  // CALCULO DE CONSUMO
  potPlacaDia += (potInstPlaca)/60; // Obter o consumo a cada minuto e acumular dividor por 60 para achar consumo em Horas
  potPlacaMes += (potInstPlaca)/60;
  
  // calculo média de tenção e corrente a cada 15 minutos
  if((Irms1/100) > 0.02){
    corrPlaca = (corrPlaca + ((Irms1/100)/15));
  }
  tensPlaca = (tensPlaca_mV/1000);
  float tensBate = (tensBat_mV/1000);

  if(min == 00 || min == 15 || min == 30 || min == 45){
    // Salvar no Firebase
    String date = String(hours) + ":" + String(min);
    salvarNoFirebase(date, StatusRede, corrPlaca, tensPlaca, tensBate, potPlacaDia, potPlacaMes);
    delay(1000);
    corrPlaca = 0.0;
  }
  
  if(hours == 01 && min == 00 ){
    // Salvar no Firebase
    String dateDay = String(day) + "/" + String(month); 
    salvarNoFirebase2(dateDay, potPlacaDia);
    delay(1000);
    potPlacaDia = 0.0;
  }
  
  if(day == 01 || (hours == 01 && min == 00)){
    // Salvar no Firebase
    String dateMes = String(month); 
    salvarNoFirebase3(dateMes, potPlacaMes);
    delay(1000);
    potPlacaMes = 0.0;
  }

    
    // Impressão dos valores
    Serial.print(" |>>>> Corrente (A): ");
    Serial.println(Irms1/100);
    Serial.print(" |>>>> Leitura de Tensão (V): ");
    Serial.println(leitTensPlaca);
    Serial.print(" |>>>> Tensão (V): ");
    Serial.println(tensPlaca_mV/1000);
    Serial.print(" |>>>> Leitura Bateria: ");
    Serial.println(leitNivelBat);
    Serial.print(" |>>>> Bateria: ");
    Serial.println(tensBat_mV);
    Serial.print(" |>>>> Status: ");
    Serial.println(StatusRede);
    Serial.println(" ");
    Serial.println(" ");
  
  delay(60000);
}

//==================================================//=================================================//=====================================

void salvarNoFirebase(String date, String StatusRede, float corrPlaca, float tensPlaca, float tensBate, float potPlacaDia, float potPlacaMes) {
  // Construir o caminho no banco de dados onde você deseja salvar os dados
  String path = "/Tensao_Corrente";

  // Criar um objeto FirebaseJson
  FirebaseJson json;
  json.set("Data", date);
  json.set("Status", StatusRede);
  json.set("CorrPlaca", corrPlaca);
  json.set("TensPlaca", tensPlaca);
  json.set("TensBate", tensBate);
  json.set("PotDia", potPlacaDia);
  json.set("PotMes", potPlacaMes);
  
  // Salvar os dados no Firebase
  if (Firebase.pushJSON(firebaseData, path, json)) {
    Serial.println("Dados enviados com sucesso!");
  }
  delay(500);
}


void salvarNoFirebase2(String dateDay, float potPlacaDia) {
  // Construir o caminho no banco de dados onde você deseja salvar os dados
  String path2 = "/Consumo_Diario";

  // Criar um objeto FirebaseJson
  FirebaseJson json2;
  json2.set("DataDay", dateDay);
  json2.set("ConsPlacaDia", potPlacaDia);

  // Salvar os dados no Firebase
  if (Firebase.pushJSON(firebaseData, path2, json2)) {
    Serial.println("Dados enviados com sucesso!");
  }
  delay(500);
}


void salvarNoFirebase3(String dateMes, float potPlacaMes) {
  // Construir o caminho no banco de dados onde você deseja salvar os dados
  String path3 = "/Consumo_Mes";

  // Criar um objeto FirebaseJson
  FirebaseJson json3;
  json3.set("DataMes", dateMes);
  json3.set("ConsPlacaMes", potPlacaMes);

  // Salvar os dados no Firebase
  if (Firebase.pushJSON(firebaseData, path3, json3)) {
    Serial.println("Dados enviados com sucesso!");
  }
  delay(500);
}


//==============================================================//=============================================================//=============
