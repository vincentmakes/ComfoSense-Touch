#include <PubSubClient.h>
#include <WiFi.h>
#include <map>
#include "../secrets.h"
#include "mqtt.h"

namespace comfoair {

WiFiClient wifiClient;
  MQTT::MQTT() {
    this->client = PubSubClient(wifiClient);
  }

  void MQTT::subscribeTo(const char* topic, MQTT_CALLBACK_SIGNATURE) {
    this->callbackMap[topic] = callback;
    if (this->client.connected()) {
      this->subscribeToTopics();
    }
  }

  void MQTT::setup() {
    this->client.setServer(MQTT_HOST, MQTT_PORT);
    this->client.setSocketTimeout(2);  // Cap TCP operations at 2s (default 15s blocks main loop)
    this->client.setBufferSize(512);   // Default 256 bytes silently drops larger messages
    this->client.setCallback([this](char* topic, unsigned char* payload, unsigned int length){
      // Null-terminate payload (PubSubClient buffer has room after payload)
      payload[length] = '\0';

      Serial.println("-------new message from broker-----");
      Serial.print("channel:");
      Serial.println(topic);
      Serial.print("data:");
      Serial.write(payload, length);
      Serial.println();

      // Safe dispatch: only call callback if topic is registered
      auto it = callbackMap.find(topic);
      if (it != callbackMap.end()) {
        it->second(topic, payload, length);
      } else {
        Serial.print("MQTT: No handler for topic: ");
        Serial.println(topic);
      }
    });
  }

  void MQTT::loop() {
    this->ensureConnected();
    client.loop();
  }

  void MQTT::writeToTopic(const char* topic, const char* payload, bool retained) {
    if (!this->client.connected()) {
      return;  // Skip publish when disconnected; data re-sent on next cycle
    }
    this->client.publish(topic, payload, retained);
  }

// PRIVATE STUFF

  void MQTT::ensureConnected() {
    if (this->client.connected()) {
      return;
    }

    // Non-blocking: only attempt reconnection every 5 seconds
    unsigned long now = millis();
    if (now - lastReconnectAttempt < 5000) {
      return;
    }
    lastReconnectAttempt = now;

    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
      Serial.println("connected");
      subscribeToTopics();
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" will retry in 5 seconds");
    }
  }

  void MQTT::subscribeToTopics() {
    std::map<std::string, std::function<void(char*, uint8_t*, unsigned int)>>::iterator it;
    for (it=callbackMap.begin(); it!=callbackMap.end(); ++it) {
      std::string s = it->first;
      Serial.print("Subscribing to: ");
      Serial.println(s.c_str());
      client.subscribe(s.c_str());
    }
  }

} // namespace comfoair