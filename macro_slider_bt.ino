#include <Wire.h>
#include <Adafruit_MotorShield.h>
#include "utility/Adafruit_MS_PWMServoDriver.h"
#include <SoftwareSerial.h>

const char nameBT[10] = "MACROBT";
const char bpsBT = '4'; // 9600 bps
const char pinBT[5] = "0000";

const int RAIL = 50;        // mm
const float M8STEP = 0.04;  // mm
const int MOTORSTEPS = 200; // units

int pinPHOTO = 7;
int pinLED = 11;
int pinRX = 3;
int pinTX = 4;

int currentPhotos;
int maxPhotos;
int currentSteps;
int maxSteps;
int delaySeconds;

bool isRunning;
bool isCmd;
bool isCmdCompleted;
bool isCmdWaiting;

String blueToothCmd;
String blueToothVal;

char blueToothChar;

SoftwareSerial btSerial(pinRX, pinTX);

Adafruit_MotorShield AFMS = Adafruit_MotorShield();
Adafruit_StepperMotor *myMotor = AFMS.getStepper(200, 2);

void setup() {
  pinMode(pinPHOTO, OUTPUT);
  pinMode(pinLED, OUTPUT);
  pinMode(pinRX, INPUT);
  pinMode(pinTX, OUTPUT);

  digitalWrite(pinLED, LOW);
  digitalWrite(pinPHOTO, LOW);

  Serial.begin(9600);
  btSerial.begin(9600);

  btSerial.print("AT");
  delay(1000);

  btSerial.print("AT+NAME");
  btSerial.print(nameBT);
  delay(1000);

  btSerial.print("AT+BAUD");
  btSerial.print(bpsBT);
  delay(1000);

  //btSerial.print("AT+PIN");
  //btSerial.print(pinBT);
  //delay(1000);

  isRunning = false;

  currentPhotos = 0;
  maxPhotos = 0;
  currentSteps = 0;
  maxSteps = 0;
  delaySeconds = 1;

  isCmd = true;
  isCmdCompleted = false;
  isCmdWaiting = false;

  blueToothCmd = "";
  blueToothVal = "";

  AFMS.begin();
  myMotor->setSpeed(10);  // RPM

  // Flush the BT buffer
  while (btSerial.available()) {
    btSerial.read();
  }

  // BT is ready
  Serial.println("Listening ...");
  digitalWrite(pinLED, HIGH);
}

void loop()
{
  // Continue the started session
  processSession();

  if (btSerial.available()) {
    blueToothChar = btSerial.read();

    isCmdWaiting = true;

    if (blueToothChar != ' ') {
      if (isCmd) {
        if (blueToothChar != '=' && blueToothChar != ';') {
          blueToothCmd += blueToothChar;
        }
        else if (blueToothChar == '=') {
          isCmd = false;
        }
        else {
          isCmdCompleted = true;
          isCmdWaiting = false;
        }
      }
      else {
        if (blueToothChar != ';') {
          blueToothVal += blueToothChar;
        }
        else {
          isCmdCompleted = true;
          isCmdWaiting = false;
        }
      }
    }

    if (isCmdCompleted) {
      // Execute command
      processCommand();

      // Update flags for new command
      isCmdCompleted = false;
      isCmd = true;

      // Initialize command/parameter variables
      blueToothCmd = "";
      blueToothVal = "";
    }
  }
}

void processCommand()
{
  blueToothCmd.toUpperCase();

  if (blueToothCmd.compareTo("STEPS") == 0) {
    maxSteps = blueToothVal.toInt();
  }
  else if (blueToothCmd.compareTo("PHOTOS") == 0) {
    maxPhotos = blueToothVal.toInt();
  }
  else if (blueToothCmd.compareTo("DELAY") == 0) {
    delaySeconds = blueToothVal.toInt();
  }
  else if (blueToothCmd.compareTo("START") == 0) {
    startSession();
  }
  else if (blueToothCmd.compareTo("FINISH") == 0) {
    finishSession();
  }
  else if (blueToothCmd.compareTo("FORWARD") == 0) {
    moveForward(blueToothVal.toInt());
  }
  else if (blueToothCmd.compareTo("BACKWARD") == 0) {
    moveBackward(blueToothVal.toInt());
  }
  else if (blueToothCmd.compareTo("STATUS") == 0) {
    showStatus();
  }
  else if (blueToothCmd.compareTo("HELP") == 0) {
    showHelp();
  }

}

String boolToString(bool value)
{
  if (value) {
    return "Yes";
  }
  else {
    return "False";
  }
}

void showStatus()
{
  btSerial.println(">> Running = " + boolToString(isRunning) + " <<");
  btSerial.println(">> Steps = " + String(maxSteps) + " <<");
  btSerial.println(">> Current Steps = " + String(currentSteps) + " <<");
  btSerial.println(">> Photos = " + String(maxPhotos) + " <<");
  btSerial.println(">> Current Photos = " + String(currentPhotos) + " <<");
  btSerial.println(">> Delay = " + String(delaySeconds) + " <<");
}

void showHelp()
{
  btSerial.println(">> STEPS=n; Steps between photos. <<");
  btSerial.println(">> PHOTOS=n; Number of photos of the session. <<");
  btSerial.println(">> DELAY=n; Seconds waiting before take the photo <<");
  btSerial.println(">> START; Start session <<");
  btSerial.println(">> FINISH; End session <<");
  btSerial.println(">> FORWARD=n; Move forward motor 'n' steps <<");
  btSerial.println(">> BACKWARD=n; Move backward motor 'n' steps <<");
  btSerial.println(">> STATUS; Show current session info <<");
  btSerial.println(">> HELP; Show this help <<");
}

void startSession()
{
  btSerial.println(">> Start Session <<");
  isRunning = true;
  currentPhotos = 0;
  currentSteps = 0;
}

void processSession()
{
  if (!isCmdWaiting) {
    if (maxPhotos > 0 && maxSteps > 0 && isRunning) {
      moveForward(1);
      takePhoto();
    }
    else {
      // If try to execute session and the setup is not valid, cancel the session.
      isRunning = false;
    }
  }
}

void finishSession()
{
  if (isRunning) {
    isRunning = false;
    moveBackward(currentSteps);

    maxPhotos = 0;
    maxSteps = 0;

    currentSteps = 0;
    currentPhotos = 0;

    btSerial.println(">> End Session <<");
  }
}

void moveForward(int steps)
{
  myMotor->step(steps, FORWARD, MICROSTEP);
  currentSteps += steps;
}

void moveBackward(int steps)
{
  myMotor->step(steps, BACKWARD, MICROSTEP);
  currentSteps -= steps;
}

void takePhoto()
{
  if (currentPhotos < maxPhotos && currentSteps % maxSteps == 0) {
    delay(delaySeconds * 1000);
    digitalWrite(pinPHOTO, HIGH);
    delay(1000);
    digitalWrite(pinPHOTO, LOW);

    currentPhotos++;

    btSerial.println(">> Take Photo " + String(currentPhotos));

    if (currentPhotos == maxPhotos) {
      finishSession();
    }
  }
}

