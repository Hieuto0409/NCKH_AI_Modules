#pragma once
// Copy to network_secrets.h (ignored by Git). ESP32-S3 needs 2.4 GHz Wi-Fi.
// Profile ...-thingsboard uses HTTPS 443 with CA validation (GTS Root R4).
// Profile ...-mqtt uses TCP 1883 and is intended only for a trusted LAN broker.
// Use a DEVICE access token, never the ThingsBoard tenant password.
#define PPGFW_WIFI_SSID ""
#define PPGFW_WIFI_PASSWORD ""
#define PPGFW_TB_HOST ""
#define PPGFW_TB_TOKEN ""
