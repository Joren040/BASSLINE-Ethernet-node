// DMX-Ethernet Node (Art-Net / sACN)
// This sketch provides a dual-universe DMX output device that can be configured via an
// on-device menu using an OLED screen and physical buttons. Settings are saved to EEPROM

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>           // Include Wire library for I2C communication
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Ethernet.h>
#include <ArtnetEther.h>    // Please include ArtnetEther.h to use Artnet on the platform which can use both WiFi and Ethernet
#include <sACN.h>
#include <DmxOutput.h>
#include "config.h"         // User-defined settings (MAC, DEFAULT_IP, DEFAULT_UNIVERSE, OLED logo.)

// EEPROM Constants (24LC256-I/ST)
#define EEPROM_ADDRESS 0x50       // I2C address of 24LC256 (A0, A1, A2 tied low)
#define EEPROM_INIT_FLAG 0xA5     // Magic byte to indicate valid data is stored

// EEPROM Memory Map (10 bytes total storage)
#define EEPROM_FLAG_ADDR    0x0000 // 1 byte: Initialization flag
#define EEPROM_IP_ADDR      0x0001 // 4 bytes: IP Address (ip[0]...ip[3])
#define EEPROM_PROTOCOL_ADDR 0x0005 // 1 byte: Protocol Type
#define EEPROM_UNIA_ADDR    0x0006 // 2 bytes: Universe A
#define EEPROM_UNIB_ADDR    0x0008 // 2 bytes: Universe B

//OLED Stuff
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Ethernet and Protocol Globals
ArtnetReceiver artnet;     //Artnet Receiver object

EthernetUDP sacn;         //1 UDP object since i have 1 connection

Receiver recv1(sacn);     //2 receiver objects coupled to the same UDP object
Receiver recv2(sacn);

// DMX Stuff
#define OUTPUT_A_GPIO 0         // Output pin of DMX universe A
#define OUTPUT_B_GPIO 1         // Output pin of DMX universe A
#define UNIVERSE_LENGTH 512     // Size of DMX universe

// Hardware pins
const int setupPin = 9;         // the number of the setupbutton pin
const int upPin = 8;            // the number of the upbutton pin
const int downPin = 7;          // the number of the downbutton pin
const int enterPin = 6;         // the number of the enterbutton pin
const int netwerRSTPin = 20;    // the number of the netwerk chip reset pin
const int netwerkCSNPin = 17;   // the number of the netwerk chip reset pin

// variables to save the button states
int setupState = 0;
int upState = 0;
int downState = 0;
int enterState = 0;
int enterCount = 0;

// in MenuMode or not
bool MenuMode = false;

// Flag to check if settings have been completed
bool settingFlag = LOW;

// Flag to check if Debug mode has been chosen
bool debugFlag = LOW;

// Buffers to store the individual universes
uint8_t channelsA[UNIVERSE_LENGTH + 1];
uint8_t channelsB[UNIVERSE_LENGTH + 1];

// Init 2 DMX output
DmxOutput dmx[2];

// Function prototypes
void mainMenu();
void settings();
void resetNetwerk();
void exitProgram();
void dmxReceivedA();
void dmxReceivedB();
void saveSettingsToEEPROM();
void loadSettingsFromEEPROM();
void loadDefaultSettingsToRAM();

