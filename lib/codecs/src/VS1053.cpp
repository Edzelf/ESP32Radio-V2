//**************************************************************************************************
// VS1053 class implementation.                                                                    *
//**************************************************************************************************
#include "VS1053.h"

VS1053*     vs1053player ;                          // The object for the MP3 player

extern bool  pin_exists ( uint8_t pin ) ;           // Check GPIO pin number

//**************************************************************************************************
// VS1053 constructor.                                                                             *
//**************************************************************************************************
VS1053::VS1053 ( int8_t _cs_pin, int8_t _dcs_pin, int8_t _dreq_pin,
                 int8_t _shutdown_pin, int8_t _shutdownx_pin ) :
  cs_pin(_cs_pin), dcs_pin(_dcs_pin), dreq_pin(_dreq_pin), shutdown_pin(_shutdown_pin),
  shutdownx_pin(_shutdownx_pin)
{
}

void VS1053::control_mode_on() const
{
  SPI.beginTransaction ( VS1053_SPI ) ;       // Prevent other SPI users
  digitalWrite ( cs_pin, LOW ) ;
}

void VS1053::control_mode_off() const
{
  digitalWrite ( cs_pin, HIGH ) ;             // End control mode
  SPI.endTransaction() ;                      // Allow other SPI users
}

void VS1053::data_mode_on() const
{
  SPI.beginTransaction ( VS1053_SPI ) ;       // Prevent other SPI users
  //digitalWrite ( cs_pin, HIGH ) ;           // Bring slave in data mode
  digitalWrite ( dcs_pin, LOW ) ;
}

void VS1053::data_mode_off() const
{
  digitalWrite ( dcs_pin, HIGH ) ;            // End data mode
  SPI.endTransaction() ;                      // Allow other SPI users
}

uint16_t VS1053::read_register ( uint8_t _reg ) const
{
  uint16_t result ;

  control_mode_on() ;                               // Start control-mode transaction
  SPI.write ( 3 ) ;                                 // Read operation
  SPI.write ( _reg ) ;                              // Register to read (0..0xF)
  // Note: transfer16 does not seem to work
  result = ( SPI.transfer ( 0xFF ) << 8 ) |         // Read 16 bits data
           ( SPI.transfer ( 0xFF ) ) ;
  await_data_request() ;                            // Wait for DREQ to be HIGH again
  control_mode_off() ;                              // End control_mode transaction
  return result ;
}

void VS1053::write_register ( uint8_t _reg, uint16_t _value ) const
{
  control_mode_on() ;                               // Start control-mode transaction
  SPI.write ( 2 ) ;                                 // Write operation
  SPI.write ( _reg ) ;                              // Register to write (0..0xF)
  SPI.write16 ( _value ) ;                          // Send 16 bits data
  await_data_request() ;
  control_mode_off() ;                              // End control_mode transaction
}

bool VS1053::sdi_send_buffer ( uint8_t* data, size_t len )
{
  size_t chunk_length ;                             // Length of chunk 32 byte or shorter

  while ( len )                                     // More to do?
  {
    chunk_length = len ;
    if ( len > vs1053_chunk_size )
    {
      chunk_length = vs1053_chunk_size ;
    }
    len -= chunk_length ;
    await_data_request() ;                          // Wait for space available
    data_mode_on() ;                                // Start data-mode transaction
    SPI.writeBytes ( data, chunk_length ) ;
    data_mode_off() ;                               // End data-mode transaction
    data += chunk_length ;
  }
  return data_request() ;                           // True if more data can be stored in fifo
}

void VS1053::sdi_send_fillers ( uint8_t numchunks )
{
  while ( numchunks-- )                             // More to do?
  {
    await_data_request() ;                          // Wait for space available
    data_mode_on() ;                                // Start data-mode transaction
    for ( uint8_t i = 0 ; i < vs1053_chunk_size ; i++ )
    {
      SPI.write ( endFillByte ) ;
    }
    data_mode_off() ;                               // End data-mode transaction
  }
}

