#define Use_SoftI2CMaster
//#define Use_SoftWire
#define I2CFAST

#if defined(I2CFAST)
  #define I2C_FASTMODE  1
  #define I2CCLOCK  400000L   //100000L for StandarMode, 400000L for FastMode and 1000000L for FastModePlus
#else
  #define I2C_FASTMODE  0
  #define I2CCLOCK  100000L   //100000L for StandarMode, 400000L for FastMode and 1000000L for FastModePlus
#endif

// default SPI clock speed
#ifndef SD_SPI_CLOCK_SPEED
  #ifndef SD_SPI_CLOCK_SPEED
    #define SD_SPI_CLOCK_SPEED SPI_FULL_SPEED
  #endif
#endif

#if defined(__AVR_ATmega4809__) || defined (__AVR_ATmega4808__)
  #ifdef Use_SoftI2CMaster
    #undef Use_SoftI2CMaster
    //#error This chip does not support SoftI2CMaster. Please undefine Use_SoftI2CMaster
  #endif
  #ifdef Use_SoftWire
    #undef Use_SoftWire
    //#error This chip does not support Softwire. Please undefine Use_SoftWire
  #endif  
                  

#elif defined(__AVR_ATmega328P__)
  //#define TimerOne
  
#else
  #error I2C definitions (SoftI2CMaster/SoftWire/etc) not defined for board
#endif


#ifdef TimerOne
  #include <TimerOne.h>
  const TimerOne &Timer = Timerl;
#else
  #include "TimerCounter.h"
#endif





#include <SdFat.h>

#define scrollSpeed   600           //text scroll delay
#define scrollWait    3000          //Delay before scrolling starts

#ifdef LCDSCREEN20x4
  //#include <Wire.h>
  #include "LiquidCrystal_I2C_Soft.h"
  LiquidCrystal_I2C lcd(LCD_I2C_ADDR,20,4); // set the LCD address to 0x27 for a 16 chars and 2 line display
  #define SCREENSIZE 20  
#endif
