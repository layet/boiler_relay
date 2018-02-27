#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DallasTemperature.h>
#include <ESP8266httpUpdate.h>

#define ONE_WIRE_BUS 4

const char* ssid = "*";
const char* password = "*";
const char* mqtt_server = "i-alice.ru";
const char* update_url = "http://i-alice.ru/ota/index.php";
const char* update_class = "boiler_relay-0.0.1";

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress insideThermometer;
WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

void setup() {
  pinMode(5, OUTPUT);
  pinMode(14, OUTPUT);
  pinMode(12, OUTPUT);
  pinMode(13, OUTPUT);
  Serial.begin(115200);
  setup_wifi();
  updateProc();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  sensors.begin();
  if (!sensors.getAddress(insideThermometer, 0)) Serial.println("Unable to find address for Device 0");
  sensors.setResolution(insideThermometer, 9);
}

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("STA-MAC: ");
  uint8_t macAddr[6];
  WiFi.macAddress(macAddr);
  Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if (String(topic) == String("/alexhome/boiler/relay1")) if ((char)payload[0] == '1') { digitalWrite(5, HIGH); } else { digitalWrite(5, LOW); }
  if (String(topic) == String("/alexhome/boiler/relay2")) if ((char)payload[0] == '1') { digitalWrite(14, HIGH); } else { digitalWrite(14, LOW); }
  if (String(topic) == String("/alexhome/boiler/relay3")) if ((char)payload[0] == '1') { digitalWrite(12, HIGH); } else { digitalWrite(12, LOW); }
  if (String(topic) == String("/alexhome/boiler/relay4")) if ((char)payload[0] == '1') { digitalWrite(13, HIGH); } else { digitalWrite(13, LOW); }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("BoilerRelay", "/alexhome/boiler/alive", 0, 0, "0")) {
      Serial.println("connected");
      client.subscribe("/alexhome/boiler/+");
      client.publish("/alexhome/boiler/alive", "1");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void updateProc() {
  if (WiFi.status() == WL_CONNECTED) {
    t_httpUpdate_return ret = ESPhttpUpdate.update(update_url, update_class);
    switch(ret) {
        case HTTP_UPDATE_FAILED:
            Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
            Serial.println("");
            break;
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("[update] Update no Update.");
            break;
        case HTTP_UPDATE_OK:
            Serial.println("[update] Update ok."); // may not called we reboot the ESP
            break;
    }
  }
}

void loop() {
  char buf[10];
  float temp;
  
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 120000) {
    lastMsg = now;
    if (digitalRead(5) == 1) client.publish("/alexhome/boiler/relay1", "1"); else client.publish("/alexhome/boiler/relay1", "0");
    if (digitalRead(14) == 1) client.publish("/alexhome/boiler/relay2", "1"); else client.publish("/alexhome/boiler/relay2", "0");
    if (digitalRead(12) == 1) client.publish("/alexhome/boiler/relay3", "1"); else client.publish("/alexhome/boiler/relay3", "0");
    if (digitalRead(13) == 1) client.publish("/alexhome/boiler/relay4", "1"); else client.publish("/alexhome/boiler/relay4", "0");
    Serial.print("Requesting temperatures...");
    sensors.requestTemperatures(); // Send the command to get temperatures
    Serial.println("DONE");
    Serial.print("Temperature for the device 1 (index 0) is: ");
    temp = sensors.getTempCByIndex(0);
    if ((temp != 0) || (temp != 85) || (temp != -127)) {
      Serial.println(dtostrf(temp, 3, 2, buf));
      client.publish("/alexhome/boiler/temp", dtostrf(sensors.getTempCByIndex(0), 3, 2, buf));
    }
  }
}
