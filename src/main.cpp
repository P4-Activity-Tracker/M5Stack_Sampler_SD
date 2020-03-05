#include <Arduino.h>
#include <M5Stack.h>
#include "FS.h"
#include "SD.h"
#include "utility/MPU9250.h"
#include "SPI.h"

const uint8_t maxLength = 100; // Max karakterlængde for display

MPU9250 IMU; //Skaber en instans af typen MPU9250 der kaldes IMU. MPU9250 typen indeholder funktioner relateret til vores accelerometer/gyroskop

hw_timer_t * timer = NULL; // Klargøring af timer

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

volatile bool runTimer = false; // Timer sampler state 

File dataFile; // SD kort data fil instans
uint8_t fileNumber = 0; // Navn på fil der skal skrives til næste gang

volatile bool stopSampling = false;
volatile bool startSampling = false;
volatile bool doSample = false;

const uint8_t buttPin = 39; //Pinnumber til knap A på M5Stack

int SampleRate = 10000; //Bruges til at bestemme sampleraten. 1000 mikrosekunder = 1000 samples/sekund

uint32_t lastISR = 0;



void IRAM_ATTR TimerISR () { // TimerISR
	portENTER_CRITICAL_ISR(&mux); // Sikre at kun en kan tilgå variablen ad gangen
	doSample = true;
	portEXIT_CRITICAL_ISR(&mux);
}

void IRAM_ATTR buttonISR() { // ButtonISR
    if ((millis() - lastISR) > 1000) {
        portENTER_CRITICAL_ISR(&mux); // Sikre at kun en kan tilgå variablen ad gangen
        runTimer = !runTimer; // Invertere variablen runTimer
        if (runTimer == true) { // Vi spørger om runTimer er true
			startSampling = true;
        } 
        else {
			stopSampling = true;
        }
		portEXIT_CRITICAL_ISR(&mux);
        lastISR = millis();
    }
}

void startTimer() {
    timer = timerBegin(0, 80, true); // Start timer (params: timerNum, divider, countUp)
    timerAttachInterrupt(timer, &TimerISR, true); // Attatch interrupt to timer (params: timer, function, edge triggered)
    timerAlarmWrite(timer, SampleRate, true); // Set timer to trigger interrupt (params: timer, trigger time, repeat)
    timerAlarmEnable(timer); // Enable the alarm
}

void stopTimer() {
    timerEnd(timer); // Stop and free timer
    timer = NULL;
}

void writeToLCD(String text, uint8_t line) {
    M5.Lcd.setCursor(0, 20 + (21 * line));
    uint8_t length = text.length();
    for (uint8_t i = 0; i < (maxLength - length); i++) {
        text = text + " ";
    }
    M5.Lcd.print(text); 
}

void setupLCD(){ //Sætter skærmfarve, textfarve, textstørrelse og skriver "starting" til LCD
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(WHITE ,BLACK);
    M5.Lcd.setTextSize(2);
}

void disableSpeaker(){
	digitalWrite(25, 0);
}

void setupIMU(){
	const uint8_t gyroScale = 3; //From 0 to 3, where 0 is 2G, 1 is 4G, 2 is 8G and 3 is 16G
	const uint8_t accScale = 3; //From 0 to 3, where 0 is 250DPS,1 is 500DPS, 2 is 1000DPS and 3 is 2000DPS
   	IMU.initMPU9250(); //Initialiserer vores MPU9250 chip
	uint8_t c = IMU.readByte(MPU9250_ADDRESS, GYRO_CONFIG); // get current GYRO_CONFIG register value
	// c = c & ~0xE0; // Clear self-test bits [7:5]
	c = c & ~0x02; // Clear Fchoice bits [1:0]
	c = c & ~0x18; // Clear AFS bits [4:3]
	c = c | gyroScale << 3; // Set full scale range for the gyro
	// c =| 0x00; // Set Fchoice for the gyro to 11 by writing its inverse to bits 1:0 of GYRO_CONFIG
	IMU.writeByte(MPU9250_ADDRESS, GYRO_CONFIG, c); // Write new GYRO_CONFIG value to register 
   	// Set accelerometer full-scale range configuration
	c = IMU.readByte(MPU9250_ADDRESS, ACCEL_CONFIG); // get current ACCEL_CONFIG register value
	// c = c & ~0xE0; // Clear self-test bits [7:5]
	c = c & ~0x18;  // Clear AFS bits [4:3]
	c = c | accScale << 3; // Set full scale range for the accelerometer
	IMU.writeByte(MPU9250_ADDRESS, ACCEL_CONFIG, c); // Write new ACCEL_CONFIG register value
}

void setupSD() { 
    if(!SD.begin(4)){ // begin(4) sætter chip select pin til pin 4 i stedet for pin 5 som er default. Begin retunere true, hvis SD kortet blev startet.
        Serial.println("Card Mount Failed");
        writeToLCD("Not ready",1);
        writeToLCD("No SD card found", 2);
        for(;;);
    } else {
        Serial.println("SD mount succesfull");
    }
}