// --- EEPROM HELPER FUNCTIONS ---
// Function to write a byte to a 16-bit address
void eepromWriteByte(uint16_t address, uint8_t data) {
    Wire.beginTransmission(EEPROM_ADDRESS);
    Wire.write((uint8_t)(address >> 8));    // MSB address
    Wire.write((uint8_t)(address & 0xFF)); // LSB address
    Wire.write(data);
    Wire.endTransmission();
    // 24LC256 requires a short delay for write cycle completion
    delay(5);
}
// Function to read a byte from a 16-bit address
uint8_t eepromReadByte(uint16_t address) {
    uint8_t rdata = 0xFF;
    // Set the address pointer
    Wire.beginTransmission(EEPROM_ADDRESS);
    Wire.write((uint8_t)(address >> 8));
    Wire.write((uint8_t)(address & 0xFF));
    Wire.endTransmission();
    // Read the byte
    Wire.requestFrom(EEPROM_ADDRESS, 1);
    if (Wire.available()) {
        rdata = Wire.read();
    }
    return rdata;
}
// Function to write a 16-bit integer (High Byte first)
void eepromWriteInt(uint16_t address, uint16_t data) {
    eepromWriteByte(address, (uint8_t)(data >> 8));      // MSB
    eepromWriteByte(address + 1, (uint8_t)(data & 0xFF)); // LSB
}
// Function to read a 16-bit integer (High Byte first)
uint16_t eepromReadInt(uint16_t address) {
    uint16_t data = 0;
    data = eepromReadByte(address) << 8;
    data |= eepromReadByte(address + 1);
    return data;
}
// Function to write all global settings to EEPROM
void saveSettingsToEEPROM() {
    Serial.println("Saving settings to EEPROM...");
    for (int i = 0; i < 4; i++) {
        eepromWriteByte(EEPROM_IP_ADDR + i, ip[i]);         // 1. IP Address (4 bytes)
    }
    eepromWriteByte(EEPROM_PROTOCOL_ADDR, PROTOCOL_TYPE);   // 2. Protocol Type (1 byte)
    eepromWriteInt(EEPROM_UNIA_ADDR, OUTPUT_A_UNIVERSE);    // 3. Universe A (2 bytes)
    eepromWriteInt(EEPROM_UNIB_ADDR, OUTPUT_B_UNIVERSE);    // 4. Universe B (2 bytes)
    eepromWriteByte(EEPROM_FLAG_ADDR, EEPROM_INIT_FLAG);    // 5. Write Magic Flag last to indicate valid data
    Serial.println("Settings saved.");
}
// Function to read all settings from EEPROM into RAM
void loadSettingsFromEEPROM() {
    Serial.println("Loading settings from EEPROM...");
    for (int i = 0; i < 4; i++) {
        ip[i] = eepromReadByte(EEPROM_IP_ADDR + i);         // 1. IP Address (4 bytes)
    }
    PROTOCOL_TYPE = eepromReadByte(EEPROM_PROTOCOL_ADDR);   // 2. Protocol Type (1 byte)
    OUTPUT_A_UNIVERSE = eepromReadInt(EEPROM_UNIA_ADDR);    // 3. Universe A (2 bytes)
    OUTPUT_B_UNIVERSE = eepromReadInt(EEPROM_UNIB_ADDR);    // 4. Universe B (2 bytes)
    Serial.println("Settings loaded.");
}
// Function to load default settings from config.h into RAM
void loadDefaultSettingsToRAM() {
    Serial.println("Loading default settings from config.h into RAM...");
    memcpy(ip, DEFAULT_IP, 4);
    OUTPUT_A_UNIVERSE = DEFAULT_OUTPUT_A_UNIVERSE;
    OUTPUT_B_UNIVERSE = DEFAULT_OUTPUT_B_UNIVERSE;
    PROTOCOL_TYPE = DEFAULT_PROTOCOL_TYPE;
    Serial.println("Defaults loaded into RAM.");
}
// --- sACN DMX PACKET CALLBACK HANDLER (Replaces checkSACN) ---
// This function is called by the sACN library when a valid DMX packet is received for a subscribed universe.
void dmxReceivedA() {
    if (debugFlag == HIGH){
        Serial.print("s/ACN A: ");
    }
    for (size_t i = 0; i < UNIVERSE_LENGTH+1; ++i) {
        if (debugFlag == HIGH){
            Serial.print(recv1.dmx(i));
            Serial.print(",");
        }
        channelsA[i] = recv1.dmx(i);
    }
    if (debugFlag == HIGH){
        Serial.println();
    }
    //dmx[0].write(channelsA, UNIVERSE_LENGTH+1);
}

