const char P_PRODUCT_NAME[] PROGMEM = "MaxDuino     LCD20x4";

#include "version.h"
#define XXSTR(a) XSTR(a)
#define XSTR(a) #a
const char P_VERSION[] PROGMEM = XXSTR(_VERSION);

// ---------------------------------------------------------------------------------
// USE CLASS-4 AND CLASS-10 CARDS on this project WITH SDFAT2 FOR BETTER ACCESS SPEED
// ---------------------------------------------------------------------------------

#include "userconfig.h"


#ifndef lineaxy
#if defined(XY)
#define lineaxy 1
#elif defined(XY2)
#define lineaxy 2
#endif
#endif

#include "MaxDuino.h"
#include "hwconfig.h"
#include "buttons.h"

#if defined(BLOCK_EEPROM_PUT) || defined(LOAD_EEPROM_LOGO) || defined(RECORD_EEPROM_LOGO) || defined(LOAD_EEPROM_SETTINGS)
#include "EEPROM.h"
#endif

char fline[17];

SdFat sd;                           //Initialise Sd card 
SdBaseFile entry, currentDir, tmpdir;                       //SD card file and directory

#ifdef FREERAM
  #define filenameLength 160
  #define nMaxPrevSubDirs 7  
#else 
  #define filenameLength 255
  #define nMaxPrevSubDirs 10  
#endif

char fileName[filenameLength + 1];    //Current filename
char prevSubDir[SCREENSIZE+1];
uint16_t DirFilePos[nMaxPrevSubDirs];  //File Positions in Directory to be restored (also, history of subdirectories)
byte subdir = 0;
unsigned long filesize;             // filesize used for dimensioning AY files

byte scrollPos = 0;                 //Stores scrolling text position
//unsigned long scrollTime = millis() + scrollWait;
unsigned long scrollTime = 0;

#ifndef NO_MOTOR
byte motorState = 1;                //Current motor control state
byte oldMotorState = 1;             //Last motor control state
#endif

byte start = 0;                     //Currently playing flag

bool pauseOn = false;                   //Pause state
uint16_t currentFile;               //File index (per filesystem) of current file, relative to current directory (pointed to by currentDir)
uint16_t maxFile;                   //Total number of files in directory
bool dirEmpty;                      //flag if directory is completely empty
uint16_t oldMinFile = 0;
uint16_t oldMaxFile = 0;

#ifdef SHOW_DIRNAMES
  #define fnameLength  5
  char oldMinFileName[fnameLength];
  char oldMaxFileName[fnameLength];
#endif

bool isDir;                         //Is the current file a directory
unsigned long timeDiff = 0;         //button debounce

#if (SPLASH_SCREEN && TIMEOUT_RESET)
    unsigned long timeDiff_reset = 0;
    byte timeout_reset = TIMEOUT_RESET;
#endif

char PlayBytes[17];

#ifdef BLOCKID_INTO_MEM 
unsigned long blockOffset[maxblock];
byte blockID[maxblock];
#endif

byte lastbtn=true;

#if (SPLASH_SCREEN && TIMEOUT_RESET)
    void(* resetFunc) (void) = 0;//declare reset function at adress 0
#endif

void setup() {

  #include "pinSetup.h"
  pinMode(chipSelect, OUTPUT);      //Setup SD card chipselect pin

  #ifdef LCDSCREEN20x4
    lcd.init();                     //Initialise LCD (16x2 type)
    lcd.backlight();
    lcd.clear();
    #if (SPLASH_SCREEN)
      lcd.setCursor(0,0);
      lcd.print(reinterpret_cast <const __FlashStringHelper *>(P_PRODUCT_NAME)); // Set the text at the initialization for LCD Screen (Line 1)
      lcd.setCursor(0,1); 
      lcd.print(reinterpret_cast <const __FlashStringHelper *>(P_VERSION)); // Set the text at the initialization for LCD Screen (Line 2)
      lcd.setCursor(0,2);
      lcd.print("Arduino ATMEGA328P");
      lcd.setCursor(0,3);
      lcd.print("Push button...");
    #endif   
  #endif
  
  #ifdef SERIALSCREEN
  Serial.begin(115200); 
  #endif
  

  setup_buttons();
 
  #ifdef SPLASH_SCREEN
    while (!button_any()){
      delay(100);                // Show logo (OLED) or text (LCD) and remains until a button is pressed.
    }   
  #endif

  while (!sd.begin(chipSelect, SD_SPI_CLOCK_SPEED)) {
    //Start SD card and check it's working
    printtextF(PSTR("No SD Card"),0);
    delay(50);
  }    

  changeDirRoot();
  UniSetup();                       //Setup TZX specific options
    
  printtextF(PSTR("Reset.."),0);
  delay(500);
  
  #ifdef LCDSCREEN20x4
    lcd.clear();
  #endif

       
  getMaxFile();                     //get the total number of files in the directory

  seekFile();            //move to the first file in the directory

  #ifdef LOAD_EEPROM_SETTINGS
    loadEEPROM();
  #endif  

}

