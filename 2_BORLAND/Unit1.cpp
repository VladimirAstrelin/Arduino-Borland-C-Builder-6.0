//---------------------------------------------------------------------------
// Подключаем основные библиотеки
#include <vcl.h>           // Визуальные компоненты C++ Builder
#pragma hdrstop            // Директива компилятора - стоп заголовочным файлам

#include "Unit1.h"         // Наш заголовочный файл (описание формы)
#include <windows.h>       // Windows API - функции для работы с портами, файлами
#include <registry.hpp>    // Работа с реестром Windows (список COM-портов)

#pragma package(smart_init)   // Оптимизация компиляции
#pragma resource "*.dfm"      // Подключаем файл с описанием формы

// Глобальная переменная Form1 - теперь она существует
TForm1 *Form1;

//---------------------------------------------------------------------------
// ============================================================
// ФУНКЦИЯ ПРОВЕРКИ ARDUINO
// Что делает: отправляет команду TEST и ждёт ответ ARDUINO_OK
// ============================================================
bool CheckArduino(HANDLE hPort)  // hPort - дескриптор открытого COM-порта
{
    // Если порт не открыт (дескриптор неправильный) - сразу выходим
    if (hPort == INVALID_HANDLE_VALUE)
        return false;
    
    // Команда для отправки. \n - символ новой строки, Arduino ждёт его
    char testCmd[] = "TEST\n";
    DWORD bytesWritten;  // Сюда запишется, сколько байт реально отправили
    
    // Пробуем 3 раза, вдруг Arduino занята или не сразу ответит
    for (int attempt = 0; attempt < 3; attempt++)
    {
        // WriteFile - отправляем данные в порт
        if (!WriteFile(hPort, testCmd, strlen(testCmd), &bytesWritten, NULL))
        {
            Sleep(100);  // Не получилось - ждём 100 мс и пробуем снова
            continue;
        }
        
        // ЖДЁМ ОТВЕТА 300 мс - Arduino нужно время на обработку
        Sleep(300);
        
        // Буфер для ответа от Arduino
        char buffer[100] = {0};  // {0} - заполняем весь массив нулями
        DWORD bytesRead;          // Сколько байт прочитали
        
        // Читаем ответ из порта
        if (ReadFile(hPort, buffer, sizeof(buffer)-1, &bytesRead, NULL))
        {
            // Если что-то прочитали (bytesRead > 0)
            if (bytesRead > 0)
            {
                // Ставим нуль в конце строки - в C++ это конец текста
                buffer[bytesRead] = '\0';
                
                // Превращаем массив символов в удобную строку String
                String response = String(buffer);
                
                // Trim() - обрезаем пробелы и переносы строк
                response = response.Trim();
                
                // Если ответ "ARDUINO_OK" - это наша Arduino!
                if (response == "ARDUINO_OK")
                {
                    return true;  // Ура!
                }
            }
        }
        
        // Очищаем буфер порта перед следующей попыткой
        PurgeComm(hPort, PURGE_RXCLEAR | PURGE_TXCLEAR);
        Sleep(100);  // Ждём перед повторной попыткой
    }
    
    // После трёх попыток так и не дождались ответа - нет Arduino
    return false;
}

//---------------------------------------------------------------------------
// ============================================================
// КОНСТРУКТОР ФОРМЫ - выполняется при создании окна программы
// ============================================================
__fastcall TForm1::TForm1(TComponent* Owner)
        : TForm(Owner)  // Вызываем конструктор родительского класса
{
    // --- Инициализация переменных (задаём начальные значения) ---
    
    hCom = INVALID_HANDLE_VALUE;  // Пока порт не открыт
    connected = false;              // Ещё не подключены
    lastButtonState = false;        // Кнопка не нажата (по умолчанию)

    // --- Настройка интерфейса (что видит пользователь) ---
    
    // Заголовок окна
    Caption = "Arduino Control v1.0";
    
    // Надпись статуса подключения
    LBL_CONNECTION_STATUS->Caption = "DISCONNECTED";
    
    // Статус светодиода
    LBL_LED_STATUS->Caption = "OFF";
    
    // Переключатели по умолчанию: OFF и SLOW
    RAD_BTN_OFF->Checked = true;
    RAD_BTN_SLOW->Checked = true;
    
    // Кнопка D3 - начальное состояние: отпущена, цвет жёлтый
    LBL_BTN_D3_STATUS->Caption = "D3 BUTTON: RELEASED";
    SH_BTN_D3_COLOR->Brush->Color = clYellow;  // Жёлтый
    SH_BTN_D3_COLOR->Shape = stCircle;          // Круг
    SH_BTN_D3_COLOR->Width = 30;                 // Ширина 30 пикселей
    SH_BTN_D3_COLOR->Height = 30;                // Высота 30 пикселей

    // Переменные для защиты от повторных команд
    commandPending = false;   // Никакая команда не выполняется
    lastCommand = "";         // Последняя команда - пустая
    commandStartTime = 0;     // Время отправки = 0
}

