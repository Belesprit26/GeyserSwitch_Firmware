#include "interfaces/mqtt.h"

namespace mqttns {

// Module globals
WiFiServer brokerServer(MQTT_BROKER_PORT);
CommandQueue commandQueue;

// Initialize softAP and start broker server
void mqttInit()
{
    // AP provides local network for clients when STA is unavailable
    WiFi.softAP("GeyserHub", "password123");
    brokerServer.begin();
    Serial.println("MQTT Broker started on ESP32 AP");
}

// Very small broker emulation: reads lines and extracts simple commands
void mqttBrokerTaskLoop()
{
    WiFiClient client = brokerServer.available();
    if (!client) {
        return;
    }

    Serial.println("New MQTT client connected");
    unsigned long idleStart = millis();
    while (client.connected()) {
        if (client.available()) {
            String msg = client.readStringUntil('\n');
            // Example format: "PUBLISH geyser/cmd on" or "PUBLISH geyser/cmd off"
            if (msg.startsWith("PUBLISH geyser/cmd")) {
                String cmd = msg.substring(msg.lastIndexOf(' ') + 1);
                commandQueue.enqueue(cmd);
                Serial.println("MQTT Command Queued: " + cmd);
            }
            idleStart = millis();
        }
        // avoid tight loop
        if (millis() - idleStart > 5000) break;
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    client.stop();
    Serial.println("MQTT client disconnected");
}

String mqttDequeueCommand()
{
    return commandQueue.dequeue();
}

} // namespace mqttns