extern unsigned long soft_poweroff_timer;

void loop(void) {
  if(start==1)
  {
    //TZXLoop only runs if a file is playing, and keeps the buffer full.
    uniLoop();
  } else {
    WRITE_LOW;    
  }
  
  if((millis()>=scrollTime) && start==0 && (strlen(fileName)> SCREENSIZE)) {
    //Filename scrolling only runs if no file is playing to prevent I2C writes 
    //conflicting with the playback Interrupt
    
    //default values in hwconfig.h: scrollSpeed=250 and scrollWait=3000
    //scrollTime = millis()+scrollSpeed;
    //if (scrollPos ==0) scrollTime+=(scrollWait-scrollSpeed);
    scrollTime = millis();
    scrollTime+= (scrollPos? scrollSpeed : scrollWait);
    scrollText(fileName);
    scrollPos = (scrollPos+1) %strlen(fileName) ;
  }

  #ifndef NO_MOTOR
    motorState=digitalRead(btnMotor);
  #endif
  
  #if (SPLASH_SCREEN && TIMEOUT_RESET)
    if (millis() - timeDiff_reset > 1000) //check timeout reset every second
    {
      timeDiff_reset = millis(); // get current millisecond count
      if (start==0)
      {
        timeout_reset--;
        if (timeout_reset==0)
        {
          timeout_reset = TIMEOUT_RESET;
          resetFunc();
        }
      }
      else
      {
        timeout_reset = TIMEOUT_RESET;
      }    
    }
  #endif
    
  if (millis() - timeDiff > 50) {   // check switch every 50ms 
    timeDiff = millis();           // get current millisecond count


    if(button_play()) {
      //Handle Play/Pause button
      if(start==0) {
        //If no file is play, start playback
        playFile();
        #ifndef NO_MOTOR
        if (mselectMask){  
          //Start in pause if Motor Control is selected
          oldMotorState = 0;
        }
        #endif
        delay(50);
        
      } else {
        //If a file is playing, pause or unpause the file                  
        if (!pauseOn) {
          printtext2F(PSTR("Paused  "),0);
          jblks =1; 
          firstBlockPause = true;
        } else  {
          printtext2F(PSTR("Playing      "),0);
          currpct=100;
          firstBlockPause = false;      
        }
        pauseOn = !pauseOn;
      }
      
      debounce(button_play);
    }

  #ifdef ONPAUSE_POLCHG
    if(button_root() && start==1 && pauseOn 
                                        #ifdef btnRoot_AS_PIVOT   
                                                && button_stop()
                                        #endif
                                                ){             // change polarity

      // change tsx speed control/zx polarity/uefTurboMode
      TSXCONTROLzxpolarityUEFSWITCHPARITY = !TSXCONTROLzxpolarityUEFSWITCHPARITY; 
      debounce(button_root);
    }
  #endif

  #ifdef btnRoot_AS_PIVOT
    lastbtn=false;     
    if(button_root() && start==0 && !lastbtn) {                                          // show min-max dir
      
      #ifdef SHOW_DIRPOS
        #if defined(LCDSCREEN20x4)
          #if !defined(SHOW_STATUS_LCD) && !defined(SHOW_DIRNAMES)
            char len=0;
            lcd.setCursor(0,0); 
      
            lcd.print(utoa(oldMinFile,input,10));
            lcd.print('<');
            len += strlen(input) + 1;
            lcd.print(utoa(currentFile,input,10));
            lcd.print('<');
            len += strlen(input) + 1;
            lcd.print(utoa(oldMaxFile,input,10));
            len += strlen(input); 
            for(char x=len;x<20;x++) {
              lcd.print(' '); 
            }
          #elif defined(SHOW_STATUS_LCD)        
            lcd.setCursor(0,3);
            lcd.print("Bd:");
            lcd.print(BAUDRATE);
            //lcd.print(' ');
            if(mselectMask) lcd.print(F(" M:ON"));
            else lcd.print(F(" M:off"));
            lcd.print(' ');
            if (TSXCONTROLzxpolarityUEFSWITCHPARITY) lcd.print(F("%^ON"));
            else lcd.print(F("%^off"));    
                 
          #elif defined(SHOW_DIRNAMES)
            str4cpy(input,fileName);
            GetFileName(oldMinFile);
            str4cpy(oldMinFileName,fileName);
            GetFileName(oldMaxFile);
            str4cpy(oldMaxFileName,fileName);
            GetFileName(currentFile); 
          
            lcd.setCursor(0,2);
            lcd.print(oldMinFileName);
            lcd.print(' ');
            lcd.print('<');
            lcd.print((char *)input);
            lcd.print('<');
            lcd.print(' ');
            lcd.print(oldMaxFileName);                  
          #endif
        #endif // defined(LCDSCREEN20x4)

      #endif // SHOW_DIRPOS
        
      while(button_root() && !lastbtn) {
        //prevent button repeats by waiting until the button is released.
        lastbtn = 1;
        checkLastButton();           
      }        
      printtext(PlayBytes,0);
    }
      
    #if defined(LCDSCREEN20x4) && defined(SHOW_BLOCKPOS_LCD)
      if(button_root() && start==1 && pauseOn && !lastbtn) {                                          // show min-max block
        lcd.setCursor(15,2);
        if (TSXCONTROLzxpolarityUEFSWITCHPARITY) {
          //lcd.print(F(" %^ON"));
        } else {
          //lcd.print(F(" %^off"));
        }
                
        while(button_root() && start==1 && !lastbtn) {
          //prevent button repeats by waiting until the button is released.
          lastbtn = 1;
          checkLastButton();           
        }

        lcd.setCursor(12,2);
        lcd.print(' ');
        lcd.print(' ');
        lcd.print(PlayBytes);        
      }
    #endif
  #endif // btnRoot_AS_PIVOT

  if(button_root() && start==0
                          #ifdef btnRoot_AS_PIVOT
                                && button_stop()
                          #endif        
                                ){                   // go menu

    #if (SPLASH_SCREEN && TIMEOUT_RESET)
      timeout_reset = TIMEOUT_RESET;
    #endif

    #if defined(Use_MENU) && !defined(RECORD_EEPROM_LOGO)
      menuMode();
      printtext(PlayBytes,0);
      #ifdef LCDSCREEN20x4
        printtextF(PSTR(""),1);
      #endif      
        
      scrollPos=0;
      scrollText(fileName);
    #elif defined(RECORD_EEPROM_LOGO)
      init_OLED();
      delay(1500);              // Show logo
      reset_display();           // Clear logo and load saved mode
      printtextF(PSTR("Reset.."),0);
      delay(500);
      strcpy_P(PlayBytes, P_PRODUCT_NAME);
      printtextF(P_PRODUCT_NAME, 0);
      #if defined(OSTATUSLINE)
        OledStatusLine();
      #endif
    #else             
      subdir=0;
      changeDirRoot();
      getMaxFile();
      seekFile();
    #endif         

    debounce(button_root);
  }

  if(button_stop() && start==1
                        #ifdef btnRoot_AS_PIVOT
                                && !button_root()
                        #endif
                              ){      

    stopFile();
    debounce(button_stop);
  }

  if(button_stop() && start==0 && subdir >0) {               // back subdir
    #if (SPLASH_SCREEN && TIMEOUT_RESET)
        timeout_reset = TIMEOUT_RESET;
    #endif     
    changeDirParent();

    debounce(button_stop);   
  }
     
  #ifdef BLOCKMODE
    if(button_up() && start==1 && pauseOn
                                  #ifdef btnRoot_AS_PIVOT
                                            && !button_root()
                                  #endif
                                          ){             //  up block sequential search                                                                 
      firstBlockPause = false;
      #ifdef BLOCKID_INTO_MEM
        oldMinBlock = 0;
        oldMaxBlock = maxblock;
        if (block > 0) block--;
        else block = maxblock;      
      #endif
      #if defined(BLOCK_EEPROM_PUT)
        oldMinBlock = 0;
        oldMaxBlock = 99;
        if (block > 0) block--;
        else block = 99;
      #endif
      #if defined(BLOCKID_NOMEM_SEARCH)
        if (block > jblks) block=block-jblks;
        else block = 0;
      #endif        

      GetAndPlayBlock();       
      debouncemax(button_up);
    }

    #if defined(btnRoot_AS_PIVOT)
      if(button_up() && start==1 && pauseOn && button_root()) {  // up block half-interval search
        if (block >oldMinBlock) {
          oldMaxBlock = block;
          block = oldMinBlock + (oldMaxBlock - oldMinBlock)/2;
        }

        GetAndPlayBlock();    
        debounce(button_up);
      }
    #endif
  #endif // BLOCKMODE

  if(button_up() && start==0
                        #ifdef btnRoot_AS_PIVOT
                              && !button_root()
                        #endif
                              ){                         // up dir sequential search                                           

    #if (SPLASH_SCREEN && TIMEOUT_RESET)
      timeout_reset = TIMEOUT_RESET;
    #endif
    //Move up a file in the directory
    scrollTime=millis()+scrollWait;
    scrollPos=0;
    upFile();
    debouncemax(button_up);
  }

  #ifdef btnRoot_AS_PIVOT
    if(button_up() && start==0 && button_root()) {      // up dir half-interval search
      #if (SPLASH_SCREEN && TIMEOUT_RESET)
        timeout_reset = TIMEOUT_RESET;
      #endif
      //Move up a file in the directory
      scrollTime=millis()+scrollWait;
      scrollPos=0;
      upHalfSearchFile();
      debounce(button_up);
    }
  #endif

  #if defined(BLOCKMODE) && defined(BLKSJUMPwithROOT)
    if(button_root() && start==1 && pauseOn) {      // change blocks to jump 

      if (jblks==BM_BLKSJUMP) {
        jblks=1;
      } else {
        jblks=BM_BLKSJUMP;
      }

      #ifdef LCDSCREEN20x4
        lcd.setCursor(18,2);
        if (jblks==BM_BLKSJUMP) {
          lcd.print(F("^"));
        } else {
          lcd.print(F("\'"));
        }
      #endif
  #endif
          
      debounce(button_root);
    }


  #ifdef BLOCKMODE
    if(button_down() && start==1 && pauseOn
                                          #ifdef btnRoot_AS_PIVOT
                                                && !button_root()
                                          #endif
                                                ){      // down block sequential search                                                           

      #ifdef BLOCKID_INTO_MEM
        oldMinBlock = 0;
        oldMaxBlock = maxblock;
        if (firstBlockPause) {
          firstBlockPause = false;
          if (block > 0) block--;
          else block = maxblock;
        } else {
          if (block < maxblock) block++;
          else block = 0;       
        }             
      #endif
      #if defined(BLOCK_EEPROM_PUT)
        oldMinBlock = 0;
        oldMaxBlock = 99;
        if (firstBlockPause) {
          firstBlockPause = false;
          if (block > 0) block--;
          else block = 99;
        } else {
          if (block < 99) block++;
          else block = 0;       
        }         
      #endif
      #if defined(BLOCKID_NOMEM_SEARCH)
        if (firstBlockPause) {
          firstBlockPause = false;
          if (block > 0) block--;
        } else {
          block = block + jblks;
        }         
      #endif

      GetAndPlayBlock();    
      debouncemax(button_down);
    }
  #endif

  #if defined(BLOCKMODE) && defined(btnRoot_AS_PIVOT)
    if(button_down() && start==1 && pauseOn && button_root()) {     // down block half-interval search
      if (block <oldMaxBlock) {
        oldMinBlock = block;
        block = oldMinBlock + 1+ (oldMaxBlock - oldMinBlock)/2;
      } 
      GetAndPlayBlock();    
      debounce(button_down);
    }
  #endif

  if(button_down() && start==0
                        #ifdef btnRoot_AS_PIVOT
                                && !button_root()
                        #endif
                              ){                    // down dir sequential search                                             
    #if (SPLASH_SCREEN && TIMEOUT_RESET)
      timeout_reset = TIMEOUT_RESET;
    #endif
    //Move down a file in the directory
    scrollTime=millis()+scrollWait;
    scrollPos=0;
    downFile();
    debouncemax(button_down);
  }

  #ifdef btnRoot_AS_PIVOT
    if(button_down() && start==0 && button_root()) {              // down dir half-interval search
      #if (SPLASH_SCREEN && TIMEOUT_RESET)
        timeout_reset = TIMEOUT_RESET;
      #endif
      //Move down a file in the directory
      scrollTime=millis()+scrollWait;
      scrollPos=0;
      downHalfSearchFile();
      
      debounce(button_down);
    }
  #endif

  #ifndef NO_MOTOR
    if(start==1 && (oldMotorState!=motorState) && mselectMask) {  
      //if file is playing and motor control is on then handle current motor state
      //Motor control works by pulling the btnMotor pin to ground to play, and NC to stop
      if(motorState==1 && !pauseOn) {
        printtext2F(PSTR("PAUSED  "),0);
        scrollPos=0;
        scrollText(fileName);
        pauseOn = true;
      } 
      if(motorState==0 && pauseOn) {
        printtext2F(PSTR("PLAYing "),0);
        scrollPos=0;
        scrollText(fileName);
        pauseOn = false;
      }
      oldMotorState=motorState;
    }
  #endif
  }
}

