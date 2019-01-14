#include <SPI.h>
#include <SD.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <stdio.h>
// Setting RFID
#define RFID_POWER_PIN    7
#define RST_PIN      A0    // reset pin
#define SS_PIN       10    // chip select pin
MFRC522 mfrc522(SS_PIN, RST_PIN);
// Setting SD
#define CS_PIN 4
#define MAX_LINENUM 50
Sd2Card card;
SdVolume volume;
SdFile root;
File openFile;
// Setting LCD
LiquidCrystal_I2C lcd(0x3F, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);
// Setting KeyPad (10:'C', 11:'*', 12:'#')
#define GET_KEY(x) (((x)>=615 && (x)<=630) ? 1 : \
                    ((x)>=600 && (x)<=614) ? 2 : \
                    ((x)>=590 && (x)<=599) ? 3 : \
                    ((x)>=563 && (x)<=580) ? 4 : \
                    ((x)>=550 && (x)<=562) ? 5 : \
                    ((x)>=540 && (x)<=549) ? 6 : \
                    ((x)>=520 && (x)<=535) ? 7 : \
                    ((x)>=510 && (x)<=519) ? 8 : \
                    ((x)>=502 && (x)<=509) ? 9 : \
                    ((x)>=493 && (x)<=501) ? 10 : \
                    ((x)>=481 && (x)<=492) ? 11 : \
                    ((x)>=473 && (x)<=480) ? 0 : \
                    ((x)>=460 && (x)<=472) ? 12 : -1)
unsigned long lastDebounceTime;
unsigned long debounceDelay = 50;
int lastButtonState = -1;
int keepval;
// Global Setting
#define CHANGE_TIME 20
int mode = 0;
byte lockFlag = 0; // no other actiob
byte pressFlag = 0; // button flag
byte lendFlag = 0; // lend state machine
byte wrong_Flag = 0;
byte timer = 0;
String UID,itemUID;
byte itemType[4];
byte cursor = 0;
int l_len = 0;
int l_line = 0;

void SDIntital(){
  //----------- 寫入檔案
  Serial.print(F("\nWaiting for SD card ready..."));

  if (!SD.begin(4)) {
    Serial.println(F("Fail!"));
    return;
  }
  Serial.println(F("Success!"));
  if(SD.remove("card.txt")){
      Serial.println(F("delete card.txt success"));
  }
  if(SD.remove("items.txt")){
      Serial.println(F("delete items.txt success"));
  }
  if(SD.remove("user.txt")){
      Serial.println(F("delete user.txt success"));
  }
  if(SD.remove("lend.txt")){
      Serial.println(F("delete lend.txt success"));
  }
  delay(CHANGE_TIME);
}

void turnOnRFID(){
  digitalWrite(RFID_POWER_PIN, HIGH);
  delay(CHANGE_TIME);
  mfrc522.PCD_Init();
}

void turnOnSD(){  
  digitalWrite(RFID_POWER_PIN, LOW);
  delay(CHANGE_TIME);
}

byte ledMode = 0;
void ledState(byte mode){
  analogWrite(6, 0); // R
  analogWrite(5, 0); // G
  analogWrite(3, 0); // B
  if(mode == 0){
    analogWrite(5, 180); // G  
  }else if(mode == 1){
    analogWrite(3, 120); // B  
  }else{
    analogWrite(6, 180); // R  
  }
}

void setup() {

  Serial.begin(9600);  // 開啟通訊串列埠開啟
  while (!Serial) {    // 等待串列埠連線
  }
  SPI.begin();
  // Global Setting
  pinMode(CS_PIN, OUTPUT);
  pinMode(RFID_POWER_PIN, OUTPUT);
  digitalWrite(RFID_POWER_PIN, LOW);
  // Timer Setting
  // Timer setting
  noInterrupts();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;
  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << CS12) | (1 << CS10);
  OCR1A = 1562;// 0.1sec
  interrupts();
  // Setting sd and list
  digitalWrite(CS_PIN, LOW);
  SDIntital();
  // Setting lcd
  lcd.begin(16,2);
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Ready!");
  // Setting RFID
  turnOnRFID();
  // Setting LED
  ledState(0);
}

void writeSD(String str, const char* filename){
    openFile = SD.open(filename, FILE_WRITE);
    if(openFile){
        openFile.println(str);
        openFile.close();
        Serial.println(F("Write File Success!"));
    }else{
        Serial.println(F("Open WriteFile Fail!"));
    }
}

