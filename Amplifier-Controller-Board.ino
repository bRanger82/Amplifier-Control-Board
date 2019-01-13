#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Definition of the rotary encoder connection (hardware de-bounced)
#define RE_OUTPUT_A  5
#define RE_OUTPUT_B  2
#define RE_BUTTON    4
// Definition of the additional button (hardware de-bounced)
#define EXT_BUTTON   3


// Definition(s) for the EEPROM
#define I2C_EEPROM     0x50 // --> I2C EEPROM memory address

// Definition(s) for the digital potentiometers
#define m_steps        1   // Defines how many steps the digital potiometer has, from Low to High

#define UP_STEP        1   // Difference for one step up on rotary encoder
#define DW_STEP        1   // Difference for one step down on the rotary encoder
#define MAX_STEP_UP  255   // Maxumum value for the digital potentiometer
#define MIN_STEP_DW    0   // Minimum value for the digital potentiometer
#define MAX_ITEMS      5

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

enum DSP_IDX : byte { Volume = 0, Treble = 1, Bass = 2, Balance = 3, Save = 4, Load = 5 };  // different potentiometer types

// Definition of the save/load menu
volatile byte idx_save = 1;
volatile byte idx_load = 1;
volatile byte idx_menu = 1;
volatile bool ShowSave = false;

// I2C potentiometer definitions
#define POT_U6_W 0x28
#define POT_U7_W 0x29
#define POT_U8_W 0x2B

#define POT_ZERO  0xA9
#define POT_ONE   0xAA
#define POT_BOTH  0xAF

byte SetWiperPositionI2C(const byte PotAddr, const byte PotIdx, const byte NewValue)
{
  // Send value to the digital potentiometer IC
  Wire.beginTransmission(PotAddr); // transmit to device
  Wire.write(byte(PotIdx));        // sends command byte
  Wire.write(NewValue);            // sends potentiometer value byte
  Wire.endTransmission();          // stop transmitting

  // take some time to breath
  delay(5);

  // return the new value set
  return NewValue;
}

byte UpdateWiperPositionI2C(const DSP_IDX dsp_idx, int Value)
{
  byte NewValue = 0;
  if (Value > 255)
  {
    NewValue = 255;
  }
  if (Value < 0)
  {
    NewValue = 0 ;
  }
  NewValue = (byte)Value;
  
  byte Addr = 0;
  byte Poti = 0;
  switch (dsp_idx)
  {
    case Volume:
      Addr = POT_U6_W;
      Poti = POT_ZERO;
      break;
    case Treble:
      Addr = POT_U6_W;
      Poti = POT_ONE;
      break;
    case Bass:
      Addr = POT_U7_W;
      Poti = POT_ZERO;
      break;
    case Balance:
      Addr = POT_U7_W;
      Poti = POT_ONE;
      break;
    default:
      Addr = 0;
      Poti = 0;
      break;
  }

  m_wiperPosition[dsp_idx] = SetWiperPositionI2C(Addr, Poti, NewValue);
  DisplayWiperItem(dsp_idx);
  return m_wiperPosition[dsp_idx];
}

void DisplayWiperItem(const DSP_IDX idx)
{
  String dsp = "";
  
  switch (idx)
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
  DisplayText(dsp, String(m_wiperPosition[idx]));
}

void InitiateRotaryEncoder()
{
  pinMode(RE_BUTTON,   INPUT);
  pinMode(RE_OUTPUT_A, INPUT);
  pinMode(RE_OUTPUT_B, INPUT);
}

void InitiateDisplay()
{
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("Welcome back"); 
  lcd.setCursor(0, 2);
  lcd.print("Stephanie"); 
  delay(5000);
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
  InitiateRotaryEncoder();
  InitiateExternalButtons();

  curr_IDX = 0;  // Set default index value
  
  delay(30);
  LoadPreset((byte)1);
  delay(30);
  
  DisplayWiperItem((DSP_IDX)curr_IDX);
  delay(30);
}

#define CNT_ITM_MAX 3

void DisplayMenu(bool ShowSave, byte idxCursor)
{
  
  lcd.clear();
  lcd.setCursor(0, 0);
  if (ShowSave)
  {
    lcd.print("Save preset:");
    idx_save = idxCursor;
  } else
  {
    lcd.print("Load preset:");
    idx_load = idxCursor;
  }
  
  String StdText  = "   ";
  String TextStar = "(*)"; // selected item
  for (byte pos = 1; pos <= CNT_ITM_MAX; pos++)
  {
    lcd.setCursor(0, pos);
    if (idxCursor == pos)
    {
      lcd.print(TextStar + " Preset " + String(pos) + " " + TextStar);  
    } else
    {
      lcd.print(StdText + " Preset " + String(pos));  
    }
  }
}

void ProcessStep(bool StepUp, const DSP_IDX idx)
{
  if (StepUp)
  {
    digitalWrite(LED_G, HIGH);
    delay(50);
    digitalWrite(LED_G, LOW);
    
    if (idx <= 3)
    {
      int NewWiperPosition = (int)m_wiperPosition[idx] + (int)UP_STEP;
      UpdateWiperPositionI2C(idx, NewWiperPosition);
    } else if (idx == 4 || idx == 5)
    {
      if (idx == 4) { idx_menu = idx_save; } else { idx_menu = idx_load; }
      idx_menu++;
      if (idx_menu > CNT_ITM_MAX)
      {
        idx_menu = CNT_ITM_MAX;
      }
      DisplayMenu((idx == 4), idx_menu);
    } 
  } else
  {
    digitalWrite(LED_R, HIGH);
    delay(50);
    digitalWrite(LED_R, LOW);
    
    if (idx <= 3)
    {
      int NewWiperPosition = (int)m_wiperPosition[idx] - (int)DW_STEP;
      UpdateWiperPositionI2C(idx, NewWiperPosition);
    } else if (idx == 4 || idx == 5)
    {
      if (idx == 4) { idx_menu = idx_save; } else { idx_menu = idx_load; }
      idx_menu--;
      if (idx_menu < 1)
      {
        idx_menu = 1;
      }
      DisplayMenu((idx == 4), idx_menu);
    } 
  }
}

void loop() 
{
  if (digitalRead(RE_OUTPUT_B) == HIGH)
  {
    if (digitalRead(RE_OUTPUT_A) == LOW)
    {
      // Rotary encoder was turned 'up'
      ProcessStep(true, (DSP_IDX)curr_IDX);
    } else
    {
      // Rotary encoder was turned 'down'
      ProcessStep(false, (DSP_IDX)curr_IDX);
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
      DisplayWiperItem((DSP_IDX)curr_IDX);
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
    String txt = "";
    String value = "";
    if (curr_IDX == 4)
    {
      SavePreset(idx_save);
      txt = "Saved";
      value = "preset: " + String(idx_save);
    } else if (curr_IDX == 5)
    {
      LoadPreset(idx_load);
      txt = "Loaded";
      value = "preset: " + String(idx_load);
      
    } else
    {
      txt = "EXT_BTN";
      value = "Pressed!";
    }
    
    DisplayText(txt, value);
    
    // debouncing
    while(digitalRead(EXT_BUTTON) == HIGH) { }
  }
}

void SavePreset(byte idx)
{
  for (byte b = 0; b <= 3; b++)
  {
    // Calculate the correct address for saving the value
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
    m_wiperPosition[b] = ValueFromEEPROM;
    UpdateWiperPositionI2C(b, m_wiperPosition[b]);
  }
}

void DisplayText(String Text, String Value)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(Text);  
  lcd.setCursor(0, 1);
  lcd.print(Value);
}
