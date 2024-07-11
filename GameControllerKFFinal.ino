#include <WiFi.h>
#include <OSCMessage.h>
#include <OSCBundle.h>
#include <EasyButton.h>
#include <FastLED.h>
#include <Adafruit_SleepyDog.h>

const char* ssid = "KFtech";
const char* password = "9447376869";
const IPAddress hostIP_game_system(192, 168, 1, 198);  // IP address of the buzzer system
const IPAddress hostIP_companion_app(192, 168, 1, 202);  // IP address of the companion app
const unsigned int hostPort = 8000;                    // Port for sending OSC messages
const unsigned int hostCOMPANIONPort = 12321;          // Port for sending OSC messages to the companion app

const int resetPin = 38;
const int wifiPin = 21;
const int playerLockPins[] = {18, 17, 47};
const int playerWinPins[] = {9, 8, 7};

bool playerLocked[] = {false, false, false};  // Track player lock status
bool playerLockColor[] = {false, false, false};
bool wifiConnected = false;                   // Track WiFi connection status
bool resetButtonPressed = false;              // Track reset button state
bool playerWon = false;                       // Track if a player has won

WiFiUDP udp;
OSCMessage oscMessage;

#define WINNUM_LEDS 3
#define LOCKNUM_LEDS 3
#define RESETNUM_LEDS 1
#define WIFINUM_LEDS 1
#define TEAMNUM_LEDS 11
#define WINLED_PIN 5
#define LOCKLED_PIN 6
#define RESETLED_PIN 10
#define WIFILED_PIN 11
#define TEAMLED_PIN 4

CRGB WINLED[WINNUM_LEDS];
CRGB LOCKLED[LOCKNUM_LEDS];
CRGB RESETLED[RESETNUM_LEDS];
CRGB WIFILED[WIFINUM_LEDS];
CRGB TEAMLED[TEAMNUM_LEDS];

EasyButton resetButton(resetPin);  // Initialize the reset button
EasyButton wifiButton(wifiPin);
EasyButton playerLockButtons[] = {EasyButton(playerLockPins[0]), EasyButton(playerLockPins[1]), EasyButton(playerLockPins[2])};  // Initialize EasyButton objects for player lock buttons
EasyButton playerWinButtons[] = {EasyButton(playerWinPins[0]), EasyButton(playerWinPins[1]), EasyButton(playerWinPins[2])};      // Initialize EasyButton objects for player win buttons

CRGB teamColors[] = {CRGB(0, 222, 255),    // Team Blue
                     CRGB(204, 0, 204),    // Team Purple
                     CRGB(255, 128, 0)};   // Team Orange

/// Variables for flashing effect
unsigned long previousMillis = 0;
const long flashInterval = 500;  // Interval for the flashing effect
bool ledState = false;

void setup() {
  Serial.begin(115200);
  Watchdog.enable(10000);
  connectToWiFi();
  FastLED.addLeds<WS2812B, WINLED_PIN, GRB>(WINLED, WINNUM_LEDS);
  FastLED.addLeds<WS2812B, LOCKLED_PIN, GRB>(LOCKLED, LOCKNUM_LEDS);
  FastLED.addLeds<WS2812B, RESETLED_PIN, GRB>(RESETLED, RESETNUM_LEDS);
  FastLED.addLeds<WS2812B, WIFILED_PIN, GRB>(WIFILED, WIFINUM_LEDS);
  FastLED.addLeds<WS2812B, TEAMLED_PIN, GRB>(TEAMLED, TEAMNUM_LEDS);
  FastLED.setBrightness(100);

  resetButton.begin();  // Initialize the reset button
  wifiButton.begin();   // Initialize the WiFi button

  for (int i = 0; i < 3; i++) {
    playerLockButtons[i].begin();  // Initialize player lock buttons
    playerWinButtons[i].begin();   // Initialize player win buttons
  }

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  udp.begin(8000);  // Change 8000 to the port number you want to listen on

  // Print the port number being listened on
  Serial.print("Listening on port: ");
  Serial.println(8000);                           // Change 8000 to the port number you want to listen on
  udp.beginPacket(hostIP_game_system, hostPort);  // Initialize UDP for sending OSC messages to game system
  checkResetButton();                             // Check the reset button state

  for (int i = 0; i < 3; i++) {
    TEAMLED[i] = teamColors[0];
  }
  for (int i = 3; i < 7; i++) {
    TEAMLED[i] = teamColors[1];
  }
  for (int i = 7; i < 12; i++) {
    TEAMLED[i] = teamColors[2];
  }
  for (int i = 0; i < 3; i++) {
    LOCKLED[i] = CRGB::Green;
  }
  FastLED.show();
}

void loop() {
  Watchdog.reset();
  resetButton.read();  // Read the state of the reset button
  wifiButton.read();   // Read the state of the WiFi button
  checkResetButton();
  checkWiFiButton();   // Check and handle the WiFi button state
  checkPlayerLocks();  // Check player lock switches
  checkPlayerWins();   // Check player win buttons
  recvOSCMessage();    // Receive OSC messages from game system
  updateWiFiIndicator();
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();   // Attempt to reconnect if WiFi is disconnected
  }
  if (playerWon) {
    pulseResetButton();  // Pulse the reset button if a player has won
  }
  FastLED.show();  // Update WiFi connection indicator
}

void connectToWiFi() {
  Serial.println("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting...");
    WIFILED[0] = CRGB::Red;  // Show red LED when trying to connect
    FastLED.show();
  }
  Serial.println("Connected to WiFi");
  wifiConnected = true;  // WiFi connected
  WIFILED[0] = CRGB::Green;  // Show green LED when connected
  FastLED.show();
}