void upFile() {    
  // move up a file in the directory.
  // find prior index, using currentFile.
  if (dirEmpty) return;
  oldMinFile = 0;
  oldMaxFile = maxFile;
  while(currentFile!=0)
  {
    currentFile--;
    // currentFile might not point to a valid entry (since not all values are used)
    // and we might need to go back a bit further
    // Do this here, so that we only need this logic in one place
    // and so we can make seekFile dumber
    entry.close();
    if (entry.open(&currentDir, currentFile, O_RDONLY))
    {
      entry.close();
      break;
    }
  }

  if(currentFile==0)
  {
    // seek up wrapped - should now be reset to point to the last file
    currentFile = maxFile;
  }
  seekFile();
}

void downFile() {    
  //move down a file in the directory
  if (dirEmpty) return;
  oldMinFile = 0;
  oldMaxFile = maxFile;
  currentFile++;
  if (currentFile>maxFile) { currentFile=0; }
  seekFile();
}

void upHalfSearchFile() {    
  //move up to half-pos between oldMinFile and currentFile
  if (dirEmpty) return;

  if (currentFile >oldMinFile) {
    oldMaxFile = currentFile;
    currentFile = oldMinFile + (oldMaxFile - oldMinFile)/2;
    seekFile();
  }
}

void downHalfSearchFile() {    
  //move down to half-pos between currentFile amd oldMaxFile
  if (dirEmpty) return;

  if (currentFile <oldMaxFile) {
    oldMinFile = currentFile;
    currentFile = oldMinFile + 1+ (oldMaxFile - oldMinFile)/2;
    seekFile();
  } 
}

