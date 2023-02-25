// Hardware headers
#include <SPI.h>
#include <string.h>

// Network headers
#include <RHRouter.h>
#include <RHMesh.h>
#include <RH_RF95.h>
#include <LoPoDeMesh.h>

// Gateway headers
// #include <WiFi.h>
// #include <WiFiClient.h>
// #include <WiFiConfig.h>
// #include <PubSubClient.h>
// #include <MQTTConfig.h>

// Log header
#include <Preferences.h>


RH_RF95 rf95w(FREENOVE_CS, FREENOVE_ITQ);  // LoRa transceiver driver
RHMesh *manager;                           // Mesh network instance
char buf[MAX_MESSAGE_LEN];                 // String buffer for receiving

uint8_t routes[NODES];  // Routing table (Debug data | Gateway data)
int16_t rssi[NODES];    // RSSI table against all other nodes (Debug data | Gateway data)

// WiFiClient espClient;                 // Arduino-ESP32 WiFiSTA instance
// PubSubClient mqtt_client(espClient);  // MQTT client instance
// #define PUBLISH_INTERVAL 3000         // Interval for publishing visualize route data
// unsigned long publishTime = 0;        // previous publish time

#define LOG_NAMESPACE "log"        // log namespace name
#define LOG_COUNT_KEY "log-count"  // log count key name
#define LISTEN_INTERVAL 3000       // Interval for propagating visualize route data
Preferences preferences;           // Log Instance
uint32_t logCount = 0;             // log count


// Initalize routes[] and rssi[]
void initLoRaData() {
  for (uint8_t node = 1; node <= NODES; node++) {
    if (node == GROUND_ID) {
      routes[node - 1] = 255;
    } else {
      routes[node - 1] = 0;
    }
    rssi[node - 1] = 0;
  }
}

// Initalize and confiture all objects and variables related with LoRa
void initLoRaDriver() {
  // Initalize RHMesh
  Serial.print(F("Init node.. "));

  // Configure SPI pin in the board
  SPI.begin(FREENOVE_SCK, FREENOVE_MISO, FREENOVE_MOSI, FREENOVE_CS);

  // Initalize RadioHead Mesh object
  manager = new RHMesh(rf95w, GROUND_ID);
  if (!manager->init()) {
    Serial.println(F("!! init failed !!"));
  } else {
    Serial.println("done");
  }

  // Configure LoRa driver
  rf95w.setTxPower(LORA_TX_POWER, LORA_TX_USERFO);
  rf95w.setFrequency(LORA_FREQUENCY);
  rf95w.setCADTimeout(LORA_TIMEOUT);
  // Configure Model for configuring LoRa parameter
  if (!rf95w.setModemConfig(LORA_MODEM_CONFIG)) {
    Serial.println(F("!! Modem config failed !!"));
  }

  Serial.println("RF95 Ready");

  initLoRaData();
}

// // Connect to MQTT broker
// void mqtt_connect() {
//   // Loop connecting until mqtt client is connected
//   while (!mqtt_client.connected()) {
//     Serial.print(F("Connecting to MQTT...  "));

//     // Generate randome mqtt client id
//     String mqtt_clientId = "gateway-";
//     mqtt_clientId += String(random(0xffff), HEX);

//     // Attempt to connect
//     if (mqtt_client.connect(mqtt_clientId.c_str(), MQTT_GROUND_USERNAME, MQTT_GROUND_PASSWORD)) {  // if connection is successful
//       Serial.println(F("[Connected]"));
//     } else {  // if connection is failed
//               // Log the reason of fail and Retry after 2000ms
//       Serial.print(F("[Failed, rc="));
//       Serial.print(mqtt_client.state());
//       Serial.println("]");
//       delay(2000);
//     }
//   }
// }

/*
 Initalize features needed as the gateway of visualizing server

  Related code
  @header   WiFi.h, WiFiClient.h, WiFiConfig.h, PubSubClient.h, MQTTConfig.h
  @func     mqtt_connect()
  @var      mqtt_client, espClient
*/
// void initGateway() {
//   // Initialize WiFi STA
//   Serial.print("Connecting to AP - ");
//   Serial.print(WIFI_SSID);

