#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

#include <PN532_I2C.h>
#include <NfcAdapter.h>

#include "bitmaps.h"

#define codeLength 4
#define greenLed 2
#define redLed 4
#define piezoPin 3
#define lock A0
#define simReset A1

// Define states for the state machine
enum State {
  NORMAL,
  CHANGE_PASS,
  ENTER_NEW_PASS,
  CONFIRM_NEW_PASS
};

State systemState = NORMAL;

// SIM    
SoftwareSerial mySerial(A3,A2);

// lcd
LiquidCrystal_I2C lcd(0x27, 16, 2);

// nfc
PN532_I2C pn532_i2c(Wire);
NfcAdapter nfc = NfcAdapter(pn532_i2c);
String tagId = "None";
byte nuidPICC[4];

// keypad section
char newPass[codeLength];
char Code[codeLength];
char Pass[codeLength] = {'9', '8', '7', '8'};
int keycount = 0;

const byte ROWS = 4;
const byte COLS = 4;

char Keys[ROWS][COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};

byte rowPins[ROWS] = {8, 7, 6, 5};
byte colPins[COLS] = {12, 11, 10, 9};

Keypad newKeypad = Keypad(makeKeymap(Keys), rowPins, colPins, ROWS, COLS);
// end of keypad section

//gyroscope section
const int MPU=0x68; 
int16_t GyX, GyY, GyZ, oldGyX, oldGyY, oldGyZ;
int iteration = 0;
int stolen = 0;
// end of gyroscope section

void setup()
{ 
  Serial.begin(9600);

  // setup pin modes
  pinMode(simReset, OUTPUT);
  pinMode(greenLed, OUTPUT);
  pinMode(redLed, OUTPUT);
  pinMode(piezoPin, OUTPUT);
  pinMode(lock,OUTPUT);
  
  // SIM module setup
  gsmSetup();

  // nfc
  nfc.begin();

  // UI display setup
  setupUI();

  // setup of gyroscope
  gyroSetup();
}

void loop()
{ 
  
  // listener for key input
  char newKey = newKeypad.getKey();

  // nfc listener
  if(nfc.tagPresent(10)){
    readNFC();
  }
  
  //comparing with passcode
  if(newKey){
    handleKey(newKey);
  }

  
  if(stolen != 1){
    gyroLoop();
  }


}

void handleKey(char key) {
  switch (systemState) {
    case NORMAL:
      lcd.setCursor(0, 0);
      lcd.print("Nfc or pass:");
      if (key == 'A') {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Enter current:");
        systemState = CHANGE_PASS;
      } else {
        doComparing(key);
      }
      break;

    case CHANGE_PASS:
      lcd.setCursor(keycount, 1);
      Code[keycount] = key;
      lcd.print("*");
      keycount++;

      if (keycount == codeLength) {
        if (compareCodes(Code, Pass)) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Enter new pass:");
          keycount = 0;
          systemState = ENTER_NEW_PASS;
        } else {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Incorrect pass!");
          delay(2000);
          lcd.clear();
          keycount = 0;
          systemState = NORMAL;
        }
      }
      break;

    case ENTER_NEW_PASS:
      lcd.setCursor(keycount, 1);
      newPass[keycount] = key;
      lcd.print("*");
      keycount++;

      if (keycount == codeLength) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Confirm pass:");
        keycount = 0;
        systemState = CONFIRM_NEW_PASS;
      }
      break;

    case CONFIRM_NEW_PASS:
      lcd.setCursor(keycount, 1);
      if (key == 'D') {
        if (compareCodes(Code, newPass)) {
          // Update the password
          for (size_t i = 0; i < codeLength; i++) {
            Pass[i] = Code[i];
          }
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Password changed!");
          delay(2000);
          lcd.clear();
          keycount = 0;
          systemState = NORMAL;
        } else {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Not match!");
          delay(2000);
          lcd.clear();
          systemState = NORMAL;
          keycount = 0;
        }
      } else {
        lcd.print("*");
        Code[keycount] = key;
        keycount++;
      }
      break;
  }
}

// nfcReading
void readNFC() {
    NfcTag tag = nfc.read();
    tagId = tag.getUidString();
    tag.print();
    if(tagId == "03 B3 68 3B" || tagId == "62 2E 1E 2E" || tagId == "08 71 C3 C9"){
        correct();
        resetValues();
    }
  

}

// setup gsm
void gsmSetup(){
  digitalWrite(simReset, LOW);
  delay(100);
  digitalWrite(simReset, HIGH);

  mySerial.begin(9600);
  delay(1000);

  mySerial.println("AT"); //Once the handshake test is successful, it will back to OK

 


}
  
