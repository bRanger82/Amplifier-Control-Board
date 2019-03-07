#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <U8g2lib.h>

#define AMP_STDBY    12  // Stand-By for the Audio Amplifier
#define RELAIS_K2    21  // Used for the relais K2
#define RELAIS_K1    18  // Used for the relais K1

// Definition of the rotary encoder connection (hardware de-bounced)
#define RE_BUTTON    2
#define RE_OUTPUT_A  10
#define RE_OUTPUT_B  11

// Definition of the additional button (hardware de-bounced)
#define EXT_BTN_ONE  22
#define EXT_BTN_TWO  23

// Definition(s) for the EEPROM
#define I2C_EEPROM     0x50 // --> I2C EEPROM memory address
#define OFFSET_MEMORY    16

// Definition(s) for the digital potentiometers
#define m_steps        1   // Defines how many steps the digital potiometer has, from Low to High

#define UP_STEP        5   // Difference for one step up on rotary encoder
#define DW_STEP        5   // Difference for one step down on the rotary encoder
#define MAX_STEP_UP  255   // Maxumum value for the digital potentiometer
#define MIN_STEP_DW    0   // Minimum value for the digital potentiometer
#define MAX_ITEMS      5




byte m_wiperPosition [5] = { 0 };

volatile byte ext_IDX  = 1;
#define IDX_EXT_REL_1 1
#define IDX_EXT_REL_2 2
#define IDX_EXT_MAX   2


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

enum DSP_IDX : byte { PreVolume = 0, Volume = 1, Treble = 2, Bass = 3, Balance = 4, Save = 5, Load = 6 };  // different potentiometer types

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

#include "splash.h"


U8G2_SSD1327_MIDAS_128X128_F_4W_SW_SPI u8g2(U8G2_R0, /* clock=*/ 7, /* data=*/ 5, /* cs=*/ 4, /* dc=*/ 3, /* reset=*/ 1);

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

void DisplayText(String Text, String Value, bool SmallText = false)
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
    default:
      Addr = 0;
      Poti = 0;
      break;
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
    default:
      dsp += "Undefined";
      break;
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
  u8g2.clearBuffer();
  delay(10000);
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
  pinMode(AMP_STDBY, OUTPUT); // Stand-By for the Audio Amplifier
  pinMode(RELAIS_K2, OUTPUT); // Used for the relais K2
  pinMode(RELAIS_K1, OUTPUT); // Used for the relais K1
  digitalWrite(AMP_STDBY, LOW);
  digitalWrite(RELAIS_K1, LOW);
  digitalWrite(RELAIS_K2, LOW);
}

void setup() 
{
  InitiateDisplay();
  InitiateRotaryEncoder();
  InitiateExternalButtons();
  InitialExternalDevices();
  
  curr_IDX = 0;  // Set default index value

  delay(30);
  LoadPreset((byte)1);
  delay(30);
  DisplayWiperItem((DSP_IDX)curr_IDX);
  delay(30);
}

#define CNT_ITM_MAX 6


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
  u8g2.clearBuffer();
  delay(10);
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

volatile byte RE_STATE = 0;

#define RE_STATE_UP  1
#define RE_STATE_DW  2
#define RE_STATE_NO  3

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

volatile byte ROT_ENC_BTN_PRESSED = false;
void RotaryEncoderBtnPressed(void)
{
  ROT_ENC_BTN_PRESSED = true;
}

void loop() 
{
  u8g2.clearBuffer();
  delay(10);
  
  if (RE_STATE == RE_STATE_DW)
  {
    // Rotary encoder was turned 'down'
    ProcessStep(false, (DSP_IDX)curr_IDX);
    RE_STATE = RE_STATE_NO;
    return;
  } else if (RE_STATE == RE_STATE_UP)
  {
    // Rotary encoder was turned 'up'
    ProcessStep(true, (DSP_IDX)curr_IDX);
    RE_STATE = RE_STATE_NO;
    return;
  }
  
  if (ROT_ENC_BTN_PRESSED)
  {
    ROT_ENC_BTN_PRESSED = false;
    
    if (++curr_IDX > MAX_IDX)
    {
      curr_IDX = IDX_PRE;
    }
    
    DisplayIdx();
    return;
  } else if (digitalRead(EXT_BTN_ONE) == HIGH)
  {
    String txt = "";
    String value = "";
    switch(curr_IDX)
    {
      case IDX_SAV:
        SavePreset(idx_save);
        txt = "Saved";
        value = "Preset: " + String(idx_save);
        DisplayText(txt, value, true);
        DelayTimer();
        DisplayIdx();
        break;
      case IDX_LOD:
        LoadPreset(idx_load);
        txt = "Loaded";
        value = "Preset: " + String(idx_load);
        DisplayText(txt, value, true);
        DelayTimer();
        DisplayIdx();
        break;
      case IDX_EXT:
        ChangeExtValue(ext_IDX);
        ShowExternal(ext_IDX);
        break;
      default:
        break;
    }

    // debouncing
    while(digitalRead(EXT_BTN_ONE) == HIGH) { }
  } else if (digitalRead(EXT_BTN_TWO) == HIGH)
  {
    if (digitalRead(AMP_STDBY) == HIGH)
    {
      DisplayText("Standby", "SET TO OFF", true);
    } else
    {
      DisplayText("Standby", "SET TO ON", true);
    }
    digitalWrite(AMP_STDBY, !digitalRead(AMP_STDBY));
    // debouncing
    while(digitalRead(EXT_BTN_TWO) == HIGH) { }
    DelayTimer();
    DisplayIdx();
    return;
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
void DelayTimer(void)
{
  for (byte tmr = 0; tmr <= 30; tmr++)
  {
    delay(100);
  }
  DisplayMenu(false, idx_load);
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
    delay(20);
  }
}

void LoadPreset(byte idx)
{
  for (byte b = 0; b < (sizeof(m_wiperPosition)/sizeof(m_wiperPosition[0])); b++)
  {
    unsigned int startPos = (sizeof(m_wiperPosition) * idx) + OFFSET_MEMORY + b;
    byte ValueFromEEPROM = i2c_eeprom_read_byte(I2C_EEPROM, startPos);
    delay(20);
    m_wiperPosition[b] = ValueFromEEPROM;
    UpdateWiperPositionI2C((DSP_IDX)b, m_wiperPosition[b]);
  }
}
