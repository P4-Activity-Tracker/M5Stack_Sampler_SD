#define M5STACK_MPU6886

#include <Arduino.h>
#include <M5Stack.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <time.h> 

int SampleRate = 1000; //Bruges til at bestemme sampleraten. 1000 mikrosekunder = 1000samples/sekund

static uint64_t lastIsr = 0;// debaunce til ButtonISR funktion.

// Temp variables for accl and gyro data
int16_t ax;
int16_t ay;
int16_t az;
int16_t gx;
int16_t gy;
int16_t gz;


const uint8_t buttPin = 39;
bool runTimer = false; // Denne funktion er til for at sample, herved sættes den til "false" for ikke at starte med at sample før den bliver "true"

bool doSample = false; //Benyttes når funktionen skal sample en værdi

uint8_t fileNum = 0; // Denne sørge for at når der gemmes, gemmes den nye data i ny fil. Tæller den fil vi skriver til.  

hw_timer_t * timer = NULL; // Klargøring af timer


void IRAM_ATTR TimerISR(){ //IRAM_ATTR er for at gemme funktionen i hurtig memory, så funktionen hurtigere fungere. 
  doSample = true; 
}


void startTimer() {
  timer = timerBegin(0, 80, true); // Start timer (params: timerNum, divider, countUp)
  timerAttachInterrupt(timer, &TimerISR, true); // Attatch interrupt to timer (params: timer, function, edge triggered)
  timerAlarmWrite(timer, SampleRate, true); // Set timer to trigger interrupt (params: timer, trigger time, repeat)
  timerAlarmEnable(timer); // Enable the alarm
}

void stopTimer() {
  // Stop and free timer
  timerEnd(timer);
  timer = NULL;
}

void IRAM_ATTR ButtonISR(){
  if ((millis() - lastIsr) > 500) {
    runTimer = !runTimer;
    if (runTimer) {
      startTimer();
      Serial.println("Sampling");
      M5.Lcd.setCursor(0, 20);
      M5.Lcd.print("Sampling              ");
    } else {
      Serial.flush();
      fileNum++;
      stopTimer();
      M5.Lcd.setCursor(0, 20);
      M5.Lcd.print("Not sampling             ");
      Serial.println("Not sampling");
    }
    lastIsr = millis();
  }
}


void setupSD() {
  if(!SD.begin(4)){
      Serial.println("Card Mount Failed");
      //for(;;) {}
  } else {
    Serial.println("SD mount succesfull");
  }
}

void setupSerial() {
  Serial.begin(115200);
  Serial.println("Starting...");
}

void setupM5() {
  // Initialize the M5Stack object
  M5.begin(); // Initialize M5
  M5.Power.begin(); 
  M5.IMU.Init(); // Start IMU
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(GREEN , BLACK);
  M5.Lcd.setTextSize(2);
  Serial.println("Not sampling");
}

void setupButton() {
  pinMode(buttPin, INPUT);
  attachInterrupt(buttPin, &ButtonISR, FALLING);
}

void setup() {
  setupM5();
  setupSerial();
  setupSD(); 
  setupButton();
}

void sampleAndSave() {
  doSample = false;
  M5.IMU.getAccelAdc(&ax, &ay, &az);
  M5.IMU.getGyroAdc(&gx, &gy, &gz);
  String dataString = String(ax);
  dataString = dataString + "," + String(ay);
  dataString = dataString + "," + String(az);
  dataString = dataString + "," + String(gx);
  dataString = dataString + "," + String(gy);
  dataString = dataString + "," + String(gz);
  File dataFile = SD.open("datalog" + String(fileNum) + ".txt", FILE_WRITE);
  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close();
    Serial.println(dataString);
  }
  else {
    Serial.println("error opening datalog.txt");
  }
}

void loop() {
  if (doSample == true){
    sampleAndSave();
  }
}