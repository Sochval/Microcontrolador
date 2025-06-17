#include <WiFiS3.h>
#include <HTTPClient.h>
#include <EEPROM.h>
#include <AccelStepper.h>
#include <Arduino_JSON.h>
#include <RTC.h>

//definicion de pines para el motor
#define motorPin1 8
#define motorPin2 9
#define motorPin3 10
#define motorPin4 11

AccelStepper stepper(AccelStepper::HALF4WIRE, motorPin1, motorPin3, motorPin2, motorPin4);

// credenciales para la conexion WiFi
char ssid[32];
char password[32];
WiFiServer server(80);
//API (usando IP para pruebas con XAMPP)
const char* serverName = "http://10.161.148.238/obtener_dispensacion.php";

// reloj 
RTC rtc;
String horarios[4];
int cantidades[4];
bool ejecutado[4] = {false, false, false, false};

//obtener la direccion MAC
String getDeviceID() {
 byte mac[6];
 WiFi.macAddress(mac);
 char macStr[18];
 sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
 return String(macStr);
}

void setup() {
 Serial.begin(9600);
 EEPROM.begin();
 EEPROM.get(0, ssid);
 EEPROM.get(32, password);

 if (strlen(ssid) == 0 || strlen(password) == 0) {
  startAPMode();
 } else {
  connectToWiFi();
 }


 stepper.setMaxSpeed(1000);
 stepper.setAcceleration(500);

 rtc.begin();
 rtc.setTime(0, 0, 0); //ajustar a la hora real 

 obtenerHorariosDesdeAPI();
}

void loop() {
 String horaActual = obtenerHoraActual();

 //reinicia ejecutado[] a medianoche
 if (horaActual == "00:00") {
    for (int i = 0; i < 4; i++) {
     ejecutado[i] = false;
    }
    delay(60000); //espera 1 minuto para evitar reinicios en el mismo minuto
 }

 for (int i = 0; i < 4; i++) {
  if (horarios[i].length() > 0 && horaActual == horarios[i] && !ejecutado[i]) {
   dispensarComida(cantidades[i]);
   ejecutado[i] = true;
  }
 }
 delay(10000); //verificar cada 10 segundos
}

String obtenerHoraActual() {
 char buffer[6];
 sprintf(buffer, "%02d:%02d", rtc.getHours(), rtc.getMinutes());
 return String(buffer);
}

obtenerHorariosDesdeAPI() {
 if (WiFi.status() == WL_CONNECTED) {
  HTTPClient http;
  String url = String(serverName) + "?mac=" + getDeviceID();
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    JSONVar doc = JSON.parse(payload);

    if (JSON.typeof(doc) == "undefined") {
      Serial.println("Error al parsear JSON");
      return;
    }

    JSONVar horariosJson = doc["horarios"];
    JSONVar cantidadesJson = doc["cantidades"];

    for (int i = 0; i < horariosJson.length() && i < 4; i++) {
      horarios[i] = (const char*)horariosJson[i];
      cantidades[i] = (int)cantidadesJson[i];
      ejecutado[i] = false;
    }
  }
  
  http.end();
 }
}
      
void dispensarComida(int gramos) {
 int pasos = gramos * 10; //relacion de 10 pasos por gramo
 stepper.moveTo(stepper.currentPosition() + pasos);
 while (stepper.distanceToGo() != 0) {
  stepper.run();
 }
 Serial.print("Comida dispensada: ");
 Serial.print(gramos);
 Serial.println(" gramos");
}

void startAPMode() {
  WiFi.beginAP("DispensadorAP", "12345678");
  server.begin();
  Serial.println("Modo AP iniciado. Conectate a 'DispensadorAP'");

  while (true) {
    WiFiClient client = server.available();
    if (client) {
      Serial.println("Cliente conectado");
      String request = client.readStringUntil('');
      client.flush();

      if (request.indexOf("ssid=") > 0 && request.indexOf("password=") > 0) {
        int ssidStart = request.indexOf("ssid=") + 5;
        int ssidEnd = request.indexOf("&", ssidStart);
        int passStart = request.indexOf("password=") + 9;
        int passEnd = request.indexOf(" ", passStart);

        String ssidStr = request.substring(ssidStart, ssidEnd);
        String passStr = request.substring(passStart, passEnd);

        ssidStr.toCharArray(ssid, 32);
        passStr.toCharArray(password, 32);

        EEPROM.put(0, ssid);
        EEPROM.put(32, password);
        EEPROM.commit();
        
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println();
        client.println("<html><body><h1>Credenciales guardadas. Reiniciando...</h1></body></html>");
        delay(3000);
        NVIC_SystemReset();  // reinicio
      } else {        
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println();
        client.println("<html><body><form method='GET'>");
        client.println("SSID: <input name='ssid'><br>");
        client.println("Password: <input name='password'><br>");
        client.println("<input type='submit'></form></body></html>");
      }

      client.stop();
    }
  }
}

void connectToWiFi() {
 WiFi.begin(ssid, password);
 Serial.print("Conectando a WiFi");
 while (WiFi.status() != WL_CONNECTED) {
  delay(1000);
  Serial.print(".");
 }
 Serial.println("\nConectado a WiFi");
}
