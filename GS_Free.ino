// Entry point for Arduino runtime.
// This sketch wires up the high-level Application that manages tasks and subsystems.

#include "src/app/Application.h"

static Application app;  // Single application instance for the device

void setup() {
  // Initialize the application (logging, configuration, etc.).
  app.begin();
}

void loop() {
  // Delegate to the application loop (non-blocking where possible).
  app.runLoop();
}
