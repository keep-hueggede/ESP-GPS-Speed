#include <SoftwareSerial.h>
#include "TinyGPS++.h"
#include <time.h>
// #include <Wire.h>  // Wire Bibliothek einbinden
#include <LiquidCrystal_I2C.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <Bounce2.h>

//Global defines
#define CS 4
#define DSELECT 2
#define RACESTART 3

//global vars
SoftwareSerial SerialGPS(0, 1);  // RX, TX
TinyGPSPlus gps;
LiquidCrystal_I2C lcd(0x27, 16, 2);

typedef struct {
  int samplingDelay;
  int aggregationCount;
  char drivers[3][3];
} Config;

typedef struct {
  String raceID;      //which race dataset?
  double speedPoint;  //speedpoints in kph --> max 15min recording
  int iPoint;
} SpeedPoint;

typedef struct {
  String raceID;     //which race dataset?
  String driver;     //driver
  time_t startTime;  //startTime of measurement
  time_t endTime;    //endTime of measurement
  int iPoints = 0;   //SpeedPoint Counter
  // double maxSpeed = 0;  //max speed // to expensive to read all data from filestore
  // double avgSpeed = 0;  //avg speed
} RaceSet;

//global vars
// volatile int sStat2 = 0;  //Interrupt status Pin2
// volatile int sStat3 = 0;  //Interrupt status Pin3
Config conf;
RaceSet race;
int currentDriver;
boolean raceRunning = false;

Bounce DSelect = Bounce();
Bounce RaceStart = Bounce();

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(DSELECT, INPUT_PULLUP);
  pinMode(RACESTART, INPUT_PULLUP);

  DSelect.attach(DSELECT);
  DSelect.interval(5);
  RaceStart.attach(RACESTART);
  RaceStart.interval(5);

  Serial.begin(115200);
  // while (!Serial) continue;  // wait for serial port to connect. Needed for native USB port only
  Serial.println("---");

  //init GPS (serial connection)
  SerialGPS.begin(9600);

  initConf();
  currentDriver = 0;

  lcd.init();
  lcd.backlight();

  //print initial data to lcd
  printLCD(0.0);
}

void loop() {
  // sStat2 = digitalRead(DSELECT);
  // sStat3 = digitalRead()

  if (raceRunning) {
    SpeedPoint sp;
    measureSpeed(&sp);
    printLCD(sp.speedPoint);

    // //@TODO Write to SD card
    // Serial.print("\nRaceID: ");
    // Serial.print(race.raceID);
    // Serial.print("\nDriver: ");
    // Serial.print(race.driver);
    // Serial.print("\nKM/H: ");
    // Serial.print(sp.speedPoint);
    // // Serial.print("\nMax KM/H: ");
    // // Serial.print(race.maxSpeed);
    // // Serial.print("\nAvg KM/H: ");
    // // Serial.print(race.avgSpeed);
    // Serial.print("\nPoint Count: ");
    // Serial.println(race.iPoints);

    // // delay(conf.samplingDelay);
    // digitalWrite(LED_BUILTIN, LOW);
  }

  RaceStart.update();
  if (RaceStart.fallingEdge()) startEndRace();
  DSelect.update();
  if (DSelect.fallingEdge()) switchDriver();
}

void writeRaceSetJsonFile(RaceSet* race, boolean overwrite) {
  char path[13];
  strcpy(path, race->raceID.c_str());
  strcat(path, ".json");
  StaticJsonDocument<128> raceJson;

  Serial.print("path: ");
  Serial.println(path);

  //TODO Map
  raceJson["raceID"] = race->raceID;
  raceJson["driver"] = race->driver;
  raceJson["startTime"] = race->startTime;
  raceJson["endTime"] = race->endTime;
  raceJson["iPoints"] = race->iPoints;
  // raceJson["maxSpeed"] = race->maxSpeed;
  // raceJson["avgSpeed"] = race->avgSpeed;

  writeJsonFile(path, &raceJson, overwrite);
}
void writeSpeedpointToRaceFile(SpeedPoint* speedo) {
  char path[13];
  strcpy(path, race.raceID.c_str());
  strcat(path, ".json");

  StaticJsonDocument<64> speedoJson;
  speedoJson["raceID"] = speedo->raceID;
  speedoJson["speedPoint"] = speedo->speedPoint;
  speedoJson["iPoint"] = speedo->iPoint;

  writeJsonFile(path, &speedoJson, true);
}

//write to file overwrite: true -> overwrite, false -> append
void writeJsonFile(char* path, JsonDocument* fileJson, boolean overwrite) {
  // StaticJsonDocument<size> fileJson;
  //init SD card
  boolean sdCheck = SD.begin(CS);
  if (!sdCheck) {
    Serial.println("SD Card open failed");
  }

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
    Serial.println("Err read file");
  }

  DeserializationError error = serializeJson(*fileJson, file);  //"{'drivers': ['Robin', 'Florian', 'Nils', 'Tim'], 'samplingHz': 1}");

  if (error) {
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("Err file to json");
  }

  file.close();
  SD.end();
}

