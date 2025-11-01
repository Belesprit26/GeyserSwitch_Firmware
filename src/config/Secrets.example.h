// Secrets.example.h
// Copy to Secrets.h and fill in your Wi-Fi and Firebase credentials.

#pragma once

// Wi-Fi credentials
#define SECRETS_WIFI_SSID      "Afrihost AN"
#define SECRETS_WIFI_PASSWORD  "0716424363"

// Firebase RTDB configuration (mobizt FirebaseClient)
// Provide the RTDB database URL and API key. For auth, either supply email/password
// or a custom token/refresh token via your provisioning flow.
#define SECRETS_FIREBASE_DATABASE_URL "https://geyserswitch-bloc-default-rtdb.firebaseio.com/"
#define SECRETS_FIREBASE_API_KEY      "AIzaSyCvhm9ZC2e_-o6KWSPqOlK7PxEhJoRucFk"
#define SECRETS_FIREBASE_AUTH_EMAIL   "butizwide@gmail.com"
#define SECRETS_FIREBASE_AUTH_PASS    "buti12345"

// Base path and userId for RTDB root composition.
#define SECRETS_BASE_PATH "/GeyserSwitch"
#define SECRETS_USER_ID   "rOh6zwTBmJO2Q7edNkoNiK08oX83"
