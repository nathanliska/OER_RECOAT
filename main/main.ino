/*
 * OER_RECOAT
 * Version 0.1
 */

// WiFi
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// Webhook
#include <ESP8266Webhook.h>

// ADXL313
#include <Wire.h>
#include <SparkFunADXL313.h>

#define BLUE_LED 16

/* Put your SSID & Password */
#define WIFI_SSID "SSID"

#define WIFI_PASSWORD "PASSWORD"


/* IFTTT Information */
#define KEY "KEY"        // Webhooks Key
#define EVENT "message_send"               // Webhooks Event Name

/* Threshold Information */
#define THRESHOLD_ACTIVITY 50
#define THRESHOLD_INACTIVITY 20
#define THRESHOLD_READINGS 20
#define THRESHOLD_IMPACT 300

/* Range of ADXL 313 */
#define RANGE ADXL313_RANGE_05_G            // 0.5g Range

/* Global Objects */
ESP8266WebServer server(80);

Webhook webhook(KEY, EVENT);

ADXL313 myAdxl;

/* Global Variables */
bool started = false;
bool calibrated = false;

int offset = -1;
int rawReading, zeroedReading, prevReading;
int iterator = 0, impacts = 0, layer = 1;
int response, i;
long int sum;

// Enum for states
enum State {
  LEFT,
  MOVE_RIGHT,
  RIGHT,
  MOVE_LEFT,
  SEND
} currentState;

