#include <SoftwareSerial.h>
#include "TinyGPS++.h"
#include <time.h>

// include the SD library:
#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>

//Global defines
#define CS 4

//global vars
SoftwareSerial SerialGPS(2, 3);  // RX, TX
TinyGPSPlus gps;

typedef struct {
  int samplingDelay;
  char drivers[5][15];
} Config;

typedef struct {
  String raceID;      //which race dataset?
  double speedPoint;  //speedpoints in kph --> max 15min recording
} SpeedPoint;

typedef struct {
  String raceID;        //which race dataset?
  String driver;        //driver
  time_t startTime;     //startTime of measurement
  time_t endTime;       //endTime of measurement
  int iPoints = 0;      //SpeedPoint Counter
  double maxSpeed = 0;  //max speed
  double avgSpeed = 0;  //avg speed
} RaceSet;

//global vars
Config conf;
RaceSet race;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  
  Serial.begin(9600);
  while (!Serial) continue;  // wait for serial port to connect. Needed for native USB port only

  //init GPS (serial connection)
  SerialGPS.begin(9600);
  while (!SerialGPS) continue;

  //init SD card
  boolean sdCheck = SD.begin(CS);  
  if(!sdCheck)  {
    Serial.println("SD Card open failed");
  }
  initConf(&conf);

  race.driver = "Robin";
  race.raceID = random();  // @TODO find a unique value that's different foreach measure
  writeRaceSetJsonFile(&race, "/race" + race.raceID, true);
}

void loop() {
  // SpeedPoint sp;
  // measureSpeed(&sp);
  // // //@TODO Write to SD card
  // // Serial.print("\nRaceID: ");
  // // Serial.print(race.raceID);
  // // Serial.print("\nDriver: ");
  // // Serial.print(race.driver);
  // // Serial.print("\nKM/H: ");
  // // Serial.print(sp.speedPoint);
  // // // Serial.print("\nMax KM/H: ");
  // // // Serial.print(race.maxSpeed);
  // // // Serial.print("\nAvg KM/H: ");
  // // // Serial.print(race.avgSpeed);
  // // Serial.print("\nPoint Count: ");
  // // Serial.print(race.iPoints);

  // delay(conf.samplingDelay);
  // digitalWrite(LED_BUILTIN, LOW);
}

void writeRaceSetJsonFile(RaceSet *race, String path, boolean overwrite){
  StaticJsonDocument<256> raceJson;
  //TODO Map

  writeJsonFile(path, &raceJson, overwrite);
}

//write to file overwrite: true -> overwrite, false -> append
void writeJsonFile(String path, JsonDocument* fileJson, boolean overwrite) {
  // StaticJsonDocument<size> fileJson;
  File file;
  switch (overwrite) {
    case true:
      file = SD.open(path, FILE_WRITE);
      break;
    case false:
      file = SD.open(path, O_APPEND);
      break;
  }


  if (!file) {
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println(F("\nErr read file"));
  }

  DeserializationError error = serializeJson(*fileJson, file);  //"{'drivers': ['Robin', 'Florian', 'Nils', 'Tim'], 'samplingHz': 1}");

  if (error) {
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println(F("Err file to json"));
  }

  file.close();
}

// read & deserialize json file
void readJsonFile(char* path, JsonDocument* fileJson) {
  Serial.print("Read file: ");
  Serial.print(path);
  Serial.print(" - existing: ");
  Serial.println(SD.exists(path));


  File file = SD.open(path, FILE_READ);

  if (!file) {
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("Err read file");
  }

  DeserializationError error = deserializeJson(*fileJson, file);  //"{'drivers': ['Robin', 'Florian', 'Nils', 'Tim'], 'samplingHz': 1}");

  if (error) {
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("Err file to json");
  }
  file.close();
}

//read config
void initConf(Config* conf) {
  //init config file
  StaticJsonDocument<256> confJson;
  char fName[] = "/config.rob";
  readJsonFile(fName, &confJson);


  conf->samplingDelay = confJson["samplingHz"];
  conf->samplingDelay = (1000 / conf->samplingDelay) | 2000;
  Serial.print("Delay: ");
  Serial.println(conf->samplingDelay);

  JsonArray arr = (confJson["drivers"]).as<JsonArray>();

  for (int i = 0; i < 5; i++) {
    int siz = sizeof(confJson["drivers"][i]) + 1;  //+1 because of termination (i guess)
    if (siz > 0) strlcpy(conf->drivers[i], confJson["drivers"][i], siz);

    //   Serial.print("\ndrivers: ");
    //   Serial.print(conf->drivers[i]);

    //   Serial.print(" - ");
    //   Serial.print(siz);
    // }
  }
}

void measureSpeed(SpeedPoint* speedo) {
  // Serial.println("Start GPS measure");
  if (SerialGPS.available()) {
    speedo->speedPoint = gps.speed.kmph();
    race.iPoints++;
    //race->maxSpeed = findMax(speedo->speedPoints, speedo->iPoints);
    //speedo->avgSpeed = calcAvg(speedo->speedPoints, speedo->iPoints);
  } else {
    // Serial.println("No GPS available");
    digitalWrite(LED_BUILTIN, HIGH);
  }
}

double findMax(double inputArray[], int* size) {
  // assert(inputArray && size);
  double max = inputArray[0];
  for (int i = 1; i < size; i++) {
    if (inputArray[i] > max) {
      max = inputArray[i];
    }
  }
  return max;
}

double calcAvg(double inputArray[], int* size) {
  double sum = 0;
  for (int i = 0; i < size; i++) {
    sum = sum + inputArray[i];
  }
  return sum / *size;
}