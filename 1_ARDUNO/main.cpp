#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =====================================================
// НАСТРОЙКИ — все константы в одном месте
// =====================================================

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1
#define OLED_ADDR     0x3C

#define LED_PIN       13
#define BUTTON_PIN    3

#define DEBOUNCE_MS   5      // Время антидребезга кнопки (мс)
#define BTN_SEND_MS   50     // Минимальный интервал между отправками состояния кнопки (мс)

// =====================================================
// ОБЪЕКТ ДИСПЛЕЯ
// =====================================================

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// =====================================================
// СОСТОЯНИЕ СИСТЕМЫ
// =====================================================

// --- Светодиод ---
bool ledState = false;

// --- Мигание ---
bool          blinkActive   = false;
int           blinkCount    = 0;
int           blinkMaxCount = 0;
unsigned long blinkInterval = 0;
unsigned long lastBlinkTime = 0;

// --- Кнопка ---
bool buttonPressed = false;

// --- Дисплей ---
bool displayNeedsUpdate = false;

// =====================================================
// ФУНКЦИЯ: верификация состояния пина
//
// После любой команды, которая меняет состояние железа,
// мы читаем пин обратно через digitalRead() и сравниваем
// с тем, что хотели установить.
//
// Зачем это нужно:
//   - digitalRead() читает реальный физический уровень на пине,
//     а не просто переменную в памяти программы.
//   - Если пин повреждён, закорочен, или где-то в коде
//     была ошибка конфигурации — мы это узнаем.
//   - Это правильная инженерная привычка: не доверяй,
//     а проверяй. Особенно важно при управлении реальными
//     устройствами (реле, моторы, клапаны и т.д.)
//
// Аргументы:
//   pin      — номер пина, который проверяем
//   expected — ожидаемое состояние (HIGH или LOW)
//
// Возвращает:
//   true  — пин действительно в ожидаемом состоянии
//   false — пин НЕ в ожидаемом состоянии (что-то пошло не так)
// =====================================================

bool verifyPin(int pin, int expected) {
  int actual = digitalRead(pin);  // Читаем реальный уровень с пина
  return (actual == expected);    // Сравниваем с тем, что хотели
}

// =====================================================
// ФУНКЦИЯ: обновление дисплея
// =====================================================

void updateDisplay() {
  display.clearDisplay();

  display.setCursor(0, 0);
  display.print("LED STATUS: ");
  display.println(ledState ? "ON" : "OFF");

  display.setCursor(0, 20);
  display.print("BUTTON: ");
  display.println(buttonPressed ? "ON" : "OFF");

  // Отправляем буфер на экран по I2C
  display.display();
}

// =====================================================
// ФУНКЦИЯ: запуск мигания
//
// maxCount — сколько переключений сделать
//            (-1 = мигать бесконечно)
// interval — интервал между переключениями (мс)
//
// Возвращает true если мигание успешно запущено,
// false если начальное состояние пина не подтвердилось.
// =====================================================

bool startBlink(int maxCount, unsigned long interval) {

  // Сбрасываем всё в начальное состояние перед стартом
  blinkActive   = true;
  blinkCount    = 0;
  blinkMaxCount = maxCount;
  blinkInterval = interval;
  lastBlinkTime = millis();

  // Сбрасываем ledState и физически гасим LED.
  // Мигание всегда начинается с выключенного состояния,
  // чтобы первый такт был предсказуемым: ВЫКЛ → ВКЛ → ВЫКЛ...
  ledState = false;
  digitalWrite(LED_PIN, LOW);

  // ---- ВЕРИФИКАЦИЯ СТАРТОВОГО СОСТОЯНИЯ ----
  // Убеждаемся, что LED реально выключился перед началом мигания.
  // Если пин не в LOW — что-то не так с железом, мигание лучше не запускать.
  if (!verifyPin(LED_PIN, LOW)) {
    // Не смогли установить начальное состояние — отменяем запуск
    blinkActive = false;
    return false;  // Сообщаем вызывающему коду: что-то пошло не так
  }

  displayNeedsUpdate = true;
  return true;  // Всё хорошо, мигание запущено
}

// =====================================================
// setup() — выполняется один раз при старте
// =====================================================

void setup() {
  pinMode(LED_PIN,    OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(115200);
  delay(1000);
  Serial.println("ARDUINO_READY");

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("Ошибка инициализации дисплея"));
    while (true);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  updateDisplay();
}

// =====================================================
// loop() — выполняется бесконечно
//
// Структура каждой итерации:
//   1. Обработка команд от компьютера (Serial)
//   2. Неблокирующее мигание (millis)
//   3. Антидребезг и чтение кнопки (millis)
//   4. Обновление дисплея (один раз, если нужно)
// =====================================================

