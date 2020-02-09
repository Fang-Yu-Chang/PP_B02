#include <Wire.h>
#include <LiquidCrystal_I2C.h>
 
#define outputA 2           // 定義 outputA = 2, CLK
#define outputB 7           // 定義 outputB = 7, DT
#define sw 3                // 長按做電壓閥值切換
#define sw2 4               // 鬆軔、緊軔標號改變用
#define sw3 5               // 清除螢幕用
#define sw4 6               // 合併前一次秒數用
#define Voltage_input A0    // 讀取外部電壓
#define Detect_vol_led 9    // 外部電壓供給顯示(LED)

// 旋轉編碼器
float counter = 0.5;        // 顯示位置計數器
int counter2 = 0;           // 顯示位置計數器，實際輸出值
int counter2_old = 0;       // 前一次輸出值
float counter3 = 108.5;     // 閥值電壓計數器
int counter4 = 108;         // 閥值電壓計數器，實際輸出值(電壓顯示)
int detect_vol = 885;       // 閥值電壓計數器，實際輸出值(ADC顯示)
int aLastState;             // 定義 aLastState 為 int 類型變數

// LCD 顯示位置之座標
int lcd_aryc[8] = {0,0,0,0,11,11,11,11};           // lcd column
int lcd_aryr[8] = {0,1,2,3, 0, 1, 2, 3};           // lcd row
int lcd_aryc_sign[8] = {8,8,8,8,19,19,19,19};      // lcd star sign, 顯示星號用
int lcd_aryc_num[8] = {7,6,5,4,3,2,1,0};           // lcd顯示標號用
int lcd_aryc_time[8] = {2,2,2,2,13,13,13,13};      // 顯示時間之位置用
int lcd_aryc_time_sign[8] = {6,6,6,6,17,17,17,17}; // 顯示 ms 之位置用

// 按鈕，電壓閥值切換
unsigned long button_time = 0; // 按鈕持續按住的時間
boolean st1 = 0; // Long push flag
int st2 = 0; // Switch flag
int st3 = 0; // Short push flag
int st4 = 0; // State optput
int st5 = 0; // Save last state

// 按鈕2，切換鬆軔、緊軔標示
int bt3 = 0;
boolean bt4 = 0;
int bt3_r = 0; // 正常模式紀錄
int bt3_rf = 3; // 鎖定模式紀錄

// 按鈕3，清除螢幕
boolean bt5 = 0;

// 按鈕4，合併當前秒數至前項
boolean bt6 = 0;

// 旋轉編碼器兩種變數設定
boolean btr = 0; // 挑選要改變的變數

// 清除螢幕的時機
int bc = 0;

// ADC 讀取外部電壓
boolean cad = 0;
unsigned long start_time = 0;
unsigned long finally_time = 0;
unsigned long continued_time = 0;
unsigned long continued_old_time = 0;
unsigned long continued_temp_time = 0;
boolean force = 0;

// ADC debounce
unsigned long debounce_time = 0;

// 紀錄過去的時間
unsigned long record_time[8] = {0,0,0,0,0,0,0,0};

// 讓螢幕從星號位置開始顯示
boolean lda = 0;

// 最後一項停止紀錄時間
boolean stop_last = 0;

// 降低外部電壓顯示速度
unsigned long display_ext_time = 0;
unsigned long display_det_time = 0;

LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // 設定 LCD I2C 位址, PCF8574T, 0x27

void setup() {
  lcd.begin(20, 4); // 初始化 LCD，一行 20 的字元，共 2 行
  lcd.backlight();  // 開啟背光

  pinMode (sw,INPUT);  // 按鈕
  pinMode (sw2,INPUT); // 按鈕2
  pinMode (sw3,INPUT); // 按鈕3
  pinMode (sw4,INPUT); // 按鈕4

  pinMode (Detect_vol_led, OUTPUT); //LED顯示

  // 旋轉編碼器
  pinMode (outputA,INPUT); // 埠口模式設定：outputA 設為 輸入
  pinMode (outputB,INPUT); // 埠口模式設定：outputB 設為 輸入
  attachInterrupt (0, Rotary, CHANGE); // 啟用中斷函式(中斷0,Rotary函式,CHANGE模式)
   
  aLastState = digitalRead(outputA); // 將初始outputA的讀取值 設給 aLastState
} 

