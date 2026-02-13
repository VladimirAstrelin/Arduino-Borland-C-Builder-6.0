//---------------------------------------------------------------------------
// Это защита от повторного включения файла
// Если Unit1H ещё не определён - определяем его
#ifndef Unit1H
#define Unit1H
//---------------------------------------------------------------------------

// Подключаем нужные библиотеки C++ Builder
// В них описаны все компоненты: кнопки, надписи, вкладки и т.д.
#include <Classes.hpp>    // Базовые классы
#include <Controls.hpp>   // Элементы управления
#include <StdCtrls.hpp>   // Стандартные контролы (кнопки, надписи)
#include <Forms.hpp>      // Формы и окна
#include <ComCtrls.hpp>   // Дополнительные контролы (вкладки)
#include <ExtCtrls.hpp>   // Расширенные контролы (таймер, фигуры)

//---------------------------------------------------------------------------
// ОПИСАНИЕ КЛАССА НАШЕЙ ФОРМЫ
// class - это шаблон, описывающий, из чего состоит наша программа
// public - доступно всем
// private - доступно только внутри класса
//---------------------------------------------------------------------------
class TForm1 : public TForm  // Наследуемся от стандартного класса формы
{
// __published - компоненты, которые мы разместили на форме визуально
__published:
        // --- Главный PageControl (контейнер с вкладками) ---
        TPageControl *PC_MAIN_PAGE_CONTROL;

        // --- Вкладка CONNECT (подключение) ---
        TTabSheet *TS_CONNECT;              // Сама вкладка
        TGroupBox *GB_ARD_CONN;              // Рамка с заголовком
        TLabel *LBL_CONNECTION_STATUS;       // Надпись "Статус подключения"
        TLabel *LBL_CHOOSE_COM_PORT;         // Надпись "Выберите COM порт"
        TComboBox *CMB_COM_PORT;              // Выпадающий список с портами
        TButton *BTN_REFRESH;                 // Кнопка "Обновить список"
        TButton *BTN_CONNECT;                  // Кнопка "Подключиться"
        TButton *BTN_DISCONNECT;               // Кнопка "Отключиться"

        // --- Вкладка LED CONTROL (управление светодиодом) ---
        TTabSheet *TS_LED_CONTROL;
        TGroupBox *GB_LED_MODE;
        TLabel *LBL_LED_STATUS;                // Статус светодиода
        TLabel *LBL_CHOOSE_LED_MODE;           // Надпись "Выберите режим"
        TRadioButton *RAD_BTN_ON;               // Радиокнопка "Включить"
        TRadioButton *RAD_BTN_OFF;              // Радиокнопка "Выключить"
        TRadioButton *RAD_BTN_SLOW;              // Радиокнопка "Медленно"
        TRadioButton *RAD_BTN_MIDDLE;            // Радиокнопка "Средне"
        TRadioButton *RAD_BTN_FAST;               // Радиокнопка "Быстро"
        
        // --- Вкладка BUTTON D3 (кнопка на пине D3) ---
        TTabSheet *TS_BUTTON_D3;
        TGroupBox *GB_BUTTON_D3;
        TShape *SH_BTN_D3_COLOR;                 // Фигура (кружок) - меняет цвет
        TLabel *LBL_BTN_D3_STATUS;                // Надпись со статусом кнопки
        
        // --- Статусная строка внизу окна ---
        TStatusBar *SB_MAIN_STATUS_BAR;
        
        // --- ТАЙМЕР (срабатывает каждые 50 мс) ---
        TTimer *TimerReadCom;

        // --- Обработчики событий (функции, которые вызываются при действиях) ---
        void __fastcall FormCreate(TObject *Sender);        // При создании формы
        void __fastcall BTN_REFRESHClick(TObject *Sender);  // Нажали REFRESH
        void __fastcall BTN_CONNECTClick(TObject *Sender);  // Нажали CONNECT
        void __fastcall BTN_DISCONNECTClick(TObject *Sender); // Нажали DISCONNECT
        
        void __fastcall RAD_BTN_ONClick(TObject *Sender);   // Нажали ON
        void __fastcall RAD_BTN_OFFClick(TObject *Sender);  // Нажали OFF
        void __fastcall RAD_BTN_SLOWClick(TObject *Sender); // Нажали SLOW
        void __fastcall RAD_BTN_MIDDLEClick(TObject *Sender); // Нажали MIDDLE
        void __fastcall RAD_BTN_FASTClick(TObject *Sender); // Нажали FAST
        
        void __fastcall TimerReadComTimer(TObject *Sender); // Сработал таймер

// private - скрытые переменные и функции, доступные только внутри этого класса
private:
        HANDLE hCom;           // Дескриптор COM-порта (как номерок в гардеробе)
                               // По этому номерку Windows понимает, с каким портом работать
        
        bool connected;        // Флаг подключения: true - подключены, false - нет
        
        bool lastButtonState;  // Последнее известное состояние кнопки D3
        
        // --- Приватные методы (функции) ---
        void SendCommand(String command);     // Отправить команду Arduino
        void UpdateButtonUI(bool pressed);    // Обновить картинку кнопки
        void ParseArduinoData(String data);   // Разобрать данные от Arduino

        // --- Переменные для защиты от повторных команд ---
        bool commandPending;       // true - ждём ответ на команду
        AnsiString lastCommand;    // Какая команда была отправлена последней
        unsigned long commandStartTime; // Когда отправили (для таймаута)

// public - общедоступные методы (конструктор)
public:
        __fastcall TForm1(TComponent* Owner);  // Конструктор формы
};
//---------------------------------------------------------------------------

// Глобальная переменная - указатель на нашу форму
// Через неё можно обращаться к компонентам из любого места программы
extern PACKAGE TForm1 *Form1;

//---------------------------------------------------------------------------
// Глобальная функция проверки Arduino
// Не принадлежит классу, доступна отовсюду
bool CheckArduino(HANDLE hPort);
//---------------------------------------------------------------------------
#endif  // Конец защиты от повторного включения
