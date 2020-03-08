//////////////////////////////////////////////////////////////////////// 
// Контроллер управления светом для морского аквариума с возможностью //
// задания закатов и рассветов                                        //
//                                                                    //
// 6-канальный ШИМ для управления светодиодной сборкой                //
// ReefLife-DRIVE http://aquabox.by/goods2.php?type=LED&goodid=82     //
//                                                                    //
// Компоненты:                                                        //
// Arduino Nano                                                       //
//   ПЗУ - 1Кб                                                        //
//   ОЗУ - 2Кб                                                        //
// DS1307RTC (A4, A5)                                                 //
// ШИМ (D3, D5, D6, D9, D10, D11)                                     //
// D4 D7 D12 - свободны                                               //
// A0 A1 A2 A3 A6 A7 - свободны                                       //
//                                                                    //
// Уровень освещения задается в массиве DefaultValue. На каждый канал //
// можно задать 48 точек: значение на каждые полчаса от 0 до 24 часов //
// Значения от 0 до 255: 0 - канал выключен, 255 - канала светит на   //
// максимальном значении. Значения уровня освещения между точками     //
// рассчитывается пропорционально 30 минутам.                         //
//////////////////////////////////////////////////////////////////////// 

//////////////////////////////////////////////////////////////////////// 
// Подключение библиотек                                              //
////////////////////////////////////////////////////////////////////////

// Библиотека для работы с EPROM Arduino
// Автоматически создает объект EEPROM
#include <EEPROM.h>

// Библиотека для работы с EPROM 24C32
// Распологается на плате с DS1307
#include <Eeprom24C32_64.h>

// Библиотеки для работы с DS1307RTC
// Библиотека для работы с I2C интерфейсом
#include <Wire.h>

// Библиотека для работы со временем
#include <Time.h>

// Библиотека для работы с DS1307RTC
// Автоматически создает объект RTC
#include <DS1307RTC.h>

//////////////////////////////////////////////////////////////////////// 
// Переменные и константы                                             //
////////////////////////////////////////////////////////////////////////
#define ChanelsCount 6   // Количество каналов ШИМ
#define PointsCount  48  // Количество точек на канал
#define MinutesDelta 30  // Интервал минут между точками
#define AllPointsCount ChanelsCount * PointsCount // Общее количество точек

#define SignatureAddress  0 // Адрес в EPROM сигнатуры
#define DataBlockAddress  1 // Адрес в EPROM блока данных точек

#define MainSignature 123 // Сигнатура наличия данных в EPROM

#define EEPROM_24C32_ADDRESS  0x50 // I2C адресс 24С32

#define violetChanel 0
#define witeChanel   1
#define redChanel    2
#define greenChanel  3
#define blue1Chanel  4
#define blue2Chanel  5

#define FreePinsCount 14 // Цифровых пинов 14 (Arduino Nano)

volatile int pin13Value = LOW; // Будем мигать светодиодом

byte PWMPins[ChanelsCount] = { 3, 5, 6, 9, 10, 11 }; // Пины ШИМ

const byte DefaultValue[ChanelsCount*PointsCount] PROGMEM =
{  
//0   1   2   3   4   5   6   7    8     9      10      11      12      13      14      15      16      17      18      19     20    21  22  23
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,10,20,30,80,150,180,190,200,210,220,255,255,255,255,210,200,190,180,150,140,130,120,100,100,80,40,20,5,1,1,1,1,1, // violet
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0, 15,40,75 ,90 ,95, 100,105,110,112,115,112,110,105,100,95 ,90, 75, 70, 65, 60, 50, 40, 20,10,5, 0,0,0,0,0,0, // wite
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,10,40,3, 8, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 8,  4, 30,1, 0,0,0,0,0,0, // red
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0, 15,40,75, 90, 95, 100,105,110,112,115,112,110,105,100,95, 90, 75, 70, 65, 60, 50, 40, 20,10,5, 0,0,0,0,0,0, // green
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0, 30,80,150,180,190,200,210,220,255,255,255,255,210,200,190,180,150,140,130,120,100,100,80,40,20,5,0,0,0,0,0, // blue
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0, 30,80,150,180,190,200,210,220,255,255,255,255,210,200,190,180,150,140,140,130,130,100,80,40,20,5,0,0,0,0,0, // blue
};

