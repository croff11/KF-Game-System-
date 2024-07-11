#include <WiFi.h>
#include <OSCMessage.h>
#include <OSCBundle.h>
#include <FastLED.h>
#include <EasyButton.h>
#include <Adafruit_SleepyDog.h>

// WiFi credentials and IP addresses
const char* ssid = "KFtech";
const char* password = "9447376869";
const IPAddress hostIP_game_master(192, 168, 1, 178);//IP Address for the the game controller 
const IPAddress hostIP_companion_app(192, 168, 1, 202);//IP Address for the companion app
const unsigned int hostCOMPANIONPort = 12321;
const unsigned int hostMASTERPort = 8000;

// Pin definitions and constants
const int buzzerPins[] = { 17, 18, 21 };
bool playerBuzzed[] = { false, false, false };
bool playerLocked[] = { false, false, false };
bool lockout = false;
int winningPlayer = 0;
bool buzzerState = false;

WiFiUDP udp;
OSCMessage msgCOMPANION;
OSCMessage msgMASTER;
OSCMessage oscMessage;
OSCMessage receivedMessage;

const unsigned long messageInterval = 1000;
unsigned long lastMessageTime = 0;
unsigned long lastFlashTime = 0; // For non-blocking flash timing
bool flashState = false; // State of the flash

#define P1ButtLED_PIN 8
#define P2ButtLED_PIN 9
#define P3ButtLED_PIN 10
#define P1BarLED_PIN 5
#define P2BarLED_PIN 6
#define P3BarLED_PIN 7
#define WifiLED_PIN 38
#define P1ButtNUM_LEDS 24
#define P2ButtNUM_LEDS 24
#define P3ButtNUM_LEDS 24
#define P1BarNUM_LEDS 49
#define P2BarNUM_LEDS 49
#define P3BarNUM_LEDS 49
#define WifiNUM_LEDS 3

CRGB P1Buttleds[P1ButtNUM_LEDS];
CRGB P2Buttleds[P2ButtNUM_LEDS];
CRGB P3Buttleds[P3ButtNUM_LEDS];
CRGB P1Barleds[P1BarNUM_LEDS];
CRGB P2Barleds[P2BarNUM_LEDS];
CRGB P3Barleds[P3BarNUM_LEDS];
CRGB Wifileds[WifiNUM_LEDS];

EasyButton playerButtons[] = { 
  EasyButton(buzzerPins[0], 100, true, true), 
  EasyButton(buzzerPins[1], 100, true, true), 
  EasyButton(buzzerPins[2], 100, true, true) 
};

CRGB teamColors[] = {
  CRGB(0, 222, 255), // Team Blue
  CRGB(204, 0, 204), // Team Purple
  CRGB(255, 128, 0)  // Team Orange
};

void setup() {
  Serial.begin(115200);
  connectToWiFi();
  Watchdog.enable(10000);

  // Initialize FastLED for ButtLEDs
  FastLED.addLeds<WS2812B, P1ButtLED_PIN, GRB>(P1Buttleds, P1ButtNUM_LEDS);
  FastLED.addLeds<WS2812B, P2ButtLED_PIN, GRB>(P2Buttleds, P2ButtNUM_LEDS);
  FastLED.addLeds<WS2812B, P3ButtLED_PIN, GRB>(P3Buttleds, P3ButtNUM_LEDS);

  // Initialize FastLED for BarLEDs
  FastLED.addLeds<WS2812B, P1BarLED_PIN, GRB>(P1Barleds, P1BarNUM_LEDS);
  FastLED.addLeds<WS2812B, P2BarLED_PIN, GRB>(P2Barleds, P2BarNUM_LEDS);
  FastLED.addLeds<WS2812B, P3BarLED_PIN, GRB>(P3Barleds, P3BarNUM_LEDS);

  // Initialize FastLED for WiFi status LED
  FastLED.addLeds<WS2812B, WifiLED_PIN, GRB>(Wifileds, WifiNUM_LEDS);

  FastLED.setBrightness(100);

  // Initialize player buttons and LEDs
  for (int i = 0; i < 3; i++) {
    playerButtons[i].begin();
    setPlayerLEDs(i + 1, teamColors[i]); // Set to team colors
  }

  FastLED.show();
  udp.beginPacket(hostIP_game_master, hostMASTERPort);
  udp.beginPacket(hostIP_companion_app, hostCOMPANIONPort);
  Serial.println("Game system initialized.");
  Serial.println(WiFi.localIP());
  udp.begin(8000);
  Serial.print("Listening on port: ");
  Serial.println(8000);
}

void loop() {
  Watchdog.reset();
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi(); // Reconnect if WiFi is lost
  }
  if (!lockout) {
    checkBuzzers();
  } else {
    flashWinningPlayerLEDs(); // Handle flashing LEDs for the winning player
  }
  for (int i = 0; i < 3; i++) {
    playerButtons[i].read();
  }
  recvOSCMessage();
  updateWiFiStatusLED(); // Update WiFi status LED
}

void connectToWiFi() {
  Serial.println("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting...");
  }
  Serial.println("Connected to WiFi");
}

