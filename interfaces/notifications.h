#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

#include <Arduino.h>

// Cloud notification helpers
// Usage: call sendNotification*(...) after Firebase auth is ready.
// These functions will use the configured Cloud Function endpoint
// and include the current Firebase UID in the payload.

void sendNotification(String bodyMessage, String dataValue);
void sendNotificationString(String bodyMessage, String dataValue);
void sendNotificationInt(String bodyMessage, int dataValue);
void sendNotificationFloat(String bodyMessage, float dataValue);
void sendNotificationJSON(String bodyMessage, String jsonData);

#endif // NOTIFICATIONS_H


