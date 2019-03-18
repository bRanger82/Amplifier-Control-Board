#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <U8g2lib.h>
#include "splash.h"

// Definition of the external connection wiring
#define AMP_STDBY    12  // Stand-By for the Audio Amplifier
#define RELAIS_K2    21  // Used for the relais K2
#define RELAIS_K1    18  // Used for the relais K1
// End of the definition of the external connection wiring

// Definition of the rotary encoder
#define RE_BUTTON    2   // rotary encoder button pin
#define RE_OUTPUT_A  10  // rotary encoder 'up' movement pin
#define RE_OUTPUT_B  11  // rotary encoder 'down' movement pin

/* 
 *  An interrupt is used to determinate if the rotary encoder was rotated. 
 *  The interrupt method changes the RE_STATE variable which is checked in the loop() method.
 *  This was done to avoid doing all the work in the interrupt method.
*/
#define RE_STATE_UP  1   // rotary encoder was roatated 'up'
#define RE_STATE_DW  2   // rotary encoder was roatated 'down'
#define RE_STATE_NO  3   // rotary encoder was not roatated

volatile byte RE_STATE = RE_STATE_NO; // variable for the interrupt method

volatile byte ROT_ENC_BTN_PRESSED = false; // variable for the interrupt method if the rotary encoder button was pressed.
// End of definition of the rotary encoder

// Definition of the additional button
#define EXT_BTN_ONE  22
#define EXT_BTN_TWO  23
// End of definition of the additional button

// Definition for the EEPROM
#define I2C_EEPROM     0x50 // --> I2C EEPROM memory address
#define OFFSET_MEMORY    16 // The start address for saving the I2C digital-poti values. The first 16 bytes are reserved. 
// End of definition for the EEPROM

// Definition of the external menu items
volatile byte ext_IDX  = 1;
#define IDX_EXT_REL_1 1
#define IDX_EXT_REL_2 2
#define IDX_EXT_MAX   2
// End of definition of the external menu items

// Definition of menu items 
volatile byte curr_IDX = 0;
#define MAX_IDX 7
#define IDX_PRE 0
#define IDX_VOL 1
#define IDX_TRE 2
#define IDX_BAS 3
#define IDX_BAL 4
#define IDX_SAV 5
#define IDX_LOD 6
#define IDX_EXT 7


enum DSP_IDX : byte { PreVolume = IDX_PRE, Volume = IDX_VOL, Treble = IDX_TRE, Bass = IDX_BAS, Balance = IDX_BAL, Save = IDX_SAV, Load = IDX_LOD, Externals = IDX_EXT };  // different menu items
// End of definition of menu items

// Definition(s) for the digital potentiometers
#define UP_STEP        5   // Difference for one step up on rotary encoder
#define DW_STEP        5   // Difference for one step down on the rotary encoder
#define MAX_STEP_UP  255   // Maxumum value for the digital potentiometer
#define MIN_STEP_DW    0   // Minimum value for the digital potentiometer

byte m_wiperPosition [5] = { 0 };

// Definition of the save/load menu
volatile byte idx_save = 1; // saves the current position of the save menu
volatile byte idx_load = 1; // saves the current position of the load menu
volatile byte idx_menu = 1; // is used for the current index to be shown
#define CNT_ITM_MAX 6
// End of definition of the save/load menu

// I2C potentiometer definitions
#define POT_U6_W 0x28 // I2C address of DS18030-050
#define POT_U7_W 0x29 // I2C address of DS18030-050
#define POT_U8_W 0x2B // I2C address of DS18030-050

#define POT_ZERO  0xA9
#define POT_ONE   0xAA
#define POT_BOTH  0xAF

// End of I2C potentiometer definitions

// Display library definition
U8G2_SSD1327_MIDAS_128X128_F_4W_SW_SPI u8g2(U8G2_R0, /* clock=*/ 7, /* data=*/ 5, /* cs=*/ 4, /* dc=*/ 3, /* reset=*/ 1);

#define SHOW_SPLASH_TIME 5000 // shows the spash screen for 10 seconds
// End of Display library definition

// Definition for the power save mode (display only)
volatile int LoopCount = 0;
volatile bool DisplayIsPowerSaveMode = false;
#define ENABLE_POWER_SAVE  1
#define DISABLE_POWER_SAVE 0
// End of the definition for the power save mode (display only)

