// helixfuncs.h
// Functions for HELIX decoder.
// Code for SPDIF leans heavily on https://github.com/earlephilhower/ESP8266Audio/blob/master/src/AudioOutputSPDIF.cpp
//
// 26-04-2023, ES: correction setting disable_pin
#include "config.h"

#define player_AdjustRate(a)                         // Not supported function
#define player_setTone(a)                            // Not supported function

#define FRAMESIZE               1600                 // Max. frame size in bytes (mp3 and aac)
#define OUTSIZE                 1152                 // Max number of samples per channel (mp3 and aac)
                                                     // AAC is max 1024, MP3 is 1152
#define I2SSIZE                 128                  // For I2S buffer (16/32 bits words).  Multiple of 8.

extern bool      muteflag ;                          // True if output must be muted
extern String    audio_ct ;                          // Content type, like "audio/aacp"

static int16_t   vol ;                               // Volume 0..100 percent
static bool      mp3mode ;                           // True if mp3 input (not aac)
static uint8_t   mp3buff[FRAMESIZE+32] ;             // Space for enough chunks to hold one frame
static uint8_t*  mp3bpnt ;                           // Points into mp3buff
static int       mp3bcnt ;                           // Number of samples in buffer
static bool      searchFrame ;                       // True if search for startframe is needed
static int16_t   outbuf[OUTSIZE*2] ;                 // MP3 output (PCM) buffer

const  char*     HTAG = "helixfuncs" ;

#ifdef DEC_HELIX_SPDIF
  // BMC (Biphase Mark Coded) values (bit order reversed, i.e. LSB first)
  static const uint16_t PROGMEM spdif_bmclookup[256] =
    { 
      0xcccc, 0x4ccc, 0x2ccc, 0xaccc, 0x34cc, 0xb4cc, 0xd4cc, 0x54cc, // 00..07
      0x32cc, 0xb2cc, 0xd2cc, 0x52cc, 0xcacc, 0x4acc, 0x2acc, 0xaacc, // 08..0F
      0x334c, 0xb34c, 0xd34c, 0x534c, 0xcb4c, 0x4b4c, 0x2b4c, 0xab4c, // 10..17
      0xcd4c, 0x4d4c, 0x2d4c, 0xad4c, 0x354c, 0xb54c, 0xd54c, 0x554c, // 18..1F
      0x332c, 0xb32c, 0xd32c, 0x532c, 0xcb2c, 0x4b2c, 0x2b2c, 0xab2c, // 20..27
      0xcd2c, 0x4d2c, 0x2d2c, 0xad2c, 0x352c, 0xb52c, 0xd52c, 0x552c, // 28..2F
      0xccac, 0x4cac, 0x2cac, 0xacac, 0x34ac, 0xb4ac, 0xd4ac, 0x54ac, // 30..37
      0x32ac, 0xb2ac, 0xd2ac, 0x52ac, 0xcaac, 0x4aac, 0x2aac, 0xaaac, // 38..3F
      0x3334, 0xb334, 0xd334, 0x5334, 0xcb34, 0x4b34, 0x2b34, 0xab34, // 40..47
      0xcd34, 0x4d34, 0x2d34, 0xad34, 0x3534, 0xb534, 0xd534, 0x5534, // 48..4F
      0xccb4, 0x4cb4, 0x2cb4, 0xacb4, 0x34b4, 0xb4b4, 0xd4b4, 0x54b4, // 50..57
      0x32b4, 0xb2b4, 0xd2b4, 0x52b4, 0xcab4, 0x4ab4, 0x2ab4, 0xaab4, // 58..5F
      0xccd4, 0x4cd4, 0x2cd4, 0xacd4, 0x34d4, 0xb4d4, 0xd4d4, 0x54d4, // 60..67
      0x32d4, 0xb2d4, 0xd2d4, 0x52d4, 0xcad4, 0x4ad4, 0x2ad4, 0xaad4, // 68..6F
      0x3354, 0xb354, 0xd354, 0x5354, 0xcb54, 0x4b54, 0x2b54, 0xab54, // 70..77
      0xcd54, 0x4d54, 0x2d54, 0xad54, 0x3554, 0xb554, 0xd554, 0x5554, // 78..7F
      0x3332, 0xb332, 0xd332, 0x5332, 0xcb32, 0x4b32, 0x2b32, 0xab32, // 80..87
      0xcd32, 0x4d32, 0x2d32, 0xad32, 0x3532, 0xb532, 0xd532, 0x5532, // 88..8F
      0xccb2, 0x4cb2, 0x2cb2, 0xacb2, 0x34b2, 0xb4b2, 0xd4b2, 0x54b2, // 90..97
      0x32b2, 0xb2b2, 0xd2b2, 0x52b2, 0xcab2, 0x4ab2, 0x2ab2, 0xaab2, // 98..9F
      0xccd2, 0x4cd2, 0x2cd2, 0xacd2, 0x34d2, 0xb4d2, 0xd4d2, 0x54d2, // A0..A7
      0x32d2, 0xb2d2, 0xd2d2, 0x52d2, 0xcad2, 0x4ad2, 0x2ad2, 0xaad2, // A8..AF
      0x3352, 0xb352, 0xd352, 0x5352, 0xcb52, 0x4b52, 0x2b52, 0xab52, // B0..B7
      0xcd52, 0x4d52, 0x2d52, 0xad52, 0x3552, 0xb552, 0xd552, 0x5552, // B8..BF
      0xccca, 0x4cca, 0x2cca, 0xacca, 0x34ca, 0xb4ca, 0xd4ca, 0x54ca, // C0..C7
      0x32ca, 0xb2ca, 0xd2ca, 0x52ca, 0xcaca, 0x4aca, 0x2aca, 0xaaca, // C8..CF
      0x334a, 0xb34a, 0xd34a, 0x534a, 0xcb4a, 0x4b4a, 0x2b4a, 0xab4a, // D0..D7
      0xcd4a, 0x4d4a, 0x2d4a, 0xad4a, 0x354a, 0xb54a, 0xd54a, 0x554a, // D8..DF
      0x332a, 0xb32a, 0xd32a, 0x532a, 0xcb2a, 0x4b2a, 0x2b2a, 0xab2a, // E0..E7
      0xcd2a, 0x4d2a, 0x2d2a, 0xad2a, 0x352a, 0xb52a, 0xd52a, 0x552a, // E8..EF
      0xccaa, 0x4caa, 0x2caa, 0xacaa, 0x34aa, 0xb4aa, 0xd4aa, 0x54aa, // F0..F7
      0x32aa, 0xb2aa, 0xd2aa, 0x52aa, 0xcaaa, 0x4aaa, 0x2aaa, 0xaaaa  // F8..FF
    } ;
    const uint32_t VUCP_PREAMBLE_B = 0xCCE80000 ;     // 11001100 11101000
    const uint32_t VUCP_PREAMBLE_M = 0xCCE20000 ;     // 11001100 11100010
    const uint32_t VUCP_PREAMBLE_W = 0xCCE40000 ;     // 11001100 11100100
    static uint32_t i2sbuf[I2SSIZE] ;                 // Buffer for I2S biphase buffer
