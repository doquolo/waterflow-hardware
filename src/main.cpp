#include <Arduino.h> // already arduino ide

// EEPROM
#include <EEPROM.h>
#include "eeprom-support.h"

#include <WiFi.h>
#include <Firebase_ESP_Client.h>

// import lcd
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Insert your network credentials
#define WIFI_SSID "test-waterflow"
#define WIFI_PASSWORD "12345678"

// Insert device UUID
#define UUID "32203c06-33b5-4385-bb91-eb14a13657b9"

// Insert Firebase project API Key
#define API_KEY "API_KEY_FROM_FIREBASE"

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "https://waterflow-583e8-default-rtdb.asia-southeast1.firebasedatabase.app/" 

//Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
int count = 0;
bool signupOK = false;
long int update_interval = 60000;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 25200;
const int   daylightOffset_sec = 0;

unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return(0);
  }
  time(&now);
  return now;
}

struct datetime_struct {
  String year = "";
  String month = "";
  String date = ""; 
};

datetime_struct getDate() {

  datetime_struct current;

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return current;
  }

  current.year = String(1900 + timeinfo.tm_year);
  current.month = String(timeinfo.tm_mon + 1);
  current.date = String(timeinfo.tm_mday);

  return current;
}

#define LED_BUILTIN 2
#define SENSOR  27

long currentMillis = 0;
long previousMillis = 0;
int interval = 1000;
boolean ledState = LOW;
float calibrationFactor = 4.5;
volatile byte pulseCount;
byte pulse1Sec = 0;
float flowRate;
unsigned int flowMilliLitres;
long totalMilliLitres;

long previousMilliLitres;

void IRAM_ATTR pulseCounter()
{
  pulseCount++;
}

void setup()
{
  EEPROM.begin(64);
  Serial.begin(115200);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // ntp 
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(SENSOR, INPUT_PULLUP);

  pulseCount = 0;
  flowRate = 0.0;
  flowMilliLitres = 0;
  totalMilliLitres = 0;
  previousMillis = 0;

  attachInterrupt(digitalPinToInterrupt(SENSOR), pulseCounter, FALLING);

  // load previous data
  totalMilliLitres = loadNumberFromEEPROM(ADDRESS_NUMBER2)*1000; // current data / Liter
  previousMilliLitres = loadNumberFromEEPROM(ADDRESS_NUMBER1)*1000; // past data / Liter

  lcd.init();
  lcd.clear();
  lcd.backlight();
}

void loop()
{
  while (Serial.available() > 0) {
    String inp = Serial.readString();
    inp.trim();
    if (inp == "reset") {
      previousMilliLitres = totalMilliLitres;
      totalMilliLitres = 0;
    }
  }

  currentMillis = millis();
  if (currentMillis - previousMillis > interval) {
    
    pulse1Sec = pulseCount;
    pulseCount = 0;

    // Because this loop may not complete in exactly 1 second intervals we calculate
    // the number of milliseconds that have passed since the last execution and use
    // that to scale the output. We also apply the calibrationFactor to scale the output
    // based on the number of pulses per second per units of measure (litres/minute in
    // this case) coming from the sensor.
    flowRate = ((1000.0 / (millis() - previousMillis)) * pulse1Sec) / calibrationFactor;
    previousMillis = millis();

    // Divide the flow rate in litres/minute by 60 to determine how many litres have
    // passed through the sensor in this 1 second interval, then multiply by 1000 to
    // convert to millilitres.
    flowMilliLitres = (flowRate / 60) * 1000;

    // Add the millilitres passed in this second to the cumulative total
    totalMilliLitres += flowMilliLitres;
    
    // Print the flow rate for this second in litres / minute
    Serial.print("Flow rate: ");
    Serial.print(int(flowRate));  // Print the integer part of the variable
    Serial.print("L/min");
    Serial.print("\t");       // Print tab space

    // Print the cumulative total of litres flowed since starting
    Serial.print("Output Liquid Quantity: ");
    Serial.print(totalMilliLitres);
    Serial.print("mL / ");
    Serial.print(totalMilliLitres / 1000);
    Serial.println("L");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Dong chay: ");
    lcd.print(int(flowRate));
    lcd.print("L/p");

    lcd.setCursor(0, 1);
    lcd.print(totalMilliLitres / 1000);
    lcd.print(" | ");
    lcd.print(previousMilliLitres / 1000);
  }

  if (currentMillis - sendDataPrevMillis > update_interval) {
    // save data to eeprom 
    saveNumberToEEPROM(int(previousMilliLitres/1000), ADDRESS_NUMBER1); // Save number1 at ADDRESS_NUMBER1
    saveNumberToEEPROM(int(totalMilliLitres/1000), ADDRESS_NUMBER2); // Save number2 at ADDRESS_NUMBER2

    Serial.println(loadNumberFromEEPROM(ADDRESS_NUMBER2));
    // firebase 
    sendDataPrevMillis = currentMillis;
    if (Firebase.ready() && signupOK) {
      datetime_struct current = getDate();
      String timestamp = String(getTime());
      String path = String(UUID) 
                    + String("/") 
                    + current.year 
                    + String("/") 
                    + current.month 
                    + String("/") 
                    + current.date
                    + String("/") 
                    + timestamp;

      String path_waterFlowed = path + String("/waterFlowed");
      String path_waterSpeed = path + String("/waterSpeed");

      // set water flowed
      if (Firebase.RTDB.setInt(&fbdo, path_waterFlowed.c_str(), int(totalMilliLitres) / 1000)){
        Serial.println("PASSED");
        Serial.println("PATH: " + fbdo.dataPath());
        Serial.println("TYPE: " + fbdo.dataType());
      } else {
        Serial.println("FAILED");
        Serial.println("REASON: " + fbdo.errorReason());
      }
      
      // set water speed
      if (Firebase.RTDB.setInt(&fbdo, path_waterSpeed.c_str(), int(flowRate))){
        Serial.println("PASSED");
        Serial.println("PATH: " + fbdo.dataPath());
        Serial.println("TYPE: " + fbdo.dataType());
      } else {
        Serial.println("FAILED");
        Serial.println("REASON: " + fbdo.errorReason());
      }
    }
  }
}
