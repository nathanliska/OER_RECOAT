/*
 * OER_RECOAT
 * Version 0.1
 */

/*
 * Libraries
 */
#include <Wire.h>
#include <SparkFunADXL313.h>
#include <rtc_memory.h>

#define RANGE ADXL313_RANGE_1_G             // 1g Range
#define ACTIVITY_INTERRUPT ADXL313_INT1_PIN // INT1 Pin
#define ACTIVITY_THRESHOLD 20               // 0-255
#define INACTIVITY_THRESHOLD 10             // 0-255

typedef struct {
  int layerCounter;
} myRtcData;

ADXL313 myAdxl;
RtcMemory rtcMemory("/etc/trail.bin");

void setupAdxl() {
  if (myAdxl.begin() == false) {
    Serial.println("ADXL313 did not respond. Freezing MCU");
    while(1);
  }

  // Soft Reset to Configure ADXL313
  myAdxl.softReset();
  myAdxl.setRange(RANGE);

  // Activity
  myAdxl.setActivityXYZ(false, false, true);
  myAdxl.setActivityThreshold(ACTIVITY_THRESHOLD); // 0-255 (62.5mg/LSB)

  // Inactivity
  myAdxl.setInactivityXYZ(false, false, false);
  myAdxl.setInactivityThreshold(INACTIVITY_THRESHOLD); // 0-255 (62.5mg/LSB)
  myAdxl.setTimeInactivity(5); // 0-255 (1sec/LSB)

  myAdxl.setInterruptMapping(ADXL313_INT_ACTIVITY_BIT, ACTIVITY_INTERRUPT);
  myAdxl.setInterruptLevelBit(1); // Active LOW Interrupt
  myAdxl.ActivityINT(true);
  myAdxl.InactivityINT(false);
}

myRtcData* getRtcData() {
  if (rtcMemory.begin()) {
    Serial.println("Initialization done!");
  } else {
    Serial.println("No previous data found. The memory is reset to zeros!");
    // Here you can initialize your data structure.
  }

  return rtcMemory.getData<myRtcData>();
}

/*
 * Main Setup
 */
void setup() { 
  Serial.begin(115200);
  // Serial.println("Turned On!");

  // ADXL313
  Wire.begin();
  setupAdxl();
  // myAdxl.measureModeOn();
  Serial.println("ADXL313 Setup Complete");


  // RTC Memory
  myRtcData* data = getRtcData();
  Serial.println(String("Value read: ") + data->layerCounter);
  data->layerCounter++;
  
  rtcMemory.persist();
  myAdxl.measureModeOn();
  delay(1000);
  ESP.deepSleep(0);
}

/*
 * Main Loop
 */
void loop() {
  if (myAdxl.dataReady()) // Check dataReady interrupt, clears all other int bits in INT_SOURCE reg
  {
    myAdxl.readAccel(); // Read all enabled axis
    
    Serial.print("x:");
    Serial.print(myAdxl.x);
    Serial.print("\ty:");
    Serial.print(myAdxl.y);
    Serial.print("\tz:");
    Serial.print(myAdxl.z);
    Serial.println();
  } else {
    Serial.println("Waiting for dataReady.");
  }

  myAdxl.updateIntSourceStatuses();
  if (myAdxl.intSource.activity == true) {
    Serial.println("ACTIVITY DETECTED");
  }
  
  delay(10);
}
