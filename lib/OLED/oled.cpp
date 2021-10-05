//***************************************************************************************************
//                                   O L E D . C P P                                                *
//***************************************************************************************************
// Driver for SSD1306/SSD1309/SH1106 display                                                        *
//***************************************************************************************************
// 25-02-2021, ES: Correct bug, isSH1106 was always set.                                            *
//***************************************************************************************************
#include <driver/i2c.h>
#include <string.h>
#include "oled.h"

char*       dbgprint ( const char* format, ... ) ;          // Print a formatted debug line

OLED*        tft ;                                    // Object for display
scrseg_struct OLED_tftdata[TFTSECS] =                 // Screen divided in 3 segments + 1 overlay
{                                                     // One text line is 8 pixels
  { false, WHITE,   0,  8, "" },                      // 1 top line
  { false, CYAN,    8, 32, "" },                      // 4 lines in the middle
  { false, YELLOW, 40, 22, "" },                      // 3 lines at the bottom, leave room for indicator
  { false, GREEN,  40, 22, "" }                       // 3 lines at the bottom for rotary encoder
} ;


bool oled_dsp_begin ( uint8_t sda_pin, uint8_t scl_pin )
{
  dbgprint ( "Init OLED, I2C pins %d,%d",
             sda_pin,
             scl_pin ) ;
  if ( ( sda_pin >= 0 ) &&
       ( scl_pin >= 0 ) )
  {
    tft = new OLED ( sda_pin,
                     scl_pin ) ;                    // Create an instant for TFT
  }
  else
  {
    dbgprint ( "Init OLED failed!" ) ;
  }
  return ( tft != NULL ) ;
}


//***********************************************************************************************
//                                O L E D :: W R I 2 C C H A N                                  *
//***********************************************************************************************
// Write 1 byte to the I2C buffer.                                                              *
//***********************************************************************************************
void OLED::wrI2Cchan ( uint8_t b )                  // Send 1 byte to I2C buffer
{
  i2c_master_write_byte ( i2Cchan, b, true ) ;      // Add 1 byte to I2C buffer
}