byte SetWiperPositionI2C(const byte, const byte, const byte);
void DisplayText(String, String, bool = false);
byte UpdateWiperPositionI2C(const DSP_IDX, int);
bool IsBitSet(byte, byte);
byte SetBit(byte, byte);
void DisplayWiperItem(const DSP_IDX);
void InitiateRotaryEncoder(void);
void InitiateDisplay(void);
void InitiateExternalButtons(void);
void i2c_eeprom_write_byte(int, unsigned int, byte) ;
byte i2c_eeprom_read_byte(int, unsigned int) ;
void InitialExternalDevices(void);
void DisplayMenu(bool, byte);
void ProcessStep(bool, const DSP_IDX);
void DisplayIdx(void);
void DelayTimer(byte = 30);
void ChangeExtValue(byte);
void ShowExternal(byte);
void SavePreset(byte);
void LoadPreset(byte);
void PrintRotaryState(void);
void RotaryEncoderBtnPressed(void);
void LoadDefaultPreset(void);
void SetStandby(bool StandByOn);

byte SetWiperPositionI2C(const byte PotAddr, const byte PotIdx, const byte NewValue)
{
  // Send value to the digital potentiometer IC
  Wire.beginTransmission(PotAddr); // transmit to device
  Wire.write(byte(PotIdx));        // sends command byte
  Wire.write(NewValue);            // sends potentiometer value byte
  Wire.endTransmission();          // stop transmitting

  // take some time to breath, removing this delay can cause issue -> value will not be set
  delay(5);

  // return the new value set
  return NewValue;
}

void DisplayText(String Text, String Value, bool SmallText)
{
  u8g2.clearBuffer();
  delay(1);
  if (SmallText) 
  {
    u8g2.setFont(u8g2_font_profont17_tf);
  } else
  {
    u8g2.setFont(u8g2_font_profont29_tf);
  }
  char cText [15];
  Text.toCharArray(cText, 29);
  u8g2.drawStr(15, 40, cText);
  char cValue [15];
  Value.toCharArray(cValue, 29);
  if (SmallText)
  {
    u8g2.drawStr(15, 90, cValue);
  } else if (Text.length() > 3)
  {
    u8g2.drawStr(50, 90, cValue);
  } else
  {
    u8g2.drawStr(10, 90, cValue);
  }
  
  u8g2.sendBuffer();          // transfer internal memory to the display
  delay(1);
  u8g2.clearBuffer();
  delay(1);
}

byte UpdateWiperPositionI2C(const DSP_IDX dsp_idx, int Value)
{
  byte NewValue = 0;
  if (Value > 255)
  {
    NewValue = 255;
  } else if (Value < 0)
  {
    NewValue = 0 ;
  } else
  {
    NewValue = (byte)Value;
  }
  byte Addr = 0;
  byte Poti = 0;
  switch (dsp_idx)
  {
    case PreVolume:
      Addr = POT_U6_W;
      Poti = POT_ZERO;
      break;
    case Volume:
      Addr = POT_U7_W;
      Poti = POT_ZERO;
      break;
    case Treble:
      Addr = POT_U7_W;
      Poti = POT_ONE;
      break;
    case Bass:
      Addr = POT_U8_W;
      Poti = POT_ZERO;
      break;
    case Balance:
      Addr = POT_U8_W;
      Poti = POT_ONE;
      break;
    default: // the DSP_IDX is not assigned to a digital potientiometer 
      return (byte)0;
  }
  m_wiperPosition[dsp_idx] = SetWiperPositionI2C(Addr, Poti, NewValue);
  
  return m_wiperPosition[dsp_idx];
}

// Check if a specific bit is set (pos means position from the least significant bit)
bool IsBitSet(byte b, byte pos)
{
  return (b & (1 << pos)) != 0;
}

// sets a speficif bit, pos means position from the least significant bit)
byte SetBit(byte b, byte pos)
{
  byte mask = (byte)(1 << pos);
  return (b |= mask);
}
        
void DisplayWiperItem(const DSP_IDX idx)
{
  String dsp = "";
  switch (idx)
  {
    case PreVolume:
      dsp += "Pre-Amp";
      break;
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
    default: // the DSP_IDX is not assigned to a digital potientiometer
      return;
  }
  DisplayText(dsp, String(m_wiperPosition[idx]));
}