volatile byte LastSecond;                // Секунда
volatile byte LastMinute;                // Последняя минута, для которой был расчет
volatile byte LastHour;                  // Последний час, для которого были сохранены данные
byte chanels[ChanelsCount][PointsCount]; // Данные точек для каналов, возможные значения 0-255

// Объект для работы с 24С32
static Eeprom24C32_64 EEPROM_24C32_64(EEPROM_24C32_ADDRESS);

// Получить данные точек расчета ШИМ
void getArduinoNanoEpromData()
{
  // Считываем сигнатуру
  byte signature = EEPROM.read(SignatureAddress);

  if (signature == MainSignature)
  {
    // Нашли сигнатуру, перекидываем данные в ОЗУ
    for (int i = 0; i < ChanelsCount; i++)
      for (int j = 0; j < PointsCount; j++)
        chanels[i][j] = EEPROM.read(DataBlockAddress + i * PointsCount + j);
  }
  else
  {
    // Если нет сигнатуры задаем значения по умолчанию
    for (int i = 0; i < ChanelsCount; i++)
      for (int j = 0; j < PointsCount; j++)
        chanels[i][j] = pgm_read_byte(DefaultValue + i * PointsCount + j);
  }
}

// Установить значения ШИМ
void setPWMsValue()
{
  // Получаем текущее время
  // и рассчитываем значения ШИМ 
  if (timeStatus() == timeSet)
  {
    byte curSecond = second();
    byte curHour   = hour();
    byte curMinute = minute();   
    // Моргаем диодиком
    if (curSecond != LastSecond)
    {
      pin13Value = !pin13Value; // Моргаем 13-м светодиодом
      digitalWrite(13, pin13Value);
      LastSecond = curSecond;
    }
    // Сохранение значение датчиков будем производить раз в час
    if (curHour != LastHour)
    {
      LastHour = curHour;
    }
    // Расчет ШИМ будем производить 1 раз в минуту
    if (curMinute != LastMinute)
    {
      byte beginPoint = curHour + curHour + ((curMinute < MinutesDelta) ? 0 : 1);
      byte endPoint = (beginPoint == (PointsCount - 1)) ? 0 : beginPoint + 1;

      for (int i = 0; i < ChanelsCount; i++)
      {
        byte beginPointValue;
        byte endPointValue;
        byte curMinutesDelta = (curMinute < MinutesDelta) ? curMinute : (curMinute - MinutesDelta);

        if (chanels[i][beginPoint] >= chanels[i][endPoint])
        {
          beginPointValue = chanels[i][endPoint];
          endPointValue = chanels[i][beginPoint];
          curMinutesDelta = 30 - curMinutesDelta;
        }
        else
        {
          beginPointValue = chanels[i][beginPoint];
          endPointValue = chanels[i][endPoint];
        }
        byte curPWMValueDelta = endPointValue - beginPointValue;
        byte curPWMDelta = (curPWMValueDelta * curMinutesDelta) / MinutesDelta;
        int curPWM = beginPointValue + curPWMDelta;

        analogWrite(PWMPins[i], curPWM);
      }
      LastMinute = curMinute;
    }
  }
}

void setup() {
  // Включаем внутренние нагрузочные резисторы
  for (int i = 1; i < FreePinsCount; i++)
  {
    pinMode(PWMPins[i], INPUT);
    digitalWrite(PWMPins[i], HIGH);
  }

      // Зададім для DS1307 генерацию сигнала 1Гц, для прерывания
      // Т.о. прерывание обработки расчета ШИМ будет происходить 
      // 1 раз в секунду
  Wire.beginTransmission(0x68);
  Wire.write(0x7);
  Wire.write(0x10);
  Wire.endTransmission();
  // Инициализируем EPROM DS1307
  EEPROM_24C32_64.initialize();
  // Для библиотеки Time устанавливаем источник реального времени DS1307
  setSyncProvider(RTC.get);
  // Инициализация данных
  getArduinoNanoEpromData();
  // Определяем пины ШИМ как выходные
  for (int i = 0; i < ChanelsCount; i++)
    pinMode(PWMPins[i], OUTPUT);
  // Инициализируем 13 выход со светодиодом.
  pinMode(13, OUTPUT);
  // Добавляем обработчик прерывания от часов DS1307
  //attachInterrupt(0, setPWMsValue, FALLING);
}

void loop() {
  // Установить значения ШИМ
  setPWMsValue();
}