//***********************************************************************************************
//                                O L E D :: O P E N I 2 C C H A N                              *
//***********************************************************************************************
// Open an I2c channel for communication.                                                       *
//***********************************************************************************************
void OLED::openI2Cchan()
{
  i2Cchan = i2c_cmd_link_create() ;                            // Create the link
  i2c_master_start ( i2Cchan ) ;                               // Start collecting data in buffer
  wrI2Cchan ( (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE ) ;   // Send I2C address and write bit
}


//***********************************************************************************************
//                                O L E D :: C L O S E I 2 C C H A N                            *
//***********************************************************************************************
// Seend buffer and close the I2c channel.                                                      *
//***********************************************************************************************
void OLED::closeI2Cchan()
{
  i2c_master_stop ( i2Cchan ) ;                           // Stop filling buffer
  i2c_master_cmd_begin ( I2C_NUM_0, i2Cchan,              // Start actual transfer
                         10 / portTICK_PERIOD_MS ) ;      // Time-out
  i2c_cmd_link_delete ( i2Cchan ) ;                       // Delete the link
}


//***********************************************************************************************
//                                      O L E D :: P R I N T                                    *
//***********************************************************************************************
// Put a character in the buffer.                                                               *
//***********************************************************************************************
void OLED::print ( char c )
{
  if ( ( c >= ' ' ) && ( c <= '~' ) )                    // Check the legal range
  {
    memcpy ( &ssdbuf[ychar].page[xchar],                 // Copy bytes for 1 character
             &font[(c-32)*OLEDFONTWIDTH],
             OLEDFONTWIDTH ) ;
    ssdbuf[ychar].dirty = true ;                         // Notice change
  }
  else if ( c == '\n' )                                  // New line?
  {
    xchar = SCREEN_WIDTH ;                               // Yes, force next line
  }
  xchar += ( OLEDFONTWIDTH + 1 ) ;                    // Move x cursor
  if ( xchar > ( SCREEN_WIDTH - OLEDFONTWIDTH ) )     // End of line?
  {
    xchar = 0 ;                                          // Yes, mimic CR
    ychar = ( ychar + 1 ) & 7 ;                          // And LF
  }
}


//***********************************************************************************************
//                                   O L E D :: P R I N T                                    *
//***********************************************************************************************
// Put a character in the buffer.                                                               *
//***********************************************************************************************
void OLED::print ( const char* str )
{
  while ( *str )                                // Print until delimeter
  {
    print ( *str ) ;                            // Print next character
    str++ ;
  }
}


//***********************************************************************************************
//                                OLED :: D R A W B I T M A P                                   *
//***********************************************************************************************
// Copy a bitmap to the display buffer.                                                         *
//***********************************************************************************************
void OLED::drawBitmap ( uint8_t x, uint8_t y, uint8_t* buf, uint8_t w, uint8_t h )
{
  uint8_t  pg ;                                           // Page to fill
  uint8_t  pat ;                                          // Fill pattern
  uint8_t  xc, yc ;                                       // Running x and y in rectangle
  uint8_t* p ;                                            // Point into ssdbuf
  uint8_t* s = buf ;                                      // Byte in input buffer
  uint8_t  m = 0x80 ;                                     // Mask in input buffer
  
  for ( yc = y ; yc < ( y + h ) ; yc++ )                  // Loop vertically
  {
    pg = ( yc / 8 ) % OLED_NPAG ;                         // Page involved
    pat = 1 << ( yc & 7 ) ;                               // Bit involved
    p = ssdbuf[pg].page + x ;                             // Point to right place
    for ( xc = x ; xc < ( x + w ) ; xc++ )                // Loop horizontally
    {
      if ( *s & m )                                       // Set bit?
      {
        *p |= pat ;                                       // Yes, set bit
      }
      else
      {
        *p &= ~pat ;                                      // No, clear bit
      }
      p++ ;                                               // Next 8 pixels
      if ( ( m >>= 1 ) == 0 )                             // Shift input mask
      {
        m = 0x80 ;                                        // New byte
        s++ ;
      }
    }
    ssdbuf[pg].dirty = true ;                             // Page has been changed
  }
}


//***********************************************************************************************
//                                O L E D :: D I S P L A Y                                   *
//***********************************************************************************************
// Refresh the display.                                                                         *
//***********************************************************************************************
void OLED::display()
{
  uint8_t        pg ;                                         // Page number 0..OLED_NPAG - 1
  static uint8_t fillbuf[] = { 0, 0, 0, 0 } ;                 // To clear 4 bytes of SH1106 RAM

  for ( pg = 0 ; pg < OLED_NPAG ; pg++ )
  {
    if ( ssdbuf[pg].dirty )                                   // Refresh needed?
    {
      ssdbuf[pg].dirty = false ;                              // Yes, set page to "up-to-date"
      openI2Cchan() ;                                         // Open I2C channel
      wrI2Cchan ( OLED_CONTROL_BYTE_CMD_SINGLE ) ;            // Set single byte command mode
      wrI2Cchan ( 0xB0 | pg ) ;                               // Set page address
      if ( isSH1106 || isSSD1309 )                            // Is it an SH1106 or SSD1309?
      {
        wrI2Cchan ( 0x00 ) ;                           	      // Set lower column address to 0
        wrI2Cchan ( 0x10 ) ;                                  // Set higher column address to 0
      }
      closeI2Cchan() ;                                        // Send and close I2C channel
      // Channel is closed and reopened because of limited buffer space
      openI2Cchan() ;                                         // Open I2C channel again
      wrI2Cchan ( OLED_CONTROL_BYTE_DATA_STREAM ) ;           // Set multi byte data mode
      if ( isSH1106 )                                         // Is it a SH1106?
      {
        i2c_master_write ( i2Cchan, fillbuf, 4, true ) ;      // Yes, fill extra RAM with zeroes
      }
      i2c_master_write ( i2Cchan, ssdbuf[pg].page, 128,       // Send 1 page with data
                         true ) ;
      closeI2Cchan() ;                                        // Send and close I2C channel
    }
  }
}


//***********************************************************************************************
//                                 O L E D :: C L E A R                                         *
//***********************************************************************************************
// Clear the display buffer and the display.                                                    *
//***********************************************************************************************
void OLED::clear()
{
  for ( uint8_t pg = 0 ; pg < OLED_NPAG ; pg++ )         // Handle all pages
  {
    memset ( ssdbuf[pg].page, BLACK, SCREEN_WIDTH ) ;    // Clears all pixels of 1 page
    ssdbuf[pg].dirty = true ;                            // Force refresh
  }
  xchar = 0 ;
  ychar = 0 ;
}


//***********************************************************************************************
//                                   O L E D :: S E T C U R S O R                               *
//***********************************************************************************************
// Position the cursor.  It will be set at a page boundary (line of texts).                     *
//***********************************************************************************************
void OLED::setCursor ( uint8_t x, uint8_t y )          // Position the cursor
{
  xchar = x % SCREEN_WIDTH ;
  ychar = y / 8 % OLED_NPAG ;
}


//***********************************************************************************************
//                                   O L E D :: F I L L R E C T                                 *
//***********************************************************************************************
// Fill a rectangle.  It will clear a number of pages (lines of texts ).                        *
// Not very fast....                                                                            *
//***********************************************************************************************
void OLED::fillRect ( uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color )
{
  uint8_t  pg ;                                           // Page to fill
  uint8_t  pat ;                                          // Fill pattern
  uint8_t  xc, yc ;                                       // Running x and y in rectangle
  uint8_t* p ;                                            // Point into ssdbuf
  
  for ( yc = y ; yc < ( y + h ) ; yc++ )                  // Loop vertically
  {
    pg = ( yc / 8 ) % OLED_NPAG ;                         // Page involved
    pat = 1 << ( yc & 7 ) ;                               // Bit involved
    p = ssdbuf[pg].page + x ;                             // Point to right place
    for ( xc = x ; xc < ( x + w ) ; xc++ )                // Loop horizontally
    {
      if ( color )                                        // Set bit?
      {
        *p |= pat ;                                       // Yes, set bit
      }
      else
      {
        *p &= ~pat ;                                      // No, clear bit
      }
      p++ ;                                               // Next 8 pixels
    }
    ssdbuf[pg].dirty = true ;                             // Page has been changed
  }
}


//***********************************************************************************************
//                                O L E D constructor                                           *
//***********************************************************************************************
// Constructor for the display.                                                                 *
//***********************************************************************************************
OLED::OLED ( uint8_t sda, uint8_t scl )
{
  i2c_config_t   i2c_config =                                // Set-up of I2C configuration
    {
      I2C_MODE_MASTER, 
      (gpio_num_t)sda,                                       // Pull ups for sda and scl
      GPIO_PULLUP_ENABLE,
      (gpio_num_t)scl,
      GPIO_PULLUP_ENABLE,
      400000                                                 // High speed
    } ;
  uint8_t      initbuf[] =                                   // Initial commands to init OLED
                  {
                    OLED_CONTROL_BYTE_CMD_STREAM,            // Stream next bytes
                    OLED_CMD_DISPLAY_OFF,                    // Display off
                    OLED_CMD_SET_DISPLAY_CLK_DIV, 0x80,      // Set divide ratio
                    OLED_CMD_SET_MUX_RATIO, SCREEN_HEIGHT-1, // Set multiplex ration (1:HEIGHT)
                    OLED_CMD_SET_DISPLAY_OFFSET, 0,          // Set diplay offset to 0
                    OLED_CMD_SET_DISPLAY_START_LINE,         // Set start line address
                    OLED_CMD_SET_CHARGE_PUMP, 0x14,          // Enable charge pump
                    OLED_CMD_SET_MEMORY_ADDR_MODE, 0x00,     // Set horizontal addressing mode
                    OLED_CMD_SET_SEGMENT_REMAP,              // Mirror X
                    OLED_CMD_SET_COM_SCAN_MODE,              // Mirror Y
                    OLED_CMD_SET_COM_PIN_MAP, 0x12,          // Set com pins hardware config
                    OLED_CMD_SET_CONTRAST, 0xFF,             // Set contrast
                    OLED_CMD_SET_PRECHARGE, 0xF1,            // Set precharge period
                    OLED_CMD_SET_VCOMH_DESELCT, 0x20,        // Set VCOMH
                    OLED_CMD_DISPLAY_RAM,                    // Output followes RAM
                    OLED_CMD_DISPLAY_NORMAL,                 // Set normal color
                    OLED_CMD_SCROLL_OFF,                     // Stop scrolling
                    OLED_CMD_DISPLAY_ON                      // Display on
                  } ;
  #if defined ( OLED1106 )                                      // Is it an SH1106?
    isSH1106 = true ;                                           // Set display type                         
  #endif
  dbgprint ( "ISSH116 is %d", (int)isSH1106 ) ;
  #if defined ( OLED1309 )                                      // Is it an SSD1309?
    isSSD1309 = true ;                                          // Set display type                         
  #endif
  dbgprint ( "ISSSD1309 is %d", (int)isSSD1309 ) ;
  ssdbuf = (page_struct*) malloc ( 8 * sizeof(page_struct) ) ;  // Create buffer for screen
  font = OLEDfont ;
  i2c_param_config ( I2C_NUM_0, &i2c_config ) ;
  i2c_driver_install ( I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0 ) ;
  openI2Cchan() ;                                               // Open I2C channel
  i2c_master_write (  i2Cchan, initbuf, sizeof(initbuf),
                      true ) ;
  closeI2Cchan() ;                                              // Send and close I2C channel
  clear() ;                                                     // Clear the display
}


//**************************************************************************************************
//                               O L E D _ D I S P L A Y B A T T E R Y                             *
//**************************************************************************************************
// Show the current battery charge level on the screen.                                            *
// It will overwrite the top divider.                                                              *
// No action if bat0/bat100 not defined in the preferences.                                        *
//**************************************************************************************************
void oled_displaybattery ( uint16_t bat0, uint16_t bat100, uint16_t adcval )
{
  if ( tft )
  {
    if ( bat0 < bat100 )                                  // Levels set in preferences?
    {
      static uint16_t oldpos = 0 ;                        // Previous charge level
      uint16_t        ypos ;                              // Position on screen
      uint16_t        v ;                                 // Constrainted ADC value
      uint16_t        newpos ;                            // Current setting

      v = constrain ( adcval, bat0,                       // Prevent out of scale
                      bat100 ) ;
      newpos = map ( v, bat0,                             // Compute length of green bar
                     bat100,
                     0, dsp_getwidth() ) ;
      if ( newpos != oldpos )                             // Value changed?
      {
        oldpos = newpos ;                                 // Remember for next compare
        ypos = tftdata[1].y - 5 ;                         // Just before 1st divider
        dsp_fillRect ( 0, ypos, newpos, 2, GREEN ) ;      // Paint green part
        dsp_fillRect ( newpos, ypos,
                       dsp_getwidth() - newpos,
                       2, RED ) ;                          // Paint red part
      }
    }
  }
}


//**************************************************************************************************
//                             O L E D _ D I S P L A Y V O L U M E                                 *
//**************************************************************************************************
// Show the current volume as an indicator on the screen.                                          *
// The indicator is 2 pixels heigh.                                                                *
//**************************************************************************************************
void oled_displayvolume ( uint8_t vol )
{
  if ( tft )
  {
    static uint8_t oldvol = 0 ;                         // Previous volume
    uint16_t       pos ;                                // Positon of volume indicator

    if ( vol != oldvol )                                // Volume changed?
    {
      oldvol = vol ;                                    // Remember for next compare
      pos = map ( vol, 0, 100, 0, dsp_getwidth() ) ;   // Compute position on TFT
      dsp_fillRect ( 0, dsp_getheight() - 2,
                     pos, 2, RED ) ;                    // Paint red part
      dsp_fillRect ( pos, dsp_getheight() - 2,
                     dsp_getwidth() - pos, 2, GREEN ) ; // Paint green part
    }
  }
}


//**************************************************************************************************
//                            O L E D _ D I S P L A Y T I M E                                      *
//**************************************************************************************************
// Show the time on the LCD at a fixed position in a specified color                               *
// To prevent flickering, only the changed part of the timestring is displayed.                    *
// An empty string will force a refresh on next call.                                              *
// A character on the screen is 8 pixels high and 6 pixels wide.                                   *
//**************************************************************************************************
void oled_displaytime ( const char* str, uint16_t color )
{
  static char oldstr[9] = "........" ;             // For compare
  uint8_t     i ;                                  // Index in strings
  uint16_t    pos = dsp_getwidth() + TIMEPOS ;     // X-position of character, TIMEPOS is negative

  if ( str[0] == '\0' )                            // Empty string?
  {
    for ( i = 0 ; i < 8 ; i++ )                    // Set oldstr to dots
    {
      oldstr[i] = '.' ;
    }
    return ;                                       // No actual display yet
  }
  if ( tft )                                       // TFT active?
  {
    dsp_setTextColor ( color ) ;                   // Set the requested color
    for ( i = 0 ; i < 8 ; i++ )                    // Compare old and new
    {
      if ( str[i] != oldstr[i] )                   // Difference?
      {
        dsp_fillRect ( pos, 0, 6, 8, BLACK ) ;     // Clear the space for new character
        dsp_setCursor ( pos, 0 ) ;                 // Prepare to show the info
        dsp_print ( str[i] ) ;                     // Show the character
        oldstr[i] = str[i] ;                       // Remember for next compare
      }
      pos += 6 ;                                   // Next position
    }
  }
}