// read & deserialize json file
// void readJsonFile(char* path, JsonDocument* fileJson) {
//   // Serial.print("Read file: ");
//   // Serial.print(path);
//   // Serial.print(" - existing: ");
//   // Serial.println(SD.exists(path));

//  //init SD card
//   boolean sdCheck = SD.begin(CS);
//   if (!sdCheck) {
//     Serial.println("SD Card open failed");
//   }

//   File file = SD.open(path, FILE_READ);

//   if (!file) {
//     digitalWrite(LED_BUILTIN, HIGH);
//     Serial.println("Err read file");
//   }

//   DeserializationError error = deserializeJson(*fileJson, file);  //"{'drivers': ['Robin', 'Florian', 'Nils', 'Tim'], 'samplingHz': 1}");

//   if (error) {
//     digitalWrite(LED_BUILTIN, HIGH);
//     Serial.println("Err file to json");
//   }
//   file.close();
//   SD.end();
// }

//read config
void initConf() {
  conf.samplingDelay = 500;
  conf.aggregationCount = 10;
  for (int i = 0; i < 3; i++) {
    switch (i) {
      case 0:
        strcpy(conf.drivers[i], "RW");
        break;
      case 1:
        strcpy(conf.drivers[i], "FH");
        break;
      case 2:
        strcpy(conf.drivers[i], "NK");
        break;
    }
  }
}


void measureSpeed(SpeedPoint* speedo) {
  // Serial.println("Start GPS measure");
  if (SerialGPS.available()) {
    double sum = 0.0;
    for (int i = 0; i < conf.aggregationCount; i++) {
      sum += gps.speed.kmph();
      delay(conf.samplingDelay);
    }

    speedo->speedPoint = sum / conf.aggregationCount;
    speedo->iPoint = race.iPoints++;
    //race->maxSpeed = findMax(speedo->speedPoints, speedo->iPoints);
    //speedo->avgSpeed = calcAvg(speedo->speedPoints, speedo->iPoints);
  } else {
    // Serial.println("No GPS available");
    digitalWrite(LED_BUILTIN, HIGH);
  }
}

// double findMax(double inputArray[], int* size) {
//   // assert(inputArray && size);
//   double max = inputArray[0];
//   for (int i = 1; i < size; i++) {
//     if (inputArray[i] > max) {
//       max = inputArray[i];
//     }
//   }
//   return max;
// }

// double calcAvg(double inputArray[], int* size) {
//   double sum = 0;
//   for (int i = 0; i < size; i++) {
//     sum = sum + inputArray[i];
//   }
//   return sum / *size;
// }

void buildGPSDateTime(char* string) {
  TinyGPSDate d = gps.date;
  TinyGPSTime t = gps.time;

  sprintf(string, "%04d-%02d-%02dT%02d:%02d:%02d.000Z", d.year(), d.month(), d.day(), t.hour(), t.minute(), t.second());
}

void printLCD(double speed) {
  Serial.print("Print LCD: ");
  Serial.print(conf.drivers[currentDriver]);
  Serial.print(" - race: ");
  Serial.println(raceRunning);


  lcd.clear();
  // delay(5000);
  lcd.setCursor(0, 0);
  lcd.print("Wheel:");
  lcd.print(conf.drivers[currentDriver]);

  lcd.setCursor(0, 1);
  if (raceRunning) {
    lcd.print("Speed: ");
    lcd.print(speed);
    lcd.print("km/h");
  } else {
    lcd.print("Press Start");
  }
}

/**********************
** Interrupt methods **
**********************/
void startEndRace() {
  if (!raceRunning) {
    //TODO: new Raceset --> driver, starttime
    //TODO: trigger via interrupt

    race.driver = conf.drivers[currentDriver];
    race.raceID = random(0, 1000000);
    buildGPSDateTime(race.startTime);
    raceRunning = true;
    // Serial.print("Start race");
  } else {
    //TODO: finish Raceset --> endtime, calc's, write to file
    //TODO: trigger via interrupt
    raceRunning = false;
    buildGPSDateTime(race.startTime);
    writeRaceSetJsonFile(&race, true);
    // Serial.print("End race");

    SerialGPS.end();
  }
  printLCD(0.0);
}

void switchDriver() {
  // Serial.print("Switch driver ");
  if (raceRunning) return;

  if (currentDriver == 2) {
    currentDriver = 0;
  } else {
    currentDriver++;
  }
  // Serial.println(conf.drivers[currentDriver]);
  printLCD(0.0);
}