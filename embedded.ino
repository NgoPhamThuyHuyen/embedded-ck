#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <WiFiClient.h>
#include <SoftwareSerial.h>

#define BLYNK_TEMPLATE_ID "TMPL6RkbUty3y"
#define BLYNK_TEMPLATE_NAME "Motion detect"
#define BLYNK_AUTH_TOKEN "ELlMSQ2UTzdjDMfmi7iUSrPZTNegLcEY"
#define BLYNK_PRINT Serial

#define BUZZER_PIN 14     // Chân điều khiển buzzer
#define SENSOR_PIN 4      // Chân nhận tín hiệu từ cảm biến SR602
#define BUTTON_PIN 25     // Chân nhận tín hiệu từ nút nhấn
#define Motion_detected V0
#define Sensor_STR V1

// Virtual Pin cho nút bật/tắt hệ thống
#define SYSTEM_CONTROL_PIN V2 

#define HALL_SENSOR_PIN 33   // Chân đọc giá trị cảm biến Hall

bool doorClosed = true;      // Biến lưu trạng thái cửa (đóng hoặc mở)
int sensorValue = 0;          // Biến lưu giá trị cảm biến
bool systemEnabled = true;    // Trạng thái hệ thống (bật/tắt)
bool buzzerActive = false;    // Trạng thái buzzer (đang kêu hoặc không)
unsigned long buzzerStartTime = 0; // Thời điểm bắt đầu bật buzzer
bool buttonPressed = false;   // Trạng thái nút nhấn
unsigned long lastButtonPressTime = 0; // Thời gian nút nhấn cuối cùng được bấm
unsigned long debounceDelay = 200; // Thời gian debounce cho nút nhấn
unsigned long buzzerCooldownTime = 2000; // Thời gian chờ sau khi tắt buzzer

// Thông tin Wi-Fi
char ssid[] = "ThuyHuyen";       // Tên mạng Wi-Fi
char pass[] = "thuyhuyen";      // Mật khẩu Wi-Fi

// Khởi tạo LCD (địa chỉ I2C thường là 0x27 hoặc 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Khởi tạo Wi-Fi và Blynk
bool wifiConnected = false;

// Thông số bộ lọc trung bình
#define SENSOR_SAMPLES 10                // Số lần lấy mẫu để tính trung bình
int sensorReadings[SENSOR_SAMPLES] = {0}; // Mảng lưu giá trị cảm biến
int sensorIndex = 0;                     // Chỉ số của mảng
int sensorSum = 0;                       // Tổng giá trị cảm biến
int filteredSensorValue = 0;             // Giá trị cảm biến sau khi lọc


void setup() {
    Serial.begin(115200);

    // Khởi động LCD
    lcd.init();

    // Khởi tạo kết nối Wi-Fi
    WiFi.begin(ssid, pass);
    unsigned long startAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 5000) {  
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi connected");
        wifiConnected = true;
        Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass); 
    } else {
        Serial.println("WiFi connection failed.");
        wifiConnected = false;
    }

    pinMode(SENSOR_PIN, INPUT);  
    pinMode(BUZZER_PIN, OUTPUT); 
    pinMode(BUTTON_PIN, INPUT_PULLUP); 
    pinMode(HALL_SENSOR_PIN, INPUT); // Thiết lập chân cảm biến Hall là đầu vào
}

void loop() {
    if (wifiConnected) {
        Blynk.run();
    }

    // Lọc tín hiệu từ cảm biến chuyển động
    int newSensorValue = digitalRead(SENSOR_PIN);
    sensorSum -= sensorReadings[sensorIndex];
    sensorReadings[sensorIndex] = newSensorValue;
    sensorSum += newSensorValue;
    sensorIndex = (sensorIndex + 1) % SENSOR_SAMPLES;
    filteredSensorValue = sensorSum / SENSOR_SAMPLES;

    if (systemEnabled) {
        if (filteredSensorValue == HIGH && !buzzerActive) {
            digitalWrite(BUZZER_PIN, HIGH);
            buzzerActive = true;
            buzzerStartTime = millis();
            Serial.println("Phát hiện chuyển động! Buzzer KÊU.");
            lcd.setCursor(0, 0);
            lcd.print("Motion Detected");
            if (wifiConnected) {
                Blynk.virtualWrite(Motion_detected, 1);
                Blynk.virtualWrite(Sensor_STR, "Motion Detected.");
            }
        }

        if (digitalRead(BUTTON_PIN) == LOW) {
            unsigned long currentMillis = millis();
            if (currentMillis - lastButtonPressTime > debounceDelay) {
                lastButtonPressTime = currentMillis;
                buttonPressed = true;
            }
        }

        if (buzzerActive && buttonPressed) {
            digitalWrite(BUZZER_PIN, LOW);
            buzzerActive = false;
            buttonPressed = false;
            delay(buzzerCooldownTime);
            
        }

        if (buzzerActive && millis() - buzzerStartTime >= 5000) {
            digitalWrite(BUZZER_PIN, LOW);
            buzzerActive = false;
            lcd.setCursor(0, 0);
            lcd.print("Deep Sleep......");
            enterDeepSleep();
        }
    } else {
        digitalWrite(BUZZER_PIN, LOW);
        buzzerActive = false;
        if (wifiConnected) {
            Blynk.virtualWrite(Motion_detected, 0);
            Blynk.virtualWrite(Sensor_STR, "Motion Stopped");
        }
    }

    // Đọc giá trị từ cảm biến Hall
    int hallSensorValue = digitalRead(HALL_SENSOR_PIN);
    if (hallSensorValue == HIGH && doorClosed) {
    doorClosed = false;
    lcd.setCursor(0, 0);
    lcd.print("Open door.......");
    Serial.println("Cửa đã mở. Bật còi!");
    digitalWrite(BUZZER_PIN, HIGH);
    buzzerActive = true;
    buzzerStartTime = millis();
} else if (hallSensorValue == LOW && !doorClosed) {
    doorClosed = true;
    Serial.println("Cửa đã đóng. Tắt còi!");
    lcd.print("Close door......");
    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false;
    enterDeepSleep();
}

}

BLYNK_WRITE(SYSTEM_CONTROL_PIN) {
    systemEnabled = param.asInt();
    if (systemEnabled) {
        Serial.println("Hệ thống được bật.");
    } else {
        Serial.println("Hệ thống bị tắt.");
    }
}

void enterDeepSleep() {
    if (wifiConnected) {
        Blynk.virtualWrite(Motion_detected, 0);
        Blynk.virtualWrite(Sensor_STR, "Motion Stopped");
    }
    esp_sleep_enable_ext1_wakeup(
        (1ULL << SENSOR_PIN) | (1ULL << HALL_SENSOR_PIN), 
        ESP_EXT1_WAKEUP_ANY_HIGH );
    esp_deep_sleep_start();
}
