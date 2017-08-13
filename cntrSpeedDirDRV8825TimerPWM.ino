// cntrSpeedDirDRV8825TimerPWM by impexeris
// This file was frorked from cntrSpeedDir.ino, which was originally created by lucadentella 
// It is adjusted for the DRV8825 motor driver chip. In fact for the Pololu DRV8825 board (adjustmets are simple - just added control for M0, M1 and M2.
// This file would get backwards compatible with A4988  by commenting out lines 42,43,44,77,78,79,83,84,85, -
// pins of the DRV8825 driver which alow to define microstepping mode on that particular board. In this particular case (1/32 microstepping for most smooth operation)
// another cardinal change is use of the Timer1 PWM output on Arduino pin9 instead of interrupt driven pulsing. Rationale for this decission is that for the
// simple constant driving of the motor PWM signal can be used from the Timer1, which is completely unblocking the micro-controller.



#include <LiquidCrystal.h>
#include <TimerOne.h>


// buttons code 
#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5

// directions
#define FORWARD   HIGH
#define BACKWARD  LOW

// debounce time (milliseconds)
#define DEBOUNCE_TIME  200

// PINs for Pololu controller
#define PIN_STEP  9
#define PIN_DIR   8

// lookup tables for PWM frequency and corresponding rotating speed - to display
// frequencies here and corresponding speeds to display were chosen in accordance to my measured ones on a geared solution (2.17 gear ratio) so you might wish to change them
// in accordance to your needs. By the way I am driving a NEMA17 motor (Busheng 17HD40005-22b) which is rated for 2 V and 1.3A current, 1.8 steps (200steps per full circle).
// I drive it from 12 V by limiting the current to 1.3A with the potentiometer on the Pololu DRV8825 board. This way miscrostepping works truly well.


const int speed_frequency[] = {0, 4320, 1440, 864, 617, 432, 332, 254, 216, 188, 160, 144, 131, 117, 108};
const int speed_table[] = {0, 1, 3, 5, 7, 10, 13, 17, 20, 23, 27, 30, 33, 37, 40}; // values of my geared RPM which correspond to frequencies in the speed_frequency table
// global variables
// I have chosen these particular LCD control pins in accordance to my particular solution so you might want to change them to your liking
LiquidCrystal lcd(2, 3, 4, 5, 6, 7);

int MODE0 = 10;
int MODE1 = 11;
int MODE2 = 12;


int actual_speed;
int display_speed;
int actual_direction;

int frequency;
int duty; 

int button;
boolean debounce;
boolean emergency;
unsigned long previous_time;


// custom LCD square symbol for progress bar
byte square_symbol[8] = {
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
};

// string constants
char forward_arrow[] = "-->";
char backward_arrow[] = "<--";

void setup() {

   pinMode(MODE0, OUTPUT); 
   pinMode(MODE1, OUTPUT); 
   pinMode(MODE2, OUTPUT);

   //Motor is now running in Microstepping mode (1/32)

   digitalWrite(MODE0, LOW);
   digitalWrite(MODE1, HIGH);
   digitalWrite(MODE2, HIGH);

  
//  Timer1.attachInterrupt(timerIsr);
  
  // init LCD and custom symbol  
  lcd.begin(16, 2);
  lcd.setCursor(0,0);
  lcd.createChar(0, square_symbol);
  
  // pins direction
  pinMode(PIN_STEP, OUTPUT);
  pinMode(PIN_DIR, OUTPUT);
  
  // initial values
  actual_speed = 0; // index for speed_frequency and speed_table
  actual_direction = FORWARD;
  duty = 250; // duty cycle for PWM. From practical tests it does not seem to be of importance when driving motor using the driver chips but should be more than 3 otherwise it starts
              // to to influence the rotation (PWM duty of Timer1 can be in the range of 0 to 1023 with 0 holding the PWM pin always low
  frequency = 0;
  debounce = false;
  emergency = false; // this flag is set when emergency stop button is pressed and is used to drive PWM duty to 0 to stop the mottor and display a message informing of the emergency
                     // stop and need to cycle the power to resume operation

  digitalWrite(PIN_DIR, actual_direction); // set the initial direction 
  
  updateLCD();

// initialize the timer but set the initial frequenc to 0
Timer1.initialize(frequency);
Timer1.pwm(PIN_STEP, duty); // start PWM on pin9 but with zero frequency and 0 duty - meaning no rotation. So this is just a part of initialization sequence

}
 