void InitiateRotaryEncoder(void)
{
  pinMode(RE_BUTTON,   INPUT);
  pinMode(RE_OUTPUT_A, INPUT);
  pinMode(RE_OUTPUT_B, INPUT);
  attachInterrupt(digitalPinToInterrupt(RE_OUTPUT_B), PrintRotaryState, RISING);
  attachInterrupt(digitalPinToInterrupt(RE_BUTTON), RotaryEncoderBtnPressed, RISING);
}

void InitiateDisplay(void)
{
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setColorIndex(1);
  u8g2.setFont(u8g2_font_ncenB10_tr);  // choose a suitable font
  u8g2.drawStr(3, 22, "Hello Stephanie!");
  u8g2.drawXBM(0, 32, splash1_width, splash1_height, splash1_data);  
  u8g2.drawStr(3, 112, "  Welcome Back  ");
  u8g2.sendBuffer();         
  delay(10);
  u8g2.clearBuffer();
  delay(SHOW_SPLASH_TIME);
}

void InitiateExternalButtons()
{
  pinMode(EXT_BTN_ONE, INPUT);
  pinMode(EXT_BTN_TWO, INPUT);
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

void InitialExternalDevices(void)
{
  pinMode(AMP_STDBY, OUTPUT);   // Stand-By for the Audio Amplifier
  pinMode(RELAIS_K2, OUTPUT);   // Used for the relais K2
  pinMode(RELAIS_K1, OUTPUT);   // Used for the relais K1
  // Setting default values
  digitalWrite(AMP_STDBY, LOW); 
  digitalWrite(RELAIS_K1, LOW);
  digitalWrite(RELAIS_K2, LOW);
}

void setup() 
{
  InitialExternalDevices();
  SetStandby(true);
  InitiateDisplay();
  InitiateRotaryEncoder();
  InitiateExternalButtons();
  curr_IDX = IDX_PRE;  // First item to be shown
  LoadDefaultPreset();
  DisplayWiperItem((DSP_IDX)curr_IDX);
  delay(10);
  SetStandby(false);
}

void LoadDefaultPreset(void)
{
  delay(10);
  LoadPreset((byte)1); // Preset one is the default preset which is loaded from the beginning
  delay(10);
}

void SetStandby(bool StandByOn)
{
  if (StandByOn)
  {
    digitalWrite(AMP_STDBY, HIGH);
  } else
  {
    digitalWrite(AMP_STDBY, LOW);
  }
}

void DisplayMenu(bool ShowSave, byte idxCursor)
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_prospero_bold_nbp_tf);
  if (ShowSave)
  {
    const char * savePS = "Save preset";
    u8g2.drawStr(27, 12, savePS);
    idx_save = idxCursor;
  } else
  {
    //lcd.print("Load preset:");
    const char * loadPS = "Load preset";
    u8g2.drawStr(27, 12, loadPS);
    idx_load = idxCursor;
  }
  u8g2.setFont(u8g2_font_profont12_tf);
  String StdText  = "   ";
  String TextStar = "(*)"; // selected item
  for (byte pos = 1; pos <= CNT_ITM_MAX; pos++)
  {
    char itm [40];
    String timStr;
    if (idxCursor == pos)
    {
      timStr = TextStar + " Preset " + String(pos) + " " + TextStar;  
    } else
    {
      timStr = StdText + " Preset " + String(pos);  
    }
    timStr.toCharArray(itm, 41);
    u8g2.drawStr(15, 18  + (pos * 15), itm);
  }
  u8g2.sendBuffer();
  delay(5);
  u8g2.clearBuffer();
  delay(5);
}

