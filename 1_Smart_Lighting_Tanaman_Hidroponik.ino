//  #include <LiquidCrystal.h>

//LiquidCrystal lcd(13, 12, 14, 27, 26, 25);
#include <HTTPClient.h>
#define BLYNK_PRINT Serial
//#define BLYNK_TEMPLATE_ID "TMPL6Q8LxFM8Z"
//#define BLYNK_TEMPLATE_NAME "Monitoring DHT22"
#define BLYNK_TEMPLATE_ID "TMPL6YZiZYD6S"
#define BLYNK_TEMPLATE_NAME "MonitoringAir"
#include <BlynkSimpleEsp32.h>
#include "DHT.h"
#include <Wire.h>
#include <BH1750.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);

#define DHTPIN 13
#define DHTTYPE DHT22

#define APP_DEBUG
#define RL 10
#define Ro 9.8 // nilai default di udara bersih (bisa disesuaikan setelah kalibrasi)

// parameter dari datasheet MQ-2 (disesuaikan dari grafik log-log)
#define alpg -0.47
#define blpg 1.32

#define aco -0.38
#define bco 1.34

#define ach4 -0.42
#define bch4 1.32

float VRL;
float Rs_mq;
float ratio;
float lpg;
float co;
float ch4;
float t;
int h;
int lux;

int i;
int value;
int numReadings = 10;

#define lampu 16
#define kipas 18
#define MQ_sensor 34

//char auth[] = "jBqxUvQOab40HTvULT762wc7nag-sy3-";
char auth[] = "-8JNvdMM8bVmhDEvYJ3YXSIelIWq2rrU";
char ssid[] = ".";
char pass[] = "123456677890";

bool statusLampuManual = false;
bool statusKipasManual = false;
bool modeOtomatis = true;  // Default: otomatis

DHT dht(DHTPIN, DHTTYPE);
BlynkTimer timer;

// BH1750 Setup
BH1750 lightSensor(0x23);  // ADDR tidak dicolok (alamat default 0x23)

const char* scriptURL = "https://script.google.com/macros/s/AKfycbz3PNK3AbUUupV7Zw-Oxxc1UwepRI_Ct45NvBVkSyWEB8HNKOWTkccjX7LDv8HUUXes/exec"; // ganti dengan URL Web Apps kamu

void setup() {
  Serial.begin(115200);
  pinMode(lampu, OUTPUT);
  pinMode(kipas, OUTPUT);
  // WiFi dan Blynk
  Blynk.begin(auth, ssid, pass);
  Wire.begin(21, 22); // SDA = GPIO21, SCL = GPIO22

  lcd.init();        // Inisialisasi LCD
  lcd.backlight();   // Nyalakan lampu latar
  lcd.setCursor(0, 0);
  lcd.print("Halo, woee!");
  lcd.setCursor(0, 1);
  lcd.print("Aing hidup");

  // DHT

  dht.begin();

  // BH1750
  if (lightSensor.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println("Sensor BH1750 siap");
  } else {
    Serial.println("Gagal mendeteksi sensor BH1750!");
  }

  // Timer

  timer.setInterval(1000L, sendSensor_mq);
  timer.setInterval(1000L, sendSensor_dht);
  timer.setInterval(1000L, sendSensor_light);  // Tambahkan pengiriman data lux
  timer.setInterval(1000L, updateLCD);  // LCD update tiap 2 detik
    timer.setInterval(1000L, sendToGoogleSheets);  // Kirim tiap 10 detik

}

void loop() {
  Blynk.run();
  timer.run();
}

void sendSensor_dht() {
  h = dht.readHumidity();
  t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("Gagal membaca sensor DHT!");
    return;
  }

  Serial.print("Temperature: ");
  Serial.println(t);
  Serial.print("Humidity: ");
  Serial.println(h);

  Blynk.virtualWrite(V0, t);
  Blynk.virtualWrite(V1, h);
  // ======== Kontrol Otomatis KIPAS Berdasarkan dht ==========

  if (modeOtomatis) {
    if (h > 80 || lpg > 5 || co > 30) { // INI HIDUPKAN KIPAS
      digitalWrite(kipas, LOW);
    } else {
      digitalWrite(kipas, HIGH);
    }
  }

}

// void sendSensor_mq() {
//   value = 0;
//   for (i = 0; i < numReadings; i++) {
//     value += analogRead(MQ_sensor);
//     delay(10);
//   }
//   value = value / numReadings;
//   Serial.print("Value : ");
//   Serial.println(value);

//   VRL = value * (3.3 / 4095.0);
//   Rs_mq = ((3.3 * RL) / VRL) - RL;
//   ratio = Rs_mq / Ro;

//   // Hitung LPG
//   lpg = pow(10, (log10(ratio) - blpg) / alpg);
//   Serial.print("LPG   : ");
//   Serial.print(lpg);
//   Serial.println(" ppm");

//   // Hitung CO
//   co = pow(10, (log10(ratio) - bco) / aco);
//   Serial.print("CO    : ");
//   Serial.print(co);
//   Serial.println(" ppm");

//   // Hitung CH4
//   ch4 = pow(10, (log10(ratio) - bch4) / ach4);
//   Serial.print("CH4   : ");
//   Serial.print(ch4);
//   Serial.println(" ppm");

//   Serial.println(" ");