//---------------------------------------------------------------------------
// ============================================================
// СОЗДАНИЕ ФОРМЫ - ЗАПОЛНЯЕМ СПИСОК COM-ПОРТОВ
// Вызывается автоматически при создании формы
// ============================================================
void __fastcall TForm1::FormCreate(TObject *Sender)
{
    // Очищаем выпадающий список
    CMB_COM_PORT->Clear();
    
    // Работа с реестром Windows - там хранятся все COM-порты
    // new - создаём новый объект в памяти
    TRegistry *reg = new TRegistry();
    
    // RootKey - корневой ключ реестра
    // HKEY_LOCAL_MACHINE - информация о компьютере
    reg->RootKey = HKEY_LOCAL_MACHINE;
    
    // Пытаемся открыть ключ, где лежат COM-порты
    // OpenKeyReadOnly - открыть только для чтения
    if (reg->OpenKeyReadOnly("HARDWARE\\DEVICEMAP\\SERIALCOMM"))
    {
        // Создаём список для хранения имён значений
        TStringList *values = new TStringList();
        
        // GetValueNames - получаем все имена значений в этом ключе
        reg->GetValueNames(values);
        
        // Перебираем все найденные значения
        // values->Count - сколько значений нашли
        for (int i = 0; i < values->Count; i++)
        {
            // ReadString - читаем строковое значение
            // Это имя порта, например "COM13"
            String portName = reg->ReadString(values->Strings[i]);
            
            // Pos("COM") - проверяем, есть ли в строке "COM"
            // Если есть и она в начале (Pos == 1) - это COM-порт
            if (portName.Pos("COM") == 1)
            {
                // Добавляем порт в выпадающий список
                CMB_COM_PORT->Items->Add(portName);
            }
        }
        
        // Освобождаем память, удаляя ненужные объекты
        delete values;
        reg->CloseKey();  // Закрываем ключ реестра
    }
    
    // Удаляем объект реестра
    delete reg;
    
    // Если не нашли ни одного порта
    if (CMB_COM_PORT->Items->Count == 0)
    {
        // Добавляем заглушку "Порты не найдены"
        CMB_COM_PORT->Items->Add("No COM ports found");
    }
    
    // Выбираем первый порт в списке (по умолчанию)
    CMB_COM_PORT->ItemIndex = 0;
}

//---------------------------------------------------------------------------
// ============================================================
// ОТПРАВКА КОМАНДЫ ARDUINO
// Что делает: берёт команду и отправляет её в порт
// ============================================================
void TForm1::SendCommand(String command)
{
    // Проверяем: подключены ли мы и открыт ли порт
    if (!connected || hCom == INVALID_HANDLE_VALUE)
    {
        // Если нет - показываем окошко с ошибкой
        MessageBox(0, "Not connected to Arduino!", "Connection Error", MB_OK);
        return;  // Выходим из функции
    }
    
    // Если предыдущая команда ещё не получила ответ - не отправляем новую
    // Это защита от того, чтобы не завалить Arduino кучей команд
    if (commandPending) {
        SB_MAIN_STATUS_BAR->SimpleText = "Busy, waiting for response...";
        return;  // Выходим, не отправляя команду
    }
    
    // Arduino ждёт команду с символом \n в конце
    command = command + "\n";
    DWORD bytesWritten;  // Сколько байт реально отправили
    
    // WriteFile - отправляем данные в порт
    if (WriteFile(hCom, command.c_str(), command.Length(), &bytesWritten, NULL))
    {
        // Если отправили успешно:
        commandPending = true;          // Теперь ждём ответ
        lastCommand = command;          // Запоминаем команду
        commandStartTime = GetTickCount(); // Запоминаем время отправки
    }
}

