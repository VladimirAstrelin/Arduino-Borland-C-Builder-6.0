#include <Arduino.h>
#include <Wire.h>            // Для работы с I2C (дисплей подключен по I2C)
#include <Adafruit_GFX.h>    // Графическая библиотека для дисплеев
#include <Adafruit_SSD1306.h> // Библиотека для дисплея SSD1306

// ===== НАСТРОЙКИ ДИСПЛЕЯ =====
#define SCREEN_WIDTH 128     // Ширина дисплея в пикселях
#define SCREEN_HEIGHT 32     // Высота дисплея в пикселях
#define OLED_RESET -1        // Пин сброса (-1 означает не используется)
#define OLED_ADDR 0x3C  // Адрес дисплея на шине I2C

#define LED_PIN 13    // Встроенный LED на пине 13
#define BUTTON_PIN 3   // Кнопка на D3 (один контакт кнопки на D3, второй на GND)

// Создаем объект дисплея
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// =====================================================
// Переменные для НЕБЛОКИРУЮЩЕГО мигания
// =====================================================

bool blinkActive = false;
int blinkCount = 0;
int blinkMaxCount = 0;
unsigned long blinkInterval = 0;
unsigned long lastBlinkTime = 0;
bool ledState = false;

// Текущее состояние кнопки для отображения на дисплее
bool buttonPressed = false;

// =====================================================
// Функция обновления дисплея
// Вызывается при любом изменении состояния LED или кнопки
// =====================================================
void updateDisplay() {
  display.clearDisplay();

  // Строка 1: состояние LED
  display.setCursor(0, 0);
  display.print("LED STATUS: ");
  display.println(ledState ? "ON" : "OFF");

  // Строка 2: состояние кнопки
  display.setCursor(0, 20);
  display.print("BUTTON: ");
  display.println(buttonPressed ? "ON" : "OFF");

  display.display();
}

// =====================================================
// Функция setup()
// =====================================================
void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(115200);
  delay(1000);
  Serial.println("ARDUINO_READY");

  // Инициализация дисплея
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("Ошибка инициализации дисплея"));
    while (true);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // Отображаем начальное состояние
  updateDisplay();
}

// =====================================================
// Функция loop()
// =====================================================
void loop() {

  // =====================================================
  // 1. ОБРАБОТКА КОМАНД ОТ КОМПЬЮТЕРА
  // =====================================================

  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "TEST") {
      Serial.println("ARDUINO_OK");
    }

    else if (command == "LED_ON") {
      blinkActive = false;
      ledState = true;
      digitalWrite(LED_PIN, HIGH);
      Serial.println("LED_ON_OK");
      updateDisplay();
    }

    else if (command == "LED_OFF") {
      blinkActive = false;
      ledState = false;
      digitalWrite(LED_PIN, LOW);
      Serial.println("LED_OFF_OK");
      updateDisplay();
    }

    else if (command == "MODE_SLOW") {
      blinkActive = true;
      blinkCount = 0;
      blinkMaxCount = 10;
      blinkInterval = 1000;
      lastBlinkTime = millis();
      ledState = false;
      digitalWrite(LED_PIN, LOW);
      Serial.println("MODE_SLOW_OK");
      updateDisplay();
    }

    else if (command == "MODE_MIDDLE") {
      blinkActive = true;
      blinkCount = 0;
      blinkMaxCount = 20;
      blinkInterval = 500;
      lastBlinkTime = millis();
      ledState = false;
      digitalWrite(LED_PIN, LOW);
      Serial.println("MODE_MIDDLE_OK");
      updateDisplay();
    }

    else if (command == "MODE_FAST") {
      blinkActive = true;
      blinkCount = 0;
      blinkMaxCount = 40;
      blinkInterval = 100;
      lastBlinkTime = millis();
      ledState = false;
      digitalWrite(LED_PIN, LOW);
      Serial.println("MODE_FAST_OK");
      updateDisplay();
    }
  }

  // =====================================================
  // 2. НЕБЛОКИРУЮЩЕЕ МИГАНИЕ
  // =====================================================

  if (blinkActive) {
    unsigned long now = millis();

    if (now - lastBlinkTime >= blinkInterval) {
      lastBlinkTime = now;

      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
      blinkCount++;

      // Обновляем дисплей при каждом переключении LED
      updateDisplay();

      if (blinkCount >= blinkMaxCount) {
        blinkActive = false;
        ledState = false;
        digitalWrite(LED_PIN, LOW);
        updateDisplay();
      }
    }
  }

  // =====================================================
  // 3. ЧТЕНИЕ КНОПКИ D3
  // =====================================================

  static int lastButtonState = HIGH;
  static unsigned long lastButtonSendTime = 0;

  int currentButtonState = digitalRead(BUTTON_PIN);

  if (currentButtonState != lastButtonState) {
    delay(5);
    currentButtonState = digitalRead(BUTTON_PIN);

    if (currentButtonState != lastButtonState) {
      lastButtonState = currentButtonState;

      if (millis() - lastButtonSendTime > 50) {
        if (lastButtonState == LOW) {
          buttonPressed = true;
          Serial.println("B:1");
        } else {
          buttonPressed = false;
          Serial.println("B:0");
        }

        // Обновляем дисплей при изменении состояния кнопки
        updateDisplay();

        lastButtonSendTime = millis();
      }
    }
  }

  delay(5);
}