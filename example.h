// Example for DS18030-* (for the project, DS18030-050 was used).

#include <Wire.h>

#define DIG_POT_ONE_ADDR  0x51
#define DIG_POT_TWO_ADDR  0x52
#define DIG_POT_THR_ADDR  0x53

enum Potentiometer : byte { POT_ZERO = 0xA9, POT_ONE = 0xAA, POT_BOTH = 0xAF }; // Command values from the datasheet

enum DSP_IDX : byte { Volume = 0, Treble = 1, Bass = 2, Balance = 3, Save = 4, Load = 5 };

byte UpdateWiperPosition(byte PotAddr, Potentiometer PotIdx, int NewValue)
{
  // Normalize value, if value exceeds limits of type byte
  byte val = 0;
  if (NewValue > 255)
  {
    val = 255;
  }
  if (NewValue < 0)
  {
    val = 0 ;
  }
  val = (byte)NewValue;

  // Send value to the digital potentiometer IC
  Wire.beginTransmission(PotAddr); // transmit to device #44 (0x2c)
  Wire.write(byte(PotIdx));        // sends command byte
  Wire.write(val);                 // sends potentiometer value byte
  Wire.endTransmission();          // stop transmitting

  // take some time to breath
  delay(20);

  // return the new value set
  return val;
}

byte SetWiperPosition(DSP_IDX dsp_idx, int Value)
{
  byte Addr = 0;
  byte Poti = 0;
  switch (dsp_idx)
  {
    case Volume:
      Addr = DIG_POT_ONE_ADDR;
      Poti = POT_ZERO;
      break;
    case Treble:
      Addr = DIG_POT_ONE_ADDR;
      Poti = POT_ONE;
      break;
    case Bass:
      Addr = DIG_POT_TWO_ADDR;
      Poti = POT_ZERO;
      break;
    case Balance:
      Addr = DIG_POT_TWO_ADDR;
      Poti = POT_ONE;
      break;
    default:
      Addr = 0;
      Poti = 0;
      break;
  }

  return UpdateWiperPosition(Addr, Poti, Value);
}

void setup() 
{
  Wire.begin();
}

void loop() 
{
  UpdateWiperPosition(DIG_POT_ONE_ADDR, POT_ZERO, 230);
  delay(5000);
}
