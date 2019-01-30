#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <SPI.h>

#define AMP_STDBY    11  // Stand-By for the Audio Amplifier
#define OPTP_CPLR    13  // Used for the Opto-Coupler
#define RELAIS_K2    14  // Used for the relais K2
#define RELAIS_K1    15  // Used for the relais K1

const byte OPTO_CPLR  = B00000001;
const byte RLY_K1_ACT = B00000010;
const byte RLY_K2_ACT = B00000100;

// This variable stores the relais and opto-coupler states and is used to store/obtain the information from EEPROM.
volatile byte EXT_STATE = 0;

// Definition of the rotary encoder connection (hardware de-bounced)
#define RE_OUTPUT_A  22
#define RE_OUTPUT_B  21
#define RE_BUTTON    23

// Definition of the additional button (hardware de-bounced)
#define EXT_BTN_ONE  19
#define EXT_BTN_TWO  20

#define LED 14
// Definition(s) for the EEPROM
#define I2C_EEPROM     0x50 // --> I2C EEPROM memory address

// Definition(s) for the digital potentiometers
#define m_steps        1   // Defines how many steps the digital potiometer has, from Low to High

#define UP_STEP        1   // Difference for one step up on rotary encoder
#define DW_STEP        1   // Difference for one step down on the rotary encoder
#define MAX_STEP_UP  255   // Maxumum value for the digital potentiometer
#define MIN_STEP_DW    0   // Minimum value for the digital potentiometer
#define MAX_ITEMS      5


#define OFFSET_MEMORY 16

byte m_wiperPosition [4] = { 0 };

volatile byte ext_IDX  = 1;
#define IDX_EXT_REL_1 1
#define IDX_EXT_REL_2 2
#define IDX_EXT_OPTPC 3
#define IDX_EXT_MAX   3


volatile byte curr_IDX = 0;
#define MAX_IDX 6
#define IDX_VOL 0
#define IDX_TRE 1
#define IDX_BAS 2
#define IDX_BAL 3
#define IDX_SAV 4
#define IDX_LOD 5
#define IDX_EXT 6

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
  DisplayWiperItem((DSP_IDX)dsp_idx);
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
}

void InitiateDisplay(void)
{
  u8g2.setBusClock(9000000UL);
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
  pinMode(OPTP_CPLR, OUTPUT); // Used for the Opto-Coupler
  pinMode(RELAIS_K2, OUTPUT); // Used for the relais K2
  pinMode(RELAIS_K1, OUTPUT); // Used for the relais K1
  digitalWrite(OPTP_CPLR, LOW);
  digitalWrite(RELAIS_K1, LOW);
  digitalWrite(RELAIS_K2, LOW);
}

void setup() 
{
  pinMode(LED, OUTPUT);
  
  // Invoke the serial communicatiom
  Serial.begin(9600);
  InitiateDisplay();
  InitiateRotaryEncoder();
  InitiateExternalButtons();
  InitialExternalDevices();
  
  curr_IDX = 0;  // Set default index value

  delay(30);
  //LoadPreset((byte)1);
  Serial.println("LoadPreset done!");
  delay(30);
  
  DisplayWiperItem((DSP_IDX)curr_IDX);
  Serial.println("DisplayWiperItem done!");
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
}

