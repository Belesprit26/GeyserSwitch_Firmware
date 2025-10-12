#ifndef TRACKING_H
#define TRACKING_H

#include <Arduino.h>

// Initialize the tracking system
void initTracking();

// Track usage of a geyser (single)
// - geyserPath: Identifier for the geyser (e.g., "geyser_1")
// - currentState: Current state of the geyser (true for ON, false for OFF)
void trackGeyserUsage(const String& geyserPath, bool currentState);

// Perform weekly reset and calculate average runtime (if implemented)
void checkWeeklyReset(const String& realDate);

#endif // TRACKING_H
