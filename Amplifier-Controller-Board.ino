

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Definition of the rotary encoder connection (hardware de-bounced)
#define RE_OUTPUT_A  5
#define RE_OUTPUT_B  2
#define RE_BUTTON    4
// Definition of the additional button (hardware de-bounced)
#define EXT_BUTTON   3


// Definition and declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#define I2C_DISPLAY    0x3C // --> I2C OLED display address

// Definition(s) for the EEPROM
#define I2C_EEPROM     0x50 // --> I2C EEPROM memory address

// Definition(s) for the digital potentiometers
#define ANALOG_REFERENCE  5.0             // Alter for 3.3V Arduino
#define m_steps            99             // Defines how many steps the digital potiometer has, from Low to High
#define POT_VALUE      10000L             // Nominal POT value
#define PULSE_TIMED        10             // millisecond delay 
#define STEP_OHMS      POT_VALUE/m_steps  // Number of ohms per tap point

#define DS1804_FIFTY 50000
#define DS1804_TEN   10000

#define UP_STEP   5
#define DW_STEP   5
#define MAX_ITEMS 5

#define pinINC    8   // --> X9C103P pin 1
#define pinUD     9   // --> X9C103P pin 2
#define CS_VOL   10   // --> Select pin for the volume potentiometer
#define CS_TRE   11   // --> Select pin for the treble potentiometer
#define CS_BAS   12   // --> Select pin for the bass potentiometer
#define CS_BAL   13   // --> Select pin for the balance potentiometer

// Definition of the LED pins
#define LED_G     6 // Green LED
#define LED_R     7 // Red LED

#define OFFSET_MEMORY 16

byte m_wiperPosition [4] = { 0 };

volatile byte curr_IDX = 0;

#define tapPoints  100           // X9C103P specs
boolean countDirection  = true;  // for testing only, TBD: replace in production code

enum Poti : byte { Volume = 0, Treble = 1, Bass = 2, Balance = 3, Save = 4, Load = 5 };  // different potentiometer types

void InitDigitalPotentiometer();                      // initialization procedure for the digital potentiometers
byte SetWiperPosition(byte);                          // Sets the wiper position, which is currently acive (curr_IDX)
void InitiateDisplay();                               // initalization of the display
void DisplayText(String);                             // outputs the given text on the display

// Definition of the save/load menu
volatile byte idx_save = 1;
volatile byte idx_load = 1;
volatile byte idx_menu = 1;
volatile bool ShowSave = false;


// initalization of the digital potentiometer
void InitDigitalPotentiometer()
{
  pinMode( CS_VOL, OUTPUT );
  pinMode( CS_TRE, OUTPUT );
  pinMode( CS_BAS, OUTPUT );
  pinMode( CS_BAL, OUTPUT );
  
  digitalWrite( CS_VOL, HIGH );
  digitalWrite( CS_TRE, HIGH );
  digitalWrite( CS_BAS, HIGH );
  digitalWrite( CS_BAL, HIGH );  
  
  pinMode( pinINC, OUTPUT );
  pinMode( pinUD, OUTPUT );
  
  digitalWrite( pinINC, HIGH );
}

byte SetWiperPosition( byte wiperPosition ) 
{
  
  SetWiperPositionIdx(curr_IDX, wiperPosition);
  DisplayWiperItem(curr_IDX);
  return m_wiperPosition[curr_IDX];
}

void DisplayWiperItem(byte idx)
{
  String dsp = "";
  
  switch (curr_IDX)
  {
    case Volume:
      dsp += "Volume";
      break;
    case Treble:
      dsp += "Treble";
      break;
    case Bass: 
      dsp += "Bass";
      break;
    case Balance: 
      dsp += "Balance";
      break;
    default:
      dsp += "Undefined";
      break;
  }
  dsp += "\n";
  dsp += String(m_wiperPosition[curr_IDX]);
  DisplayText(dsp);
}

unsigned long wiperPositionToResistance( byte proposedWiperPosition ) 
{
  // work out the resistance from a wiper position
  byte newWiperPosition = constrain( proposedWiperPosition, 0, m_steps );
  return map( newWiperPosition, 0, m_steps, 0, DS1804_TEN );
}