void ProcessStep(bool StepUp, const DSP_IDX idx)
{
  if (StepUp)
  {
    if (idx <= 3)
    {
      int NewWiperPosition = (int)m_wiperPosition[idx] + (int)UP_STEP;
      UpdateWiperPositionI2C((DSP_IDX)idx, NewWiperPosition);
    } else if (idx == IDX_SAV || idx == IDX_LOD)
    {
      if (idx == 4) { idx_menu = idx_save; } else { idx_menu = idx_load; }
      idx_menu++;
      if (idx_menu > CNT_ITM_MAX)
      {
        idx_menu = CNT_ITM_MAX;
      }
      DisplayMenu((idx == IDX_SAV), idx_menu);
    } 
  } else
  {
    if (idx <= 3)
    {
      int NewWiperPosition = (int)m_wiperPosition[idx] - (int)DW_STEP;
      UpdateWiperPositionI2C((DSP_IDX)idx, NewWiperPosition);
    } else if (idx == IDX_SAV || idx == IDX_LOD)
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

volatile byte printVal = 0;
volatile byte idx_ext_test = 1;
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
    //while (digitalRead(RE_OUTPUT_B) == HIGH) {}
    
  } else if (digitalRead(RE_BUTTON) == HIGH || true)
  {
    curr_IDX++;
    if (curr_IDX > MAX_IDX)
    {
      curr_IDX = IDX_VOL;
    }
    
    if (curr_IDX <= 3)
    {
      DisplayWiperItem((DSP_IDX)curr_IDX);
    } else if (curr_IDX == IDX_SAV)
    {
      idx_save++;
      if (idx_save > CNT_ITM_MAX)
      {
        idx_save = 1;
      }
      DisplayMenu(true, idx_save);
    } else if (curr_IDX == IDX_LOD) 
    {
      idx_load++;
      if (idx_load > CNT_ITM_MAX)
      {
        idx_load = 1;
      }
      DisplayMenu(false, idx_load);
    } else if (curr_IDX == IDX_EXT)
    {
      ShowExternal(idx_ext_test++);
      if (idx_ext_test > IDX_EXT_MAX)
      {
        idx_ext_test = 1;
      }
    }

    // debouncing
    // while(digitalRead(RE_BUTTON) == HIGH) { }
    
  } else if (digitalRead(EXT_BTN_ONE) == HIGH)
  {
    String txt = "";
    String value = "";
    if (curr_IDX == IDX_SAV)
    {
      SavePreset(idx_save);
      txt = "Saved";
      value = "Preset: " + String(idx_save);
    } else if (curr_IDX == IDX_LOD)
    {
      LoadPreset(idx_load);
      txt = "Loaded";
      value = "Preset: " + String(idx_load);
    } else
    {
      txt = "EXT_BTN";
      value = "Pressed!";
    }
    
    DisplayText(txt, value);
    
    // debouncing
    // while(digitalRead(EXT_BTN_ONE) == HIGH) { }
  }
  digitalWrite(LED, !digitalRead(LED));
  delay(500);
  
  u8g2.clearBuffer();
  delay(1);
}

volatile bool state = true;
void ShowExternal(byte idx_ext)
{
  if (idx_ext > IDX_EXT_MAX)
  {
    idx_ext = IDX_EXT_MAX;
  }
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
  if (idx_ext == IDX_EXT_OPTPC)
  {
    u8g2.drawStr(10, 90, "*OPTO COP");
  } else
  {
    u8g2.drawStr(10, 90, " OPTO COP");
  }
  u8g2.drawStr(70, 90, "ON");
  u8g2.drawStr(90, 90, "OFF");
  if (digitalRead(OPTP_CPLR) == HIGH) //ON
  {
    u8g2.drawFrame(67, 81, 19, 12);
  } else
  {
    u8g2.drawFrame(87, 81, 20, 12);
  }
  u8g2.sendBuffer();          // transfer internal memory to the display
  delay(1);
  u8g2.clearBuffer();
  delay(1); 
  state = !state;
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
    UpdateWiperPositionI2C((DSP_IDX)b, m_wiperPosition[b]);
  }
}

void DisplayText(String Text, String Value)
{
  u8g2.clearBuffer();
  delay(1);
  u8g2.setFont(u8g2_font_profont29_tf);
  char cText [15];
  Text.toCharArray(cText, 29);
  u8g2.drawStr(15, 40, cText);
  char cValue [15];
  Value.toCharArray(cValue, 29);
  if (Text.length() > 3)
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
