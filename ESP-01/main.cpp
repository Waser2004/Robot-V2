#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

#define SERIAL_BAUD 115200

// Wifi credentials
const char* WIFI_SSID      = "******"; // Replate with your WiFi SSID (network name)
const char* WIFI_PASS      = "******"; // Replace with your WiFi password

// mDNS hostname (for local network access: "RobotV2.local")
const char* HOSTNAME       = "RobotV2";
const uint16_t SERVER_PORT = 80;

// Wifi server and client objects
WiFiServer server(SERVER_PORT);
WiFiClient client;

void connectWiFi() {
    /* This functino handles the connection to the Wifi*/
    // If already connected, do nothing
    if (WiFi.status() == WL_CONNECTED) return;

    // initialize WiFi in station mode
    WiFi.mode(WIFI_STA);
    WiFi.hostname(HOSTNAME);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    // await connection
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(250);
        Serial.print('.');
    }

    // connection established (log IP)
    Serial.print("\nWiFi OK: IP=");
    Serial.println(WiFi.localIP());
}

bool startMDNS() {
    /* This function starts the mDNS responder */
    if (MDNS.begin(HOSTNAME)) {
        // mDNS responder started successfully
        Serial.print("mDNS started: http://");
        Serial.print(HOSTNAME);
        Serial.println(".local");

        return true;
    } else {
        // mDNS responder failed to start
        Serial.println("mDNS start failed");

        return false;
    }
}

void SerialToWifi() {
    /* This function handles the data transfer from Serial to WiFi client */
    // if no client is connected replay to Serial with no connection error
    if (!client || !client.connected()) {
        Serial.println("No WiFi client connected");
        return;
    }

    // send data from serial to WiFi client
    while (Serial.available() && client && client.connected()) {
        uint8_t b = Serial.read();
        client.write(&b, 1);
    }
}

void WifiToSerial() {
    /* This function handles the data transfer from WiFi client to Serial */
    while (client && client.connected() && client.available()) {
        uint8_t b = client.read();
        Serial.write(b);
    }
}

void setup() {
    // Initialize serial communication with Arduino Mega
    Serial.begin(SERIAL_BAUD);

    // Connect to Wifi and start mDNS
    connectWiFi();
    startMDNS();

    // Start TCP server
    server.begin();
    Serial.print("TCP server started on port ");
    Serial.println(SERVER_PORT);
}

void loop() {
    // Keep mDNS responder running
    MDNS.update();

    // Accept new client if none is connected
    if (!client || !client.connected()) {
        client = server.available();
    }

    // Handle data transfer
    SerialToWifi();
    WifiToSerial();

    // Give WiFi stack time
    yield();
}