void VS1053::wram_write ( uint16_t address, uint16_t data )
{
  write_register ( SCI_WRAMADDR, address ) ;
  write_register ( SCI_WRAM, data ) ;
}

uint16_t VS1053::wram_read ( uint16_t address )
{
  write_register ( SCI_WRAMADDR, address ) ;            // Start reading from WRAM
  return read_register ( SCI_WRAM ) ;                   // Read back result
}

bool VS1053::testComm ( const char *header )
{
  // Test the communication with the VS1053 module.  The result wille be returned.
  // If DREQ is low, there is problably no VS1053 connected.  Pull the line HIGH
  // in order to prevent an endless loop waiting for this signal.  The rest of the
  // software will still work, but readbacks from VS1053 will fail.
  int            i ;                                    // Loop control
  uint16_t       r1, r2, cnt = 0 ;
  uint16_t       delta = 300 ;                          // 3 for fast SPI
  const uint16_t vstype[] = { 1001, 1011, 1002, 1003,   // Possible chip versions
                              1053, 1033, 0000, 1103 } ;
  
  ESP_LOGI ( VTAG, "%s", header ) ;                     // Show a header
  if ( !digitalRead ( dreq_pin ) )
  {
    ESP_LOGE ( VTAG, "VS1053 not properly installed!" ) ;
    return false ;                                      // Return bad result
  }
  // Further TESTING.  Check if SCI bus can write and read without errors.
  // We will use the volume setting for this.
  // Will give warnings on serial output if DEBUG is active.
  // A maximum of 20 errors will be reported.
  if ( strstr ( header, "Fast" ) )
  {
    delta = 3 ;                                         // Fast SPI, more loops
  }
  for ( i = 0 ; ( i < 0xFFFF ) && ( cnt < 20 ) ; i += delta )
  {
    write_register ( SCI_VOL, i ) ;                     // Write data to SCI_VOL
    r1 = read_register ( SCI_VOL ) ;                    // Read back for the first time
    r2 = read_register ( SCI_VOL ) ;                    // Read back a second time
    if  ( r1 != r2 || i != r1 || i != r2 )              // Check for 2 equal reads
    {
      ESP_LOGE ( VTAG, "SPI error. SB:%04X R1:%04X R2:%04X", i, r1, r2 ) ;
      cnt++ ;
      delay ( 10 ) ;
    }
  }
  okay = ( cnt == 0 ) ;                                 // True if working correctly
  // Further testing: is it the right chip?
  r1 = ( read_register ( SCI_STATUS ) >> 4 ) & 0x7 ;    // Read status to get the version
  if ( r1 !=  4 )                                       // Version 4 is a genuine VS1053
  {
    ESP_LOGW ( VTAG, "This is not a VS1053, "           // Report the wrong chip
               "but a VS%d instead!",
               vstype[r1] ) ;
    //okay = false ;                                    // Standard codecs not fully supported
  }
  return ( okay ) ;                                     // Return the result
}