void dmxReceivedB() {
    if (debugFlag == HIGH){
        Serial.print("s/ACN B: ");
    }
    for (size_t i = 0; i < UNIVERSE_LENGTH+1; ++i) {
        if (debugFlag == HIGH){
            Serial.print(recv2.dmx(i));
            Serial.print(",");
        }
        channelsB[i] = recv2.dmx(i);
    }
    if (debugFlag == HIGH){
        Serial.println();
    }
    //dmx[1].write(channelsB, UNIVERSE_LENGTH+1);
}
//main Menu function
void mainMenu(){
    static int selection = 0;
    const int menuLength = 3;

    if (digitalRead(upPin) == HIGH) {
        selection--;
        if (selection < 0) selection = menuLength - 1;
        delay(200);
    }
    if (digitalRead(downPin) == HIGH) {
        selection++;
        if (selection >= menuLength) selection = 0;
        delay(200);
    }
    display.clearDisplay();
    display.setTextSize(1);
    for (int i = 0; i < menuLength; i++) {
        if (i == selection) {
            display.setTextColor(BLACK, WHITE);
        } else {
            display.setTextColor(WHITE);
        }
        display.setCursor(0, i * 10);
        if (i == 0) display.println("Settings");
        if (i == 1) display.println("Reset Network");
        if (i == 2) display.println("Exit");
    }
    display.display();
    if (digitalRead(enterPin) == HIGH) {
        delay(200);
        if (selection == 0) {
            settings();
        } else if (selection == 1) {
            resetNetwerk();
        } else if (selection == 2) {
            exitProgram();
        }
    }
}
//settings function
void settings(){
    Serial.println("enter setup menu");      // show we entered the function
    enterCount = 0; // Reset enterCount to start from IP 0
    // Arrays for display cleanup (only used internally)
    const char* debug_options[] = {"YES", "NO"};
    int debug_selection = debugFlag ? 0 : 1;

    while(settingFlag == LOW){              // as long as the setupflag is low remain in settings loop
        // Read all buttons once per loop iteration for responsiveness
        upState = digitalRead(upPin);
        downState = digitalRead(downPin);
        enterState = digitalRead(enterPin);
        // --- IP ADDRESS SETTINGS (enterCount 0-3) ---
        if(enterCount < 4){
            display.clearDisplay();         // clear display before use
            display.setTextSize(1);
            display.setTextColor(WHITE);
            display.setCursor(0, 0);
            display.println("IP address");      // Display static text
            display.setCursor(0, 14);
            // --- START: VERBOSE IP DISPLAY LOGIC ---
            // IP Segment 0
            if(enterCount == 0){ display.setTextColor(BLACK, WHITE); }
            else{ display.setTextColor(WHITE); }
            display.print(ip[0]);
            display.setTextColor(WHITE);
            display.print('.');
            // IP Segment 1
            if(enterCount == 1){ display.setTextColor(BLACK, WHITE); }
            else{ display.setTextColor(WHITE); }
            display.print(ip[1]);
            display.setTextColor(WHITE);
            display.print('.');
            // IP Segment 2
            if(enterCount == 2){ display.setTextColor(BLACK, WHITE); }
            else{ display.setTextColor(WHITE); }
            display.print(ip[2]);
            display.setTextColor(WHITE);
            display.print('.');
            // IP Segment 3
            if(enterCount == 3){ display.setTextColor(BLACK, WHITE); }
            else{ display.setTextColor(WHITE); }
            display.print(ip[3]);
            // --- END: VERBOSE IP DISPLAY LOGIC ---
            // Read buttons and apply logic with bounds check (0-255)
            if (upState == HIGH){
                if (ip[enterCount] < 255) {
                    ip[enterCount]++;
                }
                delay(200);
            }
            if (downState == HIGH){
                if (ip[enterCount] > 0) {
                    ip[enterCount]--;
                }
                delay(200);
            }
            if(enterState == HIGH){
                enterCount = enterCount + 1;
                delay(300);
            }
        }
        else if (enterCount == 4) {
            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(WHITE);
            display.setCursor(0, 0);
            display.println("Protocol:");
            display.setCursor(0, 14);
            display.setTextColor(PROTOCOL_TYPE == 0 ? BLACK : WHITE, PROTOCOL_TYPE == 0 ? WHITE : BLACK);
            display.print("Art-Net");
            display.setCursor(64, 14);
            display.setTextColor(PROTOCOL_TYPE == 1 ? BLACK : WHITE, PROTOCOL_TYPE == 1 ? WHITE : BLACK);
            display.print("sACN");
            display.display();

            upState = digitalRead(upPin);
            downState = digitalRead(downPin);
            if (upState == HIGH || downState == HIGH) {
                PROTOCOL_TYPE = !PROTOCOL_TYPE;
                delay(200);
            }
            if(enterState == HIGH){
                enterCount = enterCount + 1;
                delay(300);
            }
        }
        // --- UNIVERSE SETTINGS (enterCount 5, 6) ---
        else if(enterCount == 5 || enterCount == 6){
            display.clearDisplay();         // clear display before use
            display.setTextSize(1);
            display.setTextColor(WHITE);
            display.setCursor(0, 0);
            display.println("set universes");     // Display static text
            // Output A (enterCount 5)
            display.setCursor(0, 14);
            display.print("Output A: ");
            if(enterCount == 5){
                display.setTextColor(BLACK, WHITE);
            } else {
                display.setTextColor(WHITE);
            }
            display.print(OUTPUT_A_UNIVERSE);
            if(enterCount == 5){
                if (upState == HIGH){
                    OUTPUT_A_UNIVERSE++; // Rollover handled by uint16_t
                    delay(200);
                }
                if (downState == HIGH){
                    OUTPUT_A_UNIVERSE--;
                    delay(200);
                }
            }
            // Output B (enterCount 6)
            display.setCursor(0, 24);
            display.setTextColor(WHITE);
            display.print("Output B: ");
            if(enterCount == 6){
                display.setTextColor(BLACK, WHITE);
            } else {
                display.setTextColor(WHITE);
            }
            display.print(OUTPUT_B_UNIVERSE);
            if(enterCount == 6){
                if (upState == HIGH){
                    OUTPUT_B_UNIVERSE++;
                    delay(200);
                }
                if (downState == HIGH){
                    OUTPUT_B_UNIVERSE--;
                    delay(200);
                }
            }
            // Read enter button and advance count
            if(enterState == HIGH){
                enterCount = enterCount + 1;
                delay(300);
            }
        }
        // --- DEBUG MODE TOGGLE (enterCount 7) ---
        else if(enterCount == 7){
            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(WHITE);
            display.setCursor(0, 0);
            display.println("Debug Mode?");
            display.setCursor(0, 14);
            // Toggle logic using up/down
            if (upState == HIGH || downState == HIGH){
                debugFlag = !debugFlag; // Toggle selection
                debug_selection = debugFlag ? 0 : 1;
                delay(200);
            }
            // Display YES/NO with highlight
            for (int i = 0; i < 2; i++){
                if (i == debug_selection) {
                    display.setTextColor(BLACK, WHITE);
                } else {
                    display.setTextColor(WHITE);
                }
                display.print(debug_options[i]);
                display.print("   "); // Add space between options
            }
            // Advance count
            if(enterState == HIGH){
                enterCount = enterCount + 1;
                delay(300);
            }
        }
        // --- EXIT CONDITION ---
        if(enterCount >= 8){
            saveSettingsToEEPROM(); // Save newly set values to EEPROM
            settingFlag = HIGH;     // set setupflag high to leave settings menu loop
        }
        display.display(); // Only update display once per loop
    }
    settingFlag = LOW; // Reset flag on exit
}