#else
    static int16_t  i2sbuf[I2SSIZE] ;                 // Buffer for I2S
#endif

//**************************************************************************************************
//                              P L A Y E R _ S E T V O L U M E                                    *
//**************************************************************************************************
// Set volume percentage.                                                                          *
//**************************************************************************************************
void player_setVolume ( int16_t v )
{
  if ( vol != v )
  {
    vol = v ;   	                                     // Save volume percentage
    ESP_LOGI ( HTAG, "Volume set to %d", vol ) ;
    #ifdef DEC_HELIX_AI                                // For AI Audio kit: set volume directly
      int8_t db = map ( vol, 0, 100, 0x0, 0x3F ) ;     // 0..100% to -43.5 .. 0 dB (0..63)
      dac.SetVolumeSpeaker ( db ) ;                    // Set volume control of amplifier
      dac.SetVolumeHeadphone ( db ) ;
    #endif
  }
}


//**************************************************************************************************
//                              P L A Y E R _ G E T V O L U M E                                    *
//**************************************************************************************************
// Get volume percentage.                                                                          *
//**************************************************************************************************
int16_t player_getVolume()
{
  return vol ;   	                                   // Return volume percentage
}


//**************************************************************************************************
//                                    H E L I X I N I T                                            *
//**************************************************************************************************
// Initialize helix buffering.                                                                     *
//**************************************************************************************************
void helixInit ( int8_t enable_pin, int8_t disable_pin )
{
  ESP_LOGI ( HTAG, "helixInit called for %s",         // Show activity
             audio_ct.c_str() ) ;
  mp3mode = ( audio_ct.indexOf ( "mpeg" ) > 0 ) ;     // Set mp3/aac mode
  mp3bpnt = mp3buff ;                                 // Reset pointer
  mp3bcnt = 0 ;                                       // Buffer empty
  searchFrame = true ;                                // Start searching for frame
  if ( enable_pin >= 0 )                              // Enable pin defined?
  {
    pinMode ( enable_pin, OUTPUT ) ;                  // Yes, set pin to output
    digitalWrite ( enable_pin, HIGH ) ;               // Enable output
  }
  if ( disable_pin >= 0 )                             // Disable pin defined?
  {
    pinMode ( disable_pin, OUTPUT ) ;                 // Yes, set pin to output
    digitalWrite ( disable_pin, LOW ) ;               // Enable output
  }
}