//   WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
//   // If connection isn't done yet, wait for 250ms till done
//   while (WiFi.status() != WL_CONNECTED) {
//     delay(250);
//     Serial.print(". ");
//   }
//   Serial.println("[Connected]");

//   // Initialize MQTT server connection
//   mqtt_client.setServer(MQTT_SERVER, MQTT_PORT);
//   mqtt_connect();
// }

/*
 Initalize logging feature

  Related code
  @header   Preferences.h
  @var      preferences, logCount
*/
void initLog() {
  // Open namespace with Preferences instance
  Serial.print(F("Logging."));
  while (!preferences.begin(LOG_NAMESPACE, false)) {
    Serial.print('.');
    delay(500);
  }
  Serial.println(F(" Ready"));

  // Get the saved number of log to use as key, otherewise default 0
  if (preferences.isKey(LOG_COUNT_KEY)) {
    logCount = preferences.getInt(LOG_COUNT_KEY);
  }
}

void setup() {
  randomSeed(analogRead(0));
  Serial.begin(BUAD_RATE);
  while (!Serial)
    ;  // Wait untill serial is ready

  Serial.println(F("[Ground Node 1]"));

  // Initialize LoRa network
  initLoRaDriver();

  // * Execute to enable gateway features
  // initGateway();
  // * Execute to enable logging
  initLog();
}


// Exchange error number to error string by flash string
const __FlashStringHelper *getErrorString(uint8_t error) {
  switch (error) {
    case 1:
      return F("INVALID LENGTH");
      break;
    case 2:
      return F("NO ROUTE");
      break;
    case 3:
      return F("TIMEOUT");
      break;
    case 4:
      return F("NO REPLY");
      break;
    case 5:
      return F("UNABLE TO DELIVER");
      break;
  }
  return F("UNKNOWN");
}

// Propagate a signal to all other nodes to update routing
void propagateSignal() {
  for (uint8_t destination = 1; destination <= NODES; destination++) {
    if (destination == GROUND_ID) {   // if destination is self
      routes[destination - 1] = 255;  // mark itself

    } else {
      // Transmit a signal to destination
      // Debug transmission
      Serial.print("[SEND TO ");
      Serial.print(destination);
      Serial.print("] ");

      // Small piece of signal
      buf[0] = 'H';
      buf[1] = 'e';
      buf[2] = 'r';
      buf[3] = 'e';
      buf[4] = '\0';
      //

      // Transmit a signal to destination
      uint8_t error = manager->sendtoWait((uint8_t *)buf, sizeof(buf), destination);
      if (error != RH_ROUTER_ERROR_NONE) {  // if transmit error is occured, print detail error
        Serial.print(F(" !! "));
        Serial.println(getErrorString(error));

        // if transmission is failed, reset the routing data
        routes[destination - 1] = 0;
        rssi[destination - 1] = 0;
      } else {  // if transmit is successful
        Serial.println(F(" OK"));
      }

      // Update the routing table after transmission is done
      RHRouter::RoutingTableEntry *route = manager->getRouteTo(destination);
      if (route == NULL) continue;  // If routing is fail, don't update routing data to avoid NULL reference

      // Update the routing data (next_hop, rssi)
      routes[destination - 1] = route->next_hop;
      rssi[destination - 1] = rf95w.lastRssi();

      if (routes[destination - 1] == 0) {  // if there is no route
        rssi[destination - 1] = 0;         // reset rssi
      }
    }
  }
}

// Generate route info as formatted string in buffer
void generateRouteInfoStringInBuf() {
  // Mark start of buffer by null for sprintf(), strcat()
  buf[0] = '\0';

  // Loop all nodes
  for (uint8_t node = 1; node <= NODES; node++) {
    sprintf(buf + strlen(buf), "%d", routes[node - 1]);
    strcat(buf, ",");
    sprintf(buf + strlen(buf), "%d", rssi[node - 1]);

    // Node spliter
    if (node < NODES) {
      strcat(buf, "/");
    }
  }
}

// // Publish route info saved in bffer on MQTT server
// void publishRouteInfoInBuf(uint8_t nodeId) {
//   // Append the source of route info at buffer
//   strcat(buf, "[");
//   sprintf(buf + strlen(buf), "%d", nodeId);

//   // Print route info in serial for debugging
//   Serial.print("[PUB] ");
//   Serial.println(buf);

