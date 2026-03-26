#ifndef MQTT_H
#define MQTT_H

#include <inttypes.h>
#include <PubSubClient.h>
#include <map>
#include <functional>

namespace comfoair {

// MQTT configuration (passed at runtime instead of compile-time secrets.h)
struct MqttConfig {
    const char* host;
    uint16_t port;
    const char* user;
    const char* pass;
};

class MQTT {
public:
    MQTT();
    void configure(const MqttConfig& config);
    void subscribeTo(const char* topic, MQTT_CALLBACK_SIGNATURE);
    void setup();
    void loop();
    void writeToTopic(const char* topic, const char* payload, bool retained = false);
    bool isConnected() { return client.connected(); }

private:
    PubSubClient client;
    MqttConfig config;
    bool configured;
    std::map<std::string, std::function<void(char*, uint8_t*, unsigned int)>> callbackMap;
    unsigned long lastReconnectAttempt = 0;
    void subscribeToTopics();
    void ensureConnected();
};

} // namespace comfoair

#endif