//**************************************************************************************************
//                               O U T P U T S A M P L E                                           *
//**************************************************************************************************
// Add a sample to the I2S buffer.  Send to I2S driver if buffer is full.                          *
// For SPDIFF: convert to a valid frame.  16-bit sample to a frame of 2 x 32 bits.                 *
//**************************************************************************************************
void outputSample ( int16_t c )
{
  static uint8_t  frame_num = 0 ;                     // Frame number for spdif
  static bool     left = true ;                       // Switch between left and right sample
  static uint8_t  i2sinx = 0 ;                        // Index in i2sbuf
  size_t          bw ;                                // Number of bytes written to I2S
  
  #ifndef DEC_HELIX_AI
    #ifdef DEC_HELIX_INT                              // Internal DAC used?
      c = c * vol / 100 + 0x8000 ;                    // Yes, scale according to volume and shift above negative values 
    #else
      c = c * vol / 100 ;                             // Scale according to volume
    #endif
  #endif
  #ifdef DEC_HELIX_SPDIF                              // Spdif output?
    uint16_t hi, lo, aux ;                            // Needed for spdif conversion
    hi = spdif_bmclookup[(uint8_t)(c >> 8)] ;         // Convert high byte of sample, sign extend later
    lo = spdif_bmclookup[(uint8_t)c] ;                // Convert low byte of sample
    // Low word is inverted depending on sign bit of high word. Positive causes inversion (XOR with 0xFFFF).
    lo ^= ( ~( (int16_t)hi ) >> 16 ) ;                // XOR with 0x0000 or 0xFFFF
    // Compute auxiliary audio databits, XOR with 0x0000 or 0X7FFF
    aux = 0xb333 ^ (((uint32_t)((int16_t)lo)) >> 17) ;
    if ( left )                                       // Left channel?
    {
      if ( frame_num )                                // First frame?
      {
        i2sbuf[i2sinx++] = VUCP_PREAMBLE_M | aux ;    // No, use preamble M
      }
      else
      {
        i2sbuf[i2sinx++] = VUCP_PREAMBLE_B | aux ;    // Yes, use preamble B
      }
      left = false ;                                  // Next channel is right
    }
    else                                              // Right channel
    {
      i2sbuf[i2sinx++] = VUCP_PREAMBLE_W | aux ;      // Use preamble W
      if ( ++frame_num == 192 )                       // Update and check frame number
      {
        frame_num = 0 ;                               // Start a new frame
      }
      left = true ;                                   // Next sample is left
    }
    i2sbuf[i2sinx++] = ( (uint32_t)lo << 16 ) | hi ;  // Store the 32 data bits (16 bit sample) 
  #else
    i2sbuf[i2sinx++] = c ;                            // Store 16 bits data
  #endif
  if ( i2sinx == I2SSIZE )                            // Buffer filled?
  {
    if ( i2s_write ( I2S_NUM_0, i2sbuf,               // Yes, send to I2S
                     sizeof(i2sbuf), &bw,
                     portMAX_DELAY ) != ESP_OK )
    {
      ESP_LOGI ( HTAG, "i2s_write error!" ) ;
    }
    if ( bw != sizeof(i2sbuf) )
    {
      ESP_LOGI ( HTAG, "i2s_write bytes written %d, should be %d!",
                 bw, sizeof(i2sbuf) ) ;
    }
    i2sinx = 0 ;                                      // Start at new buffer
  }
}