void loop() {

  // =====================================================
  // 1. КОМАНДЫ ОТ КОМПЬЮТЕРА
  // =====================================================

  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    // ---------- TEST ----------
    if (command == "TEST") {
      // Простая проверка связи — верификация не нужна,
      // это не команда управления железом
      Serial.println("ARDUINO_OK");
    }

    // ---------- LED_ON ----------
    else if (command == "LED_ON") {

      blinkActive = false;  // Останавливаем мигание если было активно
      ledState    = true;
      digitalWrite(LED_PIN, HIGH);

      // ВЕРИФИКАЦИЯ: читаем пин обратно и проверяем реальное состояние.
      // verifyPin(LED_PIN, HIGH) вернёт true только если на пине
      // действительно HIGH после нашей команды.
      if (verifyPin(LED_PIN, HIGH)) {
        // Всё хорошо — пин реально в HIGH, LED горит
        Serial.println("LED_ON_OK");
      } else {
        // Что-то пошло не так — синхронизируем переменную с реальностью
        // и сообщаем компьютеру об ошибке
        ledState = false;  // Раз пин не HIGH — LED не горит, обновляем переменную
        Serial.println("LED_ON_FAIL");
      }

      displayNeedsUpdate = true;
    }

    // ---------- LED_OFF ----------
    else if (command == "LED_OFF") {

      blinkActive = false;
      ledState    = false;
      digitalWrite(LED_PIN, LOW);

      // ВЕРИФИКАЦИЯ: проверяем что пин действительно LOW
      if (verifyPin(LED_PIN, LOW)) {
        Serial.println("LED_OFF_OK");
      } else {
        // Не удалось выключить — синхронизируем переменную
        ledState = true;  // Раз пин не LOW — LED всё ещё горит
        Serial.println("LED_OFF_FAIL");
      }

      displayNeedsUpdate = true;
    }

    // ---------- MODE_SLOW ----------
    // Для режимов мигания логика верификации двухэтапная:
    //
    // Этап 1 (здесь): startBlink() проверяет что начальное
    //   состояние пина (LOW) установлено успешно.
    //   Ответ "_OK" или "_FAIL" говорит о том, запустилось ли мигание.
    //
    // Этап 2 (в блоке мигания): когда мигание завершится,
    //   отправляем "BLINK_DONE" — это подтверждение что
    //   все N миганий были выполнены до конца.
    //   Если мигание прервётся другой командой — "BLINK_DONE"
    //   отправлен не будет, и компьютер это заметит.

    else if (command == "MODE_SLOW") {
      if (startBlink(10, 1000)) {
        // startBlink вернул true — начальное состояние подтверждено,
        // мигание запущено. Компьютер знает что процесс начался.
        Serial.println("MODE_SLOW_OK");
      } else {
        // startBlink вернул false — не удалось установить начальное состояние
        Serial.println("MODE_SLOW_FAIL");
      }
    }

    else if (command == "MODE_MIDDLE") {
      if (startBlink(20, 500)) {
        Serial.println("MODE_MIDDLE_OK");
      } else {
        Serial.println("MODE_MIDDLE_FAIL");
      }
    }

    else if (command == "MODE_FAST") {
      if (startBlink(40, 100)) {
        Serial.println("MODE_FAST_OK");
      } else {
        Serial.println("MODE_FAST_FAIL");
      }
    }
  }

  // =====================================================
  // 2. НЕБЛОКИРУЮЩЕЕ МИГАНИЕ
  // =====================================================

  if (blinkActive) {
    unsigned long now = millis();

    if (now - lastBlinkTime >= blinkInterval) {
      lastBlinkTime = now;

      // Переключаем состояние
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);

      // ВЕРИФИКАЦИЯ КАЖДОГО ПЕРЕКЛЮЧЕНИЯ:
      // Проверяем что пин реально переключился в нужное состояние.
      // Если нет — останавливаем мигание и сообщаем об ошибке.
      // Это важно: при управлении реальным устройством сбой
      // на середине цикла должен быть замечен немедленно.
      int expectedLevel = ledState ? HIGH : LOW;
      if (!verifyPin(LED_PIN, expectedLevel)) {
        // Переключение не прошло — аварийная остановка мигания
        blinkActive = false;
        ledState    = false;
        digitalWrite(LED_PIN, LOW);
        displayNeedsUpdate = true;
        Serial.println("BLINK_PIN_FAIL");  // Сообщаем компьютеру о сбое
        return;  // Прерываем текущую итерацию loop()
      }

      blinkCount++;
      displayNeedsUpdate = true;

      // Проверяем завершение мигания
      if (blinkMaxCount != -1 && blinkCount >= blinkMaxCount) {
        blinkActive = false;
        ledState    = false;
        digitalWrite(LED_PIN, LOW);
        displayNeedsUpdate = true;

        // ЭТАП 2 ВЕРИФИКАЦИИ для режимов мигания:
        // Отправляем "BLINK_DONE" только когда мигание реально
        // завершилось — все N переключений выполнены, LED погашен.
        // Компьютер получит этот ответ через некоторое время после
        // MODE_*_OK, и будет точно знать момент завершения процесса.
        if (verifyPin(LED_PIN, LOW)) {
          Serial.println("BLINK_DONE");       // Мигание завершено успешно
        } else {
          Serial.println("BLINK_END_FAIL");   // Мигание завершилось, но LED не погас
        }
      }
    }
  }

  // =====================================================
  // 3. АНТИДРЕБЕЗГ И ЧТЕНИЕ КНОПКИ
  // =====================================================

  static int           lastStableState = HIGH;
  static int           rawState        = HIGH;
  static unsigned long debounceStart   = 0;
  static unsigned long lastSendTime    = 0;

  int currentRaw = digitalRead(BUTTON_PIN);

  // Если сигнал изменился — начинаем отсчёт антидребезга заново
  if (currentRaw != rawState) {
    rawState      = currentRaw;
    debounceStart = millis();
  }

  // Если сигнал стабилен уже DEBOUNCE_MS мс — это настоящее нажатие
  if (millis() - debounceStart >= DEBOUNCE_MS && currentRaw != lastStableState) {
    lastStableState = currentRaw;

    if (millis() - lastSendTime >= BTN_SEND_MS) {
      lastSendTime = millis();

      if (lastStableState == LOW) {
        buttonPressed = true;
        Serial.println("B:1");
      } else {
        buttonPressed = false;
        Serial.println("B:0");
      }

      displayNeedsUpdate = true;
    }
  }

  // =====================================================
  // 4. ОБНОВЛЕНИЕ ДИСПЛЕЯ
  // =====================================================

  if (displayNeedsUpdate) {
    updateDisplay();
    displayNeedsUpdate = false;
  }

  delay(1);
}
