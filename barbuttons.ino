/*
   BarButtons firmware v1

   This arduino code maps physical buttons from a keypad to bluetooth keyboard commands
   Intended to make using your phone for navigation on a motorcycle easier
   More info at https://jaxeadv.com/barbuttons
   
   Build instructions at https://jaxeadv.com/barbuttons/build
   
   Ensure to set DEBUG to 0 in the code below, unless you're connecting the ESP32C3 to the serial monitor

   This work is licensed under the Creative Commons Attribution-NonCommercial 4.0 International License. 
   To view a copy of this license, visit http://creativecommons.org/licenses/by-nc/4.0/ or send a letter to 
   Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.


*/

// Debugging flag. Mostly controls if serial output is enabled.
// On the C3, the serial output blocks the device unless the serial monitor is attached (!)
const int DEBUG = 0;

// Firmware version
const int firmware_version = 1;

#include <Keypad.h>      // Keypad library to handle matrix keypad setup
#include <BleKeyboard.h> // For ESP32 Bluetooth keyboard HID https://github.com/T-vK/ESP32-BLE-Keyboard

// Keypad library settings
const byte ROWS = 3;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'1', '5', '4'},
  {'2', '6', '7'},
  {'3', '8', '9'}
};

// Pin assignment, is fixed because of instructions and PCB
const int LED_PIN = 6;    //   status led 
byte rowPins[COLS] = {2, 1, 0};  // keypad pins, top to bottom
byte colPins[ROWS] = {3, 4, 5};  // keypad pins, left to right


// Initial set-up the bleKeyboard instance
BleKeyboard bleKeyboard("BarButtons", "JaxeADV", 100);


// For OTA updates
#include <WiFi.h>
#include <Update.h>
const char* SSID = "barbuttons";
const char* PSWD = "barbuttons";
String host = "jaxeadv.com";
int port = 80;
String ota_bin_stable =  "/barbuttons-files/barbuttons-stable.bin";  // bin file name with a slash in front.
String ota_bin_preview = "/barbuttons-files/barbuttons-preview.bin"; // bin file name with a slash in front.

// Initialization
/////////////////

// Initialize keypad library
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

int last_keypad_state = IDLE; // Used to distinguish between button release from HOLD or PRESSED.

// Initialize variable that determines how long a key is pressed to the current time in milliseconds
unsigned long hold_time = millis();

// define time for a long press (ms)
const int long_press_time = 500; // was: 440
const int long_press_repeat_interval = 100;
const int long_press_time_config = 4500; // Long press of 4,5 seconds, plus the 0,5 of the long_press_time = 5 seconds delay on the config mode button

int app_status = 0;       // holds the current status
int led_delays[4][2] = {  // holds the on/off pattern/times for the different status'es
  { 500,  500  },  // 0: Not connected to BT
  { 100,  3000 },  // 1: Connected (config menu)
  { 3000, 100  },  // 2: Connected (main menu led off)
  { 3000, 100  }   // 3: Connected (main menu led keymap flash)
};
int keymap_indicator_led_delays[2] = { 100, 50 };  // How long should flash the led on and off for keymap indicator?
int keymap_indicator_countdown = 0;  // keeps track of the countdown for the keymap flashes

int led_state = 0;       // holds the state of the led (0=LOW, 1=HIGH)
int led_state_time = 0;  // holds the time we've switched to the current led_state


// These keys will be sent instantly on initial press of the button at "key down"
// Other keys will be sent delayed on "key up"
// This is to be used for repeating keys or keys that are only pressed.
// First dimension is keymap, second is key
char instant_keys[10] = {'1', '2', '5', '6', '7', '8'};

