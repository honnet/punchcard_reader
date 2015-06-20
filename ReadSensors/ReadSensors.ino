// ReadSensors.ino  20/06/2015  D.J.Whale
//
// (c) 2015 D.J.Whale

//punching
//punch data in the 8 columns
//if you want an empty row, punch the REG hole
//don't punch all 8+reg else it will see a card removal

//TODO error correcting mode
//when seen 8, we have the IN phase
//repeat for another 8, this is the OUT phase
//now have 16 bytes in memory
//if IN and OUT are same (but reversed), accept card, dump 8 hexascii
//if different, see if there are any single bit errors we might resolve
//e.g. if IN saw a hole and OUT did not, might assume we missed a hole
//might just be able to do this by ORing each line together

//TODO response protocol
// return data is in hexascii
// 00 - power up status/version etc
// 01 - valid card, 8 bytes follow
// 02 - error corrected card, 8 bytes follow
// 81 - card length error (not 16 clocks)
// 82 - card data error (significantly different IN and OUT
//bits numbered from left, so D7 is on left next to REG hole


#define REGISTRATION A0

#define D7           A1
#define D6           A2
#define D5           A3
#define D4           A6
#define D3           A7
#define D2           A8
#define D1           A9
#define D0           A10

#define LED_REG      1 // TX
#define LED_D7       0 // RX
#define LED_D6       2
#define LED_D5       3
#define LED_D4       5
#define LED_D3       7
#define LED_D2       16
#define LED_D1       14
#define LED_D0       15

#define CARD_ROWS 8


void setup()
{
  Serial.begin(115200);  
  pinMode(LED_REG, OUTPUT);
  pinMode(LED_D7,  OUTPUT);
  pinMode(LED_D6,  OUTPUT);
  pinMode(LED_D5,  OUTPUT);
  pinMode(LED_D4,  OUTPUT);
  pinMode(LED_D3,  OUTPUT);
  pinMode(LED_D2,  OUTPUT);
  pinMode(LED_D1,  OUTPUT);
  pinMode(LED_D0,  OUTPUT);
  
}

byte sticky = 0;
boolean seenReg = false;
byte row = 0;
byte card[CARD_ROWS];

typedef enum 
{
  STATE_REMOVED = 0,
  STATE_INSERTING,
  STATE_WAITING_ROW,
  STATE_IN_ROW,
  STATE_GAP,
  STATE_END
} STATE;

STATE state = STATE_REMOVED;

void loop()
{  
  // Read all 9 inputs (roughly) at same time
  unsigned int reg = analogRead(A0);

  unsigned int d7  = analogRead(D7);
  unsigned int d6  = analogRead(D6);
  unsigned int d5  = analogRead(D5);
  unsigned int d4  = analogRead(D4);
  unsigned int d3  = analogRead(D3);
  unsigned int d2  = analogRead(D2);
  unsigned int d1  = analogRead(D1);
  unsigned int d0  = analogRead(D0);
  
  // Generate filtered version, as a byte
  byte freg = getData(reg, 0);
  
  byte now = getData(d7, 7) | getData(d6, 6) | getData(d5, 5) | getData(d4, 4)
           | getData(d3, 3) | getData(d2, 2) | getData(d1, 1) | getData(d0, 0);

  // Show live diagnostics on LEDs
  writeLEDs(freg, now);

  // crank round the acquisition state machine
  switch (state)
  {
    case STATE_REMOVED:
      // stay here while freg=1 and data=0xFF, all holes (card removed)
      if ((freg != 1) || now != 0xFF)
      { // at least one sensor has seen paper
        state = STATE_INSERTING;
        //Serial.println(state);
      } 
    break;
    
    case STATE_INSERTING:
      // check for early removal (all holes)
      if ((freg == 1) && (now == 0xFF))
      {
        // bail early if all on (removed)
        state = STATE_REMOVED;
        //Serial.println(state);
      }
      // wait for all paper
      else if ((freg == 0) && (now == 0x00))
      {
        row = 0;
        state = STATE_WAITING_ROW;
        //Serial.println(state);
      }
    break;
    
    case STATE_WAITING_ROW:
      // check for early card removal (all reading holes)
      if ((freg == 1) && (now == 0xFF))
      {
        state = STATE_REMOVED;
        //Serial.println(state);
      }
      else
      { // wait for any data to start appearing (at least one hole)
        if ((freg == 1) || (now != 0x00))
        {
          sticky  = now;
          state   = STATE_IN_ROW;
          //Serial.println(state);
        }
      }
    break;
    
    case STATE_IN_ROW:
      // check for early card removal (all holes)
      if ((freg == 1) && (now == 0xFF))
      {
        state = STATE_REMOVED;
        //Serial.println(state);
      }
      else
      {
        // remember if we see the registration hole while reading a row
        // this is only punched if the row is completely unpunched
        // that way we don't miss the row
        if (freg == 1)
        {
          seenReg = true;
        }
        // keep collecting sticky bits until all go zero again
        sticky |= now;
        if ((freg == 0) && (now == 0x00))
        {
          state = STATE_GAP;
          //Serial.println(state);
        }  
      }
    break;
    
    case STATE_GAP:
      //Serial.print("row:");
      //Serial.print(row);
      //Serial.print("=");
      //Serial.println(sticky);
      
      //store row data in card buffer
      card[row] = sticky;
      // advance row
      row += 1;
      // more rows?
      if (row == CARD_ROWS)
      {
        state = STATE_END;
        //Serial.println(state);
      }
      else
      {
        state = STATE_WAITING_ROW;
        //Serial.println(state);
      }
    break;
    
    case STATE_END:
      // wait here until card removal
      //Note, later version will read on way out again, and complare in and out readings
      if ((freg == 1) && (now == 0xFF))
      {
        sendCard(card, CARD_ROWS);
        state = STATE_REMOVED;
        //Serial.println(state);
      }
    break;
  }  
}


byte getData(unsigned int adc, byte bitno)
{
  if (adc < 200) return (1<<bitno);
  return 0;
}

void writeLEDs(byte reg, byte data)
{
  // Show card data on feedback LEDs
  digitalWrite(LED_REG, reg);
  digitalWrite(LED_D7,  data&(1<<7));
  digitalWrite(LED_D6,  data&(1<<6));
  digitalWrite(LED_D5,  data&(1<<5));
  digitalWrite(LED_D4,  data&(1<<4));
  digitalWrite(LED_D3,  data&(1<<3));
  digitalWrite(LED_D2,  data&(1<<2));
  digitalWrite(LED_D1,  data&(1<<1));
  digitalWrite(LED_D0,  data&(1<<0));
}

void sendCard(byte* pData, byte len)
{
  for (byte i=0; i<len; i++)
  {
    Serial.print(pData[i]);
    Serial.print(" ");
  }
  Serial.println();
}