void seekFile() {    
  //move to a set position in the directory, store the filename, and display the name on screen.
  entry.close(); // precautionary, and seems harmless if entry is already closed
  if (dirEmpty)
  {
    strcpy_P(fileName, PSTR("[EMPTY]"));
  }
  else
  {
    while (!entry.open(&currentDir, currentFile, O_RDONLY) && currentFile<maxFile)
    {
      // if entry.open fails, when given an index, sdfat 2.1.2 automatically adjust curPosition to the next good entry
      // (which means that we can just retry calling open, passing in curPosition)
      // but sdfat 1.1.4 does not automatically adjust curPosition, so we cannot do that trick.
      // We cannot call openNext, because (with sdfat 2.1.2) the position has ALREADY been adjusted, so you will actually
      // miss a file.
      // so need to do this manually (to support both library versions), via incrementing currentFile manually
      currentFile++;
    }

    entry.getName(fileName,filenameLength);
    filesize = entry.fileSize();
    if(entry.isDir() || !strcmp(fileName, "ROOT")) { isDir=1; } else { isDir=0; }
    entry.close();
  }

  if (isDir==1) {
    if (subdir >0)strcpy(PlayBytes,prevSubDir);
    else strcpy_P(PlayBytes, P_PRODUCT_NAME);
    
    
  } else {
    ultoa(filesize,PlayBytes,10);
    strcat_P(PlayBytes,PSTR(" bytes"));
    
  }

  printtext(PlayBytes,0);
  
  scrollPos=0;
  scrollText(fileName);
  #ifdef SERIALSCREEN
    Serial.println(fileName);
  #endif
}

