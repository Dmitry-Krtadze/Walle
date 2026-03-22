#include <LiquidCrystal_I2C.h>
#include <FastLED.h>
#include <DFMiniMp3.h>
#include <Servo.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#define NUM_LEDS 2
#define DATA_PIN 42

#define TRIGGER_PIN  12
#define ECHO_PIN     13
#define MAX_DISTANCE 200

#define DHTPIN 4
#define DHTTYPE DHT22

#define MOTOR_A_IN_1 9
#define MOTOR_A_IN_2 8
#define MOTOR_B_IN_1 7
#define MOTOR_B_IN_2 6

Servo myservo;

int leftSpeed = 255;
int rightSpeed = 255;

CRGB leds[NUM_LEDS];
LiquidCrystal_I2C lcd(0x20, 16, 2);
DHT_Unified dht(DHTPIN, DHTTYPE);

class Mp3Notify;
typedef DFMiniMp3<HardwareSerial, Mp3Notify> DfMp3;
DfMp3 dfmp3(Serial1);

String currentIP = "Очікування...";

// --- Змінні для Сонара ---
unsigned long sonarTimer = 0;
bool isSonarClose = false;

// --- Змінні для Анімації ---
bool isPlayingAudio = false;
bool rainbowMode = false;
uint8_t gHue = 0; 

// Змінні для плавних кольорів
CRGB currentAnimColor = CRGB::Black;
CRGB targetAnimColor = CRGB::Red;

// Змінні для сервоприводу
int currentServoPos = 90;
int targetServoPos = 90;
int servoStepDelay = 20;
unsigned long lastServoTime = 0;

class Mp3Notify {
  public:
    static void PrintlnSourceAction(DfMp3_PlaySources source, const char* action) {}
    static void OnError(DfMp3& mp3, uint16_t errorCode) {}
    static void OnPlayFinished(DfMp3& mp3, DfMp3_PlaySources source, uint16_t track) {}
    static void OnPlaySourceOnline(DfMp3& mp3, DfMp3_PlaySources source) {}
    static void OnPlaySourceInserted(DfMp3& mp3, DfMp3_PlaySources source) {}
    static void OnPlaySourceRemoved(DfMp3& mp3, DfMp3_PlaySources source) {}
};

void setup() {
  initAllPeriphery();
  greetingWalle();
  delay(2000);
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WALL-E Ready!");
}

void loop() {
  dfmp3.loop();
  
  // --- ЛОГІКА СОНАРА (читаємо кожні 200мс) ---
  EVERY_N_MILLISECONDS(200) {
    digitalWrite(TRIGGER_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIGGER_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIGGER_PIN, LOW);
    
    // Таймаут 30мс щоб не блокувати код
    long duration = pulseIn(ECHO_PIN, HIGH, 30000); 
    long distance = (duration / 2) / 29.1;
    Serial.println(distance);
    if (distance > 0 && distance < 30) {
      if (!isSonarClose) {
        isSonarClose = true;
        sonarTimer = millis();
      } else {
        if (millis() - sonarTimer > 5000) {
          // Якщо 5 секунд тримається дистанція < 30см - кидаємо команду на ESP32
          Serial3.print("X\n");
          sonarTimer = millis(); // Скидаємо таймер
        }
      }
    } else {
      isSonarClose = false;
    }
  }

  // --- ЛОГІКА АНІМАЦІЙ ---
  if (isPlayingAudio && !rainbowMode) {
    
    // 1. Плавна зміна кольорів
    EVERY_N_MILLISECONDS(20) {
      if (currentAnimColor == targetAnimColor) {
        // Вибираємо новий яскравий рандомний колір
        targetAnimColor = CHSV(random8(), 255, 255);
      }
      // Функція nblend плавно перетікає від поточного кольору до цільового
      nblend(currentAnimColor, targetAnimColor, 5); 
      leds[0] = currentAnimColor;
      leds[1] = currentAnimColor;
      FastLED.show();
    }

    // 2. Рандомні рухи сервоприводу
    if (millis() - lastServoTime > servoStepDelay) {
      lastServoTime = millis();
      
      if (currentServoPos < targetServoPos) currentServoPos++;
      else if (currentServoPos > targetServoPos) currentServoPos--;
      else {
        // Якщо досягли цілі - вибираємо новий кут і нову швидкість
        targetServoPos = random(50, 130); // Щоб не бився об краї
        servoStepDelay = random(5, 45);   // Від дуже швидкого до повільного
      }
      myservo.write(currentServoPos);
    }
  }

  // Фонова веселка (якщо увімкнена вручну з пульта)
  if (rainbowMode && !isPlayingAudio) {
    EVERY_N_MILLISECONDS(20) {
      gHue++; 
      fill_rainbow(leds, NUM_LEDS, gHue, 7);
      FastLED.show();
    }
  }

  // --- ЧИТАННЯ КОМАНД ВІД ESP32 ---
  if (Serial3.available()) {
    String msg = Serial3.readStringUntil('\n');
    msg.trim(); 
    if (msg.length() == 0) return;

    if (msg.startsWith("#IP:")) {
      currentIP = msg.substring(4);
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("Wi-Fi OK. IP:");
      lcd.setCursor(0, 1); lcd.print(currentIP);
    } 
    else if (msg.startsWith("#TR:")) {
      isPlayingAudio = true; // ПОЧИНАЄМО АНІМАЦІЮ
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("Playing:");
      lcd.setCursor(0, 1); lcd.print(msg.substring(4));
    }
    else if (msg.startsWith("#ST:")) {
      isPlayingAudio = false; // ЗУПИНЯЄМО АНІМАЦІЮ
      // Повертаємо робота у спокійний стан
      myservo.write(90);
      currentServoPos = 90;
      targetServoPos = 90;
      leds[0] = CRGB::Black;
      leds[1] = CRGB::Black;
      FastLED.show();
      
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("Wi-Fi OK. IP:");
      lcd.setCursor(0, 1); lcd.print(currentIP);
    }
    
    else if (msg.startsWith("V")) {
      int s = msg.substring(1).toInt();
      leftSpeed = s;
      rightSpeed = s;
    }

    else if (msg == "F") { setMotorsSpeed(leftSpeed, rightSpeed); }
    else if (msg == "B") { setMotorsSpeed(-leftSpeed, -rightSpeed); }
    else if (msg == "l") { setMotorsSpeed(-leftSpeed, rightSpeed); }
    else if (msg == "r") { setMotorsSpeed(leftSpeed, -rightSpeed); }
    else if (msg == "S") { setMotorsSpeed(0, 0); }

    else if (msg == "L") { digitalWrite(34, HIGH); }
    else if (msg == "Q") { digitalWrite(34, LOW); }

    else if (msg.startsWith("A")) {
      int angle = msg.substring(1).toInt();
      myservo.write(angle);
    }

    else if (msg.startsWith("C#")) {
      rainbowMode = false;
      long color = strtol(msg.substring(2).c_str(), NULL, 16);
      int r = (color >> 16) & 0xFF;
      int g = (color >> 8) & 0xFF;
      int b = color & 0xFF;
      
      leds[0] = CRGB(r, g, b);
      leds[1] = CRGB(r, g, b);
      FastLED.show();
    }
    else if (msg == "c") {
      rainbowMode = false;
      leds[0] = CRGB(random(0,255), random(0,255), random(0,255));
      leds[1] = CRGB(random(0,255), random(0,255), random(0,255));
      FastLED.show();
    }
    else if (msg == "y") {
      rainbowMode = true;
    }
  }
}

