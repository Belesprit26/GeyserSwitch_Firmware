GSv1.0.13-Track2 – Bring-up status and guidance

Current status
- Wi‑Fi connects (temp creds injected on boot while USE_TEMP_CREDS=1).
- Time is synced before TLS/auth; Firebase auth completes.
- UID is present; DB paths use /GeyserSwitch/<UID>/... and reads/writes succeed.
- AP announces for provisioning when no creds; AP is switched off when STA connects.

Temperature sensor (DS18B20)
- Symptom: Sensor 1 writes fail with HTTP 400 and logs show NaN.
- Cause: NaN cannot be encoded to JSON; likely no sensor detected on the OneWire bus.
- Hardware checklist:
  - Data pin: GPIO 21 (as per code) with a 4.7 kΩ pull‑up to 3V3.
  - Correct 3V3 and GND; common ground.
- Software safety:
  - Only write the temperature when it’s finite (isfinite(temp)). Skip otherwise.

OTA firmware check
- Code expects (per‑user) RTDB nodes:
  - /GeyserSwitch/<UID>/firmware/version
  - /GeyserSwitch/<UID>/firmware/url
- Create these nodes (string) for the current UID, or change the code to your actual global path.

Credentials management
- Temp creds live in temp_credentials.h. While USE_TEMP_CREDS=1, they are written to EEPROM on boot.
- After verification, set USE_TEMP_CREDS=0 so EEPROM is not overwritten each boot.
- Logs print EEPROM contents once per boot; the "No valid Wi‑Fi credentials" message is printed once per boot if either SSID or password is empty.

TLS and time
- TLS requires correct RTC. We now sync NTP after Wi‑Fi and before setupFirebase().
- WiFiClientSecure CA is applied after the SSL client is allocated.

Path base and auth gating
- We don’t write to the database until auth is ready.
- Once firebaseIsReady() returns true, firebaseInitUserPath() appends UID to the base path exactly once, preventing double slashes.

AP/STA behavior
- When Wi‑Fi cannot connect, the sketch announces AP (config portal).
- After STA connects, AP is explicitly disabled: WiFi.softAPdisconnect(true); WiFi.mode(WIFI_STA);

Next steps (checklist)
- [ ] Fix DS18B20 wiring (GPIO 21 + 4.7 kΩ pull‑up) and confirm finite readings.
- [ ] Add a write guard for temperature (isfinite(temp) before DB write).
- [ ] Create /firmware/version and /firmware/url under /GeyserSwitch/<UID>/ or adjust code to your actual path.
- [ ] Set USE_TEMP_CREDS=0 after confirming Wi‑Fi works with saved creds.

Notes
- Leak sensor pins for ESP32‑C6 need confirmation. Prior ESP32 pins (25/33) aren’t valid on C6. Use confirmed C6 GPIOs (e.g., POWER_PIN=18, AO_PIN=4) when re‑enabling.
- Reduce log noise later (e.g., time sync prints) once bring‑up completes.
