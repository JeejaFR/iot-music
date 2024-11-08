#include <WiFi.h>
#include <PubSubClient.h>

// Pins
#define trigPin 5
#define echoPin 18
#define buzzerPin 19

// Configuration WiFi et MQTT
const char* ssid = "Peco";
const char* password = "12345678";
const char* mqtt_server = "broker.emqx.io";
const char* mqtt_topic = "test/topicdzndjadnak";

WiFiClient espClient;
PubSubClient client(espClient);

// Mélodie
const int melody[] = {392, 392, 392, 330, 349, 392, 440, 392, 523, 494};
const int noteDurations[] = {3, 3, 3, 6, 6, 3, 3, 3, 3, 3};
const int melodyLength = sizeof(melody) / sizeof(melody[0]);

// Constantes du capteur ultrasonique
const float SOUND_SPEED = 0.034;
const int minDistance = 10;
const int maxDistance = 100;
const unsigned long debounceDelay = 5000;

long duration;
float distanceCm;
bool playMelody = false;
int currentNote = 0;
unsigned long noteStartTime = 0;
unsigned long lastDetectionTime = 0;
unsigned long lastSensorCheck = 0;
unsigned long lastMQTTPublish = 0;

// Variables pour le timing avec millis()
const unsigned long sensorInterval = 100;
const unsigned long melodyInterval = 120;

// Fonction pour la connexion WiFi
void setup_wifi() {
  Serial.print("Connexion à ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWiFi connecté !");
  Serial.print("Adresse IP: ");
  Serial.println(WiFi.localIP());
}

// Fonction pour se reconnecter au broker MQTT
void reconnect() {
  while (!client.connected()) {
    Serial.print("Connexion au broker MQTT...");
    if (client.connect("ESP32Client")) {
      Serial.println("Connecté");
      client.subscribe(mqtt_topic);
    } else {
      Serial.print("Échec, rc=");
      Serial.print(client.state());
      Serial.println(" nouvelle tentative dans 5 secondes");
      delay(500);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);
}

void checkSensor() {
  // Lire le capteur ultrasonique sans utiliser delay()
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  distanceCm = duration * SOUND_SPEED / 2;

  Serial.print("Distance (cm): ");
  Serial.println(distanceCm);

  unsigned long currentTime = millis();
  if (distanceCm >= minDistance && distanceCm <= maxDistance) {
    if (currentTime - lastDetectionTime > debounceDelay) {
      Serial.println("Présence détectée ! Lecture de la mélodie.");
      playMelody = true;
      currentNote = 0;
      tone(buzzerPin, melody[currentNote]);
      noteStartTime = currentTime;
      lastDetectionTime = currentTime;

      // Publier un message sur le topic MQTT
      client.publish(mqtt_topic, "Présence détectée");
    }
  }
}

void playMelodyNonBlocking() {
  if (playMelody) {
    unsigned long currentTime = millis();
    if (currentNote < melodyLength) {
      if (currentTime - noteStartTime >= noteDurations[currentNote] * melodyInterval) {
        noTone(buzzerPin);
        currentNote++;
        if (currentNote < melodyLength) {
          tone(buzzerPin, melody[currentNote], 180);
          noteStartTime = currentTime;
        }
      }
    } else {
      playMelody = false;
      currentNote = 0;
      noTone(buzzerPin);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Vérifier le capteur ultrasonique périodiquement
  if (millis() - lastSensorCheck >= sensorInterval) {
    lastSensorCheck = millis();
    checkSensor();
  }

  // Jouer la mélodie de manière non-bloquante
  playMelodyNonBlocking();
}