// LCD顯示標號程式
void put_num(int);

// 顯示螢幕星號位置程式
void lcd_sign(int);

// 螢幕輸出計算結果
void lcd_display_time(int);

//主程式運作區
void loop() {
  int real_vol;
  real_vol = analogRead(Voltage_input);
  
  // 計算按鈕按下的時間
  if (real_vol > detect_vol && btr == 0) {
    if (cad == 0) {
      if (micros() - debounce_time >= 500) {
        start_time = millis(); // 紀錄起始時間
        digitalWrite(Detect_vol_led, HIGH); // LED發亮
        cad = 1;
      }
    }
    else {
      debounce_time = micros();
    }
  }
  else {
    if (cad == 1) {
      if (micros() - debounce_time >= 500) {
        finally_time = millis(); // 紀錄終止時間
        continued_temp_time = finally_time - start_time; // 根據起始時間與終止時間計算時間差，得到經過時間
        
        if (continued_temp_time >= 1) { // 大於1ms才會輸出計算值
          continued_time = continued_temp_time;
        }
        
        digitalWrite(Detect_vol_led, LOW); // LED熄滅
        cad = 0;
        force = 1; // 鎖定累加旗標
      }
    }
    else {
      debounce_time = micros();
    }
  }

  // 所有按鈕之功能產生
  bu();

  // 按鈕長按後切換調整電壓閥值與返回功能
  // st4 = 0; 正常模式自動前進至下一項
  // st4 = 1; 鎖定模式手動切換至下一項
  // st4 = 2; 偵測電壓閥值設定
  if (st4 == 0) { // 正常累加模式
    if (bc != 1) { // 初始化顯示
      initial_display();
      
      if (bc == 2) { // 從鎖定累加功能轉過來時，從下一項開始顯示
        lda = 1;
      }
      
      bc = 1;
    }
    
    lcd_sign(counter2); // 顯示螢幕星號位置

    if (continued_time != continued_old_time) {
      if (lda <= 0) { // 檢測星號位置使否透過旋鈕改變
        lda = 1;
      }
      else {
        int j;
        for (j=0; j<=1; j++) {
          if (counter <= 7.5) {
            counter = counter + 0.5;
          }
        }
        counter2 = counter - 0.5;
      }

      if (counter2 <= 6) {
        record_time[counter2] = continued_time; // 儲存當前時間
      }
      else {
        if (stop_last == 0) { // 第八項之後停止紀錄時間
          record_time[counter2] = continued_time; // 儲存當前時間
          stop_last = 1;
        }
      }
      lcd_display_time(counter2);
      continued_old_time = continued_time;
    }
  }
  else if (st4 == 1) { // 鎖定累加模式
    if (bc != 2) {
      initial_display();
      continued_old_time = 0;
      continued_time = 0;
      bc = 2;
    }
    lcd_sign(counter2); // 顯示螢幕星號位置

    if (force == 1) {
      record_time[counter2] = record_time[counter2] + continued_time; // 時間累加
      continued_time = 0; // 避免重複累加
      lcd_display_time(counter2);
      force = 0;
    }
  }
  else if (st4 == 2) {
    if (bc != 0) { // 初始化顯示
      lcd.clear();
      btr = 1; // 旋轉編碼器調控電壓閥值
      lcd.print("Detection voltage"); //lcd.setCursor(0,0);
      lcd.setCursor(0,2);
      lcd.print("External voltage");
      bc = 0; // 功能返回時顯示初始化
    }

    // 顯示外部電壓，並降低外部電壓顯示速度
    if (millis() - display_ext_time >= 100) { // 每100ms更新一次
      display_ext_time = millis();
      lcd.setCursor(0,3);
      lcd.print(real_vol*0.122);
      lcd.print("V  ");
      lcd.setCursor(12,3);
      lcd.print(real_vol);
      lcd.print("   ");
    }

    // 顯示閥值電壓
    if (millis() - display_det_time >= 50) { // 每50ms更新一次
      display_det_time = millis();
      lcd.setCursor(0,1);
      lcd.print(counter4);
      lcd.print("V ");
      lcd.setCursor(12,1);
      lcd.print(detect_vol);
      lcd.print("   ");
    }

    // 偵測外部電壓是否大於設定的閥值
    if (real_vol > detect_vol) {
      digitalWrite(Detect_vol_led, HIGH); // LED發亮
    }
    else {
      digitalWrite(Detect_vol_led, LOW); // LED熄滅
    }
  }
}

