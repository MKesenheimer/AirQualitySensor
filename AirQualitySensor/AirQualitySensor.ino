/*
 *  air quality sensor
 *
 *  Author: Matthias Kesenheimer - @BartimaeusvUruk - https://github.com/MKesenheimer
 *  Date: 1 February 2021
 *
 *  Based on sketch from Rui Santos (https://randomnerdtutorials.com)
 *
 * Libraries to install:
 * - DHT sensor library by adafruit (https://github.com/adafruit/DHT-sensor-library)
 * - PubSubClient by Nick O’Leary (https://pubsubclient.knolleary.net)
 * - Sparkfun CCS811 (https://github.com/sparkfun/SparkFun_CCS811_Arduino_Library)
 * 
 *  Grenzwerte für VOCs:
 *  https://www.air-q.com/messwerte/voc
 *    0 - 150 ppb: good
 *    150 - 1000 ppb: poor
 *    > 1000 ppb: bad
 *    
 *   https://www.airthings.com/what-is-voc
 *    0 - 250 ppb: good
 *    250 - 2000 ppb: poor
 *    > 2000 ppb: bad
 */
 
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <DHT.h>
#include <SparkFunCCS811.h>
#define CCS811_ADDR 0x5A
CCS811 ccs811(CCS811_ADDR);

#define DHTPIN 15
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Replace the next variables with your SSID/Password combination
const String ssid = "SSID";           // TODO: the SSID of your Wifi-network 
const String password = "passphrase"; // TODO: the passphrase to access the Wifi-network

// Add your MQTT Broker IP address, example:
const String mqtt_server = "IP-Address";     // TODO: fill in the IP address of your MQTT broker
const long   mqtt_port = 1883;               // TODO: fill in the port on which the MQTT service is running. 1883 is the default port
const String mqtt_user = "username";         // TODO: credentials to authorize against the MQTT service
const String mqtt_passwd = "password";       // TODO: credentials to authorize against the MQTT service
const String mqtt_topic = "/home/office";    // TODO: change the topic to your likes. Remember to change this in the node-red dashboard, too.
const String mqtt_clientID = "ESP32-Office"; // TODO: ID of your client. This is not essential.

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;

#define LEDPIN 2

void setup() {
  Serial.begin(115200);
  
  dht.begin();
  
  setup_wifi();
  client.setServer(mqtt_server.c_str(), mqtt_port);
  client.setCallback(callback);

  pinMode(LEDPIN, OUTPUT);

  // Inialize I2C Hardware
  Wire.begin();
  
  // set WAK to low (enable CCS811 chip)
  pinMode(23, OUTPUT);
  digitalWrite(23, LOW);
  if (ccs811.begin() == false) {
    Serial.print("CCS811 error. Please check wiring. Freezing...");
    while (1);
  }
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid.c_str(), password.c_str());

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  
  String messageTemp;
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // If a message is received on the topic /home/living-room/output, 
  // you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  String outTopicStr = mqtt_topic + "/output";
  if (String(topic) == outTopicStr) {
    Serial.print("Changing output to ");
    if(messageTemp == "on"){
      Serial.println("on");
      digitalWrite(LEDPIN, HIGH);
    }
    else if(messageTemp == "off") {
      Serial.println("off");
      digitalWrite(LEDPIN, LOW);
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqtt_clientID.c_str(), mqtt_user.c_str(), mqtt_passwd.c_str())) {
      Serial.println("connected");
      // Subscribe
      String outTopicStr = mqtt_topic + "/output";
      client.subscribe(outTopicStr.c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 60000) {
    lastMsg = now;
    
    // Temperature in Celsius
    float temperature = dht.readTemperature();
    // Convert the value to a char array
    String tempString = String(temperature);
    Serial.print("Temperature: ");
    Serial.print(tempString);
    Serial.println("°C");
    String tempTopicStr = mqtt_topic + "/temperature";
    client.publish(tempTopicStr.c_str(), tempString.c_str());

    // Humidity in percent
    float humidity = dht.readHumidity();
    // Convert the value to a char array
    String humString = String(humidity);
    Serial.print("Humidity: ");
    Serial.print(humString);
    Serial.println("%");
    String humidTopicStr = mqtt_topic + "/humidity";
    client.publish(humidTopicStr.c_str(), humString.c_str());

    // CO2 and VOC measurement
    // Check to see if data is ready
    if (ccs811.dataAvailable()) {
      // If so, have the sensor read and calculate the results.
      ccs811.readAlgorithmResults();

      // temperature and humidity compensation
      ccs811.setEnvironmentalData(humidity, temperature);

      uint32_t co2 = ccs811.getCO2();
      String co2String = String(co2);
      Serial.print("CO2: ");
      Serial.print(co2String);
      Serial.println("ppm");
      String co2TopicStr = mqtt_topic + "/co2";
      client.publish(co2TopicStr.c_str(), co2String.c_str());

      uint32_t tvoc = ccs811.getTVOC();
      String tvocString = String(tvoc);
      Serial.print("tVOC: ");
      Serial.print(tvocString);
      Serial.println("ppb");
      String tvocTopicStr = mqtt_topic + "/tvoc";
      client.publish(tvocTopicStr.c_str(), tvocString.c_str());
    }
  }
}
