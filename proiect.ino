#include <SPI.h>
#include <SD.h>
#include <Servo.h>
#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

/* Keypad specs */
#define ROWS 4
#define COLS 3

/* Servo positions */
#define CLOSE 0
#define OPEN 180

/* Code pin constants */
#define PASS_FILE "password.txt"
#define CODE_LEN 4
#define DEFAULT_CODE "0000"
#define CHANGE_CODE "*00*"
#define RELOAD_CODE ""
#define CANCEL_KEY '#'

/* LCD specs */
#define LCD_ADDRESS 0x27
#define LCD_COLS 16
#define LCD_ROW 2
#define STANDARD_PRINT_DELAY 2000

/* LCD states */
enum lcdState
{
	LCD_STATE_ENTER_CODE,
	LCD_STATE_WRONG_CODE,
	LCD_STATE_CORRECT_CODE,
	LCD_STATE_CLOSE_BOX,
	LCD_STATE_CHANGING_CODE,
	LCD_STATE_OLD_CODE,
	LCD_STATE_OLD_CORRECT,
	LCD_STATE_NEW_CODE,
	LCD_STATE_CODE_CHANGED,
	LCD_STATE_OLD_CODE_WRONG,
	LCD_STATE_CODE_RESET,
	LCD_STATE_DIGITS_ONLY
};

/* Declare LCD */
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROW);

/* Declare keypad */
const char hexaKeys[ROWS][COLS] = {
	{'1', '2', '3'},
	{'4', '5', '6'},
	{'7', '8', '9'},
	{'*', '0', '#'}};

const byte rowPins[ROWS] = {9, 8, 7, 6};
const byte colPins[COLS] = {5, 4, A0};

Keypad customKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

/* Declare servo */
Servo servoLock;

/* Interrupt timer */
uint32_t intTime = 0;

/* Code to be read from keypad */
char code[CODE_LEN + 1];

/* Reset pin value from interrupt */
volatile uint8_t resetValue = 0;

/* Variable for lcd state */
int8_t lcdState = -1;

/* Button interrupt INT0 for resetting pin */
ISR(INT0_vect)
{
	if (millis() - intTime < 3000)
		return;
	intTime = millis();
	resetValue += 1;
}

/* Hash function for code */
uint32_t hash(unsigned char *str)
{
	uint32_t hash = 137;
	int c;

	/* hash * 33 + c * 13 */
	while (c = *str++)
		hash = ((hash << 5) + hash) + (c << 3) + (c << 2) + c;

	return hash;
}

/* Default hash for default code 0000 */
uint32_t defaultCodeHash = 0;

/* Hash of current password */
uint32_t codeHash = 0;

/* Read old's password hash from file */
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

/* Checks if a character is digit */
bool isDigit(char c)
{
	return c >= '0' && c <= '9';
}

/* Check if code only contains digits */
bool checkCode(char *str)
{
	for (int i = 0; i < CODE_LEN; i++)
	{
		if (!isDigit(str[i]))
		{
			return false;
		}
	}
	return true;
}

/* Setup function */
void setup()
{

	/* Open serial communications for debugging */
	Serial.begin(9600);

	/* Setup default hash */
	defaultCodeHash = hash(DEFAULT_CODE);

	/* Begin SPI with SDcard */
	if (!SD.begin(10))
	{
		Serial.println("Init SD failed");
	}
	Serial.println("Init SD done");

	/* If PASS_FILE, then create it and store default code, else read old one */
	if (SD.exists(PASS_FILE))
	{
		codeHash = readHashFromFile();
	}
	else
	{
		File passFile = SD.open(PASS_FILE, FILE_WRITE | O_TRUNC);
		codeHash = defaultCodeHash;
		passFile.print(defaultCodeHash);
		passFile.close();
	}

	/* Connect and init servo */
	Serial.println("Connect to servo, set pos");
	servoLock.attach(3);
	servoLock.write(CLOSE);

	/* Initialize LCD */
	Serial.println("Init LCD");
	lcd.init();
	lcd.backlight();
	lcd.setCursor(0, 0);

	/* Set the button and hall sensors as inputs */
	Serial.println("Init pins, interrupts");
	pinMode(2, INPUT_PULLUP);
	pinMode(A1, INPUT);

	/* Setup interrupt for button to reset pin */
	cli();
	EICRA |= (1 << ISC00);
	EIMSK |= (1 << INT0);
	sei();

	/* Welcome user */
	lcd.clear();
	lcd.print("Hello!");
	delay(STANDARD_PRINT_DELAY);
}

