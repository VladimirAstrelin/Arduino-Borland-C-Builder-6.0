#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =====================================================
// НАСТРОЙКИ — все константы в одном месте
// Если нужно что-то поменять, меняем только здесь
// =====================================================

#define SCREEN_WIDTH  128    // Ширина дисплея в пикселях
#define SCREEN_HEIGHT 32     // Высота дисплея в пикселях
#define OLED_RESET    -1     // Пин сброса (-1 = не используется)
#define OLED_ADDR     0x3C   // Адрес дисплея на шине I2C

#define LED_PIN       13     // Встроенный светодиод
#define BUTTON_PIN    3      // Кнопка (D3 → GND, внутренняя подтяжка к +5V)

#define DEBOUNCE_MS   5      // Время антидребезга кнопки в миллисекундах
#define BTN_SEND_MS   50     // Минимальный интервал между отправками состояния кнопки

// =====================================================
// ОБЪЕКТ ДИСПЛЕЯ
// =====================================================

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// =====================================================
// СОСТОЯНИЕ СИСТЕМЫ
// Все переменные состояния сгруппированы вместе —
// так проще понять "что сейчас происходит" одним взглядом
// =====================================================

// --- Светодиод ---
bool ledState       = false;   // Текущее состояние LED: true = горит, false = не горит

// --- Мигание ---
bool blinkActive    = false;   // Активен ли режим мигания прямо сейчас
int  blinkCount     = 0;       // Сколько переключений уже сделано
int  blinkMaxCount  = 0;       // Сколько переключений нужно сделать всего
                                // (-1 = мигать бесконечно, пока не придёт другая команда)
unsigned long blinkInterval  = 0;  // Пауза между переключениями (мс)
unsigned long lastBlinkTime  = 0;  // Время последнего переключения (мс от старта)

// --- Кнопка ---
bool buttonPressed       = false;  // Текущее состояние кнопки: true = нажата

// --- Дисплей ---
bool displayNeedsUpdate  = false;  // Флаг: нужно ли перерисовать дисплей?
                                    // Вместо вызова updateDisplay() напрямую мы ставим этот флаг,
                                    // и дисплей обновляется один раз в конце каждого цикла loop().
                                    // Это важно для MODE_FAST: без флага дисплей обновлялся бы
                                    // 10+ раз в секунду прямо внутри логики мигания, что тормозит I2C.

// =====================================================
// ФУНКЦИЯ: обновление дисплея
// Вызывается ТОЛЬКО через флаг displayNeedsUpdate,
// один раз за итерацию loop() — не чаще
// =====================================================

void updateDisplay() {
  display.clearDisplay();

  // Строка 1 (y=0): состояние светодиода
  display.setCursor(0, 0);
  display.print("LED STATUS: ");
  display.println(ledState ? "ON" : "OFF");

  // Строка 2 (y=20): состояние кнопки
  display.setCursor(0, 20);
  display.print("BUTTON: ");
  display.println(buttonPressed ? "ON" : "OFF");

  // display.display() — только эта команда реально отправляет буфер на экран по I2C.
  // Все предыдущие команды рисуют только в памяти Arduino (буфер).
  display.display();
}

// =====================================================
// ВСПОМОГАТЕЛЬНАЯ ФУНКЦИЯ: запуск мигания
// Вызывается из обработчика команд, чтобы не дублировать код
// =====================================================

void startBlink(int maxCount, unsigned long interval) {
  blinkActive   = true;
  blinkCount    = 0;
  blinkMaxCount = maxCount;
  blinkInterval = interval;
  lastBlinkTime = millis();   // Запоминаем текущее время как точку отсчёта
  ledState      = false;
  digitalWrite(LED_PIN, LOW);
  displayNeedsUpdate = true;
}

// =====================================================
// setup() — выполняется один раз при старте
// =====================================================

