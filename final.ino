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
// Setting KeyPad (10:'C', 11: '#')
#define GET_KEY(x) (((x)>=920 && (x)<=930) ? 1 : \
                    ((x)>=900 && (x)<=915) ? 2 : \
                    ((x)>=885 && (x)<=895) ? 3 : \
                    ((x)>=841 && (x)<=860) ? 4 : \
                    ((x)>=828 && (x)<=840) ? 5 : \
                    ((x)>=810 && (x)<=827) ? 6 : \
                    ((x)>=776 && (x)<=790) ? 7 : \
                    ((x)>=763 && (x)<=775) ? 8 : \
                    ((x)>=750 && (x)<=762) ? 9 : \
                    ((x)>=709 && (x)<=719) ? 0 : \     
                    ((x)>=736 && (x)<=749) ? 10 : \
                    ((x)>=699 && (x)<=708) ? 11 : -1)   
unsigned long lastDebounceTime;
unsigned long debounceDelay = 100;
int lastButtonState = -1;
int keepval;
// Global Setting
#define CHANGE_TIME 10
int mode = 0;
byte lockFlag = 0; // no other actiob
byte pressFlag = 0; // button flag
String itemUID;
byte itemType[4];
byte cursor = 0;

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
  if (!SD.begin(4)) {
    Serial.println(F("Fail!"));
    return;
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
  pinMode(SD_POWER_PIN, OUTPUT);
  digitalWrite(RFID_POWER_PIN, LOW);
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

void loop() {
    // 確認是否有新卡片
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
    // State Contents
    if(mode == 1){
        if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial() && lockFlag == 0) {
            lockFlag = 1;
            Serial.println(F("Enter RFID"));
            // basic card info
            byte *id = mfrc522.uid.uidByte;   // 取得卡片的UID
            byte idSize = mfrc522.uid.size;   // 取得UID的長度
            // item uuid 
            for(byte i=0; i < idSize; i++){
              itemUID += String(id[i],DEC);
            }
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
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("Enter ID:");
            mfrc522.PICC_HaltA();
            //>>>>>>>>>>>>>>>>turnOnRFID();
        }
        if(lockFlag == 1 && pressFlag == 1){
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
                if(key == 11){
                    Serial.println(F("Save Info"));
                    // initial 
                    mode = 0; lockFlag = 0; cursor = 0;
                    // write SD
                    String itemTypeStr;
                    //>>>>>>>>>>>>>>>>> turnOnSD();
                    for(byte i=0; i<4; i++)
                        itemTypeStr += String(itemType[i], DEC);
                    //>>>>>>>>>>>>>>>>> writeSD(itemUID + ":" + itemTypeStr,"item.txt");
                    itemUID = "";
                    //>>>>>>>>>>>>>>>>> turnOnRFID();
                    // lcd
                    lcd.clear();
                    lcd.setCursor(0,0);
                    lcd.print("Ready!");
                }
            }
        }
     } 
}