// 螢幕輸出計算結果
void lcd_display_time(int count2) {
  lcd.setCursor(lcd_aryc_time[count2],lcd_aryr[count2]);
  if (record_time[count2] >= 10000) {
    lcd.print("Over");
    lcd.setCursor(lcd_aryc_time_sign[count2],lcd_aryr[count2]);
    lcd.print("  ");
  }
  else {
    lcd.print(record_time[count2]);
    lcd.setCursor(lcd_aryc_time_sign[count2],lcd_aryr[count2]);
    lcd.print("ms");
  }
}

// 顯示初始化
void initial_display() {
  lcd.clear();
  put_num(bt3); //標示鬆軔或緊軔標號

  // 呼叫記憶顯示過去的時間記錄
  int b;
  for (b=0; b<=7; b++) {
    lcd.setCursor(lcd_aryc_time[b],lcd_aryr[b]);

    if (record_time[b] != 0 && record_time[b] <= 9999) {
      lcd.print(record_time[b]);
      lcd.setCursor(lcd_aryc_time_sign[b],lcd_aryr[b]);
      lcd.print("ms");
    }
    else if (record_time[b] >= 10000) {
      lcd.print("Over");
    }
  }

  // 初始星號顯示
  lcd.setCursor(lcd_aryc_sign[counter2],lcd_aryr[counter2]);
  lcd.print("*");

  digitalWrite(Detect_vol_led, LOW); // LED熄滅
  btr = 0; // 旋轉編碼器調控星號位置
}

// 按鈕功能
void bu() {
  // 按鈕1狀態產生，切換調整電壓閥值與返回功能
  if (digitalRead(sw) == 0) {
    if (millis() - button_time >= 600) {
      if (st1 == 0) {
        if (st4 != 2) {
          st4 = 2; // 直接輸出，電壓閥值切換功能
          st3 = 2; // 狀態鎖定
        }
        else {
          st4 = st5; // 回復至正常模式與鎖定模式
          st2 = st5; // 恢復至前一次的狀態
          st3 = 1; // 狀態解鎖
        }
        st1 = 1;
      }
    }
    else {
      if (st3 == 0) {
        if (st2 <= 0) { // 狀態改變
          st2 ++;
        }
        else {
          st2 = 0;
        }
        st3 = 1;
      }
    }
  }
  // 按鈕2狀態產生，切換鬆軔與緊軔標示
  else if (digitalRead(sw2) == 0 && st4 != 2) {
    if(bt4 == 0) {
      if (millis() - button_time >= 10) {
        if (st4 == 0) {
          if (bt3 >= 1) {
            bt3 = 0;
          }
          else {
            bt3 ++;
          }
          bt3_r = bt3; // 紀錄正常模式的最後bt3值
        }
        else if (st4 == 1) {
          if (bt3 <= 2) {
            bt3 = 3;
          }
          else {
            bt3 --;
          }
          bt3_rf = bt3; // 紀錄鎖定模式的最後bt3值
        }
        put_num(bt3);
        bt4 = 1;
      }
    }
  }
  // 按鈕3狀態產生，清除螢幕與標示回歸原點
  else if (digitalRead(sw3) == 0 && st4 != 2) {
    if (bt5 == 0) {
      if (millis() - button_time >= 1000) {
        int h;
        for (h=0; h<=7; h++) { // 清除所有的儲存值
          record_time[h] = 0;
        }
        
        bc = 0; // 初始化顯示
        counter = 0.5;
        counter2 = 0;
        counter2_old = 0;
        lda = 0; // 螢幕從當前所指位置開始顯示
        stop_last = 0; // 重新允許紀錄最後一項
        bt5 = 1;
      }
    }
  }
  // 按鈕4，合併當前秒數至前項
  else if (digitalRead(sw4) == 0 && st4 == 0) {
    if (bt6 == 0) {
      if (millis() - button_time >= 800) {
        
        if (counter2 >= 1) {
          // 覆蓋當前顯示秒數為空白
          lcd.setCursor(lcd_aryc_time[counter2],lcd_aryr[counter2]);
          lcd.print("      ");

          record_time[counter2 - 1] = record_time[counter2] + record_time[counter2 - 1]; // 回加秒數至前一項
          record_time[counter2] = 0; // 清除過去時間值
          continued_time = record_time[counter2 - 1]; // 將累加的時間儲存當成目前的時間
          continued_old_time = 0; // 強制螢幕更新，令過去值為0
        }

        // 顯示位置往上移一格
        int k;
        for (k=0; k<=1; k++) {
          if (counter >= 1) {
            counter = counter - 0.5;
          }
        }
        counter2 = counter - 0.5;
        
        lda = 0; // 從當下切換過去的那一行開始顯示，不要直接跳下一行
        stop_last = 0; // 重新允許紀錄至最後一項
        bt6 = 1;
      }
    }
  }
  else {
    button_time = millis();
    bt4 = 0;
    bt5 = 0;
    bt6 = 0;

    if (st3 == 1) { // 按鈕1放開後才產生功能切換
      st4 = st2; // 輸出結果
      st5 = st2; // 儲存狀態
      st3 = 0;

      // 在按下按鈕1後自動切換標號
      if (st4 == 0) {
        put_num(bt3_r);
        bt3 = bt3_r; // 回復初始結果
      }
      else if (st4 == 1) {
        put_num(bt3_rf);
        bt3 = bt3_rf; // 回復初始結果
      }
    }
    st1 = 0;
  }
}