void resetNetwerk(){
    static int selection = 0;
    bool resetNetworkFlag = true;

    while (resetNetworkFlag) {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(0, 0);
        display.println("Reset Network?");

        if (selection == 0) {
            display.setTextColor(BLACK, WHITE);
            display.setCursor(0, 14);
            display.println("No");
            display.setTextColor(WHITE);
            display.setCursor(0, 24);
            display.println("Yes");
        } else {
            display.setTextColor(WHITE);
            display.setCursor(0, 14);
            display.println("No");
            display.setTextColor(BLACK, WHITE);
            display.setCursor(0, 24);
            display.println("Yes");
        }
        display.display();

        if (digitalRead(upPin) == HIGH) {
            selection = 0;
            delay(200);
        }
        if (digitalRead(downPin) == HIGH) {
            selection = 1;
            delay(200);
        }
        if (digitalRead(enterPin) == HIGH) {
            delay(200);
            if (selection == 0) {
                resetNetworkFlag = false;
            } else if (selection == 1) {
                display.clearDisplay();
                display.setCursor(0, 0);
                display.println("Resetting Network");
                display.println("Restoring defaults...");
                display.display();
                loadDefaultSettingsToRAM();            // Load defaults into RAM
                saveSettingsToEEPROM();                // Save defaults to EEPROM
                // The following hardware reset logic has been commented out as it is not needed without persistent storage
                // memcpy(ip, DEFAULT_IP, 4);
                // OUTPUT_A_UNIVERSE = DEFAULT_OUTPUT_A_UNIVERSE;
                // OUTPUT_B_UNIVERSE = DEFAULT_OUTPUT_B_UNIVERSE;
                // PROTOCOL_TYPE = DEFAULT_PROTOCOL_TYPE;
                digitalWrite(netwerRSTPin, LOW);
                delay(100);
                digitalWrite(netwerRSTPin, HIGH);
                delay(500);
                display.clearDisplay();
                display.setCursor(0, 0);
                display.println("Netwerk reset done");
                display.display();
                delay(1000);
                resetNetworkFlag = false;
            }
        }
        if (digitalRead(setupPin) == HIGH) {
            resetNetworkFlag = false;
            delay(200);
        }
    }
}

