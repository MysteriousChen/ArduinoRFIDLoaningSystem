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
#define SD_POWER_PIN    8
#define CS_PIN 4
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
#define CHANGE_TIME 10
int mode = 0;
byte lockFlag = 0; // no other actiob
byte pressFlag = 0; // button flag
byte lendFlag = 0; // lend state machine
byte wrong_Flag = 0;
byte timer = 0;
String UID;
byte itemType[4];
byte cursor = 0;
byte i_idx = 0;
byte u_idx = 0;
byte l_idx = 0;
String itemSets[2];
String userSets[2];
String lendSets[4];

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
  if(SD.remove("item.txt")){
      Serial.println(F("delete item.txt success"));
  }
  
  openFile = SD.open("card.txt", FILE_WRITE);       // 開啟檔案，一次僅能開啟一個檔案
  
  if (openFile) {                                   // 假使檔案開啟正常
    Serial.print(F("Write to card.txt..."));         
    openFile.println("Test to write data to SD card...");  // 繼續寫在檔案後面
    openFile.close();                               // 關閉檔案
    Serial.println(F("Completed!"));
  } else {
    Serial.println(F("\n open file error "));    // 無法開啟時顯示錯誤
  }
  delay(CHANGE_TIME);
}

void turnOnRFID(){
  digitalWrite(SD_POWER_PIN, LOW);
  delay(CHANGE_TIME);
  digitalWrite(RFID_POWER_PIN, HIGH);
  delay(CHANGE_TIME);
  mfrc522.PCD_Init();
}

void turnOnSD(){
  
  digitalWrite(RFID_POWER_PIN, LOW);
  delay(CHANGE_TIME);
  digitalWrite(SD_POWER_PIN, HIGH);
  delay(CHANGE_TIME);
  /*if (!SD.begin(4)) {
    Serial.println(F("Fail!"));
    return;
  }*/
}

void setup() {

  Serial.begin(9600);  // 開啟通訊串列埠開啟
  while (!Serial) {    // 等待串列埠連線
  }
  SPI.begin();
  // Global Setting
  pinMode(CS_PIN, OUTPUT);
  pinMode(RFID_POWER_PIN, OUTPUT);
  pinMode(SD_POWER_PIN, OUTPUT);
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
  digitalWrite(SD_POWER_PIN, HIGH);
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
  analogWrite(6, 0); // R
  analogWrite(5, 180); // G
  analogWrite(3, 0); // B
}

void writeSD(String str, const char* filename){
    openFile = SD.open(filename, FILE_WRITE);
    if(openFile){
        openFile.println("Write!!");
        openFile.close();
        Serial.println(F("Write File Success!"));
    }else{
        Serial.println(F("Open WriteFile Fail!"));
    }
}


void readSD(){
  openFile = SD.open("card.txt", FILE_READ);
  if(openFile){
    while(openFile.available()){
      Serial.write(openFile.read());
    }
    openFile.close();
  }
}