void setup() {
  // Настройка пинов
  pinMode(LED_PIN,    OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);  // Внутренний резистор подтягивает пин к +5V:
                                       // кнопка не нажата → HIGH, нажата → LOW
  digitalWrite(LED_PIN, LOW);          // Гасим LED при старте

  // Инициализация Serial
  Serial.begin(115200);
  delay(1000);                         // Даём время USB-соединению установиться
  Serial.println("ARDUINO_READY");

  // Инициализация дисплея
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("Ошибка инициализации дисплея"));
    while (true);  // Останавливаем программу — без дисплея работать нет смысла
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // Отображаем начальное состояние
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
    command.trim();  // Убираем \r, пробелы по краям

    if (command == "TEST") {
      Serial.println("ARDUINO_OK");
    }

    else if (command == "LED_ON") {
      blinkActive = false;       // Останавливаем мигание, если было активно
      ledState    = true;
      digitalWrite(LED_PIN, HIGH);
      Serial.println("LED_ON_OK");
      displayNeedsUpdate = true; // Запрашиваем перерисовку дисплея
    }

    else if (command == "LED_OFF") {
      blinkActive = false;
      ledState    = false;
      digitalWrite(LED_PIN, LOW);
      Serial.println("LED_OFF_OK");
      displayNeedsUpdate = true;
    }

    // Для режимов мигания используем вспомогательную функцию startBlink().
    // Аргументы: количество переключений и интервал в мс.
    // Количество переключений = количество миганий × 2
    // (каждое мигание = включить + выключить = 2 переключения)

    else if (command == "MODE_SLOW") {
      startBlink(10, 1000);      // 5 миганий, раз в секунду
      Serial.println("MODE_SLOW_OK");
    }

    else if (command == "MODE_MIDDLE") {
      startBlink(20, 500);       // 10 миганий, раз в полсекунды
      Serial.println("MODE_MIDDLE_OK");
    }

    else if (command == "MODE_FAST") {
      startBlink(40, 100);       // 20 миганий, 10 раз в секунду
      Serial.println("MODE_FAST_OK");
    }
  }

  // =====================================================
  // 2. НЕБЛОКИРУЮЩЕЕ МИГАНИЕ
  //
  // Мы НЕ используем delay() — Arduino не "засыпает".
  // Вместо этого каждую итерацию loop() проверяем:
  // "прошло ли достаточно времени с последнего переключения?"
  // Если да — переключаем. Если нет — идём дальше.
  // Благодаря этому Serial и кнопка работают без задержек.
  // =====================================================

  if (blinkActive) {
    unsigned long now = millis();  // Текущее время в мс от старта Arduino

    // Проверяем: прошёл ли нужный интервал?
    if (now - lastBlinkTime >= blinkInterval) {
      lastBlinkTime = now;            // Запоминаем момент переключения

      ledState = !ledState;           // Инвертируем состояние (true→false, false→true)
      digitalWrite(LED_PIN, ledState);
      blinkCount++;
      displayNeedsUpdate = true;      // Запрашиваем обновление дисплея

      // Проверяем: выполнили ли нужное количество переключений?
      // blinkMaxCount == -1 означает "мигать бесконечно"
      if (blinkMaxCount != -1 && blinkCount >= blinkMaxCount) {
        blinkActive = false;
        ledState    = false;
        digitalWrite(LED_PIN, LOW);
        displayNeedsUpdate = true;
      }
    }
  }

  // =====================================================
  // 3. АНТИДРЕБЕЗГ И ЧТЕНИЕ КНОПКИ
  //
  // Проблема дребезга: при нажатии кнопки контакты физически
  // "прыгают" несколько раз за ~5 мс, создавая ложные срабатывания.
  //
  // Решение через millis() (без delay!):
  // Фиксируем момент когда сигнал изменился, и принимаем новое
  // состояние только если оно стабильно на протяжении DEBOUNCE_MS.
  // =====================================================

  // static — переменная живёт всё время работы программы,
  // но видна только внутри loop(). Значение сохраняется между вызовами.

  static int           lastStableState  = HIGH;  // Последнее подтверждённое состояние кнопки
  static int           rawState         = HIGH;  // Последнее "сырое" считанное состояние
  static unsigned long debounceStart    = 0;     // Момент когда сигнал начал меняться
  static unsigned long lastSendTime     = 0;     // Когда последний раз отправляли B:0/B:1

  int currentRaw = digitalRead(BUTTON_PIN);      // Читаем текущий сигнал с пина

  // Если сигнал изменился — начинаем отсчёт антидребезга заново
  if (currentRaw != rawState) {
    rawState      = currentRaw;
    debounceStart = millis();
  }

  // Если сигнал стабилен уже DEBOUNCE_MS миллисекунд — это настоящее нажатие/отпускание
  if (millis() - debounceStart >= DEBOUNCE_MS && currentRaw != lastStableState) {
    lastStableState = currentRaw;

    // Отправляем состояние не чаще одного раза в BTN_SEND_MS мс
    if (millis() - lastSendTime >= BTN_SEND_MS) {
      lastSendTime = millis();

      if (lastStableState == LOW) {   // LOW = кнопка нажата (пин соединён с GND)
        buttonPressed = true;
        Serial.println("B:1");
      } else {                         // HIGH = кнопка отпущена (подтяжка к +5V)
        buttonPressed = false;
        Serial.println("B:0");
      }

      displayNeedsUpdate = true;
    }
  }

  // =====================================================
  // 4. ОБНОВЛЕНИЕ ДИСПЛЕЯ
  //
  // Обновляем дисплей только если что-то изменилось (флаг = true).
  // Один вызов за итерацию — не важно, сколько событий произошло выше.
  // Это предотвращает лишние медленные операции записи по I2C.
  // =====================================================

  if (displayNeedsUpdate) {
    updateDisplay();
    displayNeedsUpdate = false;  // Сбрасываем флаг до следующего изменения
  }

  // Небольшая пауза чтобы не гонять loop() вхолостую тысячи раз в секунду.
  // НЕ влияет на точность мигания и отклик кнопки — мы всё равно
  // проверяем millis(), а не считаем итерации.
  delay(1);
}