void checkBuzzers() {
  for (int i = 0; i < 3; i++) {
    if (playerButtons[i].wasPressed()) {
      Serial.print("Button ");
      Serial.print(i + 1);
      Serial.println(" was pressed");
    }
    if (playerButtons[i].wasPressed() && !playerLocked[i]) {
      Serial.print("Player ");
      Serial.print(i + 1);
      Serial.println(" buzzed!");
      playerBuzzed[i] = true;
      winningPlayer = i + 1;
      lockout = true;
      sendOSCMessage(winningPlayer);
      sendOSCMessageCompanionApp(winningPlayer);

      for (int j = 0; j < 3; j++) {
        if (j == i) {
          flashState = true;
          lastFlashTime = millis(); // Initialize the flash timer
        } else {
          setPlayerLEDs(j + 1, CRGB::Black);
        }
      }
      break;
    }
  }
}

void recvOSCMessage() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    while (packetSize--) {
      oscMessage.fill(udp.read());
    }
    if (!oscMessage.hasError()) {
      if (oscMessage.match("/reset_game")) {
        Serial.println("Reset Game");
        lockout = false;
        flashState = false;
        for (int i = 0; i < 3; i++) {
          if (!playerLocked[i]) {
            setPlayerLEDs(i + 1, teamColors[i]);
          }
        }
      } else if (oscMessage.match("/lock_player*")) {
        int playerNumber = oscMessage.getInt(0);
        int lockStatus = oscMessage.getInt(1);

        Serial.print("Player ");
        Serial.print(playerNumber);
        Serial.print(" lock status: ");
        Serial.println(lockStatus);

        if (lockStatus == 1) {
          playerLocked[playerNumber - 1] = true;
          setPlayerLEDs(playerNumber, CRGB::Red);
        } else if (lockStatus == 0) {
          playerLocked[playerNumber - 1] = false;
          setPlayerLEDs(playerNumber, teamColors[playerNumber - 1]);
        } else {
          Serial.println("Invalid lock status received.");
        }
      } else {
        for (int i = 1; i <= 3; i++) {
          String oscAddress = "/player" + String(i) + "_buzz";
          if (oscMessage.match(oscAddress.c_str())) {
            Serial.print("Player ");
            Serial.print(i);
            Serial.println(" buzzed (force win)!");
            forceWin(i);
            break;
          }
        }
      }
    }
    oscMessage.empty();
  }
}

void sendOSCMessage(int playerNumber) {
  String oscAddress = "/player" + String(playerNumber) + "_buzz";
  OSCMessage msgMASTER(oscAddress.c_str());
  sendMaster(msgMASTER, hostIP_game_master);
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
      oscAddress = "/error";
      break;
  }
  OSCMessage msgCOMPANION(oscAddress.c_str());
  sendCompanion(msgCOMPANION, hostIP_companion_app);
}

void sendCompanion(OSCMessage& msg, IPAddress ip) {
  udp.beginPacket(ip, hostCOMPANIONPort);
  msg.send(udp);
  udp.endPacket();
  msg.empty();
}

void sendMaster(OSCMessage& msg, IPAddress ip) {
  udp.beginPacket(ip, hostMASTERPort);
  msg.send(udp);
  udp.endPacket();
  msg.empty();
}

void setPlayerLEDs(int player, CRGB color) {
  Serial.print("Setting LEDs for player ");
  Serial.print(player);
  Serial.print(" to color ");
  Serial.print(color.r);
  Serial.print(", ");
  Serial.print(color.g);
  Serial.print(", ");
  Serial.println(color.b);

  CRGB* buttLEDs;
  CRGB* barLEDs;
  int buttNumLEDs, barNumLEDs;

  switch (player) {
    case 1:
      buttLEDs = P1Buttleds;
      barLEDs = P1Barleds;
      buttNumLEDs = P1ButtNUM_LEDS;
      barNumLEDs = P1BarNUM_LEDS;
      break;
    case 2:
      buttLEDs = P2Buttleds;
      barLEDs = P2Barleds;
      buttNumLEDs = P2ButtNUM_LEDS;
      barNumLEDs = P2BarNUM_LEDS;
      break;
    case 3:
      buttLEDs = P3Buttleds;
      barLEDs = P3Barleds;
      buttNumLEDs = P3ButtNUM_LEDS;
      barNumLEDs = P3BarNUM_LEDS;
      break;
    default:
      return;
  }

  fill_solid(buttLEDs, buttNumLEDs, color);
  fill_solid(barLEDs, barNumLEDs, color);
  FastLED.show();
}

void flashWinningPlayerLEDs() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastFlashTime >= 500) { // Change flash interval as needed
    lastFlashTime = currentMillis;
    flashState = !flashState;
    setPlayerLEDs(winningPlayer, flashState ? teamColors[winningPlayer - 1] : CRGB::Black);
  }
}

void updateWiFiStatusLED() {
  if (WiFi.status() == WL_CONNECTED) {
    fill_solid(Wifileds, WifiNUM_LEDS, CRGB::Green);
  } else {
    fill_solid(Wifileds, WifiNUM_LEDS, CRGB::Red);
  }
  FastLED.show();
}

void forceWin(int playerNumber) {
  if (playerNumber < 1 || playerNumber > 3) return; // Validate player number

  winningPlayer = playerNumber;
  lockout = true;

  sendOSCMessage(winningPlayer);
  sendOSCMessageCompanionApp(winningPlayer);

  for (int j = 0; j < 3; j++) {
    if (j == winningPlayer - 1) {
      flashState = true;
      lastFlashTime = millis(); // Initialize the flash timer
    } else {
      setPlayerLEDs(j + 1, CRGB::Black);
    }
  }
}