// Routine to send the keystrokes on a short press of the keypad
void send_short_press(KeypadEvent key) {

  if (DEBUG) {
    Serial.print("Sending short press key ");
    Serial.println(key);
  }

  // We're in the main menu
  if (app_status == 2 || app_status == 3 || app_status == 0) {
    if (DEBUG) { Serial.println("We're in the main menu, switching key");  Serial.println(key); }
    
    switch (key) {
      case '1': bleKeyboard.write('+');                          flash_led(1, 150, 0); break;
      case '2': bleKeyboard.write('-');                          flash_led(1, 150, 0); break;
      case '3': bleKeyboard.write('n');                          flash_led(1, 150, 0); break;
      case '4': 
        // Center button
        bleKeyboard.write('c');  // Osmand and many others
        //bleKeyboard.press(KEY_LEFT_GUI); bleKeyboard.press('L'); bleKeyboard.releaseAll();  // Gurumaps on iOS
        bleKeyboard.press(KEY_LEFT_CTRL); bleKeyboard.press('L'); bleKeyboard.releaseAll();  // Gurumaps on Android
        flash_led(1, 150, 0); break;
      case '5': bleKeyboard.write(KEY_UP_ARROW);                 flash_led(1, 150, 0); break;
      case '6': bleKeyboard.write(KEY_LEFT_ARROW);               flash_led(1, 150, 0); break;
      case '7': bleKeyboard.write(KEY_RIGHT_ARROW);              flash_led(1, 150, 0); break;
      case '8': bleKeyboard.write(KEY_DOWN_ARROW);               flash_led(1, 150, 0); break;
    }
  }      
}

