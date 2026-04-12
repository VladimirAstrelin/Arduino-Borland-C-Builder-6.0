// v4.0
#include <vcl.h>           // Визуальные компоненты C++ Builder
#include <windows.h>       // Windows API - функции для работы с портами, файлами
#include <registry.hpp>    // Работа с реестром Windows (список COM-портов)

#pragma hdrstop            // Всё что написано выше будет включено в предкомпиляцию

#include "Unit1.h"         // Файл описания формы

#pragma package(smart_init)   // Оптимизация компиляции
#pragma resource "*.dfm"      // Подключаем файл с описанием формы

// Глобальная переменная Form1
TForm1 *Form1;

//---------------------------------------------------------------------------
// ============================================================
// ФУНКЦИЯ ПРОВЕРКИ ARDUINO
// Отправляет TEST и ищет ARDUINO_OK среди всех ответов.
// Учитывает что Arduino может прислать ARDUINO_READY
// до того как ответит на нашу команду.
// ============================================================
bool CheckArduino(HANDLE hPort)
{
    if (hPort == INVALID_HANDLE_VALUE)
        return false;

    char testCmd[] = "TEST\n";
    DWORD bytesWritten;

    for (int attempt = 0; attempt < 3; attempt++)
    {
        // Очищаем буфер перед каждой попыткой — убираем
        // всё что Arduino могла прислать до нашего запроса
        // (например ARDUINO_READY при старте)
        PurgeComm(hPort, PURGE_RXCLEAR | PURGE_TXCLEAR);

        // Отправляем TEST
        if (!WriteFile(hPort, testCmd, strlen(testCmd), &bytesWritten, NULL))
        {
            Sleep(100);
            continue;
        }

        // Ждём пока Arduino обработает команду и ответит
        Sleep(300);

        // Читаем всё что пришло в буфер
        char buffer[256] = {0};
        DWORD bytesRead;

        if (!ReadFile(hPort, buffer, sizeof(buffer)-1, &bytesRead, NULL))
        {
            Sleep(100);
            continue;
        }

        // Если ничего не пришло — следующая попытка
        if (bytesRead == 0)
        {
            Sleep(100);
            continue;
        }

        // Ставим нуль в конце и превращаем в строку
        buffer[bytesRead] = '\0';
        String response = String(buffer);

        // Разбиваем на отдельные строки — Arduino могла прислать
        // несколько сообщений подряд, например:
        // "ARDUINO_READY\nARDUINO_OK\n"
        // TStringList автоматически разобьёт по \n
        TStringList *lines = new TStringList();
        lines->Text = response;

        bool found = false;

        for (int i = 0; i < lines->Count; i++)
        {
            String line = lines->Strings[i].Trim();

            // Ищем ARDUINO_OK среди всех строк
            if (line == "ARDUINO_OK")
            {
                found = true;
                break;
            }
        }

        delete lines;

        if (found)
            return true;  // Arduino найдена и отвечает

        // ARDUINO_OK не нашли — ждём и пробуем снова
        Sleep(100);
    }

    return false;
}

//---------------------------------------------------------------------------
// ============================================================
//    КОНСТРУКТОР ФОРМЫ - выполняется при создании окна программы
// ============================================================
__fastcall TForm1::TForm1(TComponent* Owner)
        : TForm(Owner)  // Вызываем конструктор родительского класса
{
    // --- Инициализация переменных (задаём начальные значения) ---
    hCom = INVALID_HANDLE_VALUE;    // Пока порт не открыт
    connected = false;              // Ещё не подключены
    lastButtonState = false;        // Кнопка не нажата (по умолчанию)

    // --- Переменные для защиты от повторных команд  ---
    commandPending = false;   // Никакая команда не выполняется
    lastCommand = "";         // Последняя команда - пустая
    commandStartTime = 0;     // Время отправки = 0
}

