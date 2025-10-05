#include <LiquidCrystal.h>
#include <dht.h>
#include <uRTCLib.h>

/*
--- Pin Definitons  ---
*/
#define DHT_PIN 7   // Data pin for DHT11 sensor
#define SET_PIN 2   // SET button used for setting time and date, connected to ext interrupt pin (INT0)
#define INC_PIN A1  // Increment button used in setting mode
#define DEC_PIN A0  // Decrement button used in setting mode

/*
--- LCD Pin Mapping ---
*/
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 8;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

/*
--- DHT11 and RTC module objects ---
*/
dht DHT;
uRTCLib rtc(0x68);

/*
--- Array of day names (used for fitting dayOfWeek on the small LCD screen) ---
*/
char daysOfTheWeek[7][4] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

/*
--- Flags used by button interrupts ---
*/
volatile bool SET_BTN = false;
volatile bool INC_BTN = false;
volatile bool DEC_BTN = false;

/*
--- Disable unused pins to save power ---
*/
void disableUnusedPins(){
  const byte usedPins[] = {rs, en, d4, d5, d6, d7, DHT_PIN, A4, A5, 2};   //defining the pins which should not be disabled
  bool isUsed;    //flag for used pins

  for(byte i=0; i<20; i++){                       //for loop that iterates through all Arduino Pro Mini pins and flags them
    isUsed = false;
    for (byte j=0; j<sizeof(usedPins); j++){      //for loop that compares the previously defined pins and saves them from being disabled
      if (i == usedPins[j]){
        isUsed = true;
        break;
      }
    }
    if (!isUsed){
    pinMode(i, OUTPUT);
  }
  }
}