void stopFile() {
  TZXStop();
  if(start==1){
    printtextF(PSTR("Stopped"),0);
    #ifdef P8544
      lcd.gotoRc(3,38);
      lcd.bitmap(Stop, 1, 6);
    #endif
    start=0;
  }
}

void playFile() {
  if(isDir==1) {
    //If selected file is a directory move into directory
    changeDir();
  }
  else if (!dirEmpty) 
  {
    printtextF(PSTR("Playing"),0);
    scrollPos=0;
    pauseOn = false;
    scrollText(fileName);
    currpct=100;
    lcdsegs=0;
    UniPlay();
    UniPlay();   
    UniPlay();     
            
    start=1;       
  }    
}

void getMaxFile() {    
  // gets the total files in the current directory and stores the number in maxFile
  // and also gets the file index of the last file found in this directory
  currentDir.rewind();
  maxFile = 0;
  dirEmpty=true;
  while(entry.openNext(&currentDir, O_RDONLY)) {
    maxFile = currentDir.curPosition()/32-1;
    dirEmpty=false;
    entry.close();
  }
  currentDir.rewind(); // precautionary but I think might be unnecessary since we're using currentFile everywhere else now
  oldMinFile = 0;
  oldMaxFile = maxFile;
  currentFile = 0;
}