byte checkIDExist(String &uid, const char* filename){
    openFile = SD.open("card.txt", FILE_READ);
    if(openFile){
        Serial.println(F("Open ReadFile Success!!"));
        String str;
        while(openFile.available()){
            str = String(openFile.read());
            Serial.println(str);
            if(uid.equals(str.substring(uid.length()))){
                openFile.close();
                return 1;
            }
        }
        openFile.close();
    }else{
        Serial.println(F("Open ReadFile Fail!"));
    }
    return 0;
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

/// add
byte CheckIDExists_t(String &uid, byte mode){
    String *ref = (mode == 1)?itemSets:
                  (mode == 2)?userSets:lendSets;
    byte idx = (mode == 1)?i_idx:
               (mode == 2)?u_idx:l_idx;
    for(int i=0; i<idx; i++){
        if(uid.equals(ref[i].substring(0,uid.length()))){
            if(mode == 1){
                Serial.println(F("item ID exist!!"));
            }
            else if (mode == 2){
                Serial.println(F("User ID exist!!"));
            }
            else{
                if(ref[i][ref[i].length()-1] == '0'){
                  return 0;
                }
                Serial.println(F("item has been borrowed!!"));  
            }
            return 1;
        }
    }return 0;
}

void loop() {

    // Timer
    if(TIFR1 & (1 << OCF1A)){
        if(wrong_Flag && timer < 8){
          timer++;
            analogWrite(6, 180); // R
            analogWrite(5, 0); // G
        }else{
            timer = 0;
            wrong_Flag = false;
            analogWrite(6, 0); // R
            analogWrite(5, 180); // G
        }
        TIFR1 = (1<<OCF1A);
    }
    // Get key
    int key = returnKey();
    // Change State
    if(key == 1 && mode == 0){
        mode = 1;
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Login Item:");
        lcd.setCursor(0,1);
        lcd.print("Please Put Item");
    }
    if(key == 2 && mode == 0){
        mode = 2;
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Login User:");
        lcd.setCursor(0,1);
        lcd.print("Please Put Card");
    }
    if(key == 3 && mode == 0){
        mode = 3;
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Lend Item:");
        lcd.setCursor(0,1);
        lcd.print("Please Put Item");
    }
    if(key == 4 && mode == 0){
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
                 if(!CheckIDExists_t(UID, mode)){
                     if(mode == 1){
                          lcd.clear();
                          lcd.setCursor(0,0);
                          lcd.print("Enter ID:");
                      }else{
                          Serial.println(F("Input Card"));
                      }
                  }else{
                      wrong_Flag = true;
                      lockFlag = 0;
                      UID = "";
                  } 
            }else{
                  if(mode == 3){
                      if(lendFlag == 0 && CheckIDExists_t(UID, 1)){ // check item ID
                          if(!CheckIDExists_t(UID, 3)){
                                lendFlag = 1;
                                lendSets[l_idx] = UID;
                                lcd.clear();
                                lcd.setCursor(0,0);
                                lcd.print("Please Put Card"); 
                          }else{
                              wrong_Flag = true;
                          }
                      }else if(lendFlag == 1 && CheckIDExists_t(UID, 2)){ // check user ID
                          lendFlag = 0; mode = 0;
                          lendSets[l_idx++] += ":" + UID;
                          lcdIDLE();
                          l_idx = l_idx % 4;
                      }else{
                        wrong_Flag = true;
                      }
                      lockFlag = 0;
                      UID = "";
                  }else{
                      if(CheckIDExists_t(UID, 1)){
                          if(CheckIDExists_t(UID, 3)){
                              Serial.println(F("Return Item"));
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
                      
                  }
            }
            //// add ////
            /*>>>>>>>>>>>>>>>if(!checkIDExist(itemUID,"card.txt")){
                Serial.println(F("New Item login!!"));
                lcd.clear();
                lcd.setCursor(0,0);
                lcd.print("Enter ID:");
                lockFlag = 1;
            }else{
                Serial.println(F("Item has been logged!!"));
                lockFlag = 0;
            }>>>>>>>>>>>>>>>>>*/
            mfrc522.PICC_HaltA();
            //>>>>>>>>>>>>>>>>turnOnRFID();
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
                        itemSets[i_idx] = UID + ":" + itemTypeStr; // add
                        Serial.println(itemSets[i_idx%2]); // add
                        i_idx = (i_idx == 2)?2:i_idx+1;   // add
                        //>>>>>>>>>>>>>>>>> turnOnSD();
                        //>>>>>>>>>>>>>>>>> writeSD(itemUID + ":" + itemTypeStr,"item.txt");
                        //>>>>>>>>>>>>>>>>> turnOnRFID();
                        // lcd
                        lcdIDLE();
                        UID = "";
                    }
                } 
            }else{
                  if(key == 12){
                      mode = 0; lockFlag = 0;
                      userSets[u_idx%2] = UID;
                      u_idx = (u_idx == 2)?2:u_idx+1;   
                      lcdIDLE();
                      UID = "";
                  }
            }
        }
     }
}