//   //  Blynk.virtualWrite(V2, lpg);
//   //  Blynk.virtualWrite(V3, co);
//   //  Blynk.virtualWrite(V4, ch4);

//   // ======== Kontrol Otomatis KIPAS Berdasarkan CO ==========
//   //  if (modeOtomatis) {
//   //    if (lpg > 5 ) {
//   //      digitalWrite(kipas, LOW);
//   //    } else {
//   //      digitalWrite(kipas, HIGH);
//   //    }
//   //  }
// }

void sendSensor_mq() {
  value = 0;
  for (i = 0; i < numReadings; i++) {
    value += analogRead(MQ_sensor);
    delay(10);
  }
  value = value / numReadings;
  Serial.print("Value ADC: ");
  Serial.println(value);

  // Ubah nilai ADC ke tegangan
  VRL = value * (3.3 / 4095.0);

  if (VRL < 0.01) {
    Serial.println("VRL terlalu kecil, data tidak valid");
    return;
  }

  // Hitung Rs
  Rs_mq = ((3.3 * RL) / VRL) - RL;
  ratio = Rs_mq / Ro;

  if (ratio <= 0 || isnan(ratio)) {
    Serial.println("Ratio tidak valid");
    return;
  }

  // Hitung ppm
  //  lpg = pow(10, (log10(ratio) - blpg) / alpg);
  co = pow(10, (log10(ratio) - bco) / aco);
  //  ch4 = pow(10, (log10(ratio) - bch4) / ach4);

  // Batasi nilai agar tidak ekstrem
  if (lpg > 10000) lpg = 10000;
  if (co > 10000) co = 10000;
  //  if (ch4 > 10000) ch4 = 10000;
  Blynk.virtualWrite(V2, lpg);
  Blynk.virtualWrite(V3, co);
  Blynk.virtualWrite(V4, ch4);
  //  Serial.print("LPG  : "); Serial.print(lpg); Serial.println(" ppm");
  Serial.print("CO   : "); Serial.print(co); Serial.println(" ppm");
  //  Serial.print("CH4  : "); Serial.print(ch4); Serial.println(" ppm");
  Serial.println();
}



void sendSensor_light() {
  lux = lightSensor.readLightLevel();

  if (lux == -1) {
    Serial.println("Gagal membaca sensor cahaya BH1750!");
    return;
  }

  Serial.print("Cahaya: ");
  Serial.print(lux);
  Serial.println(" lux");

  Blynk.virtualWrite(V5, lux);  // Kirim lux ke Virtual Pin V5

  // ======== Kontrol Otomatis LAMPU Berdasarkan Lux ==========
  if (modeOtomatis) {
    if (lux < 50) { // lampu edop dalem ruangan 75
      digitalWrite(lampu, LOW);
    } else {
      digitalWrite(lampu, HIGH);
    }
  }
}


// == == == == == FUNGSI PEMILIH MODE OTOMATIS / MANUAL == == == == ==
BLYNK_WRITE(V6) {
  int mode = param.asInt();
  modeOtomatis = (mode == 1);

  if (modeOtomatis) {
    Serial.println("MODE: OTOMATIS");
  } else {
    Serial.println("MODE: MANUAL");
    // Saat masuk mode manual, atur status sesuai kontrol manual terakhir
    digitalWrite(kipas, statusKipasManual);
    digitalWrite(lampu, statusLampuManual);
  }
}

// ========== FUNGSI SWITCH MANUAL (berfungsi jika modeOtomatis == false) ==========
BLYNK_WRITE(V7) {
  int state = param.asInt();
  statusKipasManual = (state == 1);

  if (!modeOtomatis) {
    digitalWrite(kipas, statusKipasManual);
  }
}

BLYNK_WRITE(V8) {
  int state = param.asInt();
  statusLampuManual = (state == 1);

  if (!modeOtomatis) {
    digitalWrite(lampu, statusLampuManual);
  }
}

// ------------------ UPDATE LCD ------------------
void updateLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  //  lcd.print("lpg:");
  //  lcd.print(lpg);
  lcd.print("suhu:");
  lcd.print(t);

  lcd.print("hum: ");
  lcd.print(h);


  lcd.setCursor(0, 1);
  lcd.print("lux:");
  lcd.print(lux);
  lcd.print(" ");
  lcd.print(" CO:");
  lcd.print(co);
  lcd.print(" ");
  lcd.print(modeOtomatis ? "A" : "M");

  delay(2000);



}

void sendToGoogleSheets() {
  if ((WiFi.status() == WL_CONNECTED)) {
    HTTPClient http;

    String url = String(scriptURL) +
                 "?temp=" + String(t) +
                 "&hum=" + String(h) +
                 // "&lpg=" + String(lpg) +
                 "&co=" + String(co) +
                 //"&ch4=" + String(ch4) +
                 "&lux=" + String(lux) +
                 "&lampu=" + (digitalRead(lampu) == LOW ? "ON" : "OFF") +
                 "&kipas=" + (digitalRead(kipas) == LOW ? "ON" : "OFF");
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      Serial.println("Google Sheets: " + payload);
    } else {
      Serial.println("Gagal mengirim ke Google Sheets");
    }
    http.end();
  }
//  } else {
//    Serial.println("WiFi tidak terkoneksi!");  
}