void changeDir() {    
  // change directory (to whatever is currently the selected fileName)
  // if fileName="ROOT" then return to the root directory
  if (dirEmpty) {
    // do nothing because you haven't selected a valid file yet and the directory is empty
    return;
  }
  else if(!strcmp(fileName, "ROOT"))
  {
    subdir=0;    
    changeDirRoot();
  }
  else
  {
    if (subdir < nMaxPrevSubDirs) {
      DirFilePos[subdir] = currentFile;
      subdir++;
      // but, also, stash the current filename as the parent (prevSubDir) for display
      // (but we only keep one prevSubDir, we no longer need to store them all)
      fileName[SCREENSIZE] = '\0';
      strcpy(prevSubDir, fileName);
    }
    tmpdir.open(&currentDir, currentFile, O_RDONLY);
    currentDir = tmpdir;
    tmpdir.close();
  }
  getMaxFile();
  seekFile();
}

void changeDirParent()
{
  // change up to parent directory.
  // get to parent folder via re-navigating the same sequence of directories, starting at root
  // (slow-ish but, honestly, not very slow. and requires very little memory to keep a deep stack of parents
  // since you only need to store one file index, not a whole filename, for each directory level)
  // Note to self : does sdfat now support any better way of doing this e.g. is there a link back to parent like ".."  ?
  subdir--;
  uint16_t this_directory=DirFilePos[subdir]; // remember what directory we are in currently

  changeDirRoot();
  if(subdir>0)
  {
    for(int i=0; i<subdir; i++)
    {
      tmpdir.open(&currentDir, DirFilePos[i], O_RDONLY);
      currentDir = tmpdir;
      tmpdir.close();
    }
    // get the filename of the last directory we opened, because this is now the prevSubDir for display
    currentDir.getName(fileName, filenameLength);
    fileName[SCREENSIZE] = '\0'; // copy only the piece we need. small cheat here - we terminate the string where we want it to end...
    strcpy(prevSubDir, fileName);
  }
   
  getMaxFile();
  currentFile = this_directory; // select the directory we were in, as the current file in the parent
  seekFile(); // don't forget that this will put the real filename back into fileName 
}

