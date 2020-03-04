#include <Arduino.h>
#include <M5Stack.h>
#include "FS.h"
#include "SD.h"
#include "utility/MPU9250.h"
#include "SPI.h"


// #include "Speaker.h" //Indeholder dacWrite() funktion vi benytter til disabling af speaker sound


const uint8_t maxLength = 255;

MPU9250 IMU; //Skaber en instans af typen MPU9250 der kaldes IMU.             
            //MPU9250 typen indeholder funktioner relateret til vores accelerometer/gyroskop

hw_timer_t * timer = NULL; // Klargøring af timer

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

volatile bool runTimer = false; // Timer sampler state 

File root;
File dataFile; // SD kort data fil instans
uint8_t fileNumber = 0; // Navn på fil der skal skrives til næste gang

const uint8_t buttPin = 39; //Pinnumber til knap A på M5Stack

int SampleRate = 10000; //Bruges til at bestemme sampleraten. 1000 mikrosekunder = 1000 samples/sekund

uint32_t lastISR = 0;

void startTimer();
void stopTimer();
void writeToLCD(String text, uint8_t line);

void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
    file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("Failed to open file for appending");
        return;
    }
    if(file.print(message)){
        Serial.println("Message appended");
    } else {
        Serial.println("Append failed");
    }
    file.close();
}

void IRAM_ATTR TimerISR () { // TimerISR
    Serial.println("Timer");
    int16_t messurements[6];
    //Sample accelerometer - Save to local variable
    IMU.readAccelData(IMU.accelCount); //Læser x,y,z ADC værdierne
    IMU.getAres(); // Get accelerometer skale saved to "Ares"
    messurements[0] = IMU.accelCount[0]*IMU.aRes; //accelbias [0]
    messurements[1]= IMU.accelCount[1]*IMU.aRes; //accelbias [1]
    messurements[2]= IMU.accelCount[2]*IMU.aRes; //accelbias [2]
    // Sample gyroscope - Save to local variable
    IMU.readGyroData(IMU.gyroCount);
    IMU.getGres(); // Get gyroskope skale saved to "Gres"

    messurements[3] = IMU.gyroCount[0]*IMU.gRes; //gyrobias [0]
    messurements[4] = IMU.gyroCount[1]*IMU.gRes; //gyrobias [1]
    messurements[5] = IMU.gyroCount[2]*IMU.gRes; //gyrobias [2]
    // Write to datafile
    String dataString = "";
    for (uint8_t i = 0; i < 6; i++) {
        String number = String(messurements[i]);
        if (i==5) {
            dataString = dataString + number + "\n"; 
        } else {
            dataString = dataString + number + ","; 
        }
    }
    if (dataFile) {
        appendFile(SD, "/yeet.txt", dataString.c_str());
        //dataFile.println(dataString);
    } else {
        Serial.println("error opening datalog.txt");
    }
}

void IRAM_ATTR buttonISR() { // ButtonISR
    if ((millis() - lastISR) > 1000) {
        Serial.println("Button ISR!");
        portENTER_CRITICAL_ISR(&mux); // Sikre at kun en kan tilgå variablen ad gangen
        runTimer = !runTimer; // Invertere variablen runTimer
        portEXIT_CRITICAL_ISR(&mux);
        if (runTimer == true) { // Vi spørger om runTimer er true
            //String filename = "/" + String(fileNumber) + ".txt";
            //const char * filename = "/youg.txt";
            //dataFile = SD.open(filename, FILE_WRITE); // Her åbner vi SD-kortet, og skriver dataFile over derpå, filen hedder datalog filnummer .txt

            writeFile(SD, "/yeet.txt", " ");
            
            writeToLCD("Sampling",2); //Skriver "Sampling" til LCD, på linje 0
            writeToLCD("Ready", 1);
            startTimer(); // starter timerISR 
        } 
        else {
            stopTimer(); // Kører stopTimer
            if (!dataFile) {
                dataFile.close(); // Lukker dataFile
            }
            fileNumber++; // +1 på fileNumber
            writeToLCD("Ready",1); // Skriver "Ready" til LCD, på linje 0
            writeToLCD("Not sampling",2); // Skriver "Not Sampling" til LCD, på linje 1
        }
        Serial.println("Done button ISR");
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
    // Stop and free timer
    timerEnd(timer);
    timer = NULL;
}

void writeToLCD(String text, uint8_t line) {
    M5.Lcd.setCursor(line, 20);
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
    dacWrite(25,0); //Skulle fjerne irrirterende speaker lyd
}

void setupIMU(){
   IMU.initMPU9250(); //Initialiserer vores MPU9250 chip 
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

void setup() {
    M5.begin(); // Initierer M5stack   
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
    
}