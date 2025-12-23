// Secrets.example.h
// Copy to Secrets.h and fill in your Wi-Fi and Firebase credentials.

#pragma once

// Wi-Fi credentials
#define SECRETS_WIFI_SSID      ""
#define SECRETS_WIFI_PASSWORD  ""

// Firebase RTDB configuration (mobizt FirebaseClient)
// Provide the RTDB database URL and API key. For auth, either supply email/password
// or a custom token/refresh token via your provisioning flow.
#define SECRETS_FIREBASE_DATABASE_URL ""
#define SECRETS_FIREBASE_API_KEY      ""
#define SECRETS_FIREBASE_AUTH_EMAIL   ""
#define SECRETS_FIREBASE_AUTH_PASS    ""

// Base path and userId for RTDB root composition.
#define SECRETS_BASE_PATH ""
#define SECRETS_USER_ID   ""