void changeDirRoot()
{
  currentDir.close();
  currentDir.open("/", O_RDONLY);                    //set SD to root directory
}

void scrollText(char* text){
  #ifdef LCDSCREEN20x4
  //Text scrolling routine.  Setup for 16x4 screen so will only display 16 chars
  if(scrollPos<0) scrollPos=0;
  char outtext[21];
  if(isDir) { outtext[0]= 0x3E; 
    for(int i=1;i<20;i++)
    {
      int p=i+scrollPos-1;
      if(p<strlen(text)) 
      {
        outtext[i]=text[p];
      } else {
        outtext[i]='\0';
      }
    }
  } else { 
    for(int i=0;i<20;i++)
    {
      int p=i+scrollPos;
      if(p<strlen(text)) 
      {
        outtext[i]=text[p];
      } else {
        outtext[i]='\0';
      }
    }
  }
  outtext[20]='\0';
  printtext(outtext,1);
  #endif


}

void printtext2F(const char* text, int l) {  //Print text to screen. 
  
  #ifdef SERIALSCREEN
  Serial.println(reinterpret_cast <const __FlashStringHelper *> (text));
  #endif
  
  #ifdef LCDSCREEN20x4
    lcd.setCursor(0,l);
    char x = 0;
    while (char ch=pgm_read_byte(text+x)) {
      lcd.print(ch);
      x++;
    }
  #endif

   
}

void printtextF(const char* text, int l) {  //Print text to screen. 
  
  #ifdef SERIALSCREEN
    Serial.println(reinterpret_cast <const __FlashStringHelper *> (text));
  #endif
  
  #ifdef LCDSCREEN20x4
    lcd.setCursor(0,l);
    char x = 0;
    while (char ch=pgm_read_byte(text+x)) {
      lcd.print(ch);
      x++;
    }
    for(x; x<20; x++) {
      lcd.print(' ');
    }
  #endif
   
}

void printtext(char* text, int l) {  //Print text to screen. 
  
  #ifdef SERIALSCREEN
    Serial.println(text);
  #endif
  
  #ifdef LCDSCREEN20x4
    lcd.setCursor(0,l);
    char ch;
    const char len = strlen(text);
    for(char x=0;x<20;x++) {
      if(x<len) {
        ch=text[x];
      } else {
        ch=0x20;
      }
      lcd.print(ch); 
    }
  #endif
  
}


void SetPlayBlock()
{
  printtextF(PSTR(" "),0);
  #ifdef LCDSCREEN20x4
    lcd.setCursor(0,2);
    lcd.print(F("BLK:"));
    lcd.print(block);
    lcd.print(F(" ID:"));
    lcd.print(currentID,HEX); // Block ID en hex
  #endif

  #ifdef SERIALSCREEN
    Serial.print(F("BLK:"));
    Serial.print(block, DEC);
    Serial.print(F(" ID:"));
    Serial.println(currentID, HEX);
  #endif


  clearBuffer();
  currpct=100; 
  lcdsegs=0;       
  currentBit=0;                               // fallo reproducción de .tap tras .tzx
  pass=0;

#ifdef Use_CAS
  if (casduino==CASDUINO_FILETYPE::NONE) // not a CAS / DRAGON file
#endif
  {
    currentBlockTask = BLOCKTASK::READPARAM;               //First block task is to read in parameters
    Timer.setPeriod(1000);
  }
}

