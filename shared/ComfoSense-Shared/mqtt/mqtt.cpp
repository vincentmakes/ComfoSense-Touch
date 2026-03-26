#include <PubSubClient.h>
#include <WiFi.h>
#include <map>
#include "mqtt.h"

namespace comfoair {

WiFiClient wifiClient;

MQTT::MQTT() : configured(false) {
    this->client = PubSubClient(wifiClient);
}

void MQTT::configure(const MqttConfig& cfg) {
    this->config = cfg;
    this->configured = true;
}

void MQTT::subscribeTo(const char* topic, MQTT_CALLBACK_SIGNATURE) {
    this->callbackMap[topic] = callback;
    if (this->client.connected()) {
        this->subscribeToTopics();
    }
}

void MQTT::setup() {
    if (!configured) {
        Serial.println("MQTT: ERROR - not configured! Call configure() before setup()");
        return;
    }

    this->client.setServer(config.host, config.port);
    this->client.setSocketTimeout(2);  // Cap TCP operations at 2s (default 15s blocks main loop)
    this->client.setBufferSize(512);   // Default 256 bytes silently drops larger messages
    this->client.setCallback([this](char* topic, unsigned char* payload, unsigned int length) {
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

// PRIVATE

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
    if (client.connect(clientId.c_str(), config.user, config.pass)) {
        Serial.println("connected");
        subscribeToTopics();
    } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" will retry in 5 seconds");
    }
}

void MQTT::subscribeToTopics() {
    int count = 0;
    int failed = 0;
    for (auto it = callbackMap.begin(); it != callbackMap.end(); ++it) {
        const char* topic = it->first.c_str();

        // Retry each subscription up to 3 times
        bool ok = false;
        for (int attempt = 0; attempt < 3 && !ok; attempt++) {
            ok = client.subscribe(topic, 1);
            if (!ok) {
                // Let TCP stack flush before retrying
                delay(10);
                client.loop();
            }
        }

        if (!ok) {
            Serial.print("SUBSCRIBE FAILED: ");
            Serial.println(topic);
            failed++;
        }
        count++;

        // Yield every 4 subscriptions to let TCP stack breathe
        if (count % 4 == 0) {
            delay(5);
            client.loop();  // Process any incoming SUBACK/retained messages
        }
    }
    Serial.printf("MQTT: Subscribed to %d topics (%d failed)\n", count, failed);
}

} // namespace comfoair