void InitiateRotaryEncoder()
{
  pinMode(RE_BUTTON,   INPUT);
  pinMode(RE_OUTPUT_A, INPUT);
  pinMode(RE_OUTPUT_B, INPUT);
}

void SetWiperPositionIdx(byte idx, byte wiperPosition)
{
  byte SelectPin = 0;

  if (wiperPosition > 99)
  {
    wiperPosition = 99;
  }
  if (wiperPosition < 0)
  {
    wiperPosition = 0;
  }
  
  switch (idx)
  {
    case Volume:
      SelectPin = CS_VOL;
      break;
    case Treble:
      SelectPin = CS_TRE;
      break;
    case Bass: 
      SelectPin = CS_BAS;
      break;
    case Balance: 
      SelectPin = CS_BAL;
      break;
    default:
      break;
  }
  if (SelectPin == 0)
  {
    return;
  }
  
  byte delta = abs(m_wiperPosition[idx] - wiperPosition);
  digitalWrite(SelectPin, LOW); //Tell chip that we're going to send data 
  
  if (m_wiperPosition[idx] < wiperPosition)
  {
    digitalWrite(pinUD, HIGH);
  } else
  {
    digitalWrite(pinUD, LOW);
  }
   
  digitalWrite(pinINC, HIGH); 
  for(byte i = 0; i<=delta; i++)
  {
      digitalWrite(pinINC, LOW);
      delayMicroseconds(1);
      digitalWrite(pinINC, HIGH);
      delayMicroseconds(1);
  }
  digitalWrite(SelectPin, HIGH);
  m_wiperPosition[idx] = wiperPosition;  
}

void InitiateDisplay()
{
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  display.begin(SSD1306_SWITCHCAPVCC, I2C_DISPLAY);
  /*if(!) 
  { 
    Serial.println(F("OLED failed"));
    for(;;); // Don't proceed, loop forever
  }
  */
  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(5000); // Pause for 5 seconds

  // Clear the buffer
  display.clearDisplay();
  display.display();  
}

void InitiateExternalButtons()
{
  pinMode(EXT_BUTTON, INPUT);
}

void i2c_eeprom_write_byte( int deviceaddress, unsigned int eeaddress, byte data ) 
{
    int rdata = data;
    Wire.beginTransmission(deviceaddress);
    Wire.write((int)(eeaddress >> 8)); // MSB
    Wire.write((int)(eeaddress & 0xFF)); // LSB
    Wire.write(rdata);
    Wire.endTransmission();
}

byte i2c_eeprom_read_byte( int deviceaddress, unsigned int eeaddress ) 
{
  byte readData = 0;
  Wire.beginTransmission(deviceaddress);
  Wire.write((int)(eeaddress >> 8)); // MSB
  Wire.write((int)(eeaddress & 0xFF)); // LSB
  Wire.endTransmission(deviceaddress);
  Wire.requestFrom(deviceaddress,1);
  if (Wire.available())
  {
    readData = (byte)Wire.read();
  }
  return readData;
}


void setup() 
{
  // Invoke the serial communicatiom
  Serial.begin(9600);

  // LED* for testing only
  pinMode(LED_G, OUTPUT);
  pinMode(LED_R, OUTPUT);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_R, LOW);
  
  // Initiate all input and outputs
  InitiateDisplay();
  InitDigitalPotentiometer();
  InitiateRotaryEncoder();
  InitiateExternalButtons();

  delay(30);
  LoadPreset((byte)1);
  delay(30);
  DisplayWiperItem(curr_IDX);
  delay(30);
}

void DisplayMenu(bool ShowSave, byte idxCursor)
{
  display.clearDisplay();
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  if (ShowSave)
  {
    display.println("Save preset:\n");
    idx_save = idxCursor;
  } else
  {
    display.println("Load preset:\n");
    idx_load = idxCursor;
  }
  
  String StdText  = "   ";
  String TextStar = "(*)"; // selected item
  for (byte pos = 1; pos <= 6; pos++)
  {
    if (idxCursor == pos)
    {
      display.println(TextStar + " Preset " + String(pos) + " " + TextStar);  
    } else
    {
      display.println(StdText + " Preset " + String(pos));  
    }
  }
  
  display.display();
}

