#include <SPI.h>
#include <SD.h>
#include <Servo.h>
#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#define ROWS 4
#define COLS 3

#define CLOSE 0
#define OPEN 180

#define PASS_FILE "password.txt"
#define CODE_LEN 4
#define DEFAULT_CODE "0000"
#define RELOAD_CODE ""
#define CANCEL_KEY '#'

#define LCD_ADDRESS 0x27
#define LCD_COLS 16
#define LCD_ROW 2
#define STANDARD_PRINT_DELAY 2000

#define NOT_DIGIT_KEY '!'
#define NOT_DIGIT "!"
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROW);


const char hexaKeys[ROWS][COLS] = {
  { '1', '2', '3' },
  { '4', '5', '6' },
  { '7', '8', '9' },
  { '*', '0', '#' }
};

const byte rowPins[ROWS] = { 9, 8, 7, 6 };
const byte colPins[COLS] = { 5, 4, A0 };

Keypad customKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

Servo servoLock;
uint32_t intTime = 0;

char code[CODE_LEN + 1];

volatile uint8_t resetValue = 0;

int8_t lcdState = -1;

bool hasStars = false;

ISR(INT0_vect) {
  if (millis() - intTime < 3000)
    return;
  intTime = millis();
  resetValue += 1;
}

uint32_t hash(unsigned char *str)
{
    uint32_t hash = 137  ;
    int c;

    /* hash * 33 + c * 13 */
    while (c = *str++)
        hash = ((hash << 5) + hash) + (c << 3) + (c << 2) + c;

    return hash;
}

uint32_t defaultCodeHash = 0; 
uint32_t codeHash = 0; 

uint32_t readHashFromFile()
{
  File passFile = SD.open(PASS_FILE, FILE_READ);
  char number[12];
  memset(number, 0, 12);
  char len = 0;

  char c = passFile.read();
  while (c != -1)
  {
    number[len] = c;
    c = passFile.read();
    len++;
  }
  passFile.close();
  number[len] = '\0';


  uint32_t hashVal = strtoul(number, NULL, 10);
  return hashVal;
}

bool isDigit(char c)
{
  return c >= '0' && c <= '9'; 
}

bool checkCode(char *str)
{
  for(int i = 0; i < CODE_LEN; i++)
  {
    if(!isDigit(str[i]))
    {
      return false;
    }
  }
  return true;
}

void setup() {

  // Open serial communications and wait for port to open:
  Serial.begin(9600);

  defaultCodeHash = hash(DEFAULT_CODE);
  
  if (!SD.begin(10)) {
    Serial.println("Init SD failed");
  }
  Serial.println("Init SD done");

  if (SD.exists(PASS_FILE)) {
    codeHash = readHashFromFile();
  } else {
    File passFile = SD.open(PASS_FILE, FILE_WRITE | O_TRUNC);
    codeHash = defaultCodeHash;
    passFile.print(defaultCodeHash);
    passFile.close();
  }

  Serial.println("Connect to servo, set pos");
  servoLock.attach(3);
  servoLock.write(CLOSE);


  // Initialize LCD
  Serial.println("Init LCD");
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);

  Serial.println("Init pins, interrupts");
  // Input pin for hall sensor
  pinMode(2, INPUT_PULLUP);
  pinMode(A1, INPUT);

  // Setup interrupt for button to reset pin
  cli();
  EICRA |= (1 << ISC00);
  EIMSK |= (1 << INT0);
  sei();
}

