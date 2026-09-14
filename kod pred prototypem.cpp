#include <SPI.h>
#include <MFRC522.h>
#include <Keypad.h>
#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --- Nastavení I2C LCD displeje ---
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- Nastavení RFID (MFRC522) ---
#define RST_PIN 9   
#define SS_PIN  10  
MFRC522 rfid(SS_PIN, RST_PIN);
byte authorizedUID[] = {0xDE, 0xAD, 0xBE, 0xEF}; // Změň si podle své karty

// --- Nastavení 4x4 Klávesnice ---
const byte ROWS = 4; 
const byte COLS = 4; 
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {5, 4, 3, 2}; 
byte colPins[COLS] = {A3, A2, A1, A0}; 

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// --- Nastavení Serva, LED a Buzzeru ---
Servo lockServo;
const int servoPin = 6;    
const int redLedPin = A4;  // Červená LED
const int greenLedPin = A5;// Zelená LED
const int buzzerPin = 7;   // Buzzer

// --- Proměnné ---
String correctPin = "1234"; 
String enteredPin = "";     
bool isUnlocked = false;    // Stav trezoru (zamčeno/odemčeno)
int failedAttempts = 0;     // Počítadlo špatných pokusů

void setup() {
  Serial.begin(9600);
  SPI.begin();       
  rfid.PCD_Init();   
  
  lcd.init();                      
  lcd.backlight();
  
  lockServo.attach(servoPin);
  
  pinMode(redLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  
  lockSafe(); // Výchozí stav: zamčeno
}

void loop() {
  // Pokud je trezor odemčený, čekáme jen na stisknutí # pro zamknutí
  if (isUnlocked) {
    char key = keypad.getKey();
    if (key == '#') {
      lockSafe();
      playLockSound();
      showHomeScreen();
    }
    return; // Dál v loopu nepokračujeme, dokud se nezamkne
  }

  // 1. KONTROLA KLÁVESNICE
  char key = keypad.getKey();
  if (key) {
    if (key == '#') {
      if (enteredPin == correctPin) {
        failedAttempts = 0; // Reset pokusů při správném kódu
        openSafe("PIN Sprravny!");
        playHappySound();
      } else {
        failedAttempts++;
        playSadSound();
        
        if (failedAttempts >= 3) {
          lockoutCountdown(); // Spustí 10s blokádu
        } else {
          wrongCode();
        }
      }
      enteredPin = "";
    }
    else if (key == '*') {
      enteredPin = "";
      showHomeScreen();
    } 
    else {
      if (enteredPin.length() < 4) {
        enteredPin += key;
        lcd.setCursor(0, 1);
        lcd.print("PIN: ");
        for(int i = 0; i < enteredPin.length(); i++) {
          lcd.print("*");
        }
      }
    }
  }

  // 2. KONTROLA RFID KARTY
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    if (checkRFID()) {
      failedAttempts = 0;
      openSafe("RFID Platna!");
      playHappySound();
    } else {
      failedAttempts++;
      playSadSound();
      
      if (failedAttempts >= 3) {
        lockoutCountdown();
      } else {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Neznama karta!");
        delay(2000);
        showHomeScreen();
      }
    }
    rfid.PICC_HaltA(); 
  }
}

// --- POMOCNÉ FUNKCE ---

void showHomeScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Zadej PIN / RFID");
  lcd.setCursor(0, 1);
  lcd.print("PIN: ");
}

void openSafe(String message) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(message);
  lcd.setCursor(0, 1);
  lcd.print("Odemceno (# = zam)");
  
  lockServo.write(90); // Odemknout (90 stupňů)
  digitalWrite(redLedPin, LOW);
  digitalWrite(greenLedPin, HIGH); // Svítí zelená
  isUnlocked = true;
}

void lockSafe() {
  lockServo.write(0); // Zamknout (0 stupňů)
  digitalWrite(greenLedPin, LOW);
  digitalWrite(redLedPin, HIGH);   // Svítí červená
  isUnlocked = false;
}

void wrongCode() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Spatny PIN!");
  lcd.setCursor(0, 1);
  lcd.print("Pokusy: ");
  lcd.print(failedAttempts);
  lcd.print("/3");
  delay(2000);
  showHomeScreen();
}

// 10sekundový lockout při 3 špatných pokusech
void lockoutCountdown() {
  failedAttempts = 0; // Reset počitadla po trestu
  for (int i = 10; i > 0; i--) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ZABLOKOVANO!");
    lcd.setCursor(0, 1);
    lcd.print("Cekej: ");
    lcd.print(i);
    lcd.print("s   ");
    delay(1000);
  }
  showHomeScreen();
}

bool checkRFID() {
  for (byte i = 0; i < 4; i++) {
    if (rfid.uid.uidByte[i] != authorizedUID[i]) {
      return false;
    }
  }
  return true;
}

// --- ZVUKOVÉ EFEKTY PRO BUZZER ---

void playHappySound() {
  tone(buzzerPin, 1000, 150);
  delay(150);
  tone(buzzerPin, 1500, 200);
  delay(200);
}

void playSadSound() {
  tone(buzzerPin, 400, 300);
  delay(300);
  tone(buzzerPin, 250, 500);
  delay(500);
}

void playLockSound() {
  tone(buzzerPin, 800, 100);
  delay(100);
  tone(buzzerPin, 500, 150);
  delay(150);
}