/*
  Sudoku for Arduino with touch LCD
  Software by Zdeno Sekerak (c) 2015. www.trsek.com/en/curriculum

  This software is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/

#include <stdint.h>
#include <EEPROM.h>
#include "TouchScreen.h"
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_TFTLCD.h> // Hardware-specific library

#define LCD_CS A3 // Chip Select goes to Analog 3
#define LCD_CD A2 // Command/Data goes to Analog 2
#define LCD_WR A1 // LCD Write goes to Analog 1
#define LCD_RD A0 // LCD Read goes to Analog 0

#define YP A1  // must be an analog pin, use "An" notation!
#define XM A2  // must be an analog pin, use "An" notation!
#define YM 7   // can be a digital pin
#define XP 6   // can be a digital pin

#define TS_MINX 160
#define TS_MINY 140
#define TS_MAXX 880
#define TS_MAXY 940

#define LCD_RESET A4 // Can alternately just connect to Arduino's reset pin

// Assign human-readable names to some common 16-bit color values:
#define BLACK   0x0000
#define GRAY    0x0101
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF
#define LIGHTBLUE 0x000F

#define MIN_TOUCH 10

Adafruit_TFTLCD tft(LCD_CS, LCD_CD, LCD_WR, LCD_RD, LCD_RESET);
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);
Adafruit_GFX_Button button;
Adafruit_GFX_Button button_options[10];
Adafruit_GFX_Button button_new;
Adafruit_GFX_Button button_save;
Adafruit_GFX_Button button_load;

#define NUMBER_FIX     0x80
#define NUMBER_MASK    0x0F
#define NUMBER_RESERVE 1
#define NUMBER_FREE   -1
#define _EEPROM_READ   0
#define _EEPROM_WRITE  1
#define DEBUG               // when you want see debug info in serial monitor

typedef struct
{
  byte number;
  char reserved[9];
} T_sudoku_button;

T_sudoku_button num[9][9];
byte act_x, act_y;
char text[2];
// -----------------------------------------------------------------------------

#ifdef DEBUG
void debug_info()
{
  Serial.print(" act_x = "); Serial.println(act_x);
  Serial.print(" act_y = "); Serial.println(act_y);
  Serial.println(" pole[][] = ");

  for (int y = 0; y < 9; y++)
  {
    for (int x = 0; x < 9; x++)
    {
      Serial.print(num[x][y].number & NUMBER_MASK);
      if((x%3) == 2) Serial.print(" ");
    }
    Serial.println();
  }
  Serial.println("------------------------------------------------------------");
  
  for (int y = 0; y < 9; y++)
  {
    for (int x = 0; x < 9; x++)
    {
      Serial.print(num[x][y].number & NUMBER_MASK);
      Serial.print((num[x][y].number & NUMBER_FIX)? '*': ' ');
      Serial.print("[");
      for(int i=0; i<9; i++)
        Serial.print((byte)num[x][y].reserved[i]);
      Serial.print("] ");
    }
    Serial.println();
  }
  Serial.println("------------------------------------------------------------");
}
#endif
// -----------------------------------------------------------------------------


// save/load play surface to/from EEPROM
void EEPROM_modul(byte num[], short num_size, byte mode)
{
  for(int i=0; i<num_size; i++)
  {
    if( mode == _EEPROM_WRITE )
      EEPROM.write(i, num[i]);
    else
      num[i] = EEPROM.read(i);
  }
}
// -----------------------------------------------------------------------------


// x,y - coordinates
// number - 1 to 9
// new_state - add or remove from other in square
void deactiveOption(byte x, byte y, byte number, char new_state)
{
  byte xs, ys;

  // out of range
  if(( number < 1 ) || ( number > 9 ))
    return;
    
  // kris/kros
  for (int i = 0; i < 9; i++)
  {
    if ( i != x )
      num[i][y].reserved[number-1] += new_state;

    if ( i != y )
      num[x][i].reserved[number-1] += new_state;
  }

  // in square
  xs = x - (x % 3);
  ys = y - (y % 3);
  //
  for (int ix = xs; ix < xs+3; ix++)
    for (int iy = ys; iy < ys+3; iy++)
    {
      if ((ix != x) && (iy != y))
        num[ix][iy].reserved[number-1] += new_state;
    }
}
// -----------------------------------------------------------------------------


// find empty box
void sudokuFindEmpty(byte *x, byte *y)
{
  for (*y = 0; *y < 9; (*y)++)
    for (*x = 0; *x < 9; (*x)++)
    {
      if( num[*x][*y].number == 0 )
        return;
    }

  *x = *y = 0;
}
// -----------------------------------------------------------------------------


// mixed sudoku
void sudokuMix(byte count)
{
  byte x, y;
  byte number;

  for (int x = 0; x < 9; x++)
    for (int y = 0; y < 9; y++)
    {
      num[x][y].number = 0;
      for (int i = 0; i < 9; i++)
        num[x][y].reserved[i] = 0;
    }

  // how many number is it?
  for (int i = 0; i < count; i++)
  {
    x = random(0, 9);
    y = random(0, 9);
    number = random(0, 9) + 1;

    // find only empty box
    // !!! need better algoritmus for check of correct choice !!!
    while (( num[x][y].number != 0 )
        || ( num[x][y].reserved[number - 1] > 0 ))
    {
      x++;
      if ( x >= 9 )
      {
        x = 0;
        y++;
        if ( y >= 9 )
        {
          x = 0;
          y = 0;
        }
      }
    }

    // add
    num[x][y].number = number + NUMBER_FIX;
    // deactivate number in other square
    deactiveOption(x, y, number, NUMBER_RESERVE);
  }
  
  sudokuFindEmpty(&act_x, &act_y);
}
// -----------------------------------------------------------------------------


// convert number to string
char* ConvertToString(byte number)
{
  text[0] = '0' + number;
  text[number? 1:0] = '\0';
  return text;
}
// -----------------------------------------------------------------------------


void showButton(byte x, byte y)
{
  byte movex, movey;
  uint16_t color;
  uint16_t color_text;

  movex = 16 + 3*(x/3);
  movey = 16 + 3*(y/3);
  color = BLUE;
  color_text = WHITE;
  if(( x == act_x ) && ( y == act_y ))
    color = GREEN;
  if( num[x][y].number & NUMBER_FIX )
    color_text = MAGENTA;
  
  button.initButton(&tft, x * 25 + movex, y * 25 + movey, 25, 25, 1, color, color_text, ConvertToString( num[x][y].number & NUMBER_MASK ), 2);
  button.drawButton(false);
}
// -----------------------------------------------------------------------------


void sudokuRedraw()
{
  for (int x = 0; x < 9; x++)
    for (int y = 0; y < 9; y++)
      showButton(x,y);
}
// -----------------------------------------------------------------------------


void optionShow(byte act_x, byte act_y)
{
  for (int i = 0; i < 10; i++)
  {
    if( i == 0 )
    {
      button_options[i].drawButton( num[act_x][act_y].number? false: true );
    }
    else
    {
      button_options[i].drawButton(num[act_x][act_y].reserved[i-1]? true: false );
    }
  }
}
// -----------------------------------------------------------------------------


// show option for choice level
byte getLevel()
{
  tft.fillRect(0, 0, 240, 240, BLACK);
  tft.setCursor(20, 100);
  tft.setTextColor(WHITE);
  tft.setTextSize(3);
  tft.println("Press Level");
  tft.println();
  tft.println("    1 - 9");

  // znuluje moznosti
  for(int i=0; i<9; i++) num[0][0].reserved[i]=0;
  optionShow(0, 0);

  while(1)
  {
      TSPoint p = ts.getPoint();
      
      if( p.z > MIN_TOUCH )
      {
        pinMode(XM, OUTPUT);
        pinMode(YP, OUTPUT);

        p.x = map(p.x, TS_MINX, TS_MAXX, tft.width(), 0);
        p.y = map(p.y, TS_MINY, TS_MAXY, tft.height(), 0);

        // empty square
        if( button_options[0].contains(p.x, p.y))
        {
          tft.fillRect(0, 0, 240, 240, BLACK);
          return 0;
        }
        
        // click here
        for(int i=1; i<=9; i++)
          if( button_options[i].contains(p.x, p.y))
          {
            tft.fillRect(0, 0, 240, 240, BLACK);
            return 8 + i*3;
          }
      }
  }
}
// -----------------------------------------------------------------------------


void setup(void)
{
  Serial.begin(9600);
  Serial.println(F("SudokuDuino. Software by Zdeno Sekerak (c) 2015."));
  randomSeed(analogRead(0));

  tft.reset();
  tft.begin(tft.readID());

  tft.fillScreen(BLACK);
  sudokuMix(29);
  sudokuRedraw();
#ifdef DEBUG
  debug_info();
#endif

  tft.setTextColor(BLUE);
  tft.setTextSize(1);
  tft.setCursor(30, 310);
  tft.print(F("Software by Zdeno Sekerak (c) 2015"));

  // button
  button_new.initButton (&tft, 200, 252, 70, 24, 1, MAGENTA, WHITE, "New", 2);
  button_load.initButton(&tft, 200, 274, 70, 24, 1, MAGENTA, WHITE, "Load", 2);
  button_save.initButton(&tft, 200, 296, 70, 24, 1, MAGENTA, WHITE, "Save", 2);
  button_new.drawButton (false);
  button_load.drawButton(false);
  button_save.drawButton(false);

  //
  for (int x = 0; x < 5; x++)
    for (int y = 0; y < 2; y++)
      button_options[y*5+x].initButton(&tft, 20 + x*31, 255 + y*31, 30, 30, 1, GREEN, GRAY, ConvertToString(y*5+x), 2);

  for (int i = 0; i < 10; i++)
     button_options[i].drawButton(false);

  optionShow(act_x, act_y);
}
// -----------------------------------------------------------------------------


void loop(void)
{
  TSPoint p = ts.getPoint();
  int x, y;
  int old_x, old_y;

  if ( p.z > MIN_TOUCH )
  {
    pinMode(XM, OUTPUT);
    pinMode(YP, OUTPUT);

    p.x = map(p.x, TS_MINX, TS_MAXX, tft.width(), 0);
    p.y = map(p.y, TS_MINY, TS_MAXY, tft.height(), 0);

    // nova hra
    if( button_new.contains(p.x, p.y))
    {
      Serial.println("new");
      button_new.drawButton (true);
      sudokuMix(getLevel());
      sudokuRedraw();
      optionShow(act_x, act_y);
      button_new.drawButton (false);
      return;
    }

    // load from EEPROM
    if( button_load.contains(p.x, p.y))
    {
      Serial.println("load");
      button_load.drawButton (true);
      EEPROM_modul((byte*) &num, sizeof(num), _EEPROM_READ);
      sudokuFindEmpty(&act_x, &act_y);
      sudokuRedraw();
      optionShow(act_x, act_y);
      button_load.drawButton (false);
      return;
    }

    // save to EEPROM
    if( button_save.contains(p.x, p.y))
    {
      Serial.println("save");
      button_save.drawButton (true);
      //1EEPROM_modul((byte*) &num, sizeof(num), _EEPROM_WRITE);
      debug_info();
      button_save.drawButton (false);
      return;
    }

    // click on choice
    for(int i=0; i<10; i++)
    {
      if( button_options[i].contains(p.x, p.y))
      {
        button_options[i].drawButton (true);
        // previous mark number's remove
        deactiveOption(act_x, act_y, num[act_x][act_y].number, NUMBER_FREE);
        
        // clear number only
        if( i == 0 )
        {
          num[act_x][act_y].number = i;
        }
        // add number
        else if( num[act_x][act_y].reserved[i-1] == 0 )
        {
          // add
          num[act_x][act_y].number = i;
          // deactivate choices
          deactiveOption(act_x, act_y, num[act_x][act_y].number, NUMBER_RESERVE);
        }
        // show it
        showButton(act_x, act_y);
        delay(200);
        button_options[i].drawButton (false);
      }
    }

    // click on square
    x = p.x / 25;
    y = p.y / 25;
    if(( y < 9 )
    && ( x < 9 )
    && !( num[x][y].number & NUMBER_FIX ))
    {
      old_x = act_x;
      old_y = act_y;
      act_x = x;
      act_y = y;
      showButton(old_x, old_y);
      showButton(act_x, act_y);
    }
    
    optionShow(act_x, act_y);

  } // if (touch)
}
// -----------------------------------------------------------------------------