//---------------------------------------------------------------------------
// ============================================================
// ОБНОВЛЕНИЕ ИНТЕРФЕЙСА - КНОПКА D3
// Меняет цвет и текст в зависимости от состояния кнопки
// ============================================================
void TForm1::UpdateButtonUI(bool pressed)
{
    // Если кнопка нажата (pressed == true)
    if (pressed)
    {
        // Меняем текст на "PRESSED"
        LBL_BTN_D3_STATUS->Caption = "D3 BUTTON: PRESSED";
        // Делаем текст красным
        LBL_BTN_D3_STATUS->Font->Color = clRed;
        // Закрашиваем кружок красным
        SH_BTN_D3_COLOR->Brush->Color = clRed;
    }
    else  // Если кнопка отпущена
    {
        // Меняем текст на "RELEASED"
        LBL_BTN_D3_STATUS->Caption = "D3 BUTTON: RELEASED";
        // Делаем текст чёрным
        LBL_BTN_D3_STATUS->Font->Color = clBlack;
        // Закрашиваем кружок жёлтым
        SH_BTN_D3_COLOR->Brush->Color = clYellow;
    }
    
    // Запоминаем новое состояние
    lastButtonState = pressed;
}

//---------------------------------------------------------------------------
// ============================================================
// ПАРСИНГ ДАННЫХ ОТ ARDUINO
// Разбирает строки, которые пришли от Arduino
// ============================================================
void TForm1::ParseArduinoData(String data)
{
    // Убираем лишние пробелы и переносы строк
    data = data.Trim();
    
    // Если строка пустая - нечего обрабатывать
    if (data.IsEmpty()) return;
    
    // Проверяем, не является ли это ответом на нашу команду
    // Если да - сбрасываем флаг commandPending
    if (data == "LED_ON_OK" || data == "LED_OFF_OK" || 
        data == "MODE_SLOW_OK" || data == "MODE_MIDDLE_OK" || data == "MODE_FAST_OK") {
        commandPending = false;  // Команда выполнена!
    }
    
    // Формат данных от кнопки: B:1 или B:0
    // data.Length() - длина строки
    // data[1] - первый символ (в C++ Builder индексация с 1!)
    if (data.Length() >= 3 && data[1] == 'B' && data[2] == ':')
    {
        // Третий символ - '1' или '0'
        bool pressed = (data[3] == '1');
        
        // Обновляем интерфейс
        UpdateButtonUI(pressed);
        
        // Показываем в статусной строке
        SB_MAIN_STATUS_BAR->SimpleText = "Button: " + String(pressed ? "PRESSED" : "released");
    }
    else
    {
        // Другие ответы от Arduino (например, ARDUINO_READY)
        SB_MAIN_STATUS_BAR->SimpleText = data;
    }
}

//---------------------------------------------------------------------------
// ============================================================
// ТАЙМЕР - ЧТЕНИЕ ДАННЫХ ИЗ COM-ПОРТА
// Срабатывает каждые 50 миллисекунд
// ============================================================
void __fastcall TForm1::TimerReadComTimer(TObject *Sender)
{
    // Если не подключены - ничего не делаем
    if (!connected || hCom == INVALID_HANDLE_VALUE)
        return;
    
    // Буфер для чтения данных
    char buffer[256];
    DWORD bytesRead;
    
    // ReadFile - читаем данные из порта
    // С быстрыми таймаутами эта функция возвращается мгновенно
    if (ReadFile(hCom, buffer, sizeof(buffer) - 1, &bytesRead, NULL))
    {
        // Если что-то прочитали
        if (bytesRead > 0)
        {
            // Добавляем нуль в конец - это конец строки в C
            buffer[bytesRead] = '\0';
            
            // Превращаем в строку String
            String data = String(buffer);
            
            // Разбиваем на отдельные строки (может прийти несколько сразу)
            TStringList *lines = new TStringList();
            lines->Text = data;  // Текст автоматически разбивается на строки
            
            // Обрабатываем каждую строку
            for (int i = 0; i < lines->Count; i++)
            {
                String line = lines->Strings[i].Trim();
                if (!line.IsEmpty())  // Если строка не пустая
                {
                    ParseArduinoData(line);  // Разбираем её
                }
            }
            
            // Удаляем список, освобождаем память
            delete lines;
        }
    }
    else
    {
        // Ошибка чтения - возможно порт закрылся
        DWORD err = GetLastError();
        
        // Если порт сломался или дескриптор недействителен
        if (err == ERROR_BROKEN_PIPE || err == ERROR_INVALID_HANDLE)
        {
            // Отключаемся
            BTN_DISCONNECTClick(NULL);
        }
    }

    // Проверка таймаута команды (5 секунд)
    // Если команда была отправлена, но ответ не пришёл больше 5 секунд
    if (commandPending && (GetTickCount() - commandStartTime > 5000)) {
        commandPending = false;  // Сбрасываем флаг
        SB_MAIN_STATUS_BAR->SimpleText = "Command timeout";
    }
}

