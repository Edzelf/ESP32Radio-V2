// helixfuncs.h
// Fake function for HELIX decoder that are not supported.
// Also framebuffering from incoming chunks is supported.
//
#define player_getVolume()      100
#define player_AdjustRate(a)
#define player_setVolume(a)
#define player_setTone(a)

#define FRAMESIZE               1600                 // Max. frame size in bytes (mp3 and aac)
//#define OUTSIZE                 1152                 // Max number of samples per channel
#define OUTSIZE                 2048                 // Max number of samples per channel (mp3 and aac)

extern bool      muteflag ;
extern String    audio_ct ;

static bool      mp3mode ;                           // True if mp3 input (not aac)
static uint8_t   mp3buff[FRAMESIZE+32] ;             // Space for enough chunks to hold one frame
static uint8_t*  mp3bpnt ;                           // Points into mp3buff
static int       mp3bcnt ;                           // Number of samples in buffer
static bool      searchFrame ;                       // True if search for startframe is needed
static int16_t   outbuf[OUTSIZE*2] ;                 // I2S buffer


//**************************************************************************************************
//                                    H E L I X I N I T                                            *
//**************************************************************************************************
// Initialize helix buffering.                                                                     *
//**************************************************************************************************
void helixInit()
{
  dbgprint ( "helixInit called for %s",               // Show activity
             audio_ct.c_str() ) ;
  mp3mode = ( audio_ct.indexOf ( "mpeg" ) > 0 ) ;     // Set mp3/aac mode
  mp3bpnt = mp3buff ;                                 // Reset pointer
  mp3bcnt = 0 ;                                       // Buffer empty
  searchFrame = true ;                                // Start searching for frame
}


//**************************************************************************************************
//                                    P L A Y C H U N K                                            *
//**************************************************************************************************
// Play the next 32 bytes.                                                                         *
//**************************************************************************************************
void playChunk ( i2s_port_t i2s_num, const uint8_t* chunk )
{
  static uint32_t samprate ;                          // Sample rate
  static int      channels ;                          // Number of channels
  static int      smpbytes ;                          // Number of bytes for I2S
  int             s ;                                 // Position of syncword
  static bool     once ;                              // To execute part of code once
  int             n ;                                 // Number of samples decoded
  size_t          bw ;                                // I2S bytes written
  int             br = 0 ;                            // Bit rate
  int             bps = 0 ;                           // Bits per sample
  int             ops = 0 ;                           // Number of output samples

  memcpy ( mp3bpnt, chunk, 32 ) ;                     // Add chunk to frame buffer
  mp3bcnt += 32 ;                                     // Update counter
  mp3bpnt += 32 ;                                     // and pointer
  if ( searchFrame && ( mp3bcnt >= FRAMESIZE ) )      // Frame search complete?
  {
    if ( mp3mode )
    {
      s = MP3FindSyncWord ( mp3buff, mp3bcnt ) ;      // Search for first frame
    }
    else
    {
      s = AACFindSyncWord ( mp3buff, mp3bcnt ) ;      // Search for first frame
    }
    if ( ( searchFrame = ( s < 0 ) ) )                // Sync found?
    {
      mp3bcnt = 0 ;                                   // No, reset counter
      mp3bpnt = mp3buff ;                             // And fillpointer
      return ;                                        // Return with no effect
    }
    else
    {
      once = true ;                                   // Get samplerate once
      dbgprint ( "Sync found at 0x%04X", s ) ;
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
          br = MP3GetBitrate() ;                      // Get bit rate
          bps = MP3GetBitsPerSample() ;               // Get bits per sample
          ops = MP3GetOutputSamps() ;                 // Get number of output samples
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
          br = AACGetBitrate() ;                      // Get bit rate
          bps = AACGetBitsPerSample() ;               // Get bits per sample
          ops = AACGetOutputSamps() ;                 // Get number of output samples
        }
      }
    }
    int hb = mp3bcnt - newcnt ;                       // Number of bytes handled
    if ( once )
    {
      smpbytes = ops * channels ;
      dbgprint ( "Bitrate     is %d", br ) ;          // Show decoder parameters
      dbgprint ( "Samprate    is %d", samprate ) ;
      dbgprint ( "Channels    is %d", channels ) ;
      dbgprint ( "Bitpersamp  is %d", bps ) ;
      dbgprint ( "Outputsamps is %d", ops ) ;
      i2s_set_sample_rates ( i2s_num, samprate ) ;    // Set samplerate
      i2s_start ( i2s_num ) ;                         // Start DAC
      once = false ;                                  // No need to set samplerate again
    }
    if ( n < 0 )                                      // Check if okay
    {
      dbgprint ( "MP3Decode error %d", n ) ;
      helixInit() ;                                   // Totally wrong, start all over
      return ;
    }
    if ( muteflag )                                   // Muted?
    {
      memset ( outbuf, 0, smpbytes ) ;                // Yes, clear buffer
    }
    i2s_write ( i2s_num, outbuf, smpbytes, &bw,       // Send to I2S
                portMAX_DELAY  ) ;
    mp3bcnt -= hb ;
    memcpy ( mp3buff, mp3buff + hb,                   // Shift mp3 data to begin of buffer
             mp3bcnt ) ;
    mp3bpnt = mp3buff +mp3bcnt ;
  }
}

