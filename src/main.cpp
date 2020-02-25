#include <Arduino.h>
#include <M5Stack.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <time.h> 

int SampleRate = 1000; //Bruges til at bestemme sampleraten. 1000 mikrosekunder = 1000samples/sekund

// Temp variables for accl and gyro data
int16_t ax;
int16_t ay;
int16_t az;
int16_t gx;
int16_t gy;
int16_t gz;

struct Butt {
  const uint8_t PIN;
  bool state;
};

bool doSample = false; //benyttes når funktionen skal sample en værdi

Butt ButtonPressed(39,false); //Button A = 39, B = 38, C = 37 (Fra venstre mod højre)

uint8_t fileNum = 0;

hw_timer_t * timer = NULL;


void ButtonISR(){
  static int32_t lastIsr = 0;
  if ((millis() - lastIsr) > 500) {
    ButtonPressed.state = !ButtonPressed.state;
    if (ButtonPressed.state) {
      startTimer();
      Serial.println("Sampling");
      M5.Lcd.setCursor(0, 20);
      M5.Lcd.printf("Sampling");
    } else {
      fileNum++;
      stopTimer();
      M5.Lcd.setCursor(0, 20);
      M5.Lcd.printf("Not sampling");
      Serial.println("Not sampling");
    }
    lastIsr = millis();
  }
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

void TimerISR(){
  doSample = true; 
}

void setupSD() {
  if(!SD.begin(4)){
      Serial.println("Card Mount Failed");
      //for(;;) {}
  } else {
    Serial.println("SD mount succesfull")
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
}

void setupButton() {
  pinMode(ButtonPressed.PIN, INPUT);
  attachInterrupt(ButtonPressed.PIN, &ButtonISR, FALLING);
}

void setup() {
  setupM5();
  setupSerial();
  setupSD(); 
  setupButton();
}

void sampleAndSave() {
  M5.IMU.getAccelAdc(int16_t* ax, int16_t* ay, int16_t* az);
  M5.IMU.getGyroAdc(int16_t* gx, int16_t* gy, int16_t* gz);
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