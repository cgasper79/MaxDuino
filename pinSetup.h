#ifndef PINSETUP_H_INCLUDED
#define PINSETUP_H_INCLUDED

//__AVR_ATmega328P__
  //#define MINIDUINO_AMPLI     // For A.Villena's Miniduino new design
  #define outputPin           9
  #ifdef MINIDUINO_AMPLI
    #define INIT_OUTPORT         DDRB |= B00000011                              // pin8+ pin9 es el bit0-bit1 del PORTB 
    #define WRITE_LOW           (PORTB &= B11111101) |= B00000001               // pin8+ pin9 , bit0- bit1 del PORTB
    #define WRITE_HIGH          (PORTB |= B00000010) &= B11111110               // pin8+ pin9 , bit0- bit1 del PORTB  
  //  #define WRITE_LOW           PORTB = (PORTB & B11111101) | B00000001         // pin8+ pin9 , bit0- bit1 del PORTB
  //  #define WRITE_HIGH          PORTB = (PORTB | B00000010) & B11111110         // pin8+ pin9 , bit0- bit1 del PORTB 
  #else
    #define INIT_OUTPORT         DDRB |=  _BV(1)         // El pin9 es el bit1 del PORTB
    #define WRITE_LOW           PORTB &= ~_BV(1)         // El pin9 es el bit1 del PORTB
    #define WRITE_HIGH          PORTB |=  _BV(1)         // El pin9 es el bit1 del PORTB
  #endif


/////////////////////////////////////////////////////////////////////////////////////////////
  //General Pin settings
  //Setup buttons with internal pullup

  const byte chipSelect = 10;          //Sd card chip select pin
  
  #define btnPlay       17            //Play Button
  #define btnStop       16            //Stop Button
  #define btnUp         15            //Up button
  #define btnDown       14            //Down button
  #define btnMotor      6             //Motor Sense (connect pin to gnd to play, NC for pause)
  #define btnRoot       7             //Return to SD card root


  PORTC |= _BV(3);
  
  PORTC |= _BV(2);

  PORTC |= _BV(1);

  PORTC |= _BV(0);

  PORTD |= _BV(btnMotor);
  
  PORTD |= _BV(btnRoot);



#endif // #define PINSETUP_H_INCLUDED