void loop() {

  // check if debounce active
  if(debounce) {
    button = btnNONE;
    if(millis() > previous_time + DEBOUNCE_TIME) debounce = false;
  } else button = read_buttons();
  
  // if a button is pressed, start debounce time
    if(button != btnNONE) {   
    previous_time = millis();
    debounce = true;  
    
  }
    
  // check which button was pressed
  switch(button) {
    
    case btnUP:
      increase_speed();
      break;
    case btnDOWN:
      decrease_speed();
      break;
    case btnLEFT:
      change_direction(BACKWARD);
      break;
    case btnRIGHT:
      change_direction(FORWARD);
      break;
    case btnSELECT:
      emergency_stop();
      break;
  }
  
  // finally update the LCD
  updateLCD();
}

// increase speed if it's below the max (70)
void increase_speed() {
  
  if(actual_speed < 14) {
    actual_speed += 1;
    frequency = speed_frequency[actual_speed];
    Timer1.setPeriod(frequency);
    Timer1.setPwmDuty(PIN_STEP, duty);
    display_speed=speed_table[actual_speed];
  }
}

// decrease speed if it's above the min (0)
void decrease_speed() {
  
  if(actual_speed > 0) {
    actual_speed -= 1;
    frequency = speed_frequency[actual_speed];
    Timer1.setPeriod(frequency);
    Timer1.setPwmDuty(PIN_STEP, duty);
    display_speed=speed_table[actual_speed];
  }
}

// change direction if needed
void change_direction(int new_direction) {
  
  if(actual_direction != new_direction) {
    actual_direction = new_direction;
    digitalWrite(PIN_DIR, actual_direction);
  }
}

// emergency stop: speed 0
void emergency_stop() {
duty=0;
Timer1.setPwmDuty(PIN_STEP, duty);
actual_speed = 0;
frequency = 0;
display_speed = 0;
emergency = true;
}

// update LCD
void updateLCD() {
  
  if (emergency) {
  lcd.setCursor(0,0);
  lcd.print(" STOP requested ");
  lcd.setCursor(0,1);
  lcd.print(" recycle power  ");
  } 

 else {
  
   // print first line:
  // Speed: xxxRPM --> (or <--)
  lcd.setCursor(0,0);
  lcd.print("Speed: ");
  lcd.print(display_speed);
  lcd.print("RPM ");

  lcd.setCursor(13,0);
  if(actual_direction == FORWARD) lcd.print(forward_arrow);
  else lcd.print(backward_arrow);
  
  // print second line:
  // progress bar [#####         ]
  // 15 speed steps: 0 - 1 - 3 - ... - 40
  

  
  lcd.setCursor(0,1);
  lcd.print("[");
  
  for(int i = 1; i <= 14; i++) {
    
    if(actual_speed > i-1) lcd.write(byte(0));
    else lcd.print(" ");
  }
  lcd.print("]");
 }
  
}

// timer1 interrupt function
//void timerIsr() {

//  if(actual_speed == 0) return;
  
//  tick_count++;
  
//  if(tick_count == ticks) {  
    
//    // make a step
//    digitalWrite(PIN_STEP, HIGH);
//    digitalWrite(PIN_STEP, LOW);
    
//    tick_count = 0;
//  }
//}

// read buttons connected to a single analog pin
int read_buttons() {
  
 int adc_key_in = analogRead(0);
 
 if (adc_key_in >= 790) return btnNONE;
 if (adc_key_in < 50)   return btnRIGHT;  
 if (adc_key_in < 195)  return btnUP; 
 if (adc_key_in < 380)  return btnDOWN; 
 if (adc_key_in < 555)  return btnLEFT; 
 if (adc_key_in < 790)  return btnSELECT;
    
}