int checkIDExist_SD(String &uid,byte mode){
    // select file
    openFile = (mode == 1)?SD.open("items.txt", FILE_READ):
               (mode == 2)?SD.open("user.txt", FILE_READ):SD.open("lend.txt", FILE_READ);
    // read file
    if(openFile){
        Serial.println(F("Open ReadFile Success!!"));
        String str; int idx = 0;
        while(openFile.available()){
            idx++;
            str = openFile.readStringUntil('\n');
            Serial.println(str);
            if(uid.equals(str.substring(0,uid.length()))){
                openFile.close();
                return idx;
            }
        }
        openFile.close();
    }else{
        Serial.println(F("Open ReadFile Fail!"));
    }
    return 0;
}

int checkLendState(int line){
    openFile = SD.open("lend.txt", FILE_READ);
    if(openFile){
        Serial.println(F("Open ReadFile Success!!"));
        while(openFile.available()){
            String str = openFile.readStringUntil('\n');
            line--;
            if(line == 0){
                int len = str.length();
                if(str[str.indexOf(':')+1] == 'X'){ // mean xxx:X (has been return)
                    openFile.close();
                    return str.indexOf(':');
                }else{
                    openFile.close();
                    return 0;
                }
            }
        }openFile.close();
    }else{
        Serial.println(F("Open ReadFile Fail!"));
    }
    return 0;
}

String convertStr2SDFormat(String str){
    int len = str.length();
    String tmp = String(str);
    for(int i=len; i<= MAX_LINENUM; i++){
        tmp += " ";
    }return tmp;
}

void modifyLendState(String uid, int line, byte mode){
    // 0 for borrow, 1 for return
    openFile = SD.open("lend.txt", FILE_WRITE);
    if(openFile){
        Serial.println(F("Open Wtite File Success!"));
        openFile.seek((line-1)*(MAX_LINENUM+3));
        if(mode == 1){
            String str = convertStr2SDFormat(uid + ":X");
            openFile.println(str);
            openFile.close();
        }else{
            openFile.println(uid);
            openFile.close();
        } 
    }
}

byte returnKey(){ 
    byte key = GET_KEY(analogRead(A1));
    //Serial.println(analogRead(A1));
    //delay(300);
    if(key != lastButtonState){
        pressFlag = 0;
        keepval = key;
        lastDebounceTime = millis();
    }
    if((millis() - lastDebounceTime) > debounceDelay){
        if(key != -1){
            if(key == keepval){
                pressFlag++;
                pressFlag = (pressFlag == 2)?2:pressFlag;
                lastButtonState = key;
                return key;  
            }
        }
    }
    lastButtonState = key;
    return -1;
}

void lcdIDLE(){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Ready!");
}

void readSD(){
  openFile = SD.open("lend.txt", FILE_READ);
  if(openFile){
    while(openFile.available()){
      Serial.write(openFile.read());
    }
    openFile.close();
  }
  delay(CHANGE_TIME);
}