//---------------------------------------------------------------------------
// ============================================================
// СОЗДАНИЕ ФОРМЫ (Вызывается автоматически при создании формы)
// ============================================================
void __fastcall TForm1::FormCreate(TObject *Sender)
{
    // --- Настройка интерфейса (что видит пользователь) ---

    // Надпись статуса подключения
    LBL_CONNECTION_STATUS->Caption = "CONNECTION: DISCONNECTED";

    // Статус светодиода
    LBL_LED_STATUS->Caption = "LED: OFF";

    // Label для кнопки D3 - начальное состояние: отпущена
    LBL_BTN_D3_STATUS->Caption = "D3 BUTTON: RELEASED";

    // Кружочек (Shape) обозначающий LED, его цвет, форма, размеры
    SH_BTN_D3->Brush->Color = clBtnFace ;  // Цвет: Прозрачный
    SH_BTN_D3->Shape = stCircle;           // Форма: Круг
    SH_BTN_D3->Width = 30;                 // Ширина 30 пикселей
    SH_BTN_D3->Height = 30;                // Высота 30 пикселей

    // Очищаем выпадающий список
    CMB_COM_PORT->Clear();
    
// --- РАБОТА С РЕЕСТРОМ ---
// Windows хранит в реестре все данные о COM-портах

    // new - создаём новый объект в куче
    TRegistry *reg = new TRegistry();
    
    // Устанавливаем свойство RootKey класса TRegistry —
    // указываем с какой корневой ветки реестра начинать поиск.
    // HKEY_LOCAL_MACHINE это ветка с информацией о железе компьютера,
    // именно там Windows хранит список COM-портов
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
        SH_BTN_D3->Brush->Color = clRed;
        // Увеличиваем его немного в размерах
        SH_BTN_D3->Width = 35;                 // Ширина 35 пикселей
        SH_BTN_D3->Height = 35;                // Высота 35 пикселей
    }
    else  // Если кнопка отпущена
    {
        // Меняем текст на "RELEASED"
        LBL_BTN_D3_STATUS->Caption = "D3 BUTTON: RELEASED";
        // Делаем текст чёрным
        LBL_BTN_D3_STATUS->Font->Color = clBlack;
        // Возвращаем кружку прозрачный цвет
        SH_BTN_D3->Brush->Color = clBtnFace; //Прозрачный цвет
        // Возвращаем кружку его прежний размер
        SH_BTN_D3->Width = 30;                 // Ширина 30 пикселей
        SH_BTN_D3->Height = 30;                // Высота 30 пикселей
    }
    
    // Запоминаем новое состояние
    lastButtonState = pressed;
}


// ============================================================
// ОБНОВЛЕНИЕ ИНТЕРФЕЙСА - СВЕТОДИОД
// Вызывается ТОЛЬКО из ParseArduinoData после получения
// подтверждения от Arduino — не раньше.
//
// mode — текущий режим: "ON", "OFF", "SLOW", "MIDDLE", "FAST"
// ============================================================
void TForm1::UpdateLedUI(String mode)
{
    if (mode == "ON")
    {
        LBL_LED_STATUS->Caption    = "LED: ON";
        LBL_LED_STATUS->Font->Color = clGreen;
    }
    else if (mode == "OFF")
    {
        LBL_LED_STATUS->Caption    = "LED: OFF";
        LBL_LED_STATUS->Font->Color = clRed;
    }
    else if (mode == "SLOW")
    {
        LBL_LED_STATUS->Caption    = "LED: SLOW";
        LBL_LED_STATUS->Font->Color = clBlue;
    }
    else if (mode == "MIDDLE")
    {
        LBL_LED_STATUS->Caption    = "LED: MIDDLE";
        LBL_LED_STATUS->Font->Color = clPurple;
    }
    else if (mode == "FAST")
    {
        LBL_LED_STATUS->Caption    = "LED: FAST";
        LBL_LED_STATUS->Font->Color = clMaroon;
    }
}

