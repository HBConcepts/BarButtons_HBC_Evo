/*
  BarButtons_HBC_Evo

  This is an evolution of the JaxeADV orignal barbuttons to replace the buttons used for 
  directions by a 5-way digital thumb stick. The center click of the thumb stick is unused (too hard to use while riding).

  This arduino code maps physical buttons from a keypad to bluetooth keyboard commands
  Intended to make using your phone for navigation on a motorcycle easier

  The default key mapping is made to work with the Kurviger app.
  There is also an optional Keymap Switch that can be installed to switch from the default (Kurviger) keymap to an alternate keymap.
  
  You can find the 3D printable models for an updated layout at https://cults3d.com/en/users/HBConcepts/3d-models

  You can flash this code to a LOLIN C3 Mini using the Arduino IDE available at https://www.arduino.cc/en/software/

  The libraies needed are the Keypad library and the HijelHID_BLEKeyboard library, both available from the library manager in the Arduino IDE.

  Original Build instructions at https://jaxeadv.com/barbuttons/build

  Ensure to set DEBUG to 0 in the code below, unless you're connecting the ESP32C3 to the serial monitor

  This work is licensed under the Creative Commons Attribution-NonCommercial 4.0 International License. 
  To view a copy of this license, visit http://creativecommons.org/licenses/by-nc/4.0/ or send a letter to 
  Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.


*/

// Debugging flag. Mostly controls if serial output is enabled.
const int DEBUG = 1;

// Firmware version
const int firmware_version = 4;

#include <Keypad.h>      // Keypad library to handle matrix keypad setup
#include <HijelHID_BLEKeyboard.h>

// Keypad library settings
const byte ROWS = 2;
const byte COLS = 5;
char keys[ROWS][COLS] = {  
  {'+', '-', 'A', 'B', '9'},
  {'U', 'D', 'L', 'R', 'C'}
};

// Pin assignment, is fixed because of instructions and PCB
const int LED_PIN = 7;    //   status led 
byte rowPins[ROWS] = {0, 1};  // keypad pins, top to bottom
byte colPins[COLS] = {2, 3, 4, 5, 6};  // keypad pins, left to right
const int SW_ALTKEYMAP = 8; // Alt Keymap Switch pin
const int KEYMAP_DEFAULT = 1;
const int KEYMAP_ALTERNATE = 2;

// Initial set-up the bleKeyboard instance
HijelHID_BLEKeyboard bleKeyboard("BarButtonsHBC", "HBConcepts", 100);

// Initialization
/////////////////

// Initialize keypad library
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

int last_keypad_state = IDLE; // Used to distinguish between button release from HOLD or PRESSED.

// Initialize variable that determines how long a key is pressed to the current time in milliseconds
unsigned long hold_time = millis();

// define time for a long press (ms)
const int long_press_time = 500;
const int long_press_repeat_interval = 100;
const int long_press_time_config = 4500; // Long press of 4,5 seconds, plus the 0,5 of the long_press_time = 5 seconds delay on the config mode button

int led_delays[2] = { 500,  500  }; // holds the on/off pattern/times 
int keymap_indicator_led_delays[2] = { 100, 50 };  // How long should flash the led on and off for keymap indicator?
int keymap_indicator_countdown = 0;  // keeps track of the countdown for the keymap flashes

int led_state = 0;       // holds the state of the led (0=LOW, 1=HIGH)
int led_state_time = 0;  // holds the time we've switched to the current led_state

// These keys will be sent instantly on initial press of the button at "key down"
// Other keys will be sent delayed on "key up"
// This is to be used for repeating keys or keys that are only pressed.
// First dimension is keymap, second is key
char instant_keys[10] = {'+', '-', 'U', 'L', 'R', 'D'};

byte readKeymapSwitch() {  
  if (digitalRead(SW_ALTKEYMAP) == LOW) {
    return KEYMAP_ALTERNATE; // Alternate keymap
  } else {
    return KEYMAP_DEFAULT; // Default keymap
  }
};