// message sending
void sendMessage(){
    
    mySerial.println("AT");
    
    mySerial.println("AT+CMGF=1");
   
    mySerial.println("AT+CMGS=\"+421911252664\"");
   
    mySerial.print("Your Takeshi is being stolen!");

    mySerial.write(26);
   

}


// comparing correctness of password
bool compareCodes(char *Code, char *Pass)
{
  for (size_t i = 0; i < codeLength; i++)
  {
    if (Code[i] != Pass[i])
    {
      return false;
    }
  }
  return true;
}

// resets all values
void resetValues()
{
  keycount = 0;
  memset(Code, 0, sizeof(Code));
  digitalWrite(greenLed, LOW);
  digitalWrite(redLed, LOW);
}

void playSuccessSound()
{
  tone(piezoPin, 1000, 200);
  delay(400);
  noTone(piezoPin);
}

void playErrorSound()
{
  tone(piezoPin, 500, 1000);
  delay(1200);
  noTone(piezoPin);
}

// set of instructions in incorrect case
void incorrect()
{ lcd.clear();
  digitalWrite(lock, LOW);
  digitalWrite(greenLed, LOW);
  digitalWrite(redLed, HIGH);
  playErrorSound();
  incorrectUI();
  delay(4000);
  lcd.clear();
}

// set of instructions in correct case
void correct()
{
  digitalWrite(lock, HIGH);
  digitalWrite(greenLed, HIGH);
  digitalWrite(redLed, LOW);
  playSuccessSound();
  correctUI();
  digitalWrite(lock, LOW);
  lcd.clear();
  
}

// main key comparing funtion
void doComparing(char newKey){
  if(newKey == 'A'){
    lcd.setCursor(0,0);
    lcd.print("For changing");
    delay(1500);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("enter current:");
  }
  lcd.setCursor(keycount, 1);
  Code[keycount]= newKey;
  lcd.print("*");
  keycount++;
  
  if(keycount==codeLength){
    
    if(compareCodes(Code, Pass)){
      delay(300);
    	correct(); // LCD prints, LEDs, piezo
      
    }else{ 
      delay(300);
    	incorrect(); // LCD prints, LEDs, piezo
       
    }
    
    // resets values: keycount, leds, memset
	  resetValues();
    
  }
}

void gyroSetup(){
  Wire.begin();
  Wire.beginTransmission(MPU);
  Wire.write(0x6B);  
  Wire.write(0);    
  Wire.endTransmission(true);
}

void gyroLoop(){
    Wire.beginTransmission(MPU);
    Wire.write(0x3B);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU, 12, true);

    GyX = Wire.read() << 8 | Wire.read();
    GyY = Wire.read() << 8 | Wire.read();
    GyZ = Wire.read() << 8 | Wire.read();
    int variablePos = 10000;

    if(iteration != 0 && (((GyX > oldGyX + variablePos) || (GyX < oldGyX - variablePos)) || ((GyY > oldGyY + variablePos) || (GyY < oldGyY - variablePos)) || ((GyZ > oldGyZ + variablePos) || (GyZ < oldGyZ - variablePos)))){
      // --> GSM sends message about stolen takeshi
      sendMessage();
      Serial.print("Takeshi is being stolen!");
      stolen = 1;

    }

    oldGyX = GyX;
    oldGyY = GyY;
    oldGyZ = GyZ;

    delay(100);
    iteration++;

}

void correctUI()
{
  lcd.clear();
  // UI
  lcd.createChar(4, leftCrossUp);
  lcd.createChar(5, leftPipeDown);
  lcd.createChar(6, rightCrossUp);
  lcd.createChar(7, rightPipeDown);

  lcd.setCursor(14, 1);
  lcd.write(5);

  lcd.setCursor(15, 1);
  lcd.write(7);

  lcd.setCursor(0,0 );
  lcd.print("Opened!");

  lcd.setCursor(8,0 );
  lcd.print("4");
  delay(1000);
  lcd.setCursor(8,0 );
  lcd.print("3");
  delay(1000);
  lcd.setCursor(8,0 );
  lcd.print("2");
  delay(1000);
  lcd.setCursor(8,0 );
  lcd.print("1");
  delay(1000);
  lcd.setCursor(8,0 );
  lcd.print("0");
}