void ProcessStep(bool StepUp)
{
  if (StepUp)
  {
    digitalWrite(LED_G, HIGH);
    delay(150);
    digitalWrite(LED_G, LOW);
    
    if (curr_IDX <= 3)
    {
      SetWiperPosition(m_wiperPosition[curr_IDX] + UP_STEP);
    } else if (curr_IDX == 4 || curr_IDX == 5)
    {
      if (curr_IDX == 4) { idx_menu = idx_save; } else { idx_menu = idx_load; }
      idx_menu++;
      if (idx_menu > 6)
      {
        idx_menu = 6;
      }
      DisplayMenu((curr_IDX == 4), idx_menu);
    } 
  } else
  {
    digitalWrite(LED_R, HIGH);
    delay(150);
    digitalWrite(LED_R, LOW);
    
    if (curr_IDX <= 3)
    {
      SetWiperPosition(m_wiperPosition[curr_IDX] - DW_STEP);
    } else if (curr_IDX == 4 || curr_IDX == 5)
    {
      if (curr_IDX == 4) { idx_menu = idx_save; } else { idx_menu = idx_load; }
      idx_menu--;
      if (idx_menu < 1)
      {
        idx_menu = 1;
      }
      DisplayMenu((curr_IDX == 4), idx_menu);
    } 
  }
}

void loop() 
{
  if (digitalRead(RE_OUTPUT_B) == HIGH)
  {
    if (digitalRead(RE_OUTPUT_A) == LOW)
    {
      ProcessStep(true);
    } else
    {
      ProcessStep(false);
    }
    // debouncing
    while (digitalRead(RE_OUTPUT_B) == HIGH) {}
    
  } else if (digitalRead(RE_BUTTON) == HIGH)
  {
    curr_IDX++;
    if (curr_IDX > 5)
    {
      curr_IDX = 0;
    }
    
    if (curr_IDX <= 3)
    {
      DisplayWiperItem(curr_IDX);
    } else if (curr_IDX == 4)
    {
      DisplayMenu(true, idx_save);
    } else if (curr_IDX == 5) 
    {
      DisplayMenu(false, idx_load);
    } 

    // debouncing
    while(digitalRead(RE_BUTTON) == HIGH) { }
    
  } else if (digitalRead(EXT_BUTTON) == HIGH)
  {
    Serial.println("Ext. button pressed!");
    String dsp = "";
    if (curr_IDX == 4)
    {
      SavePreset(idx_save);
      dsp = "Saved\nPreset: " + String(idx_save);
      DisplayText(dsp);
    } else if (curr_IDX == 5)
    {
      LoadPreset(idx_load);
      dsp = "Load\nPreset: " + String(idx_load);
      DisplayText(dsp);
    } else
    {
      dsp = "EXT_BTN";
    }
    
    DisplayText(dsp);
    
    // debouncing
    while(digitalRead(EXT_BUTTON) == HIGH) { }
  }
}

void SavePreset(byte idx)
{
  for (byte b = 0; b <= 3; b++)
  {
    unsigned int startPos = (sizeof(m_wiperPosition) * idx) + OFFSET_MEMORY + b;
    i2c_eeprom_write_byte(I2C_EEPROM, startPos, m_wiperPosition[b]);
    delay(20);
  }
}

void LoadPreset(byte idx)
{
  for (byte b = 0; b <= 3; b++)
  {
    unsigned int startPos = (sizeof(m_wiperPosition) * idx) + OFFSET_MEMORY + b;
    byte ValueFromEEPROM = i2c_eeprom_read_byte(I2C_EEPROM, startPos);
    delay(20);
    SetWiperPositionIdx(b, ValueFromEEPROM);
  }
}

void DisplayText(String Text)
{
  display.clearDisplay();

  display.setTextSize(2);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  display.print(Text);  
  display.display();
}