// ==========================================
// ФУНКЦІЇ ПЕРИФЕРІЇ ТА РУХУ
// ==========================================

void setMotorsSpeed(int left, int right) {
  if (left > 255) left = 255;
  if (left < -255) left = -255;
  if (right > 255) right = 255;
  if (right < -255) right = -255;

  if (left < 0) {
    left *= -1;
    analogWrite(MOTOR_A_IN_1, left);
    analogWrite(MOTOR_A_IN_2, 0);
  } else {
    analogWrite(MOTOR_A_IN_1, 0);
    analogWrite(MOTOR_A_IN_2, left);
  }

  if (right < 0) {
    right *= -1;
    analogWrite(MOTOR_B_IN_1, right);
    analogWrite(MOTOR_B_IN_2, 0);
  } else {
    analogWrite(MOTOR_B_IN_1, 0);
    analogWrite(MOTOR_B_IN_2, right);
  }
}

void initAllPeriphery() {
  randomSeed(analogRead(0));
  Serial.begin(9600);
  Serial1.begin(9600);
  Serial2.begin(9600);
  Serial3.begin(9600); 
  
  dht.begin();
  lcd.init();
  lcd.backlight();
  myservo.attach(2);
  delay(2000);
  
  dfmp3.begin();
  dfmp3.setVolume(25);

  pinMode(34, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(8, OUTPUT);
  pinMode(9, OUTPUT);
  
  // НАЛАШТУВАННЯ ПІНІВ СОНАРА
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  FastLED.addLeds<WS2811, DATA_PIN, RGB>(leds, NUM_LEDS);
}

void greetingWalle() {
  dfmp3.playFolderTrack16(1, 1);
  for (int i = 0; i < 16; i++) {
    lcd.setCursor(i, 0);
    lcd.print("#");
    lcd.setCursor(i, 1);
    lcd.print("#");

    if (i % 3 == 0 && i != 15) { myservo.write(random(110, 150)); }
    if (i == 15) { myservo.write(130); }

    leds[0] = CRGB(0, 255 - map(i, 0, 15, 0, 255), map(i, 0, 15, 0, 255));
    leds[1] = CRGB(0, 255 - map(i, 0, 15, 0, 255), map(i, 0, 15, 0, 255));
    FastLED.show();
    delay(60);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" Hi! My name is");
  lcd.setCursor(5, 1);
  lcd.print("Wall-e");
  
}

void lcdPrint(String str) {
  for (int i = 0; i < str.length(); i++) {
    lcd.setCursor(i, 0);
    lcd.print(str.charAt(i));
    delay(100);
  }
}