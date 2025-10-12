#ifndef MQTT_H
#define MQTT_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>

// This module encapsulates all MQTT-related functionality:
// - Starts ESP32 softAP for local clients
// - Runs a very lightweight broker emulation via WiFiServer (port 1883)
// - Provides a minimal queue for commands parsed from MQTT payloads
// - Exposes non-blocking tick function for FreeRTOS task integration
//
// Usage:
// - Call mqttInit() from setup() once
// - Run mqttBrokerTaskLoop() inside its own FreeRTOS task
// - Call mqttDequeueCommand() from control task to consume commands

namespace mqttns {

// Fixed port for local MQTT broker
static const uint16_t MQTT_BROKER_PORT = 1883;

// Simple ring buffer for incoming commands
struct CommandQueue {
    String commands[10];
    int head = 0;
    int tail = 0;
    bool isFull() const { return ((tail + 1) % 10) == head; }
    bool isEmpty() const { return head == tail; }
    void enqueue(const String &cmd) { if (!isFull()) { commands[tail] = cmd; tail = (tail + 1) % 10; } }
    String dequeue() { if (!isEmpty()) { String c = commands[head]; head = (head + 1) % 10; return c; } return String(""); }
};

// Globals owned by this module
extern WiFiServer brokerServer;
extern CommandQueue commandQueue;

// Initialize AP and broker server
void mqttInit();

// Run broker I/O loop (non-blocking; call repeatedly from a FreeRTOS task)
void mqttBrokerTaskLoop();

// Dequeue next command, empty string if none
String mqttDequeueCommand();

} // namespace mqttns

#endif // MQTT_H