// Routine to send the keystrokes on a short press of the keypad
void send_short_press(KeypadEvent key) {

  if (DEBUG) {
    Serial.print("Sending short press key ");
    Serial.println(key);
    Serial.print("Keymap switch state: ");
    Serial.println(readKeymapSwitch());
  }

  if (DEBUG) { Serial.println("We're in the main menu, switching key");  Serial.println(key); }
  
  if (readKeymapSwitch() == KEYMAP_DEFAULT) {
    // Kurviger Keymap
    switch (key) {
      case '+': bleKeyboard.write('+');                          flash_led(1, 150, 0); break;
      case '-': bleKeyboard.write('-');                          flash_led(1, 150, 0); break;
      case 'A': bleKeyboard.write('a');                          flash_led(1, 150, 0); break;
      case 'B': bleKeyboard.write('c');                          flash_led(1, 150, 0); break;
      case 'U': bleKeyboard.tap(KEY_UP);                 flash_led(1, 150, 0); break;
      case 'L': bleKeyboard.tap(KEY_LEFT);               flash_led(1, 150, 0); break;
      case 'R': bleKeyboard.tap(KEY_RIGHT);              flash_led(1, 150, 0); break;
      case 'D': bleKeyboard.tap(KEY_DOWN);               flash_led(1, 150, 0); break;
    }
  } else  {
    // Alternate Keymap
    switch (key) {
      case '+': bleKeyboard.tap(KEY_F3);                         flash_led(1, 150, 0); break;
      case '-': bleKeyboard.tap(KEY_F4);                          flash_led(1, 150, 0); break;
      case 'A': bleKeyboard.tap(KEY_F5);                          flash_led(1, 150, 0); break;
      case 'B': bleKeyboard.tap(KEY_F6);                          flash_led(1, 150, 0); break;
      case 'U': bleKeyboard.tap(KEY_F7);                 flash_led(1, 150, 0); break;
      case 'L': bleKeyboard.tap(KEY_F8);               flash_led(1, 150, 0); break;
      case 'R': bleKeyboard.tap(KEY_F9);              flash_led(1, 150, 0); break;
      case 'D': bleKeyboard.tap(KEY_F10);               flash_led(1, 150, 0); break;
    }
  }  
}

// Routine to send the keystrokes on a long press of the keypad
void send_long_press(KeypadEvent key) {

  if (DEBUG) {
    Serial.print("Sending long press key for button ");
    Serial.println(key);
    Serial.print("Keymap switch state: ");
    Serial.println(readKeymapSwitch());
  }

  if (readKeymapSwitch() == KEYMAP_DEFAULT) {
    // Kurviger Keymap
    switch (key) {
      case '+': send_repeating_key('+'); break;
      case '-': send_repeating_key('-'); break;
      case 'A': bleKeyboard.write('b'); flash_led(1, 150, 0); break;
      case 'B': bleKeyboard.tap(KEY_F1); flash_led(1, 150, 0); break;
      case 'U': send_repeating_key(KEY_UP); break;
      case 'L': send_repeating_key(KEY_LEFT); break;
      case 'R': send_repeating_key(KEY_RIGHT); break;
      case 'D': send_repeating_key(KEY_DOWN); break;
    }
  } else {
    // Alternate Keymap
   switch (key) {
      case '+': send_repeating_key(KEY_F3); break;
      case '-': send_repeating_key(KEY_F4); break;
      case 'A': bleKeyboard.tap(KEY_F11); flash_led(1, 150, 0); break;
      case 'B': bleKeyboard.tap(KEY_F12); flash_led(1, 150, 0); break;
      case 'U': send_repeating_key(KEY_F7); break;
      case 'L': send_repeating_key(KEY_F8); break;
      case 'R': send_repeating_key(KEY_F9); break;
      case 'D': send_repeating_key(KEY_F10); break;
    }
  }
}


// Routine that waits while a key is held, returns true if the key is held
// for the entire time, otherwise false. Time is in milliseconds
bool wait_for_key_hold(int key_hold_time) {
  
  int wait_for_key_hold_start_time = millis();

  // Loop as long as both a button is held, and the time has not passed
  while (keypad.getState() == HOLD && millis() < (wait_for_key_hold_start_time + key_hold_time)) {
    delay(20); // Just wait a little
    keypad.getKey(); // update keypad event handler
  }
  
  // We are exiting the wait loop, is the button stil held?
  if (keypad.getState() == HOLD) {
      return true;
  } else {
    return false;
  }

}