void GetAndPlayBlock()
{
  #ifdef BLOCKID_INTO_MEM
    bytesRead=blockOffset[block%maxblock];
    currentID=blockID[block%maxblock];   
  #endif
  #ifdef BLOCK_EEPROM_PUT
    EEPROM_get(BLOCK_EEPROM_START+5*block, bytesRead);
    EEPROM_get(BLOCK_EEPROM_START+4+5*block, currentID);
  #endif
  #ifdef BLOCKID_NOMEM_SEARCH 
    unsigned long oldbytesRead;           //TAP
    bytesRead=0;                          //TAP 
    if (currentID!=TAP) bytesRead=10;   //TZX with blocks skip TZXHeader

    #ifdef BLKBIGSIZE
      int i = 0;
    #else
      byte i = 0;
    #endif      

    while (i<= block) {
      if(ReadByte()) {
        oldbytesRead = bytesRead-1;
        if (currentID!=TAP) currentID = outByte;  //TZX with blocks GETID
        if (currentID==TAP) bytesRead--;
      }
      else {
        block = i-1;
        break;
      }
        
      switch(currentID) {
        case ID10:  bytesRead+=2;     //Pause                
                    if(ReadWord()) bytesRead += outWord; //Length of data that follow
                    #if defined(OLEDBLKMATCH)
                      i++;
                    #endif
                    break;

        case ID11:  bytesRead+=15; //lPilot,lSynch1,lSynch2,lZ,lO,lP,LB,Pause
                    if(ReadLong()) bytesRead += outLong;
                    #if defined(OLEDBLKMATCH)
                      i++;
                    #endif                      
                    break;

        case ID12:  bytesRead+=4;
                    break;

        case ID13:  if(ReadByte()) bytesRead += (long(outByte) * 2);
                    break;

        case ID14:  bytesRead+=7;
                    if(ReadLong()) bytesRead += outLong;                  
                    break;

        case ID15:  bytesRead+=5;
                    if(ReadLong()) bytesRead += outLong; 
                    break;

        case ID19:  if(ReadDword()) bytesRead += outLong;
                    #if defined(OLEDBLKMATCH) //&& defined(BLOCKID19_IN)
                      i++;
                    #endif          
                    break;

        case ID20:  bytesRead+=2;
                    break;

        case ID21:  if(ReadByte()) bytesRead += outByte;
                    #if defined(OLEDBLKMATCH) && defined(BLOCKID21_IN)
                      i++;
                    #endif          
                    break;

        case ID22:  break;

        case ID24:  bytesRead+=2;
                    break;

        case ID25:  break;

        case ID2A:  bytesRead+=4;
                    break;

        case ID2B:  bytesRead+=5;
                    break;

        case ID30:  if (ReadByte()) bytesRead += outByte;                                            
                    break;

        case ID31:  bytesRead+=1;         
                    if(ReadByte()) bytesRead += outByte; 
                    break;

        case ID32:  if(ReadWord()) bytesRead += outWord;
                    break;

        case ID33:  if(ReadByte()) bytesRead += (long(outByte) * 3);
                    break;

        case ID35:  bytesRead += 0x10;
                    if(ReadDword()) bytesRead += outLong;
                    break;

        case ID4B:  if(ReadDword()) bytesRead += outLong;
                    #if defined(OLEDBLKMATCH)
                      i++;
                    #endif          
                    break;

        case TAP:   if(ReadWord()) bytesRead += outWord;
                    #if defined(OLEDBLKMATCH) && defined(BLOCKTAP_IN)
                      i++;
                    #endif           
                    break;
      }
      #if not defined(OLEDBLKMATCH)
        i++;
      #endif  
    }

    bytesRead=oldbytesRead;
  #endif   

  if (currentID==TAP) {
    currentTask=TASK::PROCESSID;
  }else {
    currentTask=TASK::GETID;    //Get new TZX Block
    if(ReadByte()) {
      //TZX with blocks GETID
      currentID = outByte;
      currentTask=TASK::PROCESSID;
    }
  }
   
  SetPlayBlock(); 
}

void str4cpy (char *dest, char *src)
{
  char x =0;
  while (*(src+x) && (x<4)) {
       dest[x] = src[x];
       x++;
  }
  for(x; x<4; x++) dest[x]=' ';
  dest[4]=0; 
}

void GetFileName(uint16_t pos)
{
  entry.close(); // precautionary, and seems harmless if entry is already closed
  if (entry.open(&currentDir, pos, O_RDONLY))
  {
    entry.getName(fileName,filenameLength);
  }
  entry.close();
}