void VS1053::begin()
{
  pinMode      ( dreq_pin,  INPUT_PULLUP ) ;            // DREQ is an input
  pinMode      ( cs_pin,    OUTPUT ) ;                  // The SCI and SDI signals
  pinMode      ( dcs_pin,   OUTPUT ) ;
  digitalWrite ( dcs_pin,   HIGH ) ;                    // Start HIGH for SCI en SDI
  digitalWrite ( cs_pin,    HIGH ) ;
  if ( pin_exists ( shutdown_pin ) )                    // Shutdown in use?
  {
    pinMode ( shutdown_pin,   OUTPUT ) ;
  }
  if ( pin_exists ( shutdownx_pin ) )                   // Shutdown (inversed logic) in use?
  {
    pinMode ( shutdownx_pin,   OUTPUT ) ;
  }
  output_enable ( false ) ;                             // Disable amplifier through shutdown pin(s)
  delay ( 100 ) ;
  // Init SPI in slow mode ( 0.2 MHz )
  VS1053_SPI._clock = 200000  ;
  VS1053_SPI._bitOrder = MSBFIRST ;
  VS1053_SPI._dataMode = SPI_MODE0 ;
  delay ( 20 ) ;
  //printDetails ( "20 msec after reset" ) ;
  if ( testComm ( "Slow SPI, Testing VS1053 read/write registers..." ) )
  {
    // Most VS1053 modules will start up in midi mode.  The result is that there is no audio
    // when playing MP3.  You can modify the board, but there is a more elegant way:
    wram_write ( 0xC017, 3 ) ;                            // GPIO DDR = 3
    wram_write ( 0xC019, 0 ) ;                            // GPIO ODATA = 0
    delay ( 100 ) ;
    //printDetails ( "After test loop" ) ;
    softReset() ;                                         // Do a soft reset
    // Switch on the analog parts
    write_register ( SCI_AUDATA, 44100 + 1 ) ;            // 44.1kHz + stereo
    // The next clocksetting allows SPI clocking at 5 MHz, 4 MHz is safe then.
    write_register ( SCI_CLOCKF, 6 << 12 ) ;              // Normal clock settings
    // multiplyer 3.0 = 12.2 MHz
    VS1053_SPI._clock = 5000000 ;                         // SPI Clock to 5 MHz.
    write_register ( SCI_MODE, _BV ( SM_SDINEW ) | _BV ( SM_LINE1 ) ) ;
    testComm ( "Fast SPI, Testing VS1053 read/write registers again..." ) ;
    delay ( 10 ) ;
    await_data_request() ;
    endFillByte = wram_read ( 0x1E06 ) & 0xFF ;
    delay ( 100 ) ;
  }
}

void VS1053::setVolume ( uint8_t vol )
{
  // Set volume.  Both left and right.
  // Input value is 0..100.  100 is the loudest.
  // The low limit of the VS1053 range is limited to 0x78
  // as the range between 0x78 and 0xF8 does not produce audible output.
  // Clicking reduced by using 0xf8 to 0x00 as limits.
  uint16_t value ;                                      // Value to send to SCI_VOL

  if ( vol != curvol )
  {
    curvol = vol ;                                      // Save for later use
    value = map ( vol, 0, 100, 0x78, 0x00 ) ;           // 0..100% to one channel
    if ( vol == 0 )                                     // Forc vol=0 to completely off
    {
      value = 0xF8 ;
    }
    value = ( value << 8 ) | value ;
    write_register ( SCI_VOL, value ) ;                 // Volume left and right
    if ( vol == 0 )                                     // Completely silence?
    {
      output_enable ( false ) ;                         // Yes, mute amplifier
    }
    else
    {
      output_enable ( true ) ;                          // Enable amplifier
    }
    output_enable ( vol != 0 ) ;                        // Enable/disable amplifier through shutdown pin(s)
  }
}

void VS1053::setTone ( uint8_t *rtone )                 // Set bass/treble (4 nibbles)
{
  // Set tone characteristics.  See documentation for the 4 nibbles.
  uint16_t value = 0 ;                                  // Value to send to SCI_BASS
  int      i ;                                          // Loop control

  for ( i = 0 ; i < 4 ; i++ )
  {
    value = ( value << 4 ) | rtone[i] ;                 // Shift next nibble in
  }
  write_register ( SCI_BASS, value ) ;                  // Volume left and right
}

void VS1053::startSong()
{
  sdi_send_fillers ( 60 ) ;
  output_enable ( true ) ;                              // Enable amplifier through shutdown pin(s)
}

bool VS1053::playChunk ( uint8_t* data, size_t len )
{
  return okay && sdi_send_buffer ( data, len ) ;        // True if more data can be added to fifo
}