// Routine that sends a key repeatedly (for single char)
void send_repeating_key(uint8_t key) {
  digitalWrite(LED_PIN, HIGH);
  while (keypad.getState() == HOLD) {
    bleKeyboard.tap(key);
    delay(long_press_repeat_interval); // pause between presses
    keypad.getKey(); // update keypad event handler
  }
  digitalWrite(LED_PIN, LOW);
}


// Quick flash of the led (assuming led is off)
void flash_led(int times, int length, int delay_time) {
  for (int i = 0; i < times; i++){
    
    digitalWrite(LED_PIN, HIGH);
    delay(length);
    digitalWrite(LED_PIN, LOW);

    //only wait on the 'inbetween' flashes, not at the end
    if (i < (times-1)) {
      delay(delay_time);
    }
  }
}

// Helper function to determine if a key is supposed to be instant, or we should send a key on "key up"
int is_key_instant(char key_pressed) {
  for ( int i = 0; i < sizeof (instant_keys); i++) {
    if ( instant_keys[i] == key_pressed) {
      return true;
    }
  }
  return false;
}

// This function handles events from the keypad.h library. Each event is only called once
void keypad_handler(KeypadEvent key) {

  if (DEBUG) { Serial.println("Entering keypad handler"); }

  // State changes for the buttons
  switch (keypad.getState()) {

    case PRESSED: // At the 'key down' event of a button
      if (DEBUG) { Serial.println("keypad.getState = PRESSED");}
      last_keypad_state = keypad.getState();
      if (is_key_instant(key))  { send_short_press(key);}
      break;

    case HOLD: // When a button is held beyond the long_press_time value
      if (DEBUG) { Serial.println("keypad.getState = HOLD"); }
      last_keypad_state = keypad.getState();
      send_long_press(key);
      break;    

    case RELEASED: // When a button is released
      if (DEBUG) {
        Serial.println("keypad.getState = RELEASED");
        Serial.print("Previous state: ");
        Serial.println(last_keypad_state);
      }

      if (last_keypad_state == PRESSED) {
        if (!(is_key_instant(key))) {
          send_short_press(key);
        }
      }
      
      last_keypad_state = keypad.getState();

      digitalWrite(LED_PIN, LOW);
      led_state = 0;
      led_state_time = millis(); // update last switch of the led, so it will take long to flash for status (less confusing)
      
      // Release all keys (write() should have done this, but keys that are press()-ed should be released)
      bleKeyboard.releaseAll();

      break;
    
    case IDLE: // not sure why this generates a callback
      if (DEBUG) { Serial.println("keypad.getState = IDLE"); }
      last_keypad_state = keypad.getState();

      digitalWrite(LED_PIN, LOW);
      led_state = 0;
      led_state_time = millis(); // update last switch of the led, so it will take long to flash for status (less confusing)

      // Release all keys (write() should have done this, but keys that are press()-ed should be released)
      bleKeyboard.releaseAll();

      break;
  }
}


// Arduino built-in setup loop
void setup() {
  
  if (DEBUG) { Serial.begin(9600); }

  // Start bluetooth keyboard 
  bleKeyboard.begin();

  // Handle all keypad events through this listener
  keypad.addEventListener(keypad_handler); // Add an event listener for this keypad

  // set HoldTime
  keypad.setHoldTime(long_press_time); 

  // Enable the led to indicate we're switched on
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(LED_PIN, 0); // LED off

  pinMode(SW_ALTKEYMAP, INPUT_PULLUP); // Alternate Keymap switch

  // End of setup()
  if (DEBUG) {
    Serial.println("Good to go!");
    Serial.println("Firmware version: " + String(firmware_version));
  }
  
}

void loop() {

  // Need to call this function constantly to make the keypad library work
  keypad.getKey();

  // if not paired, blink
  if (!bleKeyboard.isPaired() || led_state == 1) {
      // toggle between on/off for the led, when no buttons are pressed 
    if (keypad.getState() == IDLE) {    
      if ((millis() - led_state_time) > led_delays[led_state]) {
        led_state = 1 - led_state; // Toggle between 0 and 1
        digitalWrite(LED_PIN, led_state); // Update the led
        led_state_time = millis(); // update last switch
      }
    }
  }

  delay(10);
}