//---------------------------------------------------------------------------
// ============================================================
// КНОПКА REFRESH - ОБНОВИТЬ СПИСОК ПОРТОВ
// ============================================================
void __fastcall TForm1::BTN_REFRESHClick(TObject *Sender)
{
    // Вызываем функцию FormCreate заново
    FormCreate(Sender);
    
    // Сообщаем пользователю
    MessageBox(0, "COM port list refreshed", "COM Port list", MB_OK);
    SB_MAIN_STATUS_BAR->SimpleText = "COM port list refreshed";
}

//---------------------------------------------------------------------------
// ============================================================
// КНОПКА CONNECT - ПОДКЛЮЧИТЬСЯ (САМАЯ ВАЖНАЯ ЧАСТЬ!)
// ============================================================
void __fastcall TForm1::BTN_CONNECTClick(TObject *Sender)
{
    // Если уже подключены - не даём подключиться снова
    if (connected)
    {
        MessageBox(0, "Already connected!", "Connection Status", MB_OK);
        return;
    }

    // --- Получаем выбранный порт из списка ---
    String portName = "COM3";  // Значение по умолчанию
    if (CMB_COM_PORT->ItemIndex >= 0)  // Если что-то выбрано
    {
        // Берём текст из выпадающего списка
        portName = CMB_COM_PORT->Items->Strings[CMB_COM_PORT->ItemIndex];
    }
    
    // Проверка на заглушку "нет портов"
    if (portName == "No COM ports found")
    {
        MessageBox(0, "No COM ports available!", "Connection Error", MB_OK);
        return;
    }
    
    // --- Формируем имя порта для Windows ---
    // Проблема: Windows по-разному работает с COM1-COM9 и COM10+
    // Для COM10+ нужно специальное имя: "\\.\COM13"
    String comPort;
    if (portName.Pos("COM") == 1)  // Если имя начинается с "COM"
    {
        // Вырезаем номер порта: из "COM13" берём "13"
        String numStr = portName.SubString(4, portName.Length() - 3);
        // Превращаем строку в число
        int portNum = StrToIntDef(numStr, 0);
        
        if (portNum > 9)
            comPort = "\\\\.\\" + portName;  // Для COM10+ добавляем префикс
        else
            comPort = portName;              // Для COM1-COM9 оставляем как есть
    }
    else
    {
        comPort = portName;  // Если имя нестандартное, оставляем
    }
    
    // --- ОТКРЫВАЕМ COM-ПОРТ ---
    // CreateFile - универсальная функция Windows для открытия файлов и устройств
    // COM-порт - это тоже устройство, поэтому открываем через CreateFile
    hCom = CreateFile(
            comPort.c_str(),                // Имя порта (строка в стиле C)
            GENERIC_READ | GENERIC_WRITE,   // Разрешаем и чтение, и запись
            0,                              // Не разрешаем другим программам доступ
            NULL,                            // Безопасность по умолчанию
            OPEN_EXISTING,                    // Открываем существующий порт
            FILE_ATTRIBUTE_NORMAL,            // Обычные атрибуты
            NULL                               // Без шаблона
    );

    // Проверяем, удалось ли открыть порт
    if (hCom == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();  // Узнаём код ошибки
        String errorMsg = "Unable to open " + portName + ".\n";
        
        // Объясняем ошибку понятным языком
        if (error == ERROR_ACCESS_DENIED)
            errorMsg += "Port is busy (close Arduino IDE!)";
        else if (error == ERROR_FILE_NOT_FOUND)
            errorMsg += "Port not found";
        else
            errorMsg += "Error code: " + IntToStr((int)error);
            
        ShowMessage(errorMsg);  // Показываем сообщение
        return;
    }

    // --- НАСТРАИВАЕМ ПАРАМЕТРЫ ПОРТА (скорость, биты данных и т.д.) ---
    // DCB - Device Control Block, структура с настройками
    DCB dcbSerialParams = {0};  // Обнуляем структуру
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);  // Указываем размер
    
    // GetCommState - получаем текущие настройки порта
    if (!GetCommState(hCom, &dcbSerialParams))
    {
        CloseHandle(hCom);  // Закрываем порт
        hCom = INVALID_HANDLE_VALUE;
        MessageBox(0, "Error getting port state", "Port state", MB_OK);
        return;
    }

    // Устанавливаем параметры как у Arduino
    dcbSerialParams.BaudRate = CBR_115200;  // Скорость 115200 бит/с
    dcbSerialParams.ByteSize = 8;           // 8 бит данных
    dcbSerialParams.StopBits = ONESTOPBIT;  // 1 стоп-бит
    dcbSerialParams.Parity = NOPARITY;      // Без проверки четности

    // SetCommState - применяем новые настройки
    if (!SetCommState(hCom, &dcbSerialParams))
    {
        CloseHandle(hCom);
        hCom = INVALID_HANDLE_VALUE;
        MessageBox(0, "Error applying port settings", "Port settings", MB_OK);
        return;
    }

    // ========== ЭТАП 1: МЕДЛЕННЫЕ ТАЙМАУТЫ ДЛЯ ПРОВЕРКИ ==========
    // Сначала ставим таймауты, которые ЖДУТ ответа (до 2.6 секунды)
    COMMTIMEOUTS checkTimeouts;
    checkTimeouts.ReadIntervalTimeout = 50;          // Ждать 50 мс между байтами
    checkTimeouts.ReadTotalTimeoutConstant = 50;     // Базовое время 50 мс
    checkTimeouts.ReadTotalTimeoutMultiplier = 10;   // +10 мс на каждый байт
    checkTimeouts.WriteTotalTimeoutConstant = 50;    // Таймаут записи
    checkTimeouts.WriteTotalTimeoutMultiplier = 10;

    if (!SetCommTimeouts(hCom, &checkTimeouts))
    {
        CloseHandle(hCom);
        hCom = INVALID_HANDLE_VALUE;
        MessageBox(0, "Error setting check timeouts", "Port timeouts", MB_OK);
        return;
    }

    // Очищаем буферы порта
    PurgeComm(hCom, PURGE_RXCLEAR | PURGE_TXCLEAR);

    // --- Проверяем, что это действительно Arduino ---
    if (!CheckArduino(hCom))
    {
        CloseHandle(hCom);
        hCom = INVALID_HANDLE_VALUE;
        ShowMessage("Arduino not detected on " + portName + "!\n"
                   "Make sure:\n"
                   "1. Correct COM port\n"
                   "2. Arduino is powered\n"
                   "3. Sketch is running");
        return;
    }

    // ========== ЭТАП 2: БЫСТРЫЕ ТАЙМАУТЫ ДЛЯ РАБОТЫ ==========
    // Arduino подтверждена, теперь ставим таймауты, которые НЕ ЖДУТ
    // ReadFile будет возвращаться мгновенно, интерфейс не тормозит
    COMMTIMEOUTS workTimeouts;
    workTimeouts.ReadIntervalTimeout = 0xFFFFFFFF;  // MAXDWORD - особое значение
    workTimeouts.ReadTotalTimeoutConstant = 0;       // Не ждём вообще
    workTimeouts.ReadTotalTimeoutMultiplier = 0;     // Не ждём вообще
    workTimeouts.WriteTotalTimeoutConstant = 50;     // Таймаут записи оставляем
    workTimeouts.WriteTotalTimeoutMultiplier = 10;

    if (!SetCommTimeouts(hCom, &workTimeouts))
    {
        // Если не получилось - предупреждаем, но не отключаемся
        MessageBox(0, "Warning: Could not set fast timeouts", "Warning", MB_OK);
    }

    // Ещё раз очищаем буфер
    PurgeComm(hCom, PURGE_RXCLEAR | PURGE_TXCLEAR);

    // --- УСПЕХ! ВСЁ РАБОТАЕТ! ---
    connected = true;  // Ставим флаг подключения
    LBL_CONNECTION_STATUS->Caption = "CONNECTED to " + portName;  // Обновляем надпись
    
    // --- ЗАПУСКАЕМ ТАЙМЕР ЧТЕНИЯ ---
    TimerReadCom->Enabled = true;
    
    // Сообщаем об успехе
    MessageBox(0, ("Successfully connected to Arduino on " + portName).c_str(),
               "Connection status", MB_OK);
    SB_MAIN_STATUS_BAR->SimpleText = "Connected to Arduino";
}

