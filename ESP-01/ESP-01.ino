#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

#define SERIAL_BAUD 115200
#define CHUNK_SIZE 256

const char* WIFI_SSID = "VenusIntranet";
const char* WIFI_PASS = "Venus$Intra!";
const char* HOSTNAME  = "RobotV2";
const uint16_t SERVER_PORT = 80;

WiFiServer server(SERVER_PORT);
WiFiClient client;

void connectWiFi() {
  // exit if already connected
  if (WiFi.status() == WL_CONNECTED) return;

  // setup Wifi in station mode
  WiFi.mode(WIFI_STA);
  WiFi.hostname(HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  WiFi.setSleep(false);

  // wait for connection
  while (WiFi.status() != WL_CONNECTED) { delay(250); }
}

void setup() {
  // Initialize Serial communication
  Serial.begin(SERIAL_BAUD);
  Serial.setDebugOutput(false);
  
  // setup Wifi + mDNS for RobotV2.local
  connectWiFi();
  MDNS.begin(HOSTNAME);
  server.begin();
}

// Separate buffers for SerialToWifi and WifiToSerial
static uint8_t serialToWifiBuffer[2048];
static uint8_t wifiToSerialBuffer[2048];

inline void SerialToWifi() {
  // If there's no active TCP client, nothing to forward.
  if (!client || !client.connected()) return;

  // if no data available from UART, nothing to forward
  int availableBytes = Serial.available();
  if (availableBytes <= 0) return;

  // Clamp to local buffer size to avoid overflow.
  if (availableBytes > (int)sizeof(serialToWifiBuffer)) availableBytes = sizeof(serialToWifiBuffer);

  // Read a chunk from UART into our local buffer.
  int bytesRead = Serial.readBytes(serialToWifiBuffer, availableBytes);
  if (bytesRead <= 0) return;

  // Write to TCP with back-pressure handling. The TCP write call may accept
  // fewer bytes than requested, so loop until the full chunk is sent or the
  // client disconnects.
  int offset = 0;
  while (offset < bytesRead && client.connected()) {
    // if the send buffer is full, wait for it to drain
    int writeSpace = client.availableForWrite();
    if (writeSpace <= 0) {
        yield();
        continue;
    }

    // write as much as possible but no more than CHUNK_SIZE at a time
    int toWrite = (bytesRead - offset < writeSpace) ? (bytesRead - offset) : writeSpace;
    int written = client.write(serialToWifiBuffer + offset, toWrite);

    // advance offset if anything was written
    if (written > 0) offset += written;
    yield();
  }
}

inline void WifiToSerial() {
  // If there's no active TCP client, nothing to forward.
  if (!client || !client.connected()) return;

  // How many bytes are available from the TCP client.
  int availableBytes = client.available();
  if (availableBytes <= 0) return;

  // Clamp to the local buffer size.
  if (availableBytes > (int)sizeof(wifiToSerialBuffer)) availableBytes = sizeof(wifiToSerialBuffer);

  // Read a chunk from TCP into our local buffer.
  int bytesRead = client.read(wifiToSerialBuffer, availableBytes);
  if (bytesRead <= 0) return;

  // Write to UART honoring Serial's available-for-write/back-pressure and RTS flow control.
  int offset = 0;
  while (offset < bytesRead) {
    // if RX buffer of Arduino is full, wait
    int serialWriteSpace = Serial.availableForWrite();
    if (serialWriteSpace <= 0) { 
        yield(); 
        continue; 
    }

    // write as much as possible but no more than CHUNK_SIZE at a time
    int toWrite = (bytesRead - offset < serialWriteSpace) ? (bytesRead - offset) : serialWriteSpace;
    toWrite     = (toWrite < CHUNK_SIZE) ? toWrite : CHUNK_SIZE;

    int written = Serial.write(wifiToSerialBuffer + offset, toWrite);

    // advance offset if anything was written
    if (written > 0) offset += written;
    yield();
  }
}

void loop() {
  MDNS.update();

  if (WiFi.status() != WL_CONNECTED) connectWiFi();

  // If we don't have a connected client yet, accept an incoming one.
  if (!client || !client.connected()) {
    WiFiClient incomingClient = server.available();
    if (incomingClient) {
      // Drop any previous client and adopt the new one.
      client.stop();
      client = incomingClient;
      client.setNoDelay(true);
    }
  }

  SerialToWifi();
  WifiToSerial();
  yield();
}
