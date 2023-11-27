void lcdTime() {
  if (millis() - timeDiff2 > 1000) {   // check switch every second 
    timeDiff2 = millis();           // get current millisecond count

    #ifdef LCDSCREEN20x4

      if (lcdsegs % 10 != 0) {
        // ultima cifra 1,2,3,4,5,6,7,8,9
        utoa(lcdsegs%10,PlayBytes,10);
        lcd.setCursor(19,0);
        lcd.print(PlayBytes);
      }
      else if (lcdsegs % CNTRBASE != 0) {
        // es 10,20,30,40,50,60,70,80,90,110,120,..
        utoa(lcdsegs%CNTRBASE,PlayBytes,10);
        lcd.setCursor(18,0);
        lcd.print(PlayBytes);
      }
      else if (lcdsegs % (CNTRBASE*10) != 0) {
        // es 100,200,300,400,500,600,700,800,900,1100,..
        utoa(lcdsegs%(CNTRBASE*10)/CNTRBASE*100,PlayBytes,10);
        lcd.setCursor(17,0);
        lcd.print(PlayBytes);
      } 
      else {
        // es 000,1000,2000,...
        lcd.setCursor(17,0);
        lcd.print('0');
        lcd.print('0');
        lcd.print('0');
      }

      lcdsegs++;
    #endif


  }
}

void lcdPercent() {  
  newpct=(100 * bytesRead)/filesize;                   
  if (currpct==100) {
      currpct= 0;
  }

  if (((newpct>currpct) || currpct==0) && (newpct % 1 == 0)) {
    #ifdef LCDSCREEN20x4            
      lcd.setCursor(8,0);
      lcd.print(newpct);lcd.print('%');
    #endif             

    currpct = newpct;
  }
}