void printStar(int keycount)
{
  lcd.createChar(0, starLeftUp);
  lcd.createChar(1, starRighttUp);
  lcd.createChar(2, starLeftDown);
  lcd.createChar(3, starRightDown);

  if (keycount == 0)
  {
    lcd.setCursor(0, 0);
    lcd.write(0);

    lcd.setCursor(1, 0);
    lcd.write(1);

    lcd.setCursor(0, 1);
    lcd.write(2);

    lcd.setCursor(1, 1);
    lcd.write(3);
  }
  else if (keycount == 1)
  {
    lcd.setCursor(2, 0);
    lcd.write(0);

    lcd.setCursor(3, 0);
    lcd.write(1);

    lcd.setCursor(2, 1);
    lcd.write(2);

    lcd.setCursor(3, 1);
    lcd.write(3);
  }
  else if (keycount == 2)
  {
    lcd.setCursor(4, 0);
    lcd.write(0);

    lcd.setCursor(5, 0);
    lcd.write(1);

    lcd.setCursor(4, 1);
    lcd.write(2);

    lcd.setCursor(5, 1);
    lcd.write(3);
  }
  else if (keycount == 3)
  {
    lcd.setCursor(6, 0);
    lcd.write(0);

    lcd.setCursor(7, 0);
    lcd.write(1);

    lcd.setCursor(6, 1);
    lcd.write(2);

    lcd.setCursor(7, 1);
    lcd.write(3);
  }
}


void incorrectUI()
{
  // UI
  lcd.createChar(4, leftCrossUp);
  lcd.createChar(5, leftCrossDown);
  lcd.createChar(6, rightCrossUp);
  lcd.createChar(7, rightCrossDown);

  lcd.setCursor(14, 0);
  lcd.write(4);

  lcd.setCursor(15, 0);
  lcd.write(6);

  lcd.setCursor(14, 1);
  lcd.write(5);

  lcd.setCursor(15, 1);
  lcd.write(7);

  lcd.setCursor(0,0 );
  lcd.print("Incorrect!");
}

void setupUI(){
  lcd.init();
  delay(1000);
  lcd.backlight();
  lcd.setCursor(0, 1);

  lcd.createChar(0, full);
  lcd.setCursor(1, 0);
  lcd.write(0);
  lcd.setCursor(1, 1);
  lcd.write(0);
  lcd.setCursor(15, 0);
  lcd.write(0);
  lcd.setCursor(15, 1);
  lcd.write(0);
  delay(100);

  lcd.setCursor(2, 0);
  lcd.write(0);
  lcd.setCursor(2, 1);
  lcd.write(0);
  lcd.setCursor(14, 0);
  lcd.write(0);
  lcd.setCursor(14, 1);
  lcd.write(0);
  delay(100);

  lcd.setCursor(3, 0);
  lcd.write(0);
  lcd.setCursor(3, 1);
  lcd.write(0);
  lcd.setCursor(13, 0);
  lcd.write(0);
  lcd.setCursor(13, 1);
  lcd.write(0);
  delay(100);

  lcd.setCursor(4, 0);
  lcd.write(0);
  lcd.setCursor(4, 1);
  lcd.write(0);
  lcd.setCursor(12, 0);
  lcd.write(0);
  lcd.setCursor(12, 1);
  lcd.write(0);
  delay(100);

  lcd.setCursor(5, 0);
  lcd.write(0);
  lcd.setCursor(5, 1);
  lcd.write(0);
  lcd.setCursor(11, 0);
  lcd.write(0);
  lcd.setCursor(11, 1);
  lcd.write(0);
  delay(100);

  lcd.setCursor(6, 0);
  lcd.write(0);
  lcd.setCursor(6, 1);
  lcd.write(0);
  lcd.setCursor(10, 0);
  lcd.write(0);
  lcd.setCursor(10, 1);
  lcd.write(0);
  delay(100);

  lcd.setCursor(7, 0);
  lcd.write(0);
  lcd.setCursor(7, 1);
  lcd.write(0);
  lcd.setCursor(9, 0);
  lcd.write(0);
  lcd.setCursor(9, 1);
  lcd.write(0);
  delay(100);

  lcd.setCursor(8, 0);
  lcd.write(0);
  lcd.setCursor(8, 1);
  lcd.write(0);
  delay(100);

  lcd.clear();

  //---------------------------------------
  lcd.createChar(0, lock1);
  lcd.createChar(1, lock2);
  lcd.createChar(2, lock3);
  lcd.createChar(3, lock4);
  lcd.createChar(4, lock5);
  lcd.createChar(5, lock6);
  lcd.createChar(6, lock7);
  lcd.createChar(7, lock8);

  // lock

  lcd.setCursor(6, 0);
  lcd.write(0);

  lcd.setCursor(6, 1);
  lcd.write(1);

  lcd.setCursor(7, 0);
  lcd.write(2);

  lcd.setCursor(7, 1);
  lcd.write(3);

  lcd.setCursor(8, 0);
  lcd.write(4);

  lcd.setCursor(8, 1);
  lcd.write(5);

  lcd.setCursor(9, 0);
  lcd.write(6);

  lcd.setCursor(9, 1);
  lcd.write(7);

  delay(1000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Nfc or pass:");
}