// ============================================================
// ПАРСИНГ ДАННЫХ ОТ ARDUINO
// Разбирает каждую строку которая пришла от Arduino и
// реагирует на неё: обновляет UI, сбрасывает флаги,показывает ошибки.
// ============================================================
void TForm1::ParseArduinoData(String data)
{
        // Убираем пробелы, \r, \n по краям
    data = data.Trim();

    // Пустую строку игнорируем
    if (data.IsEmpty()) return;

    // =====================================================
    // БЛОК 1: КНОПКА D3
    // Формат: "B:1" (нажата) или "B:0" (отпущена)
    // Проверяем независимо от всего остального —
    // кнопка может прийти в любой момент
    // =====================================================
    if (data.Length() >= 3 && data[1] == 'B' && data[2] == ':')
    {
        bool pressed = (data[3] == '1');
        UpdateButtonUI(pressed);
        SB_MAIN_STATUS_BAR->SimpleText = "Button D3: " +
            String(pressed ? "PRESSED" : "released");
        return;  // Обработали кнопку — дальше не идём
    }

    // =====================================================
    // БЛОК 2: УСПЕШНОЕ ВЫПОЛНЕНИЕ КОМАНД (_OK)
    // Arduino подтвердила что команда выполнена реально.
    // Только здесь обновляем UI — не раньше.
    // =====================================================
    if (data == "LED_ON_OK")
    {
        commandPending = false;
        // Обновляем UI только после реального подтверждения
        UpdateLedUI("ON");
        SB_MAIN_STATUS_BAR->SimpleText = "LED turned ON confirmed by Arduino";
        return;
    }

    if (data == "LED_OFF_OK")
    {
        commandPending = false;
        UpdateLedUI("OFF");
        SB_MAIN_STATUS_BAR->SimpleText = "LED turned OFF confirmed by Arduino";
        return;
    }

    if (data == "MODE_SLOW_OK")
    {
        commandPending = false;
        UpdateLedUI("SLOW");
        SB_MAIN_STATUS_BAR->SimpleText = "Slow blink started confirmed by Arduino";
        return;
    }

    if (data == "MODE_MIDDLE_OK")
    {
        commandPending = false;
        UpdateLedUI("MIDDLE");
        SB_MAIN_STATUS_BAR->SimpleText = "Middle blink started confirmed by Arduino";
        return;
    }

    if (data == "MODE_FAST_OK")
    {
        commandPending = false;
        UpdateLedUI("FAST");
        SB_MAIN_STATUS_BAR->SimpleText = "Fast blink started confirmed by Arduino";
        return;
    }

    // =====================================================
    // БЛОК 3: ОШИБКИ ВЫПОЛНЕНИЯ КОМАНД (_FAIL)
    // Arduino сообщает что команда НЕ была выполнена.
    // Показываем ошибку — UI не меняем, он остаётся
    // в предыдущем состоянии которое соответствует реальности.
    // =====================================================
    if (data == "LED_ON_FAIL")
    {
        commandPending = false;
        // UI не обновляем — LED реально не включился
        SB_MAIN_STATUS_BAR->SimpleText = "ERROR: LED_ON failed on Arduino side!";
        MessageBox(0, "Arduino could not turn ON the LED!\n"
                      "Possible hardware problem.",
                      "Arduino Error", MB_OK | MB_ICONWARNING);
        return;
    }

    if (data == "LED_OFF_FAIL")
    {
        commandPending = false;
        SB_MAIN_STATUS_BAR->SimpleText = "ERROR: LED_OFF failed on Arduino side!";
        MessageBox(0, "Arduino could not turn OFF the LED!\n"
                      "Possible hardware problem.",
                      "Arduino Error", MB_OK | MB_ICONWARNING);
        return;
    }

    if (data == "MODE_SLOW_FAIL"
     || data == "MODE_MIDDLE_FAIL"
     || data == "MODE_FAST_FAIL")
    {
        commandPending = false;
        SB_MAIN_STATUS_BAR->SimpleText = "ERROR: Blink mode failed to start!";
        MessageBox(0, "Arduino could not start blink mode!\n"
                      "Possible hardware problem.",
                      "Arduino Error", MB_OK | MB_ICONWARNING);
        return;
    }

    // =====================================================
    // БЛОК 4: СОБЫТИЯ МИГАНИЯ
    // Эти сообщения приходят не как ответ на команду,
    // а самостоятельно — по факту завершения процесса.
    // =====================================================

    // Мигание завершилось успешно — все N переключений выполнены
    if (data == "BLINK_DONE")
    {
        // Мигание закончилось — LED сейчас выключен
        UpdateLedUI("OFF");
        SB_MAIN_STATUS_BAR->SimpleText = "Blink sequence completed successfully";
        return;
    }

    // Аварийная остановка: сбой пина во время мигания
    if (data == "BLINK_PIN_FAIL")
    {
        UpdateLedUI("OFF");  // Arduino погасила LED при аварии
        SB_MAIN_STATUS_BAR->SimpleText = "ERROR: Blink stopped — pin failure!";
        MessageBox(0, "Arduino detected a pin failure during blink!\n"
                      "Blink sequence was stopped.\n"
                      "Check hardware.",
                      "Arduino Error", MB_OK | MB_ICONERROR);
        return;
    }

    // Мигание завершилось но LED не погас до конца
    if (data == "BLINK_END_FAIL")
    {
        SB_MAIN_STATUS_BAR->SimpleText = "ERROR: Blink ended but LED state uncertain!";
        MessageBox(0, "Blink sequence ended but Arduino\n"
                      "could not verify final LED state.",
                      "Arduino Warning", MB_OK | MB_ICONWARNING);
        return;
    }

    // =====================================================
    // БЛОК 5: ВСЁ ОСТАЛЬНОЕ
    // Сюда попадают: ARDUINO_READY и любые
    // неизвестные сообщения — просто показываем в статусе
    // =====================================================
    SB_MAIN_STATUS_BAR->SimpleText = "Arduino: " + data;
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
    MessageBox(0, "COM port list refreshed", "COM PORT LIST", MB_OK);
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

    // Значение по умолчанию, например COM13
    // Перестраховка на случай если ItemIndex вернул -1 (ничего не выбрано)
    String portName = "COM13";

    // --- Получаем выбранный порт из списка ---
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
    // Для COM10+ нужно специальное имя, например, "\\\\.\\COM13"
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
            NULL,                           // Безопасность по умолчанию
            OPEN_EXISTING,                  // Открываем существующий порт
            FILE_ATTRIBUTE_NORMAL,          // Обычные атрибуты
            NULL                            // Без шаблона
    );

    // Проверяем, удалось ли открыть порт
    if (hCom == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();  // Узнаём код ошибки
        String errorMsg = "Unable to open " + portName + ".\n";
        
        // Объясняем ошибку понятным языком
        if (error == ERROR_ACCESS_DENIED)
            errorMsg += "Port is busy (close any Serial Monitor programs)";
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
    checkTimeouts.ReadTotalTimeoutMultiplier = 10;   // +10 мс на каждый байт
    checkTimeouts.ReadTotalTimeoutConstant = 50;     // Базовое время 50 мс
    checkTimeouts.WriteTotalTimeoutMultiplier = 10; // Множитель для расч общ вр ожид
    checkTimeouts.WriteTotalTimeoutConstant = 50;    // Таймаут записи


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
    if (!connected)
    {
        MessageBox(0, "Not connected!", "Connection status", MB_OK);
        return;
    }

    // Останавливаем таймер первым делом
    TimerReadCom->Enabled = false;

    if (hCom != INVALID_HANDLE_VALUE)
    {
        // Отправляем LED_OFF напрямую через WriteFile минуя SendCommand.
        // При отключении нам не нужна защита commandPending и не нужен
        // ответ от Arduino — просто отправляем и сразу закрываем порт.
        // Windows гарантирует что данные уйдут из буфера до CloseHandle.
        char cmd[] = "LED_OFF\n";
        DWORD bytesWritten;
        WriteFile(hCom, cmd, strlen(cmd), &bytesWritten, NULL);

        CloseHandle(hCom);
        hCom = INVALID_HANDLE_VALUE;
    }

    // Сбрасываем состояние программы
    connected      = false;
    commandPending = false;
    lastCommand    = "";

    // Обновляем UI
    LBL_CONNECTION_STATUS->Caption = "CONNECTION: DISCONNECTED";
    UpdateLedUI("OFF");

    MessageBox(0, "Disconnected", "Connection status", MB_OK);
    SB_MAIN_STATUS_BAR->SimpleText = "Disconnected from Arduino";
}