/* CONNECT TO WIFI */
void connectToWifi() {
  Serial.println();
  Serial.print("Connecting to Wi-Fi...");
  // RED LED ON
  // digitalWrite(LED_BUILTIN, LOW);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

/* MAIN SETUP */
void setup() {
  // Begin serial and setup LEDs
  Serial.begin(115200);
  digitalWrite(BLUE_LED, HIGH);
  digitalWrite(LED_BUILTIN, HIGH);
  pinMode(BLUE_LED, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  // ADXL313 Setup
  Wire.begin();
  setupAdxl();
  myAdxl.measureModeOn();
  Serial.println("ADXL313 Setup Complete");

  // Connect to Wifi
  connectToWifi();

  digitalWrite(LED_BUILTIN, LOW);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  digitalWrite(LED_BUILTIN, HIGH);

  // Send Wifi info to webhook
  int response = webhook.trigger("LAYER", "IMPACTS", "http://" + WiFi.localIP().toString());

  // Print webhook response and IP address
  Serial.println();
  if (response == 200) {
    Serial.println("Success.");
  } else {
    Serial.println("Failed.");
  }

  Serial.println();
  Serial.print("ESP IP Address: http://");
  Serial.println(WiFi.localIP());

  // Setup server routes for web page and begin server
  server.on("/", handle_OnConnect);
  server.on("/start", handle_start);
  server.on("/calibrate", handle_calibrate);
  server.onNotFound(handle_NotFound);

  server.begin();

  Serial.println("HTTP Server Started");

  while (!started) {
    server.handleClient();
  }
}

/* MAIN LOOP */
void loop() {

  /* Initial State Starts Here */
  switch(currentState) {
    /* LEFT State */
    case LEFT:
      // Serial.println("Current State: LEFT");
      if (myAdxl.dataReady()) {
        myAdxl.readAccel();
        zeroedReading = myAdxl.z + offset;

        Serial.println(zeroedReading);

        if (abs(zeroedReading) > THRESHOLD_ACTIVITY) {
          // Change currentState
          iterator = 0;

          Serial.println("Current State: MOVE_RIGHT");
          currentState = MOVE_RIGHT;
          digitalWrite(BLUE_LED, LOW);
        }
      }

      break;
    /* MOVE_RIGHT State */
    case MOVE_RIGHT:
      // Serial.println("Current State: MOVE_RIGHT");
      if (myAdxl.dataReady()) {
        myAdxl.readAccel();
        zeroedReading = myAdxl.z + offset;

        Serial.println(zeroedReading);

        if (iterator < THRESHOLD_READINGS) {
          if (abs(zeroedReading) > THRESHOLD_INACTIVITY) {
            iterator = 0;
            break;
          } else {
            iterator++;
          }
        } else {
          // Change currentState
          Serial.println("Current State: RIGHT");
          currentState = RIGHT;
          digitalWrite(BLUE_LED, HIGH);
        }
      }

      break;
    /* RIGHT State */
    case RIGHT:
      // Serial.println("Current State: RIGHT");
      if (myAdxl.dataReady()) {
        myAdxl.readAccel();
        zeroedReading = myAdxl.z + offset;

        Serial.println(zeroedReading);

        if (abs(zeroedReading) > THRESHOLD_ACTIVITY) {
          // Change currentState
          iterator = 0;

          Serial.println("Current State: MOVE_LEFT");
          currentState = MOVE_LEFT;
          impacts = 0;
          digitalWrite(LED_BUILTIN, LOW);
          delay(10); // Only want velocity, no acceleration
        }
      }

      break;
    /* MOVE_LEFT State */
    case MOVE_LEFT: // DETECT IMPACTS HERE
      // Serial.println("Current State: MOVE_LEFT");
      // read accel

      if (myAdxl.dataReady()) {
        myAdxl.readAccel();
        prevReading = zeroedReading;
        zeroedReading = myAdxl.z + offset;

        Serial.println(zeroedReading);

        if (abs(zeroedReading) > THRESHOLD_IMPACT && abs(prevReading) < THRESHOLD_IMPACT) {
          impacts++;
        }

        if (iterator < THRESHOLD_READINGS) {
          if (abs(zeroedReading) > THRESHOLD_INACTIVITY) {
            iterator = 0;
            break;
          } else {
            iterator++;
          }
        } else {
          // Change currentState
          Serial.println("Current State: RIGHT");
          currentState = SEND;
          digitalWrite(LED_BUILTIN, HIGH);
        }
      }



      break;
    /* SEND State */
    case SEND:
      // Serial.println("Current State: SENDING");
      impacts--; // Compensate for deceleration of recoater arm

      response = 400;
      if (impacts > 0) {
        response = webhook.trigger(String(layer), String(impacts), "TRUE");   // IMPACT DETECTED
      } else {
        response = webhook.trigger(String(layer), String(impacts), "FALSE");  // NO IMPACT DETECTED
      }

      Serial.println();
      if (response == 200) {
        Serial.println("Success.");
      } else {
        Serial.println("Failed.");
      }

      Serial.println("Current State: LEFT");
      currentState = LEFT;
      layer++;

      break;
    default:
      break;
  }

}

// Server "/" route
void handle_OnConnect() {
  started = false;
  calibrated = false;

  Serial.println("Root Connection");
  server.send(200, "text/html", SendHTML(calibrated, 0));
}

// Server "/calibrate" route
void handle_calibrate() {
  calibrated = true;

  sum = 0;
  i = 0;
  while (i < 50) {
    if (myAdxl.dataReady()) {
      myAdxl.readAccel();
      sum += myAdxl.z;
      i++;
    }
  }
  offset = -(sum / 50);

  Serial.println("Calibrating...");
  server.send(200, "text/html", SendHTML(calibrated, offset));
}

// Server "/start" route
void handle_start() {
  if (!calibrated) {
    server.send(404, "text/plain", "Calibration Required: Return to main page.");
    return;
  }

  started = true;

  Serial.println("Starting...");
  Serial.println("Current State: LEFT");
  server.send(200, "text/plain", "Starting...");
  server.stop();
}

// Server route not found
void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}


// Function to send HTML page
String SendHTML(uint8_t calibrated, int offset){
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr +="<title>OER_RECOAT</title>\n";
  ptr +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr +="body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
  ptr +=".button {display: block;width: 120px;background-color: #1abc9c;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
  ptr +=".button-on {background-color: #B9975B;}\n";
  ptr +=".button-on:active {background-color: #A17F45;}\n";
  ptr +=".button-off {background-color: #046A38;}\n";
  ptr +=".button-off:active {background-color: #034A27;}\n";
  ptr +="p {font-size: 16px;color: #888;margin-bottom: 10px;}\n";
  ptr +="</style>\n";
  ptr +="</head>\n";
  ptr +="<body style=\"background-color:gainsboro;\">\n";
  ptr +="<h1>OER_RECOAT Impact Detection</h1>\n";
  ptr +="<h3>Calibrate, then Start!</h3>\n";

  if (!calibrated) {
    ptr +="<p>Calibration Required!</p><a class=\"button button-off\" href=\"/calibrate\"> Calibrate Device </a>\n";
  } else {
    ptr +="<p>Calibrated with Offset: <b>";
    ptr += offset;
    ptr +="</b></p><a class=\"button button-on\" href=\"/start\"> Start </a>\n";
  }

  ptr +="</body>\n";
  ptr +="</html>\n";
  return ptr;
}

// Setup ADXL to INT on activity
void setupAdxl() {
  if (myAdxl.begin() == false) {
    Serial.println("ADXL313 did not respond. Freezing MCU");
    while(1);
  }

  // Soft Reset to Configure ADXL313
  myAdxl.softReset();
  myAdxl.setRange(RANGE);
}