uint8_t getNumberOfFiles() {
    uint8_t readFiles = 0;
    File root = SD.open("/"); // Open hovedemappen
    while(true) {
        File entry = root.openNextFile(); // Læse den næste fil i lokation
        if (!entry) { // Hvis der ikke er en næste fil, stop loopet
            break;
        } else {
            Serial.println(entry.name());
            readFiles++; // Hæv værdien af readFiles, hvis der var en fil
            entry.close(); // Luk filen
        }
    }
    root.close();  
    return readFiles; 
}

void setupButtonInterrupt(){ //Sætter vores buttonISR rutine til knap A (buttpin = pin 39). Hver gang A trykkes kører buttonISR rutinen.
    pinMode(buttPin,INPUT_PULLUP); //Input pullup sørger for at denne pin er high, ved at forhindre floating tilstand.
    attachInterrupt(digitalPinToInterrupt(buttPin), buttonISR, FALLING); //Falling betyder at buttonISR kører når buttPin går fra Høj til Lav.
}

void processStartSampling() {
	portENTER_CRITICAL(&mux);
	startSampling = false;
	portEXIT_CRITICAL(&mux);
	Serial.print("Opening file...");
	String filename = "/" + String(fileNumber) + ".txt";
	dataFile = SD.open(filename, FILE_WRITE);
	if (!dataFile) {
		Serial.println("Failure");
        writeToLCD("Failed to open file", 3);
		portENTER_CRITICAL(&mux);
		runTimer = false;
		portEXIT_CRITICAL(&mux);
	} else {
		Serial.println("Success");
		writeToLCD("Ready", 1);
		writeToLCD("Sampling", 2); //Skriver "Sampling" til LCD, på linje 0
		writeToLCD("Writing to: " + filename, 3);
		startTimer(); // starter timerISR 
	}
}

void processStopSampling() {
	portENTER_CRITICAL(&mux);
	stopSampling = false;
	portEXIT_CRITICAL(&mux);
	stopTimer(); // Kører stopTimer
	Serial.print("Closing file...");
	dataFile.close();
	if (!dataFile) {
		Serial.println("Success");
	} else {
		Serial.println("Failure");
	}
	writeToLCD("", 3);
	fileNumber++; // +1 på fileNumber
	writeToLCD("Ready",1); // Skriver "Ready" til LCD, på linje 0
	writeToLCD("Not sampling",2); // Skriver "Not Sampling" til LCD, på linje 1
}

void processDoSample() {
	portENTER_CRITICAL(&mux);
	doSample = false;
	portEXIT_CRITICAL(&mux);
	//float messurements[6];
	float messurements[6] = {0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F};
	//Sample accelerometer - Save to local variable
	IMU.readAccelData(IMU.accelCount); //Læser x,y,z ADC værdierne
	IMU.getAres(); // Get accelerometer skale saved to "Ares"
	messurements[0] = (float)IMU.accelCount[0]*IMU.aRes; //accelbias [0]
	messurements[1] = (float)IMU.accelCount[1]*IMU.aRes; //accelbias [1]
	messurements[2] = (float)IMU.accelCount[2]*IMU.aRes; //accelbias [2]
	// Sample gyroscope - Save to local variable
	IMU.readGyroData(IMU.gyroCount);
	IMU.getGres(); // Get gyroskope skale saved to "Gres"
	messurements[3] = (float)IMU.gyroCount[0]*IMU.gRes; //gyrobias [0]
	messurements[4] = (float)IMU.gyroCount[1]*IMU.gRes; //gyrobias [1]
	messurements[5] = (float)IMU.gyroCount[2]*IMU.gRes; //gyrobias [2]
	// Write to datafile
	String dataString = "";
	for (uint8_t i = 0; i < 6; i++) {
		String number = String(messurements[i], 8);
		if (i==5) {
			dataString = dataString + number + "\n"; 
		} else {
			dataString = dataString + number + ","; 
		}
	}
	if (!dataFile) {

	} else {
		dataFile.print(dataString);
	}
}

void setup() {
    M5.begin(); // Initierer M5stack   
	M5.Power.begin();
	setupLCD(); // Start LCD skærm (farve, tekststørrelse, tekstfarve)
    writeToLCD("Starting...", 1);
    disableSpeaker(); // Sluk for M5Stack speaker
    setupIMU(); // Start IMU chip
    setupSD(); // Mont SD kortet
    fileNumber = getNumberOfFiles(); //Viser hvor mange filer der er på SD kortet
    setupButtonInterrupt();
    writeToLCD("Ready", 1);
    writeToLCD("Not sampling", 2);
    Serial.println("Ready");
}

void loop() {
	if (startSampling) {
		processStartSampling();
	}
	if (stopSampling) {
		processStopSampling();
	}
	if (doSample) {
		processDoSample();
	}
}