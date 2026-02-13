object Form1: TForm1
  Left = 801
  Top = 156
  Width = 439
  Height = 492
  Caption = 'ARDUINO CONTROL'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  PixelsPerInch = 96
  TextHeight = 13
  object PC_MAIN_PAGE_CONTROL: TPageControl
    Left = 0
    Top = 0
    Width = 423
    Height = 434
    ActivePage = TS_CONNECT
    Align = alClient
    TabIndex = 0
    TabOrder = 0
    object TS_CONNECT: TTabSheet
      Caption = 'CONNECT'
      object GB_ARD_CONN: TGroupBox
        Left = 16
        Top = 24
        Width = 345
        Height = 193
        Caption = 'ARDUINO CONNECTION TO PC'
        TabOrder = 0
        object LBL_CHOOSE_COM_PORT: TLabel
          Left = 24
          Top = 64
          Width = 91
          Height = 13
          Caption = 'Choose COM Port :'
        end
        object LBL_CONNECTION_STATUS: TLabel
          Left = 24
          Top = 32
          Width = 186
          Height = 13
          Caption = 'CONNECTION STATUS : __________'
        end
        object CMB_COM_PORT: TComboBox
          Left = 24
          Top = 96
          Width = 145
          Height = 21
          ItemHeight = 13
          TabOrder = 0
          Items.Strings = (
            '')
        end
        object BTN_REFRESH: TButton
          Left = 232
          Top = 48
          Width = 89
          Height = 25
          Caption = 'REFRESH'
          TabOrder = 1
          OnClick = BTN_REFRESHClick
        end
        object BTN_CONNECT: TButton
          Left = 232
          Top = 88
          Width = 89
          Height = 25
          Caption = 'CONNECT'
          TabOrder = 2
          OnClick = BTN_CONNECTClick
        end
        object BTN_DISCONNECT: TButton
          Left = 232
          Top = 128
          Width = 89
          Height = 25
          Caption = 'DISCONNECT'
          TabOrder = 3
          OnClick = BTN_DISCONNECTClick
        end
      end
    end
    object TS_LED_CONTROL: TTabSheet
      Caption = 'LED CONTROL'
      ImageIndex = 1
      object GB_LED_MODE: TGroupBox
        Left = 16
        Top = 16
        Width = 177
        Height = 273
        Caption = 'LED MODE'
        TabOrder = 0
        object LBL_CHOOSE_LED_MODE: TLabel
          Left = 24
          Top = 64
          Width = 95
          Height = 13
          Caption = 'Choose LED mode :'
        end
        object LBL_LED_STATUS: TLabel
          Left = 24
          Top = 32
          Width = 109
          Height = 13
          Caption = 'LED STATUS: ______'
        end
        object RAD_BTN_ON: TRadioButton
          Left = 32
          Top = 96
          Width = 113
          Height = 17
          Caption = 'TURN ON'
          TabOrder = 0
          OnClick = RAD_BTN_ONClick
        end
        object RAD_BTN_OFF: TRadioButton
          Left = 32
          Top = 128
          Width = 113
          Height = 17
          Caption = 'TURN OFF'
          TabOrder = 1
          OnClick = RAD_BTN_OFFClick
        end
        object RAD_BTN_SLOW: TRadioButton
          Left = 32
          Top = 160
          Width = 113
          Height = 17
          Caption = 'BLINK SLOW'
          TabOrder = 2
          OnClick = RAD_BTN_SLOWClick
        end
        object RAD_BTN_MIDDLE: TRadioButton
          Left = 32
          Top = 192
          Width = 113
          Height = 17
          Caption = 'BLINK MEDIUM'
          TabOrder = 3
          OnClick = RAD_BTN_MIDDLEClick
        end
        object RAD_BTN_FAST: TRadioButton
          Left = 32
          Top = 224
          Width = 113
          Height = 17
          Caption = 'BLINK FAST'
          TabOrder = 4
          OnClick = RAD_BTN_FASTClick
        end
      end
    end
    object TS_BUTTON_D3: TTabSheet
      Caption = 'BUTTON D2'
      ImageIndex = 2
      object GB_BUTTON_D3: TGroupBox
        Left = 24
        Top = 24
        Width = 305
        Height = 193
        Caption = 'BUTTON D3 STATUS'
        TabOrder = 0
        object SH_BTN_D3_COLOR: TShape
          Left = 24
          Top = 40
          Width = 30
          Height = 30
          Brush.Color = clYellow
          Shape = stCircle
        end
        object LBL_BTN_D3_STATUS: TLabel
          Left = 88
          Top = 48
          Width = 125
          Height = 13
          Caption = 'D2 BUTTON: RELEASED'
        end
      end
    end
  end
  object SB_MAIN_STATUS_BAR: TStatusBar
    Left = 0
    Top = 434
    Width = 423
    Height = 19
    Panels = <>
    SimplePanel = False
  end
  object TimerReadCom: TTimer
    Enabled = False
    Interval = 50
    OnTimer = TimerReadComTimer
    Left = 392
    Top = 16
  end
end