//---------------------------------------------------------------------------
// ============================================================
// КНОПКА DISCONNECT - ОТКЛЮЧИТЬСЯ
// ============================================================
void __fastcall TForm1::BTN_DISCONNECTClick(TObject *Sender)
{
    // Если не подключены - выходим
    if (!connected) 
    {
        MessageBox(0, "Not connected!", "Connection status", MB_OK);
        return;
    }

    // --- ОСТАНАВЛИВАЕМ ТАЙМЕР ---
    TimerReadCom->Enabled = false;

    // Если порт открыт
    if (hCom != INVALID_HANDLE_VALUE)
    {
        // Выключаем светодиод перед отключением (вежливость)
        SendCommand("LED_OFF");
        Sleep(50);  // Ждём 50 мс, чтобы команда точно ушла
        
        // Закрываем порт
        CloseHandle(hCom);
        hCom = INVALID_HANDLE_VALUE;
    }

    // Сбрасываем все настройки
    connected = false;
    LBL_CONNECTION_STATUS->Caption = "DISCONNECTED";
    
    // Возвращаем переключатели в исходное состояние
    RAD_BTN_OFF->Checked = true;
    RAD_BTN_SLOW->Checked = true;
    LBL_LED_STATUS->Caption = "OFF";
    
    MessageBox(0, "Disconnected", "Connection status", MB_OK);
    SB_MAIN_STATUS_BAR->SimpleText = "Disconnected from Arduino";
}

