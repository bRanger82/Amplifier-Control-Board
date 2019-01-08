#include <Wire.h>

enum DSP_IDX : byte { Volume = 0, Treble = 1, Bass = 2, Balance = 3, Save = 4, Load = 5 };

enum POT_CMD_BYTE : byte { POT_ZERO = 0xA9, POT_ONE = 0xAA, POT_BOTH = 0xAF }; // Command values from the datasheet

#define POT_U6_W  B01010010 // write command U6
#define POT_U6_R  B01010011 // read command U6
#define POT_U7_W  B01010100 // write command U7
#define POT_U7_R  B01010101 // read command U7
#define POT_U8_W  B01010110 // write command U7
#define POT_U8_R  B01010111 // read command U7

byte SetWiperPosition(byte PotAddr, POT_CMD_BYTE PotIdx, int NewValue)
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
  Wire.beginTransmission(PotAddr); // transmit to device
  Wire.write(byte(PotIdx));        // sends command byte
  Wire.write(val);                 // sends potentiometer value byte
  Wire.endTransmission();          // stop transmitting

  // take some time to breath
  delay(20);

  // return the new value set
  return val;
}

byte UpdateWiperPosition(DSP_IDX dsp_idx, int Value)
{
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

  return SetWiperPosition(Addr, Poti, Value);
}


void setup() 
{
  Serial.begin(9600);
  Serial.println("Startup");
  Serial.print("POT_U6_W (hex): 0x");
  Serial.println(POT_U6_W, HEX);
  Serial.print("POT_U6_R (hex): 0x");
  Serial.println(POT_U6_R, HEX);
  Serial.print("POT_U7_W (hex): 0x");
  Serial.println(POT_U7_W, HEX);
  Serial.print("POT_U7_R (hex): 0x");
  Serial.println(POT_U7_R, HEX);
  Serial.print("POT_U8_W (hex): 0x");
  Serial.println(POT_U8_W, HEX);
  Serial.print("POT_U8_R (hex): 0x");
  Serial.println(POT_U8_R, HEX);
}

void loop() 
{
  delay(5000);
}