// 顯示螢幕星號位置
void lcd_sign(int count2) {
  int i;
  if (count2 != counter2_old) {
    // 先清除所有*號
    for (i=0; i<=7; i++) {
      lcd.setCursor(lcd_aryc_sign[i],lcd_aryr[i]);
      lcd.print(" ");
    }

    // 再當前位置標上星號
    lcd.setCursor(lcd_aryc_sign[count2],lcd_aryr[count2]);
    lcd.print("*");
    counter2_old = count2;
  }
}

// LCD顯示標號
// put_num(0); 緊軔
// put_num(1); 鬆軔
// put_num(2); 鬆軔(鎖定模式)
// put_num(3); 緊軔(鎖定模式)
void put_num(int pnum) {
  int i;
  for (i=0; i<=7; i++) {
    lcd.setCursor(lcd_aryc[i],lcd_aryr[i]);

    if (pnum == 0 || pnum == 3) {
      lcd.print(i+1);
    }
    else if (pnum == 1 || pnum == 2) {
      lcd.print(lcd_aryc_num[i]);
    }
    lcd.print("_");
  }
  
  if (pnum == 2 || pnum == 3) {
    lcd.setCursor(0,0);
    lcd.print("f");
  }
}

// 旋轉編碼器主程式
void Rotary() {
  int aState; // 定義 aState 為 int 類型變數
  aState = digitalRead(outputA);   // 將outputA的讀取值 設給 aState

  if (aState != aLastState && btr == 0) {  // 條件判斷，當aState 不等於 aLastState時發生
    if (digitalRead(outputB) != aState) {
      if (counter <= 7.5) {
        counter = counter + 0.5;
      }
    }
    else {
      if (counter >= 1) {
        counter = counter - 0.5;
      }
    }
    counter2 = counter - 0.5;
    lda = 0; // 螢幕從當前所指位置開始顯示
    stop_last = 0; // 重新允許紀錄最後一項
  }
  else if (aState != aLastState && btr == 1) {
    if (digitalRead(outputB) != aState) {
      if (counter3 <= 120.5) {
        counter3 = counter3 + 0.5;
      }
    }
    else {
      if (counter3 >= 2.5) {
        counter3 = counter3 - 0.5;
      }
    }
    counter4 = counter3 - 0.5;
    detect_vol = counter4*8.1967; // 1/0.122
  }
  aLastState = aState; // 將aState 最後的值 設給 aLastState
}