void lcdPrintText(uint8_t newState) {
  // If old text == new text, don't rewrite
  if (newState == lcdState) {
    return;
  }

  // On clear * dissapears
  hasStars = false;

  lcdState = newState;
  // Serial.println("Changing LCD");
  switch (newState) {
    case 0:
      lcd.clear();
      lcd.print("Enter key code");
      break;
    case 1:
      lcd.clear();
      lcd.print("WRONG CODE!");
      break;
    case 2:
      lcd.clear();
      lcd.print("Correct!");
      break;
    case 3:
      lcd.clear();
      lcd.print("Close the box!");
      break;
    case 4:
      lcd.clear();
      lcd.print("Changing code!");
      break;
    case 5:
      lcd.clear();
      lcd.print("Enter old code!");
      break;
    case 6:
      lcd.clear();
      lcd.print("Old code correct!");
      break;
    case 7:
      lcd.clear();
      lcd.print("Enter new code!");
      break;
    case 8:
      lcd.clear();
      lcd.print("Code changed!");
      break;
    case 9:
      lcd.clear();
      lcd.print("Old code wrong!");
      break;
    case 10:
      lcd.clear();
      lcd.print("Code resetted!");
      break;
    case 11:
      lcd.clear();
      lcd.print("Code can only");
      lcd.setCursor(1, 1);
      lcd.print("contain digits!");
      lcd.setCursor(0, 0);
      break;
    default:
      lcd.clear();
      lcd.print("Error");
      break;
  }
}

bool resetCode() {
  if (resetValue == 0) {
    return false;
  }

  resetValue = 0;

  File passFile = SD.open(PASS_FILE, FILE_WRITE | O_TRUNC);
  codeHash = defaultCodeHash;
  passFile.print(codeHash);
  passFile.close();

  lcdPrintText(10);
  delay(STANDARD_PRINT_DELAY);

  if (hasStars) {
    lcdState = -1;
    hasStars = false;
  }
  return true;
}

char getCodeKey() {
  char pressedKey = customKeypad.getKey();
  while (pressedKey == NO_KEY) {
    if (resetCode()) {
      return CANCEL_KEY;
    }
    if (pressedKey == CANCEL_KEY) {
      return CANCEL_KEY;
    }
    pressedKey = customKeypad.getKey();
  }

  lcd.write('*');
  hasStars = true;

  return pressedKey;
}

void getCode() {
  lcd.setCursor(2, 1);
  for (int i = 0; i < CODE_LEN; i++) {
    char pressedKey = getCodeKey();
    if (pressedKey == CANCEL_KEY) {
      strcpy(code, RELOAD_CODE);
      return;
    }
    code[i] = pressedKey;
  }
  code[CODE_LEN] = '\0';
}


void loop() {
  if (resetCode()) {
    return;
  }
  lcdPrintText(0);


  getCode();
  if (strcmp(code, RELOAD_CODE) == 0) {
    lcdState = -1;
    return;
  }

  
  if (strcmp(code, "*00*") == 0) {
    lcdPrintText(4);
    delay(STANDARD_PRINT_DELAY);

    lcdPrintText(5);
    getCode();
    if(!checkCode(code))
    {
      lcdPrintText(11);
      delay(STANDARD_PRINT_DELAY);
      return;
    }
    if (strcmp(code, RELOAD_CODE) == 0) {
      lcdState = -1;
      return;
    }
    uint32_t curr = hash(code);
    if (codeHash == curr) {
      lcdPrintText(6);
      delay(STANDARD_PRINT_DELAY);

      lcdPrintText(7);
      getCode();
      if(!checkCode(code))
      {
        lcdPrintText(11);
        delay(STANDARD_PRINT_DELAY);
        return;
      }
      curr = hash(code);

      if (strcmp(code, RELOAD_CODE) == 0) {
        lcdState = -1;
        return;
      }
      codeHash = curr;
      File passFile = SD.open(PASS_FILE, FILE_WRITE | O_TRUNC);
      passFile.print(codeHash);
      passFile.close();

      lcdPrintText(8);
      delay(STANDARD_PRINT_DELAY);


    } else {
      lcdPrintText(9);
      delay(STANDARD_PRINT_DELAY);
    }
    return;
  }

  if(!checkCode(code))
  {
    lcdPrintText(11);
    delay(STANDARD_PRINT_DELAY);
    return;
  } 
  uint32_t curr = hash(code);
  if (codeHash == curr) {
    lcdPrintText(2);
    servoLock.write(OPEN);
    delay(STANDARD_PRINT_DELAY);

    lcdPrintText(3);
    while (digitalRead(A1) != LOW) {}
    delay(500);
    servoLock.write(0);
    return;
  }   
  lcdPrintText(1);
  delay(STANDARD_PRINT_DELAY);
  
}