//---------------------------------------------------------------------------
// ============================================================
// LED CONTROL - ВКЛЮЧИТЬ (ON)
// ============================================================
void __fastcall TForm1::RAD_BTN_ONClick(TObject *Sender)
{
    // Проверяем: переключатель включён И мы подключены
    if (RAD_BTN_ON->Checked && connected)
    {
        // Меняем надпись и цвет
        LBL_LED_STATUS->Caption = "ON";
        LBL_LED_STATUS->Font->Color = clGreen;  // Зелёный
        
        // Отправляем команду на Arduino
        SendCommand("LED_ON");
        
        // Показываем в статусной строке
        SB_MAIN_STATUS_BAR->SimpleText = "LED: ON";
    }
}

//---------------------------------------------------------------------------
// ============================================================
// LED CONTROL - ВЫКЛЮЧИТЬ (OFF)
// ============================================================
void __fastcall TForm1::RAD_BTN_OFFClick(TObject *Sender)
{
    if (RAD_BTN_OFF->Checked && connected)
    {
        LBL_LED_STATUS->Caption = "OFF";
        LBL_LED_STATUS->Font->Color = clRed;  // Красный
        SendCommand("LED_OFF");
        SB_MAIN_STATUS_BAR->SimpleText = "LED: OFF";
    }
}

//---------------------------------------------------------------------------
// ============================================================
// LED CONTROL - МЕДЛЕННОЕ МИГАНИЕ (SLOW)
// ============================================================
void __fastcall TForm1::RAD_BTN_SLOWClick(TObject *Sender)
{
    if (RAD_BTN_SLOW->Checked && connected)
    {
        LBL_LED_STATUS->Caption = "SLOW";
        LBL_LED_STATUS->Font->Color = clBlue;  // Синий
        SendCommand("MODE_SLOW");
        SB_MAIN_STATUS_BAR->SimpleText = "LED: SLOW BLINK";
    }
}

//---------------------------------------------------------------------------
// ============================================================
// LED CONTROL - СРЕДНЕЕ МИГАНИЕ (MIDDLE)
// ============================================================
void __fastcall TForm1::RAD_BTN_MIDDLEClick(TObject *Sender)
{
    if (RAD_BTN_MIDDLE->Checked && connected)
    {
        LBL_LED_STATUS->Caption = "MIDDLE";
        LBL_LED_STATUS->Font->Color = clPurple;  // Фиолетовый
        SendCommand("MODE_MIDDLE");
        SB_MAIN_STATUS_BAR->SimpleText = "LED: MIDDLE BLINK";
    }
}

//---------------------------------------------------------------------------
// ============================================================
// LED CONTROL - БЫСТРОЕ МИГАНИЕ (FAST)
// ============================================================
void __fastcall TForm1::RAD_BTN_FASTClick(TObject *Sender)
{
    if (RAD_BTN_FAST->Checked && connected)
    {
        LBL_LED_STATUS->Caption = "FAST";
        LBL_LED_STATUS->Font->Color = clMaroon;  // Тёмно-красный
        SendCommand("MODE_FAST");
        SB_MAIN_STATUS_BAR->SimpleText = "LED: FAST BLINK";
    }
}
//---------------------------------------------------------------------------