void ProcessStep(bool StepUp, const DSP_IDX idx)
{
  if (StepUp)
  {
    if (idx <= 4)
    {
      int NewWiperPosition = (int)m_wiperPosition[idx] + (int)UP_STEP;
      UpdateWiperPositionI2C((DSP_IDX)idx, NewWiperPosition);
      DisplayWiperItem((DSP_IDX)idx);
    } else if (idx == IDX_SAV || idx == IDX_LOD)
    {
      if (idx == 4) { idx_menu = idx_save; } else { idx_menu = idx_load; }
      idx_menu++;
      if (idx_menu > CNT_ITM_MAX)
      {
        idx_menu = CNT_ITM_MAX;
      }
      DisplayMenu((idx == IDX_SAV), idx_menu);
    } else if (curr_IDX == IDX_EXT)
    {
      ext_IDX++;
      if (ext_IDX > IDX_EXT_MAX)
      {
        ext_IDX = IDX_EXT_MAX;
      } else if (ext_IDX < IDX_EXT_REL_1)
      {
        ext_IDX = IDX_EXT_REL_1; // min
      }
      ShowExternal(ext_IDX);
    }
  } else
  {
    if (idx <= 4)
    {
      int NewWiperPosition = (int)m_wiperPosition[idx] - (int)DW_STEP;
      UpdateWiperPositionI2C((DSP_IDX)idx, NewWiperPosition);
      DisplayWiperItem((DSP_IDX)idx);
    } else if (idx == IDX_SAV || idx == IDX_LOD)
    {
      if (idx == IDX_SAV) { idx_menu = idx_save; } else { idx_menu = idx_load; }
      idx_menu--;
      if (idx_menu < 1)
      {
        idx_menu = 1;
      }
      DisplayMenu((idx == IDX_SAV), idx_menu);
    } else if (curr_IDX == IDX_EXT)
    {
      ext_IDX--;
      if (ext_IDX > IDX_EXT_MAX)
      {
        ext_IDX = IDX_EXT_MAX;
      } else if (ext_IDX < IDX_EXT_REL_1)
      {
        ext_IDX = IDX_EXT_REL_1; // min
      }
      ShowExternal(ext_IDX);
    }
  }
}

void DisplayIdx(void)
{
  if (curr_IDX <= 4)
  {
    DisplayWiperItem((DSP_IDX)curr_IDX);
    return;
  } else if (curr_IDX == IDX_SAV)
  {
    DisplayMenu(true, idx_save);
    return;
  } else if (curr_IDX == IDX_LOD) 
  {
    DisplayMenu(false, idx_load);
    return;
  } else if (curr_IDX == IDX_EXT)
  {
    ShowExternal(ext_IDX);
    return;
  }
}

void DelayTimer(byte waitTime)
{
  for (byte tmr = 0; tmr <= waitTime; tmr++)
  {
    delay(100);
  }
}

void ChangeExtValue(byte idx)
{
  switch(idx)
  {
    case IDX_EXT_REL_1:
      digitalWrite(RELAIS_K1, !digitalRead(RELAIS_K1));
      break;
    case IDX_EXT_REL_2:
      digitalWrite(RELAIS_K2, !digitalRead(RELAIS_K2));
      break;
    default:
      break;
  }
}

void ShowExternal(byte idx_ext)
{
  u8g2.clearBuffer();
  delay(1);
  u8g2.setFont(u8g2_font_profont17_tf);
  u8g2.drawStr(15, 20, "External");
  
  u8g2.setFont(u8g2_font_profont10_tf);
  if (idx_ext == IDX_EXT_REL_1)
  {
    u8g2.drawStr(10, 50, "*Relais 1");
  } else
  {
    u8g2.drawStr(10, 50, " Relais 1");
  }
  
  u8g2.drawStr(70, 50, "ON");
  u8g2.drawStr(90, 50, "OFF");
  if (digitalRead(RELAIS_K1) == HIGH) //ON
  {
    u8g2.drawFrame(67, 41, 19, 12);
  } else
  {
    u8g2.drawFrame(87, 41, 20, 12);
  }
  if (idx_ext == IDX_EXT_REL_2)
  {
    u8g2.drawStr(10, 70, "*Relais 2");
  } else
  {
    u8g2.drawStr(10, 70, " Relais 2");
  }
  u8g2.drawStr(10, 70, " Relais 2");
  u8g2.drawStr(70, 70, "ON");
  u8g2.drawStr(90, 70, "OFF");
  if (digitalRead(RELAIS_K2) == HIGH) //ON
  {
    u8g2.drawFrame(67, 61, 19, 12);
  } else
  {
    u8g2.drawFrame(87, 61, 20, 12);
  }
  u8g2.sendBuffer();          // transfer internal memory to the display
  delay(1);
  u8g2.clearBuffer();
  delay(1); 
}

void SavePreset(byte idx)
{
  for (byte b = 0; b < (sizeof(m_wiperPosition)/sizeof(m_wiperPosition[0])); b++)
  {
    // Calculate the correct address for saving the value
    unsigned int startPos = (sizeof(m_wiperPosition) * idx) + OFFSET_MEMORY + b;
    i2c_eeprom_write_byte(I2C_EEPROM, startPos, m_wiperPosition[b]);
    delay(10);
  }
}

