#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <MPU6050.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Pin Definitions
#define HRV_PIN           34
#define GSR_PIN           35
#define TEMP_SENSOR_PIN   32
#define LDR_PIN           33
#define ONE_WIRE_BUS      32

#define LED_RED_ALERT     12
#define LED_WHITE_LIGHT   27

#define BUZZER_PIN        25
#define SERVO_FAN_PIN     26
#define BUTTON_PIN        4

// Thresholds
#define HR_HIGH           100
#define HR_LOW            60
#define GSR_HIGH          2500
#define TEMP_FEVER        38.0
#define LDR_DARK          1000
#define INACTIVITY_TIME   30000

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo servoFan;
MPU6050 mpu;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

unsigned long lastActivityTime = 0;
bool lastButtonState = HIGH;

void setup() {
  Serial.begin(115200);
  
  Wire.begin(21, 22);
  delay(100);
  
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Guardian System");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  
  sensors.begin();
  mpu.initialize();
  
  if (!mpu.testConnection()) {
    Serial.println("MPU6050 connection failed!");
  }
  
  pinMode(LED_RED_ALERT, OUTPUT);
  pinMode(LED_WHITE_LIGHT, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  servoFan.attach(SERVO_FAN_PIN);
  servoFan.write(0);
  
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Ready!");
  lcd.setCursor(0, 1);
  lcd.print("Press button");
  
  Serial.println("\n========================================");
  Serial.println("GUARDIAN: Smart Health Monitor");
  Serial.println("========================================\n");
  
  lastActivityTime = millis();
}

void turnOffAllLEDs() {
  digitalWrite(LED_RED_ALERT, LOW);
  digitalWrite(LED_WHITE_LIGHT, LOW);
}

void turnOffBuzzer() {
  digitalWrite(BUZZER_PIN, LOW);
}

void stopFan() {
  servoFan.write(0);
}

void checkHighPerspiration(int gsrValue) {
  if (gsrValue > GSR_HIGH) {
    Serial.println("\n[ALERT] High Perspiration Detected!");
    Serial.print("GSR Value: "); Serial.println(gsrValue);
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("High Perspir.");
    lcd.setCursor(0, 1);
    lcd.print("Fan ON");
    
    digitalWrite(LED_RED_ALERT, HIGH);
    servoFan.write(90);
    delay(2000);
  }
}

void checkHeartRate(int heartRate) {
  if (heartRate > HR_HIGH || heartRate < HR_LOW) {
    Serial.println("\n[CRITICAL] Abnormal Heart Rate!");
    Serial.print("Heart Rate: "); Serial.print(heartRate); Serial.println(" BPM");
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Heart Rate Alert");
    lcd.setCursor(0, 1);
    lcd.print("HR: ");
    lcd.print(heartRate);
    lcd.print(" BPM");
    
    for (int i = 0; i < 5; i++) {
      digitalWrite(LED_RED_ALERT, HIGH);
      digitalWrite(BUZZER_PIN, HIGH);
      delay(200);
      digitalWrite(LED_RED_ALERT, LOW);
      digitalWrite(BUZZER_PIN, LOW);
      delay(200);
    }
  }
}

void checkAutoLighting(int ldrValue, bool movementDetected) {
  if (ldrValue > LDR_DARK && movementDetected) {
    Serial.println("\n[AUTO] Movement in Dark - Lights ON!");
    Serial.print("LDR Value: "); Serial.println(ldrValue);
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Lights ON");
    lcd.setCursor(0, 1);
    lcd.print("Motion Detected");
    
    digitalWrite(LED_WHITE_LIGHT, HIGH);
    delay(2000);
  }
}

void checkFever(float temperature) {
  if (temperature > TEMP_FEVER) {
    Serial.println("\n[WARNING] Fever Detected!");
    Serial.print("Temperature: "); Serial.print(temperature); Serial.println(" C");
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Fever Detected!");
    lcd.setCursor(0, 1);
    lcd.print("Temp: ");
    lcd.print(temperature, 1);
    lcd.print("C");
    
    digitalWrite(LED_RED_ALERT, HIGH);
    
    for (int i = 0; i < 3; i++) {
      digitalWrite(BUZZER_PIN, HIGH);
      delay(300);
      digitalWrite(BUZZER_PIN, LOW);
      delay(300);
    }
    
    delay(2000);
  }
}

void checkInactivityOrFall(bool movementDetected, float accelMagnitude) {
  unsigned long currentTime = millis();
  
  if (accelMagnitude > 2.5) {
    Serial.println("\n[EMERGENCY] Fall Detected!");
    Serial.print("Acceleration: "); Serial.print(accelMagnitude); Serial.println(" g");
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("FALL DETECTED!");
    lcd.setCursor(0, 1);
    lcd.print("EMERGENCY!");
    
    for (int i = 0; i < 10; i++) {
      digitalWrite(LED_RED_ALERT, HIGH);
      digitalWrite(BUZZER_PIN, HIGH);
      delay(100);
      digitalWrite(LED_RED_ALERT, LOW);
      digitalWrite(BUZZER_PIN, LOW);
      delay(100);
    }
    
    lastActivityTime = currentTime;
  }
  else if (!movementDetected && (currentTime - lastActivityTime > INACTIVITY_TIME)) {
    Serial.println("\n[ALERT] Prolonged Inactivity Detected!");
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Inactivity Alert");
    lcd.setCursor(0, 1);
    lcd.print("Check Patient!");
    
    digitalWrite(LED_RED_ALERT, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(2000);
    digitalWrite(BUZZER_PIN, LOW);
    
    lastActivityTime = currentTime;
  }
  else if (movementDetected) {
    lastActivityTime = currentTime;
  }
}

void monitorHealth() {
  int hrvRaw = analogRead(HRV_PIN);
  int gsrRaw = analogRead(GSR_PIN);
  int ldrValue = analogRead(LDR_PIN);
  
  int heartRate = map(hrvRaw, 0, 4095, 50, 150);
  
  sensors.requestTemperatures();
  float temperature = sensors.getTempCByIndex(0);
  if (temperature == -127.0) temperature = 37.0;
  
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);
  float accelX = ax / 16384.0;
  float accelY = ay / 16384.0;
  float accelZ = az / 16384.0;
  float accelMagnitude = sqrt(accelX*accelX + accelY*accelY + accelZ*accelZ);
  bool movementDetected = (accelMagnitude > 1.1 || accelMagnitude < 0.9);
  
  turnOffAllLEDs();
  turnOffBuzzer();
  stopFan();
  
  Serial.println("\n--- Current Readings ---");
  Serial.print("Heart Rate: "); Serial.print(heartRate); Serial.println(" BPM");
  Serial.print("GSR: "); Serial.println(gsrRaw);
  Serial.print("Temperature: "); Serial.print(temperature); Serial.println(" C");
  Serial.print("LDR: "); Serial.println(ldrValue);
  Serial.print("Accel Mag: "); Serial.print(accelMagnitude); Serial.println(" g");
  Serial.println("------------------------");
  
  checkHighPerspiration(gsrRaw);
  checkHeartRate(heartRate);
  checkAutoLighting(ldrValue, movementDetected);
  checkFever(temperature);
  checkInactivityOrFall(movementDetected, accelMagnitude);
  
  if (gsrRaw <= GSR_HIGH && heartRate >= HR_LOW && heartRate <= HR_HIGH && 
      temperature <= TEMP_FEVER && accelMagnitude < 2.5) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Status: Normal");
    lcd.setCursor(0, 1);
    lcd.print("HR:");
    lcd.print(heartRate);
    lcd.print(" T:");
    lcd.print(temperature, 1);
  }
  
  delay(3000);
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Ready!");
  lcd.setCursor(0, 1);
  lcd.print("Press button");
}

void loop() {
  bool buttonState = digitalRead(BUTTON_PIN);
  
  if (buttonState == LOW && lastButtonState == HIGH) {
    delay(50);
    monitorHealth();
  }
  
  lastButtonState = buttonState;
  delay(10);
}