void VS1053::stopSong()
{
  uint16_t modereg ;                                    // Read from mode register
  int      i ;                                          // Loop control

  sdi_send_fillers ( 60 ) ;                             // Send 60 * 32 fillers
  output_enable ( false ) ;                             // Disable amplifier through shutdown pin(s)
  delay ( 10 ) ;
  write_register ( SCI_MODE, _BV ( SM_SDINEW ) | _BV ( SM_CANCEL ) ) ;
  for ( i = 0 ; i < 20 ; i++ )
  {
    sdi_send_fillers ( 1 ) ;                            // Send 32 fillers
    modereg = read_register ( SCI_MODE ) ;              // Read mode status
    if ( ( modereg & _BV ( SM_CANCEL ) ) == 0 )         // SM_CANCEL will be cleared when finished
    {
      sdi_send_fillers ( 60 ) ;
      ESP_LOGI ( VTAG, "Song stopped correctly after %d msec", i * 10 ) ;
      return ;
    }
    delay ( 10 ) ;
  }
  //printDetails ( "Song stopped incorrectly!" ) ;
}

void VS1053::softReset()
{
  write_register ( SCI_MODE, _BV ( SM_SDINEW ) | _BV ( SM_RESET ) ) ;
  delay ( 10 ) ;
  await_data_request() ;
}

// void VS1053::printDetails ( const char *header )
// {
//   uint16_t     regbuf[16] ;
//   uint8_t      i ;

//   ESP_LOGI ( VTAG, header ) ;
//   ESP_LOGI ( VTAG, "REG   Contents" ) ;
//   ESP_LOGI ( VTAG, "---   -----" ) ;
//   for ( i = 0 ; i <= SCI_num_registers ; i++ )
//   {
//     regbuf[i] = read_register ( i ) ;
//   }
//   for ( i = 0 ; i <= SCI_num_registers ; i++ )
//   {
//     delay ( 5 ) ;
//     ESP_LOGI ( VTAG, "%3X - %5X", i, regbuf[i] ) ;
//   }
// }

void  VS1053::output_enable ( bool ena )               // Enable amplifier through shutdown pin(s)
{
  if ( shutdown_pin >= 0 )                             // Shutdown in use?
  {
    digitalWrite ( shutdown_pin, !ena ) ;              // Shut down or enable audio output
  }
  if ( shutdownx_pin >= 0 )                            // Shutdown (inversed logic) in use?
  {
    digitalWrite ( shutdownx_pin, ena ) ;              // Shut down or enable audio output
  }
}


void VS1053::AdjustRate ( long ppm2 )                  // Fine tune the data rate 
{
  write_register ( SCI_WRAMADDR, 0x1e07 ) ;
  write_register ( SCI_WRAM,     ppm2 ) ;
  write_register ( SCI_WRAM,     ppm2 >> 16 ) ;
  // oldClock4KHz = 0 forces  adjustment calculation when rate checked.
  write_register ( SCI_WRAMADDR, 0x5b1c ) ;
  write_register ( SCI_WRAM,     0 ) ;
  // Write to AUDATA or CLOCKF checks rate and recalculates adjustment.
  write_register ( SCI_AUDATA,   read_register ( SCI_AUDATA ) ) ;
}


void VS1053::streamMode ( bool onoff )                // Set stream mode on/off
{
  uint16_t pat = _BV(SM_SDINEW) ;                     // Assume off

  if ( onoff )                                        // Activate stream mode?
  {
    pat |= _BV(SM_STREAM) ;                           // Yes, set stream mode bit
  }
  write_register ( SCI_MODE, pat ) ;                  // Set new value
  delay ( 10 ) ;
  await_data_request() ;
}

bool VS1053_begin ( int8_t cs, int8_t dcs, int8_t dreq, int8_t shutdown, int8_t shutdownx )
{
  //if ( ! pin_exists ( dreq ) )                      // Check DREC pin
  //{
  //  return false ;
  //}
  vs1053player = new VS1053 ( cs, dcs, dreq,          // Create object
                              shutdown, shutdownx ) ;
  if ( ! vs1053player->data_request() )               // DREC should be high
  {
    return false ;
  }
  vs1053player->streamMode ( true ) ;                 // Set streammode (experimental)
  vs1053player->begin() ;                             // Initialize VS1053 player
  return true ;
}