//   // Publish data on MQTT server by topic
//   mqtt_client.publish(MQTT_GATEWAY_TOPIC, buf);
// }

// Listen transmit coming to ground
// Save transmitted data in buffer and return source number
uint8_t listenIncomingRouteInfoInBuf() {
  // Listen incoming messages
  // Wait for LISTEN_INTERVAL untill next transmit
  unsigned long nextTransmit = millis() + LISTEN_INTERVAL;
  while (nextTransmit > millis()) {

    uint8_t len = sizeof(buf);
    uint8_t source;

    // Listen transmission
    if (manager->recvfromAck((uint8_t *)buf, &len, &source)) {
      // Mark last length by null
      buf[len] = '\0';

      // Debug receiving
      Serial.print("[RECV FROM ");
      Serial.print(source);
      Serial.println("] ");

      // Update the routing table after receiving is done
      RHRouter::RoutingTableEntry *route = manager->getRouteTo(source);
      if (route == NULL) continue;  // If routing is fail, don't update routing data to avoid NULL reference

      // Update the routing data (next_hop, rssi)
      routes[source - 1] = route->next_hop;
      rssi[source - 1] = rf95w.lastRssi();

      if (routes[source - 1] == 0) {  // if there is no route
        rssi[source - 1] = 0;         // reset rssi
      }

      // return source number
      return source;
    }
  }

  // If there is no transmission, return 0
  return 0;
}

/*
 Publish ground route info and other nodes route info which is transmitted on MQTT server

  Related code
  @header   WiFi.h, WiFiClient.h, WiFiConfig.h, PubSubClient.h, MQTTConfig.h
  @func     mqtt_connect(), propagateSignal(), generateRouteInfoStringInBuf(), publishRouteInfoInBuf(), listenIncomingRouteInfoInBuf()
  @var      mqtt_client, espClient
*/
// void runGateway() {
//   // If MQTTT connection isn't done, keep trying and waiting
//   if (!mqtt_client.connected()) {
//     mqtt_connect();
//   }
//   mqtt_client.loop();

//   // Loop in every PUBLISH_INTERVAL
//   if (millis() - publishTime > PUBLISH_INTERVAL) {
//     // update route info, generate string and publish it about ground node
//     propagateSignal();
//     generateRouteInfoStringInBuf();
//     publishRouteInfoInBuf(1);

//     // Update published time
//     publishTime = millis();
//   }

//   // Publish node route info whenever transmit is done
//   uint8_t transmitSource = listenIncomingRouteInfoInBuf();
//   if (transmitSource) {  // If source is not 0 (= transmission is done)
//     publishRouteInfoInBuf(transmitSource);
//   }
// }

// Log char pointer parameter in namespace
void log(char *ptr) {
  // Type casting 'logCount' from Int to Char pointer
  char logCountCharPtr[5];
  itoa(logCount, logCountCharPtr, 10);

  // Save log (key=log count, value=parameter)
  preferences.putString((const char *)logCountCharPtr, ptr);

  // Update logCount
  logCount++;
  // Preferences.h can not overwrite, so after removing, save again
  preferences.remove(LOG_COUNT_KEY);
  preferences.putInt(LOG_COUNT_KEY, logCount);
}

/*
 Propagate a signal and log a network status (route, rssi)

  Related code
  @header   Preferences.h
  @var      preferences, logCount
*/
void runExperimentWithLog() {
  // Propagate signal to all other nodes
  propagateSignal();

  // Log and print a route info of Ground
  generateRouteInfoStringInBuf();
  log((char *)buf);
  Serial.println(buf);

  // Log a receiving
  uint8_t transmitSource = listenIncomingRouteInfoInBuf();
  if (transmitSource) {  // If source is not 0 (= transmission is done)
    // print a route info of Ground
    generateRouteInfoStringInBuf();
    Serial.println(buf);

    // Type casting 'transmitSource' from uint8_t to Char pointer
    char sourceCharPtr[5];
    itoa(transmitSource, sourceCharPtr, 10);

    buf[0] = '\0';
    strcat(buf, "[RECV FROM ");
    strcat(buf, sourceCharPtr);
    log((char *)buf);
  }
}

void loop() {
  // * Execute to enable gateway features
  // runGateway();

  // * Execute to enable experiment with log
  runExperimentWithLog();
}