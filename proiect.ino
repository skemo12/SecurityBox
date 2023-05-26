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
#define SOUND_PRINT_DELAY 1200

/* BUZZER Buzzer pin and notes to be played */
#define BUZZER 9
#define NOTE_E6 1319
#define NOTE_B5 988
#define NOTE_B4 494

/* Number of tries for a code */
#define DEFAULT_NO_TRIES 3

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
	LCD_STATE_DIGITS_ONLY,
	LCD_STATE_TRY_AGAIN
};

/* Declare LCD */
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROW);

/* Declare keypad */
const char hexaKeys[ROWS][COLS] = {
	{'1', '2', '3'},
	{'4', '5', '6'},
	{'7', '8', '9'},
	{'*', '0', '#'}};

const byte rowPins[ROWS] = {A2, 8, 7, 6};
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

/* Default hash for default code 0000 */
uint32_t defaultCodeHash = 0;

/* Hash of current password */
uint32_t codeHash = 0;

/* Global variable of number of tries remaining */
uint8_t noOfTries = DEFAULT_NO_TRIES;

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
	case LCD_STATE_TRY_AGAIN:
		lcd.clear();
		lcd.print("Try again in:");
		lcd.setCursor(5, 1);
		lcd.print("seconds");
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

	/* Reset number of tries as well */
	noOfTries = DEFAULT_NO_TRIES;

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

/* Plays sound on wrong code */
void playWrongSound()
{
	tone(BUZZER, NOTE_B4, 200);
	delay(500);
	tone(BUZZER, NOTE_B4, 200);
	delay(200);
}

/* Plays sound on correct code */
void playCorrectSound()
{
	tone(9, NOTE_B5, 100);
	delay(100);
	tone(9, NOTE_E6, 850);
	delay(700);
	noTone(9);
}

/* Functions that changes the code lock */
void changeCode()
{
	/* Inform user about command*/
	lcdPrintText(LCD_STATE_CHANGING_CODE);
	delay(STANDARD_PRINT_DELAY);

	/* Get old code for security checks */
	lcdPrintText(LCD_STATE_OLD_CODE);
	getCode();

	/* If the user canceled the command return */
	if (strcmp(code, RELOAD_CODE) == 0)
	{
		lcdState = -1;
		return;
	}
	/* Check if introduced code's hash matches the password's hash */
	uint32_t curr = hash(code);
	if (codeHash == curr)
	{
		/* Reset number of tries because code is correct */
		noOfTries = DEFAULT_NO_TRIES;
		lcdPrintText(LCD_STATE_OLD_CORRECT);
		playCorrectSound();
		delay(SOUND_PRINT_DELAY);

		/* Get new code desired by user */
		lcdPrintText(LCD_STATE_NEW_CODE);
		getCode();

		/* Check that code only contains digits */
		if (!checkCode(code))
		{
			lcdPrintText(LCD_STATE_DIGITS_ONLY);
			playWrongSound();
			delay(SOUND_PRINT_DELAY);
			return;
		}

		/* Check if user canceled command */
		if (strcmp(code, RELOAD_CODE) == 0)
		{
			lcdState = -1;
			return;
		}

		/* Save new password */
		curr = hash(code);
		codeHash = curr;
		File passFile = SD.open(PASS_FILE, FILE_WRITE | O_TRUNC);
		passFile.print(codeHash);
		passFile.close();

		/* Inform user that action was completed successfully */
		lcdPrintText(LCD_STATE_CODE_CHANGED);
		playCorrectSound();
		delay(SOUND_PRINT_DELAY);
		return;
	}

	/* If we get here, the code introduce was wrong */
	noOfTries -= 1;
	lcdPrintText(LCD_STATE_OLD_CODE_WRONG);
	playWrongSound();
	delay(SOUND_PRINT_DELAY);

	return;
}

/* Functions that open the box */
void openBox()
{
	/* Print correct code */
	lcdPrintText(LCD_STATE_CORRECT_CODE);
	servoLock.write(OPEN);
	playCorrectSound();
	delay(SOUND_PRINT_DELAY);

	/* Ask the user to close the box and wait for it to happen */
	lcdPrintText(LCD_STATE_CLOSE_BOX);
	while (digitalRead(A1) != LOW)
		;

	/* After a small delay, close the box */
	delay(500);
	servoLock.write(0);
}

/* Functions that display the number of seconds until user can try again */
void tryAgain()
{
	/* Check for reset */
	if (resetCode())
	{
		return;
	}

	/* Inform users that he has tried to many times */
	lcdPrintText(LCD_STATE_TRY_AGAIN);
	for (int8_t i = 9; i >= 0; i--)
	{
		/* Check for reset */
		if (resetCode())
		{
			return;
		}
		lcd.setCursor(3, 1);
		lcd.print(i);
		delay(1000);
	}

	/* Reset number of tries */
	noOfTries = DEFAULT_NO_TRIES;
}
/* Main loop */
void loop()
{
	/* Check for reset */
	if (resetCode())
	{
		return;
	}

	/* Check if user wasted all his tries */
	if (noOfTries == 0)
	{
		tryAgain();
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
		noOfTries = DEFAULT_NO_TRIES;
		return;
	}

	/* If we get to this point, it means that the code was wrong */
	noOfTries -= 1;
	lcdPrintText(LCD_STATE_WRONG_CODE);
	playWrongSound();
	delay(SOUND_PRINT_DELAY);
}