// Routine to send the keystrokes on a long press of the keypad
void send_long_press(KeypadEvent key) {

  if (DEBUG) {
    Serial.print("Sending long press key for button ");
    Serial.println(key);
  }

  // We're in the main menu, or offline
  if (app_status == 2 || app_status == 3 || app_status == 0) {

    switch (key) {
      case '1': send_repeating_key('+'); break;
      case '2': send_repeating_key('-'); break;
      case '3': bleKeyboard.write('d'); flash_led(1, 150, 0); break;
      case '4': if(wait_for_key_hold(long_press_time_config)) { update_barbuttons_firmware(ota_bin_stable); } break;
      case '5': send_repeating_key(KEY_UP_ARROW); break;
      case '6': send_repeating_key(KEY_LEFT_ARROW); break;
      case '7': send_repeating_key(KEY_RIGHT_ARROW); break;
      case '8': send_repeating_key(KEY_DOWN_ARROW); break;
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
    bleKeyboard.write(key);
    delay(long_press_repeat_interval); // pause between presses
    keypad.getKey(); // update keypad event handler
  }
  digitalWrite(LED_PIN, LOW);
}

// Routine that sends a key repeatedly (for double char 'MediaKeyReport')
void send_repeating_key(const MediaKeyReport key) {
  digitalWrite(LED_PIN, HIGH);
  while (keypad.getState() == HOLD) {
    bleKeyboard.write(key);
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
      if (is_key_instant(key) && app_status != 1) { send_short_press(key);}
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
        if (!(is_key_instant(key) && app_status != 1)) {
          send_short_press(key);
        }
      }
      
      last_keypad_state = keypad.getState();

      // Turn off the status led  in normal mode 
      if (app_status == 2 || app_status ==3) {
        digitalWrite(LED_PIN, LOW);
        led_state = 0;
        led_state_time = millis(); // update last switch of the led, so it will take long to flash for status (less confusing)
      }
      
      // Release all keys (write() should have done this, but keys that are press()-ed should be released)
      bleKeyboard.releaseAll();

      break;
    
    case IDLE: // not sure why this generates a callback
      if (DEBUG) { Serial.println("keypad.getState = IDLE"); }
      last_keypad_state = keypad.getState();

      // Turn off the status led  in normal mode 
      if (app_status == 2 || app_status ==3) {
        digitalWrite(LED_PIN, LOW);
        led_state = 0;
        led_state_time = millis(); // update last switch of the led, so it will take long to flash for status (less confusing)
      }

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

  // End of setup()
  if (DEBUG) {
    Serial.println("Good to go!");
  }
}

void loop() {

  // Need to call this function constantly to make the keypad library work
  keypad.getKey();

  // influence led state based on BT connectivity
  //
  // if app is disconnected but the keyboard is connected, change the app_status to connected (main menu)
  if (app_status == 0 && bleKeyboard.isConnected())  {
    app_status = 2;
  }
  // if app is connected and not in config mode but the keyboard is disconnected, change the app_status to disconnected
  if (app_status != 0 && app_status != 1 && !bleKeyboard.isConnected()) {
    app_status = 0;
  }

  // toggle between on/off for the led, when no buttons are pressed 
  if (keypad.getState() == IDLE) {    

    // When we are flashing for the keymap status, this is a little bit different
    if (app_status == 3) {
      // If the current led state is expired
      if ((millis() - led_state_time) > keymap_indicator_led_delays[led_state]) {
        led_state = 1 - led_state; // Toggle between 0 and 1
        digitalWrite(LED_PIN, led_state); // Update the led
        led_state_time = millis(); // update last switch

        if (led_state == 0) { // we just completed a keymap flash
          keymap_indicator_countdown--; 
          if (keymap_indicator_countdown == 0) {
            app_status = 2;           
          }
        }
      }  
    } else { 
      if ((millis() - led_state_time) > led_delays[app_status][led_state]) {
        led_state = 1 - led_state; // Toggle between 0 and 1
        digitalWrite(LED_PIN, led_state); // Update the led
        led_state_time = millis(); // update last switch

        // switch to app_status 3 (keymap flash) if led goes on in app_status 2
        if (app_status == 2 && led_state == 1) { 
          app_status = 3; 
          keymap_indicator_countdown = 1; 
        }
      }
    }
  }

  delay(10);
}



// Routine to start firmware update
// Used from esp32 library
// Also see https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html
//
void update_barbuttons_firmware(String bin) {

  // Variables to validate
  long contentLength = 0;
  bool isValidContentType = false;
  
  if (DEBUG) {
    Serial.println("Connecting to network: " + String(SSID));
  }

  // Connect to provided SSID and PSWD
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFiClient client;
  WiFi.begin(SSID, PSWD);
  WiFi.setTxPower(WIFI_POWER_8_5dBm); // required for Wemos C3


  // Wait for connection to establish
  while (WiFi.status() != WL_CONNECTED) {
    if (DEBUG) {
      Serial.print(".");
      Serial.print(WiFi.status());
    }
    digitalWrite(LED_PIN, HIGH);
    delay(250);
    digitalWrite(LED_PIN, LOW);
    delay(250);
  }
  digitalWrite(LED_PIN, HIGH);

  // Connection Succeed
  if (DEBUG) {
    Serial.println("");
    Serial.println("Connected to " + String(SSID));
  }

  // Execute OTA Update
  if (DEBUG) {
    Serial.println("Connecting to webserver: " + String(host));
  }

  if (client.connect(host.c_str(), port)) {
    // Connection Succeed.
    // Fecthing the bin
    if (DEBUG) {
      Serial.println("Fetching Bin: " + String(bin));
    }

    // Get the contents of the bin file
    client.print(String("GET ") + bin + "?mac="+ WiFi.macAddress() + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Cache-Control: no-cache\r\n" +
                 "User-Agent: BarButtons/1.0\r\n" +
                 "Connection: close\r\n\r\n");

    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        if (DEBUG) {
          Serial.println("Client Timeout !");
        }
        client.stop();
        return;
      }
    }
    // Once the response is available,
    // check stuff

    /*
       Response Structure
        HTTP/1.1 200 OK
        x-amz-id-2: NVKxnU1aIQMmpGKhSwpCBh8y2JPbak18QLIfE+OiUDOos+7UftZKjtCFqrwsGOZRN5Zee0jpTd0=
        x-amz-request-id: 2D56B47560B764EC
        Date: Wed, 14 Jun 2017 03:33:59 GMT
        Last-Modified: Fri, 02 Jun 2017 14:50:11 GMT
        ETag: "d2afebbaaebc38cd669ce36727152af9"
        Accept-Ranges: bytes
        Content-Type: application/octet-stream
        Content-Length: 357280
        Server: AmazonS3

        {{BIN FILE CONTENTS}}

    */

    while (client.available()) {
      // read line till /n
      String line = client.readStringUntil('\n');
      // remove space, to check if the line is end of headers
      line.trim();

      // if the the line is empty,
      // this is end of headers
      // break the while and feed the
      // remaining `client` to the
      // Update.writeStream();
      if (!line.length()) {
        //headers ended
        break; // and get the OTA started
      }

      // Check if the HTTP Response is 200
      // else break and Exit Update
      if (line.startsWith("HTTP/1.1")) {
        if (line.indexOf("200") < 0) {
          if (DEBUG) {
            Serial.println("Got a non 200 status code from server. Exiting OTA Update.");
          }
          break;
        }
      }

      // extract headers here
      // Start with content length
      if (line.startsWith("Content-Length: ")) {
        contentLength = atol((getHeaderValue(line, "Content-Length: ")).c_str());
        if (DEBUG) {
          Serial.println("Got " + String(contentLength) + " bytes from server");
        }
      }

      // Next, the content type
      if (line.startsWith("Content-Type: ")) {
        String contentType = getHeaderValue(line, "Content-Type: ");
        if (DEBUG) {
          Serial.println("Got " + contentType + " payload.");
        }
        if (contentType == "application/octet-stream") {
          isValidContentType = true;
        }
      }
    }
  } else {
    // Connect failed
    // May be try?
    // Probably a choppy network?
    if (DEBUG) {
      Serial.println("Connection to " + String(host) + " failed. Please check your setup");
    }
    // retry??
    // execOTA();
  }

  // Check what is the contentLength and if content type is `application/octet-stream`
  if (DEBUG) {
    Serial.println("contentLength : " + String(contentLength) + ", isValidContentType : " + String(isValidContentType));
  }

  // check contentLength and content type
  if (contentLength && isValidContentType) {
    // Check if there is enough to OTA Update
    bool canBegin = Update.begin(contentLength);

    // If yes, begin
    if (canBegin) {
      if (DEBUG) {
        Serial.println("Begin OTA. This may take 2 - 5 mins to complete. Things might be quite for a while.. Patience!");
      }
      // No activity would appear on the Serial monitor
      // So be patient. This may take 2 - 5mins to complete
      size_t written = Update.writeStream(client);

      if (written == contentLength) {
        if (DEBUG) {
          Serial.println("Written : " + String(written) + " successfully");
        }
      } else {
        if (DEBUG) {
          Serial.println("Written only : " + String(written) + "/" + String(contentLength) + ". Retry?" );
        }
        // retry??
        // execOTA();
      }

      if (Update.end()) {
        if (DEBUG) {
          Serial.println("OTA done!");
        }
        if (Update.isFinished()) {
          if (DEBUG) {
            Serial.println("Update successfully completed. Rebooting.");
          }
          ESP.restart();
        } else {
          if (DEBUG) {
            Serial.println("Update not finished? Something went wrong!");
          }
        }
      } else {
        if (DEBUG) {
          Serial.println("Error Occurred. Error #: " + String(Update.getError()));
        }
      }
    } else {
      // not enough space to begin OTA
      // Understand the partitions and
      // space availability
      if (DEBUG) {
        Serial.println("Not enough space to begin OTA");
      }
      client.flush();
    }
  } else {
    if (DEBUG) {
      Serial.println("There was no content in the response");
    }
    client.flush();
  }
}

// Utility to extract header value from headers
String getHeaderValue(String header, String headerName) {
  return header.substring(strlen(headerName.c_str()));
}