//**************************************************************************************************
//                                    P L A Y C H U N K                                            *
//**************************************************************************************************
// Play the next 32 bytes.                                                                         *
//**************************************************************************************************
void playChunk ( const uint8_t* chunk )
{
  static uint32_t samprate ;                          // Sample rate
  static int      channels ;                          // Number of channels
  static int      smpbytes ;                          // Number of bytes for I2S
  static int      smpwords ;                          // Number of 16 bit wordsfor I2S
  int             s ;                                 // Position of syncword
  static bool     once ;                              // To execute part of code once
  int             n ;                                 // Number of samples decoded
  int             br = 0 ;                            // Bit rate
  int             bps = 0 ;                           // Bits per sample

  memcpy ( mp3bpnt, chunk, 32 ) ;                     // Add chunk to frame buffer
  mp3bcnt += 32 ;                                     // Update counter
  mp3bpnt += 32 ;                                     // and pointer
  if ( searchFrame && ( mp3bcnt >= FRAMESIZE ) )      // Frame search complete?
  {
    if ( mp3mode )
    {
      s = MP3FindSyncWord ( mp3buff, mp3bcnt ) ;      // Search for first mp3 frame
    }
    else
    {
      s = AACFindSyncWord ( mp3buff, mp3bcnt ) ;      // Search for first aac frame
    }
    if ( ( searchFrame = ( s < 0 ) ) )                // Sync found?
    {
      mp3bcnt = 0 ;                                   // No, reset counter
      mp3bpnt = mp3buff ;                             // And fillpointer
      samprate = 0 ;                                  // Still unknown
      return ;                                        // Return with no effect
    }
    else
    {
      once = true ;                                   // Get samplerate once
      ESP_LOGI ( HTAG, "Sync found at 0x%04X", s ) ;
      if ( s > 0 )
      {
        mp3bcnt -= s ;                                // Frame found, update count
        memcpy ( mp3buff, mp3buff + s, mp3bcnt ) ;    // Shift mp3 data to begin of buffer
        mp3bpnt = mp3buff + mp3bcnt ;                 // Adjust fill pointer
      }
    }
  }
  if ( mp3bcnt >= FRAMESIZE )                         // Complete frame in buffer?
  {
    int newcnt = mp3bcnt ;                            // Used to get number of bytes converted
    if ( mp3mode )
    {
      n = MP3Decode ( mp3buff, &newcnt,               // Decode the frame
                          outbuf, 0 ) ;
      if ( n == ERR_MP3_NONE )
      {
        if ( once )
        {
          samprate = MP3GetSampRate() ;               // Get sample rate
          channels = MP3GetChannels() ;               // Get number of channels
          br       = MP3GetBitrate() ;                // Get bit rate
          bps      = MP3GetBitsPerSample() ;          // Get bits per sample
          smpwords = MP3GetOutputSamps() ;            // Get number of output samples
        }
      }
    }
    else
    {
      n = AACDecode ( mp3buff, &newcnt, outbuf ) ;    // Decode the frame
      if ( n == ERR_AAC_NONE )
      {
        if ( once )
        {
          samprate = AACGetSampRate() ;               // Get sample rate
          channels = AACGetChannels() ;               // Get number of channels
          br       = AACGetBitrate() ;                // Get bit rate
          bps      = AACGetBitsPerSample() ;          // Get bits per sample
          smpwords = AACGetOutputSamps() ;            // Get number of output (16 bit) samples
        }
      }
    }
    int hb = mp3bcnt - newcnt ;                       // Number of bytes handled
    if ( n < 0 )                                      // Check if decode is okay
    {
      ESP_LOGI ( HTAG, "MP3Decode error %d", n ) ;
      helixInit ( -1, -1 ) ;                          // Totally wrong, start all over
      return ;
    }
    if ( once )
    {
      smpbytes = smpwords * 2 ;                       // Number of bytes in outbuf
      ESP_LOGI ( HTAG, "Bitrate     is %d", br ) ;    // Show decoder parameters
      ESP_LOGI ( HTAG, "Samprate    is %d", samprate ) ;
      ESP_LOGI ( HTAG, "Channels    is %d", channels ) ;
      ESP_LOGI ( HTAG, "Bitpersamp  is %d", bps ) ;
      ESP_LOGI ( HTAG, "Outputsamps is %d", smpwords ) ;
      if ( br )                                       // Prevent division by zero
      {
       #ifdef DEC_HELIX_SPDIF
        i2s_set_sample_rates ( I2S_NUM_0,             // Set samplerate (biphase)
                               samprate*2 ) ;
       #else
        i2s_set_sample_rates ( I2S_NUM_0,             // Set samplerate
                               samprate ) ;
       #endif
      }
      i2s_start ( I2S_NUM_0 ) ;                       // Start I2S output
      once = false ;                                  // No need to set samplerate again
    }
    if ( muteflag )                                   // Muted?
    {
      memset ( outbuf, 0, smpbytes ) ;                // Yes, clear buffer
    }
    for ( int i = 0 ; i < smpwords ; i++ )            // Handle all samples in outbuf
    {
      outputSample ( outbuf[i] ) ;
      if ( channels == 1 )                            // Mono signal?
      {
        outputSample ( outbuf[i] ) ;                  // Yes, right sample equals left sample
      }
    }
    mp3bcnt -= hb ;
    memcpy ( mp3buff, mp3buff + hb,                   // Shift mp3 data to begin of buffer
             mp3bcnt ) ;
    mp3bpnt = mp3buff +mp3bcnt ;
  }
}