void LoadPreset(byte idx)
{
  for (byte b = 0; b < (sizeof(m_wiperPosition)/sizeof(m_wiperPosition[0])); b++)
  {
    unsigned int startPos = (sizeof(m_wiperPosition) * idx) + OFFSET_MEMORY + b;
    byte ValueFromEEPROM = i2c_eeprom_read_byte(I2C_EEPROM, startPos);
    delay(10);
    m_wiperPosition[b] = ValueFromEEPROM;
    UpdateWiperPositionI2C((DSP_IDX)b, m_wiperPosition[b]);
  }
}



void PrintRotaryState(void)
{
  if (digitalRead(RE_OUTPUT_A) == LOW)
  {
    RE_STATE = RE_STATE_UP;
  } else 
  {
    RE_STATE = RE_STATE_DW;
  }
}


void RotaryEncoderBtnPressed(void)
{
  ROT_ENC_BTN_PRESSED = true;
}


void loop() 
{
  delay(10);
  
#ifdef POWERSAVE_MODE_ACTIVE   
  if ((RE_STATE == RE_STATE_DW) || (RE_STATE == RE_STATE_UP) || (ROT_ENC_BTN_PRESSED) || (digitalRead(EXT_BTN_ONE) == HIGH) || (digitalRead(EXT_BTN_TWO) == HIGH))
  {
    if (DisplayIsPowerSaveMode)
    {
      RE_STATE = RE_STATE_NO;
      u8g2.setPowerSave(DISABLE_POWER_SAVE);
      LoopCount = 0;
      DisplayIsPowerSaveMode = false;
      return;
    }
  } else
  {
    LoopCount++;
    if (LoopCount > 1000)
    {
      if (!DisplayIsPowerSaveMode)
      {
        u8g2.setPowerSave(ENABLE_POWER_SAVE);
        DisplayIsPowerSaveMode = true;
        return;
      }
      LoopCount = 0;
    }    
  }
#endif

  u8g2.clearBuffer();
  
  if (RE_STATE == RE_STATE_DW)
  {
    RE_STATE = RE_STATE_NO;
    // Rotary encoder was turned 'down'
    ProcessStep(false, (DSP_IDX)curr_IDX);
    return;
  } else if (RE_STATE == RE_STATE_UP)
  {
    RE_STATE = RE_STATE_NO;
    // Rotary encoder was turned 'up'
    ProcessStep(true, (DSP_IDX)curr_IDX);
    return;
  } else if (ROT_ENC_BTN_PRESSED)
  {
    // reset the button status
    ROT_ENC_BTN_PRESSED = false;
    
    // idx is the current screen to be shown
    // if the next iteration it exceeds the maximum index, the first index will be shown again
    if (++curr_IDX > MAX_IDX)
    {
      curr_IDX = IDX_PRE;
    }
    
    DisplayIdx();
    return;
  } else if (digitalRead(EXT_BTN_ONE) == HIGH)
  {
    switch(curr_IDX)
    {
      case IDX_SAV:
        SavePreset(idx_save);
        DisplayText("Saved", "Preset: " + String(idx_save), true);
        DelayTimer();
        break;
      case IDX_LOD:
        LoadPreset(idx_load);
        DisplayText("Loaded", "Preset: " + String(idx_load), true);
        DelayTimer();
        break;
      case IDX_EXT:
        ChangeExtValue(ext_IDX);
        break;
      default:
        break;
    }
    
    DisplayIdx();
    
    // debouncing
    while(digitalRead(EXT_BTN_ONE) == HIGH) { }
    return;
    
  } else if (digitalRead(EXT_BTN_TWO) == HIGH)
  {
    if (digitalRead(AMP_STDBY) == HIGH)
    {
      digitalWrite(AMP_STDBY, LOW);
      DisplayText("Standby", "SET TO OFF", true);
    } else
    {
      digitalWrite(AMP_STDBY, HIGH);
      DisplayText("Standby", "SET TO ON", true);
    }
    DelayTimer((byte)20);
    DisplayIdx();
    
    // debouncing
    while(digitalRead(EXT_BTN_TWO) == HIGH) { }
    return;
  }
}