//---------------------------------------------------------------------------
// ============================================================
// LED CONTROL - ВКЛЮЧИТЬ (ON)
// ============================================================
void __fastcall TForm1::RAD_BTN_ONClick(TObject *Sender)
{
    if (RAD_BTN_ON->Checked && connected)
    {
        // Показываем что ждём ответа — UI ещё не меняем
        LBL_LED_STATUS->Caption     = "LED: ...";
        LBL_LED_STATUS->Font->Color = clGray;
        SendCommand("LED_ON");
        SB_MAIN_STATUS_BAR->SimpleText = "Waiting for Arduino confirmation...";
    }
}

void __fastcall TForm1::RAD_BTN_OFFClick(TObject *Sender)
{
    if (RAD_BTN_OFF->Checked && connected)
    {
        LBL_LED_STATUS->Caption     = "LED: ...";
        LBL_LED_STATUS->Font->Color = clGray;
        SendCommand("LED_OFF");
        SB_MAIN_STATUS_BAR->SimpleText = "Waiting for Arduino confirmation...";
    }
}

void __fastcall TForm1::RAD_BTN_SLOWClick(TObject *Sender)
{
    if (RAD_BTN_SLOW->Checked && connected)
    {
        LBL_LED_STATUS->Caption     = "LED: ...";
        LBL_LED_STATUS->Font->Color = clGray;
        SendCommand("MODE_SLOW");
        SB_MAIN_STATUS_BAR->SimpleText = "Waiting for Arduino confirmation...";
    }
}

void __fastcall TForm1::RAD_BTN_MIDDLEClick(TObject *Sender)
{
    if (RAD_BTN_MIDDLE->Checked && connected)
    {
        LBL_LED_STATUS->Caption     = "LED: ...";
        LBL_LED_STATUS->Font->Color = clGray;
        SendCommand("MODE_MIDDLE");
        SB_MAIN_STATUS_BAR->SimpleText = "Waiting for Arduino confirmation...";
    }
}

void __fastcall TForm1::RAD_BTN_FASTClick(TObject *Sender)
{
    if (RAD_BTN_FAST->Checked && connected)
    {
        LBL_LED_STATUS->Caption     = "LED: ...";
        LBL_LED_STATUS->Font->Color = clGray;
        SendCommand("MODE_FAST");
        SB_MAIN_STATUS_BAR->SimpleText = "Waiting for Arduino confirmation...";
    }
}
//---------------------------------------------------------------------------