void loop() {

    // Timer
    if(TIFR1 & (1 << OCF1A)){
        if(wrong_Flag && timer < 8){
          timer++;
            ledState(2); // warning mode
        }else{
            timer = 0;
            wrong_Flag = false;
            ledState(ledMode);
        }
        TIFR1 = (1<<OCF1A);
    }
    // Get key
    int key = returnKey();
    // Change State
    if(key == 1 && mode == 0){
        ledMode = 1;
        mode = 1;
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Login Item:");
        lcd.setCursor(0,1);
        lcd.print("Please Put Item");
    }
    if(key == 2 && mode == 0){
        ledMode = 1;
        mode = 2;
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Login User:");
        lcd.setCursor(0,1);
        lcd.print("Please Put Card");
    }
    if(key == 3 && mode == 0){
        ledMode = 1;
        mode = 3;
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Lend Item:");
        lcd.setCursor(0,1);
        lcd.print("Please Put Item");
    }
    if(key == 4 && mode == 0){
        ledMode = 1;
        mode = 4;
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Return Item:");
        lcd.setCursor(0,1);
        lcd.print("Please Put Item");
    }
    // State Contents
    if(mode != 0){
        // jump state
        if(key == 11){
            mode = 0; lockFlag = 0; cursor = 0;
            UID = "";
            lcdIDLE();
        }
        if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial() && lockFlag == 0) {
            lockFlag = 1;
            Serial.println(F("Enter RFID"));
            // basic card info
            byte *id = mfrc522.uid.uidByte;   // 取得卡片的UID
            byte idSize = mfrc522.uid.size;   // 取得UID的長度
            // rfid uuid 
            for(byte i=0; i < idSize; i++){
              UID += String(id[i],DEC);
            }
            //// add ////
            if(mode < 3){
                 if(checkIDExist_SD(UID, mode) == 0){
                     if(mode == 1){
                          lcd.clear();
                          lcd.setCursor(0,0);
                          lcd.print("Enter ID:");
                      }else{
                          lcd.clear();
                          lcd.setCursor(0,0);
                          lcd.print("Press #");
                          lcd.setCursor(0,1);
                          lcd.print("To Login");
                      }
                  }else{
                      wrong_Flag = true;
                      lockFlag = 0;
                      UID = "";
                  } 
            }else{
                  if(mode == 3){
                      if(lendFlag == 0){
                          l_len = 0;
                          l_len = 0;
                      }
                      if(lendFlag == 0 && checkIDExist_SD(UID, 1) > 0){ // check item ID
                          if((l_line = checkIDExist_SD(UID, 3)) == 0){
                                lendFlag = 1;
                                itemUID = UID;
                                lcd.clear();
                                lcd.setCursor(0,0);
                                lcd.print("Please Put Card"); 
                          }else{
                              // item has been return
                              if(l_line > 0 && (l_len = checkLendState(l_line)) > 0){;
                                  lendFlag = 1;
                                  itemUID = UID;
                                  lcd.clear();
                                  lcd.setCursor(0,0);
                                  lcd.print("Please Put Card"); 
                              }else{
                                itemUID = "";
                                wrong_Flag = true;
                              }
                          }
                      }else if(lendFlag == 1 && checkIDExist_SD(UID, 2) > 0){ // check user ID
                          lendFlag = 0; mode = 0;
                          turnOnSD();
                          if(l_len > 0) {
                            modifyLendState(convertStr2SDFormat(String(itemUID + ":" + UID)), l_line, 0);
                            readSD();
                          }
                          else writeSD(convertStr2SDFormat(String(itemUID + ":" + UID)),"lend.txt");
                          turnOnRFID();
                          lcdIDLE();
                      }else{
                        wrong_Flag = true;
                      }
                      lockFlag = 0;
                      UID = "";
                  }else if(mode == 4){
                      int line = 0, len = 0;
                      if(checkIDExist_SD(UID, 1) > 0){
                          if((line = checkIDExist_SD(UID, 3)) > 0){
                              if((len = checkLendState(line)) == 0){
                                  Serial.println(F("Return Item"));
                                  turnOnSD();
                                  modifyLendState(UID, line, 1);
                                  turnOnRFID();
                                  mode = 0; lockFlag = 0;
                                  lcdIDLE();
                                  UID = ""; 
                              }else{
                                wrong_Flag = true;
                                lockFlag = 0;
                              }
                          }else{
                              wrong_Flag = true;
                              lockFlag = 0;
                          }
                      }else{
                          wrong_Flag = true;  
                          lockFlag = 0;
                      }
                  }
            }
            mfrc522.PICC_HaltA();
        }
        if(lockFlag == 1 && pressFlag == 1 && mode < 3){
            if(mode == 1){
                if(key == 10 && cursor > 0){ // press C
                    cursor--;
                    lcd.setCursor(cursor,1);
                    lcd.print(" ");
                }
                else if(key < 10 && cursor < 4){
                    lcd.setCursor(cursor,1); 
                    lcd.print(key);
                    itemType[cursor] = key;
                    cursor++;
                }else if(cursor >= 4){
                    // store info
                    if(key == 12){
                        Serial.println(F("Save Item Info"));
                        // initial 
                        mode = 0; lockFlag = 0; cursor = 0;
                        // write SD
                        String itemTypeStr;
                        for(byte i=0; i<4; i++)
                            itemTypeStr += String(itemType[i], DEC);
                        turnOnSD();
                        writeSD(UID + ":" + itemTypeStr,"items.txt");
                        turnOnRFID();
                        // lcd
                        lcdIDLE();
                        UID = "";
                    }
                } 
            }else{
                  if(key == 12){
                      mode = 0; lockFlag = 0;
                      turnOnSD();
                      writeSD(UID ,"user.txt");
                      turnOnRFID();
                      lcdIDLE();
                      UID = "";
                  }
            }
        }
     }else{
        ledMode = 0;
     }
}

