#include <SoftwareSerial.h>
#include "TinyGPS++.h"
#include <time.h>

#define SAMPLING_DELAY 5000

SoftwareSerial SerialGPS(2, 3);  // RX, TX
TinyGPSPlus gps;

typedef struct {
  char driver[50];            //driver
  time_t startTime;           //startTime of measurement
  time_t endTime;             //endTime of measurement  
  int iPoints = 0;            //SpeedPoint Counter
  double speedPoints[360];    //speedpoints in kph --> max 2h recording
  double maxSpeed = 0;        //max speed
  double avgSpeed = 0;        //avg speed
} jsonStruct;

jsonStruct race;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  SerialGPS.begin(9600);  
  //race.driver = "Robin";
  strcpy(race.driver, "Robin");
}

void loop() {  
  measureSpeed(&race);
  Serial.print("\nDriver: ");
  Serial.print(race.driver);
  Serial.print("\nKM/H: ");
  Serial.print(race.speedPoints[race.iPoints-1]);
  Serial.print("\nMax KM/H: ");
  Serial.print(race.maxSpeed);
  Serial.print("\nAvg KM/H: ");
  Serial.print(race.avgSpeed);
  
  delay(SAMPLING_DELAY);
}

void measureSpeed(jsonStruct* speedo){
  if (SerialGPS.available() && speedo->iPoints < 359) {
    speedo->speedPoints[speedo->iPoints++] = gps.speed.kmph();
    //speedo->iPoints++;
  }
  //speedo->avgSpeed = calcAvg(speedo->speedPoints, speedo->iPoints);
  speedo->maxSpeed = findMax(speedo->speedPoints, speedo->iPoints);
}

double findMax(double inputArray[], int* size){
  // assert(inputArray && size);  
  double max = inputArray[0];
  for(int i = 1; i<size;i++){
      if(inputArray[i] > max){
        max = inputArray[i];
      }
  }
  return max;
}

double calcAvg(double inputArray[], int* size){  
  double sum = 0;
  for(int i = 0; i<size;i++){
    sum = sum + inputArray[i];
  }
  return sum / *size;
}