void exitProgram() {
    Serial.println("Exiting Program...");
    MenuMode = false;
}

void setup() {
    Serial.begin(115200);
    Wire.begin();               // Initialize the I2C bus
    pinMode(setupPin, INPUT);   // set buttons as inputs externall pull down and debounce cap prese
    pinMode(upPin, INPUT);
    pinMode(downPin, INPUT);
    pinMode(enterPin, INPUT);
    Ethernet.init(netwerkCSNPin);
    dmx[0].begin(OUTPUT_A_GPIO);    // init DMX objects
    dmx[1].begin(OUTPUT_B_GPIO);
    // --- EEPROM LOAD/INIT LOGIC ---
    if (eepromReadByte(EEPROM_FLAG_ADDR) != EEPROM_INIT_FLAG) {      // Check if the EEPROM has valid settings (is not fully empty)
        Serial.println("EEPROM empty or uninitialized. Writing defaults...");
        loadDefaultSettingsToRAM();     // Load default values into RAM variables
        saveSettingsToEEPROM();         // Save defaults to EEPROM for next boot
    } else {
        Serial.println("EEPROM initialized. Loading stored settings...");
        loadSettingsFromEEPROM();       // Load stored values into RAM variables
    }
    // --- END EEPROM LOAD/INIT LOGIC ---
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // init OLED object Address 0x3D for 128x64
        Serial.println(F("SSD1306 allocation failed")); //error message for OLED screen
        for(;;);
    }
    // Draw the logo on startup saved in config.h (bitmap):
    display.clearDisplay(); // Make sure the display is cleared
    display.drawBitmap(0, 0, myBitmap, 128, 32, WHITE); // drawBitmap(x position, y position, bitmap data, bitmap width, bitmap height, color)
    display.display();      // Update the display
    delay(5000);            // show logo for 5 seconds
    setupState = digitalRead(setupPin);      // Check if the setup button is pressed
    if (setupState == HIGH) {                // if setup is HIGH start main menu loop to set custom IP
        MenuMode = true;
        while (MenuMode) {
            mainMenu();
            delay(50);
        }
    }
    // after settings are entered continue device setup
    Serial.println("Initialize adapter:");  // Feedback over serial port
    Ethernet.begin(mac, ip);                // Setup netwerk with following parameters
    Serial.print("My IP address: ");
    Serial.println(Ethernet.localIP());
    Serial.println("Initialize controller");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    // Display static text
    display.print("IP: ");
    display.println(Ethernet.localIP());
    if (PROTOCOL_TYPE == 0) {
        artnet.begin();
        Serial.println("Initialized Art-Net");
        display.setCursor(0, 14);
        display.print("Art-Net A: ");
        display.println(OUTPUT_A_UNIVERSE);
        display.setCursor(0, 24);
        display.print("Art-Net B: ");
        display.println(OUTPUT_B_UNIVERSE);
        display.display();
        // Art-Net Subscriber (Uses the configured universe numbers)
        artnet.subscribeArtDmx([&](const uint8_t *data, uint16_t size, const ArtDmxMetadata& metadata, const ArtNetRemoteInfo& remote) {
            if (debugFlag == HIGH){
                Serial.print("received ArtNet data from "); //uncomment for debug purposes
                Serial.print(remote.ip);
                Serial.print(":");
                Serial.print(remote.port);
                Serial.print(", net = ");
                Serial.print(metadata.net);
                Serial.print(", subnet = ");
                Serial.print(metadata.subnet);
                Serial.print(", universe = ");
                Serial.print(metadata.universe);
                Serial.print(", sequence = ");
                Serial.print(metadata.sequence);
                Serial.print(", size = ");
                Serial.print(size);
                Serial.println(")");
                // Uncomment below to print the entire frame data for debugging:
                for (size_t i = 0; i < size; ++i) {
                    Serial.print(data[i]);
                    Serial.print(",");
                }
                Serial.println();
            }
            if (metadata.universe == OUTPUT_A_UNIVERSE) {
                // FIX: Use fast memcpy instead of slow for loop
                memcpy(&channelsA[1], data, size);
                //dmx[0].write(channelsA, UNIVERSE_LENGTH);     //function now just fills buffers on parse and outputs these in a loop
                // REQUIRED BLOCKING: Wait for DMX A to finish transmitting for stability
                //while (dmx[0].busy()){}
            }
            else if (metadata.universe == OUTPUT_B_UNIVERSE) {
                // FIX: Use fast memcpy instead of slow for loop
                memcpy(&channelsB[1], data, size);
                //dmx[1].write(channelsB, UNIVERSE_LENGTH);  //function now just fills buffers on parse and outputs these in a loop
                // REQUIRED BLOCKING: Wait for DMX B to finish transmitting for stability
                //while (dmx[1].busy()){}
            }
        });
    }
    else {
        Serial.println("Initialized s/ACN (E1.31)");
        display.setCursor(0, 14);
        display.print("sACN A: ");
        display.println(OUTPUT_A_UNIVERSE);
        display.setCursor(0, 24);
        display.print("sACN B: ");
        display.println(OUTPUT_B_UNIVERSE);
        display.display();
        // Receiver 1 for Universe A
        recv1.callbackDMX(dmxReceivedA);
        recv1.begin(OUTPUT_A_UNIVERSE);
        // Receiver 2 for Universe B
        recv2.callbackDMX(dmxReceivedB);
        recv2.begin(OUTPUT_B_UNIVERSE);
    }
    display.display();
}
void loop(){
    if (PROTOCOL_TYPE == 0) {
        artnet.parse(); // check if artnet packet has come and execute callback
    }
    else {
        recv1.update();
        recv2.update();
    }
    dmx[0].write(channelsA, UNIVERSE_LENGTH); // DMX write has been taken out of the parse function to contiously send the last buffer DMX
    //while (dmx[0].busy()){}         // s/ACN only outputs changes so to keep the lights when nothing received keep sending last buffer
    dmx[1].write(channelsB, UNIVERSE_LENGTH);
    //while (dmx[1].busy()){}
    while (dmx[0].busy() || dmx[1].busy()){}
}