/*
--- Additional functions ---
*/
/* --function that make corrections for leap years based on a universal formula-- */
bool isLeapYear(int year) {           
    year += 2000;
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

void readSensorsAndDisplay(){         //function used for making sensor reads and updating the LCD
  rtc.refresh();

//displaying the Day Of The Week, month/day and time
  lcd.begin(16,2);
  lcd.clear();
  lcd.setCursor(1,0);
  lcd.print(daysOfTheWeek[rtc.dayOfWeek()-1]);
  lcd.print(" ");

//formatting date: MM/DD
  if (rtc.month() < 10){
    lcd.print('0');
  }
  lcd.print(rtc.month());lcd.print('/');

  if (rtc.day() < 10){
    lcd.print('0');
  }
  lcd.print(rtc.day());

  lcd.print(" ");

//formatting time: hh:mm
  if (rtc.hour() < 10){
    lcd.print('0');
  }
  lcd.print(rtc.hour());lcd.print(':');
  if (rtc.minute() < 10){
    lcd.print('0');
  }
  lcd.print(rtc.minute());

//printing temperature on the second row of the LCD
  lcd.setCursor(2,1);
  lcd.print("Temp: ");
  if (DHT.temperature < 10) {
    lcd.print('0');
  }
  lcd.print(DHT.temperature);
  lcd.print((char)223);/*"°" symbol*/ 
  lcd.print("C");

}

/* --function that triggers ISR when SET button is pressed-- */
void digitalInterrupt(){
  SET_BTN = true;
}

/* --function that triggers interrupt from Watchdog Time (used for waking up MCU)-- */
ISR(WDT_vect){
}

/* --function that disables Watchdog and INT0 interrupts (called when entering setting mode)-- */
void disableWDTandINT0(){
  detachInterrupt(0);
  cli();
  WDTCSR |= (1 << WDCE) | (1 << WDE);
  WDTCSR = 0x00;
  sei();
}

/* --function that enables Watchdog and INT0 interrupts (called when exiting setting mode)-- */
void enableWDTandINT0(){
  attachInterrupt(0, digitalInterrupt, RISING);
  cli();
  WDTCSR = (24);              //sets Watchdog into Interrupt Mode
  WDTCSR = (33);              //sets Watchdog Timer at 8s Time-outs
  WDTCSR |= (1<<6);           //enables watchdog interrupts
  sei();
}

/* 
 * Function that controls the clock's setting mode logic using a finite state machine (FSM).
 * Each state correspond to a different parameter (DOW, year, month, etc.), which can 
 * be modified using the INC/DEC buttons. The SET button transitions to the next state.
*/
void enterSettingMode(){

  disableWDTandINT0();        //disabling WDT and INT0 interrupts in order to make sure that all setting steps are set


  rtc.refresh();              //using local variables for RTC parameters (keeps code tidy)
  byte day = rtc.day();
  byte month = rtc.month();
  byte year = rtc.year();
  byte dow = rtc.dayOfWeek();
  byte hour = rtc.hour();
  byte minute = rtc.minute();

  bool set = true;            //flag used for setting mode
  byte settingStep = 0;       //defining the FSM state

  lcd.clear();

  while(set){
    lcd.setCursor(0,0);
    lcd.print("             ");
    lcd.setCursor(0,0);

/* 
 * FSM controlled by "settingStep" variable, which is modified by SET button.
 * Each step displays different text based on the current state. 
 * The data is also updated in real time as the user presses INC/DEC buttons.
*/
  switch(settingStep){                                               
    delay(50);                                                       
      case 0: lcd.print("Set Day: "); lcd.print(day); break;          
      case 1: lcd.print("Set Month: "); lcd.print(month); break;
      case 2: lcd.print("Set Year: "); lcd.print(year); break;
      case 3: lcd.print("Set DOW: "); lcd.print(daysOfTheWeek[dow - 1]); break;
      case 4: lcd.print("Set Hour: "); lcd.print(hour); break;
      case 5: lcd.print("Set Minute: "); lcd.print(minute); break;
  }

  if(digitalRead(SET_PIN) == HIGH){                         //function that controls FSM state by polling for the SET button
    delay(200);
    settingStep++;
    if(settingStep > 5){
      rtc.set(10, minute, hour, dow, day, month, year);     //after setting everything up, the RTC module is updated with the new data
    lcd.clear();
    SET_BTN = false;                                        //making sure the SET button is not stuck on "true"
    while(digitalRead(SET_PIN) == HIGH){                    //debouncing the SET button
      delay(50);
    }
    set = false;
  }
  }

  if(digitalRead(INC_PIN) == HIGH){                         //polling for the INC button and updating data corresponding to current FSM state
    delay(100);
    switch(settingStep){
        case 0: if(day < 31) day++; break;
        case 1: if(month < 12) month++; break;
        case 2: if(year < 99) year++; break;
        case 3: if(dow < 7) dow++; else dow = 1; break;
        case 4: if(hour < 23) hour++; break;
        case 5: if(minute < 59) minute++; break;      
    } 
  }

  if(digitalRead(DEC_PIN) == HIGH){                         //polling for the DEC button and updating data corresponding to current FSM state
    delay(100);
    switch(settingStep){
        case 0: if(day > 1) day--; break;
        case 1: if(month > 1) month--; break;
        case 2: if(year > 24) year--; break;
        case 3: if(dow > 1) dow--; else dow = 7; break;
        case 4: if(hour > 0) hour--; break;
        case 5: if(minute > 0) minute--; break;      
    } 
  }

  delay(50);
  }

  lcd.clear();
  SET_BTN = false;                                          //making sure the SET button is not stuck on "true"
  enableWDTandINT0();                                       //enabling Watchdog Timer and INT0 interrupts
}

/*
--- MCU setup function that manage all important features of the device ---
*/
void setup(){
  cli();                                                    //disables global interrupts in order to prevent interrupts when setting up the MCU

  disableUnusedPins();                                      //this function helps with overall power consumption

  attachInterrupt(0, digitalInterrupt, RISING);             //flagging DigitalPin0 rising edge as a digital interrupt

  URTCLIB_WIRE.begin();                                     //initializing I2C communication with the DS1307 RTC module

/*
--- Pin Initializations ---
*/
  pinMode(rs, OUTPUT);
  pinMode(en, OUTPUT);
  pinMode(d4, OUTPUT);
  pinMode(d5, OUTPUT);
  pinMode(d6, OUTPUT);
  pinMode(d7, OUTPUT);

  pinMode(DHT_PIN, INPUT);
  pinMode(SET_PIN, INPUT);
  pinMode(INC_PIN, INPUT_PULLUP);
  pinMode(DEC_PIN, INPUT_PULLUP);

/*
 * Assembly instructions used for working with the board peripherals.
 * These instructions set the rules for how these peripherals interact with the MCU.
*/
  WDTCSR = (24);                                            //sets Watchdog into Interrupt Mode
  WDTCSR = (33);                                            //sets Watchdog Timer at 8s Time-outs (largest interval available)
  WDTCSR |= (1<<6);                                         //enables watchdog interrupts

  ADCSRA &= ~(1<<7);                                        //sets ADEN bit to 0, disabling the ADC

  SMCR |= (1<<2);                                           //setting SM0-SM2 bits from SMCR to '010' for Power-Down mode
  SMCR |= 1;                                                //setting SE (sleep enable) bit

  sei();                                                    //enables global interrupts 
}

/*
--- MCU's Loop function that runs countiniously ---
*/
void loop(){ 
  if (SET_BTN){                                             //polling for interrupt flag when the MCU is not sleeping
    SET_BTN = false;                                        //disabling the interrupt flag
    delay(100);                                             //debouncing for the SET button in order to avoid another interrupt
    enterSettingMode();                                     //entering setting mode
    readSensorsAndDisplay();                                //updating the LCD with the new date, time and DHT11 readings
    while (digitalRead(SET_PIN) == HIGH){
      delay(50);                                            //debouncing for the SET button in order to avoid another interrupt                            
    }
    SET_BTN = false;                                        //making sure the interrupt flag is not set
    delay(100);
  }
  else {                                                    //if the interrupt flag is not triggered, the LCD updates with data
    readSensorsAndDisplay();                                //read just from the DHT11 sensor
  }

  for(byte i=0; i<5; i++){                                  //it gives 5 watchdogs cycles (~40s)   
    if(!SET_BTN)                                            //polling for interrupt flag before putting MCU to sleep (for user to be able to set whenever)
       __asm__  __volatile__("sleep");                      //putting the MCU into sleep mode (using an assembly instruction) in order to save power  
    else{                                                   //if interrupt flag is set, the ISR is triggered
      SET_BTN = false;                                      //disabling the set_btn flag since it already triggered the ISR
      delay(100);
      enterSettingMode();                                   //entering setting mode
      readSensorsAndDisplay();                              //updating the LCD with the new date, time and DHT11 readings                             
    while (digitalRead(SET_PIN) == HIGH){                   
      delay(50);                                            //debouncing for the SET button in order to avoid another interrupt 
    }
    SET_BTN = false;                                        //making sure the interrupt flag is not set
    delay(100);      
    }
  }    
}