void checkPlayerLocks() {
  for (int i = 0; i < 3; i++) {
    playerLockButtons[i].read();  // Update the state of the button
    if (playerLockButtons[i].wasPressed()) {
      playerLocked[i] = !playerLocked[i];                  // Toggle player lock status
      sendOSCMessageLock(i + 1, playerLocked[i] ? 1 : 0);  // Send player lock status to game system
      Serial.println("Player " + String(i + 1) + " lock " + (playerLocked[i] ? "engaged" : "disengaged"));
      playerLockColor[i] = !playerLockColor[i];
      // Update LED color for the player
      if (playerLocked[i]) {
        LOCKLED[i] = CRGB::Red;
      } else {
        LOCKLED[i] = CRGB::Green;
      }
    }
  }
}

void checkPlayerWins() {
  for (int i = 0; i < 3; i++) {
    playerWinButtons[i].read();  // Update the state of the button
    if (playerWinButtons[i].wasPressed()) {
      sendOSCMessageWin(i + 1);           // Send player win message to game system
      sendOSCMessageCompanionApp(i + 1);  // Send player win message to companion app
      Serial.println("Player " + String(i + 1) + " won!");
      playerWon = true;
      WINLED[i] = teamColors[i];
    }
  }
}

void sendOSCMessageWin(int playerNumber) {
  String oscAddress = "/player" + String(playerNumber) + "_buzz";
  OSCMessage msg(oscAddress.c_str());
  sendOSC(msg, hostIP_game_system);  // Send message to game system
}

void sendOSCMessageCompanionApp(int playerNumber) {
  String oscAddress;
  switch (playerNumber) {
    case 1:
      oscAddress = "/location/13/0/3/press";
      break;
    case 2:
      oscAddress = "/location/13/0/4/press";
      break;
    case 3:
      oscAddress = "/location/13/0/5/press";
      break;
    default:
      oscAddress = "/error";  // Handle invalid player numbers
      break;
  }
  OSCMessage msg(oscAddress.c_str());
  sendCompanion(msg, hostIP_companion_app);  // Send message to companion app
}

void sendCompanion(OSCMessage& msg, IPAddress ip) {
  udp.beginPacket(ip, hostCOMPANIONPort);
  msg.send(udp);
  udp.endPacket();
  msg.empty();
}

void recvOSCMessage() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    while (packetSize--) {
      oscMessage.fill(udp.read());
    }
    if (!oscMessage.hasError()) {
      if (oscMessage.match("/player1_buzz")) {
        Serial.println("Player 1 buzzed first!");
        WINLED[0] = teamColors[0];
        playerWon = true;
      } else if (oscMessage.match("/player2_buzz")) {
        Serial.println("Player 2 buzzed first!");
        WINLED[1] = teamColors[1];
        playerWon = true;
      } else if (oscMessage.match("/player3_buzz")) {
        Serial.println("Player 3 buzzed first!");
        WINLED[2] = teamColors[2];
        playerWon = true;
      } else {
        // Other OSC messages received, handle as needed
      }
    }
    oscMessage.empty();
  }
}

void sendOSCMessageLock(int playerNumber, int lockStatus) {
  String oscAddress = "/lock_player" + String(playerNumber);
  OSCMessage oscMsg(oscAddress.c_str());
  oscMsg.add(playerNumber);
  oscMsg.add(lockStatus);
  sendOSC(oscMsg, hostIP_game_system);
  Serial.println(oscAddress);  // Send message to game system
}

void updateWiFiIndicator() {
  if (WiFi.status() == WL_CONNECTED && !wifiConnected) {  // WiFi connected
    wifiConnected = true;
    WIFILED[0] = CRGB::Green;                                   // Turn on WiFi connection indicator LED
    FastLED.show();                                             // Update LED color
  } else if (WiFi.status() != WL_CONNECTED && wifiConnected) {  // WiFi disconnected
    wifiConnected = false;
    WIFILED[0] = CRGB::Red;
  }
}

// Function to send an OSC message to reset the game system
void sendResetSignal() {
  OSCMessage msg("/reset_game");
  sendOSC(msg, hostIP_game_system);  // Send reset game message to game system
}

// Function to handle the reset button press event
void handleResetButtonPress() {
  Serial.println("Reset button pressed");
  sendResetSignal();  // Send the reset signal to the game system
  playerWon = false;  // Reset the player won flag
  resetButtonPressed = true;  // Track that the reset button was pressed
  RESETLED[0] = CRGB::Black;
}

// Function to check the reset button state
void checkResetButton() {
  resetButton.read();  // Read the state of the reset button
  if (resetButton.isPressed()) {
    if (!resetButtonPressed) {
      handleResetButtonPress();  // Handle the reset button press
      for (int i = 0; i < 3; i++) {
        WINLED[i] = CRGB::Black;
      }
    }
  } else {
    resetButtonPressed = false;
  }
}

void checkWiFiButton() {
  if (wifiButton.wasPressed()) {
    Serial.println("WiFi button pressed, attempting to reconnect...");
    connectToWiFi();   // Attempt to reconnect to WiFi when button is pressed
  }
}

void sendOSC(OSCMessage& msg, IPAddress ip) {
  udp.beginPacket(ip, hostPort);
  msg.send(udp);
  udp.endPacket();
  msg.empty();
}

// Function to pulse the reset button LED
void pulseResetButton() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= flashInterval) {
    previousMillis = currentMillis;
    ledState = !ledState;  // Toggle the LED state
    RESETLED[0] = ledState ? CRGB::White : CRGB::Black;
    FastLED.show();
  }
}