/* Prints text to LCD by state */
void lcdPrintText(uint8_t newState)
{
	/* If old text == new text, don't rewrite */
	if (newState == lcdState)
	{
		return;
	}

	/* Change state to new one */
	lcdState = newState;

	/* Change LCD text */
	Serial.println("Changing LCD");
	switch (newState)
	{
	case LCD_STATE_ENTER_CODE:
		lcd.clear();
		lcd.print("Enter key code:");
		break;
	case LCD_STATE_WRONG_CODE:
		lcd.clear();
		lcd.print("Wrong code!");
		break;
	case LCD_STATE_CORRECT_CODE:
		lcd.clear();
		lcd.print("Correct!");
		break;
	case LCD_STATE_CLOSE_BOX:
		lcd.clear();
		lcd.print("Close the box!");
		break;
	case LCD_STATE_CHANGING_CODE:
		lcd.clear();
		lcd.print("Changing code!");
		break;
	case LCD_STATE_OLD_CODE:
		lcd.clear();
		lcd.print("Enter old code:");
		break;
	case LCD_STATE_OLD_CORRECT:
		lcd.clear();
		lcd.print("Old code correct!");
		break;
	case LCD_STATE_NEW_CODE:
		lcd.clear();
		lcd.print("Enter new code:");
		break;
	case LCD_STATE_CODE_CHANGED:
		lcd.clear();
		lcd.print("Code changed!");
		break;
	case LCD_STATE_OLD_CODE_WRONG:
		lcd.clear();
		lcd.print("Old code wrong!");
		break;
	case LCD_STATE_CODE_RESET:
		lcd.clear();
		lcd.print("Code reset!");
		break;
	case LCD_STATE_DIGITS_ONLY:
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

/* Reset the code if the variable resetValue was set*/
bool resetCode()
{
	/* No interrupts were called, return */
	if (resetValue == 0)
	{
		return false;
	}

	/* Only reset once */
	resetValue = 0;

	/* Save default password hash to file */
	File passFile = SD.open(PASS_FILE, FILE_WRITE | O_TRUNC);
	codeHash = defaultCodeHash;
	passFile.print(codeHash);
	passFile.close();

	/* Print that password was reset */
	lcdPrintText(LCD_STATE_CODE_RESET);
	delay(STANDARD_PRINT_DELAY);

	return true;
}

/* Waits for key to be pressed and checks for interrupts or cancel */
char getCodeKey()
{
	/* Get the key */
	char pressedKey = customKeypad.getKey();
	/* Wait until key is pressed */
	while (pressedKey == NO_KEY)
	{
		/* Check if code reset */
		if (resetCode())
		{
			return CANCEL_KEY;
		}
		/* Check for cancel */
		if (pressedKey == CANCEL_KEY)
		{
			return CANCEL_KEY;
		}
		/* Continue loop */
		pressedKey = customKeypad.getKey();
	}

	/* A key was pressed, print a star to represent the key */
	lcd.write('*');

	return pressedKey;
}

/* Gets the full code if no reset or cancel appeared */
/* On reset or cancel, code is set to RELOAD_CODE */
void getCode()
{
	/* Prepare lcd for printing stars on every key pressed */
	lcd.setCursor(2, 1);
	/* Get 4 keys */
	for (int i = 0; i < CODE_LEN; i++)
	{
		char pressedKey = getCodeKey();
		/* Check for cancel */
		if (pressedKey == CANCEL_KEY)
		{
			strcpy(code, RELOAD_CODE);
			return;
		}
		code[i] = pressedKey;
	}
	code[CODE_LEN] = '\0';
}

/* Functions that changes the code lock */
void changeCode()
{
	lcdPrintText(LCD_STATE_CHANGING_CODE);
	delay(STANDARD_PRINT_DELAY);

	lcdPrintText(LCD_STATE_OLD_CODE);
	getCode();

	if (strcmp(code, RELOAD_CODE) == 0)
	{
		lcdState = -1;
		return;
	}
	uint32_t curr = hash(code);
	if (codeHash == curr)
	{
		lcdPrintText(LCD_STATE_OLD_CORRECT);
		delay(STANDARD_PRINT_DELAY);

		lcdPrintText(LCD_STATE_NEW_CODE);
		getCode();
		if (!checkCode(code))
		{
			lcdPrintText(LCD_STATE_DIGITS_ONLY);
			delay(STANDARD_PRINT_DELAY);
			return;
		}
		curr = hash(code);

		if (strcmp(code, RELOAD_CODE) == 0)
		{
			lcdState = -1;
			return;
		}
		codeHash = curr;
		File passFile = SD.open(PASS_FILE, FILE_WRITE | O_TRUNC);
		passFile.print(codeHash);
		passFile.close();

		lcdPrintText(LCD_STATE_CODE_CHANGED);
		delay(STANDARD_PRINT_DELAY);
	}
	else
	{
		lcdPrintText(LCD_STATE_OLD_CODE_WRONG);
		delay(STANDARD_PRINT_DELAY);
	}
	return;
}

/* Functions that open the box */
void openBox()
{
	/* Print correct code */
	lcdPrintText(LCD_STATE_CORRECT_CODE);
	servoLock.write(OPEN);
	delay(STANDARD_PRINT_DELAY);

	/* Ask the user to close the box and wait for it to happen */
	lcdPrintText(LCD_STATE_CLOSE_BOX);
	while (digitalRead(A1) != LOW)
		;

	/* After a small delay, close the box */
	delay(500);
	servoLock.write(0);
}

/* Main loop */
void loop()
{
	/* Check for reset */
	if (resetCode())
	{
		return;
	}

	/* Wait for input */
	lcdPrintText(LCD_STATE_ENTER_CODE);
	getCode();
	if (strcmp(code, RELOAD_CODE) == 0)
	{
		lcdState = -1;
		return;
	}

	/* Change code was introduced */
	if (strcmp(code, CHANGE_CODE) == 0)
	{
		changeCode();
		return;
	}

	/* Check whether the hash of introduced code matches password */
	uint32_t curr = hash(code);
	if (codeHash == curr)
	{
		openBox();
		return;
	}

	/* If we get to this point, it means that the code was wrong */
	lcdPrintText(LCD_STATE_WRONG_CODE);
	delay(STANDARD_PRINT_DELAY);
}
