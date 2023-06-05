// SDcard.h
// Includes for SD card interface.
// The tracklist will be written to a file on the SD card itself.
//
#if ESP_ARDUINO_VERSION_MAJOR < 2                     // File function "path()" not available in older versions
  #define path() name()                               // Use "name()" instead
#endif
#define MAXFNLEN    255                               // Max length of a full filespec
#define SD_MAXDEPTH 4                                 // Maximum depths.  Note: see mp3play_html.

struct mp3spec_t                                      // For List of mp3 file on SD card
{
  uint8_t entrylen ;                                  // Total length of this entry
  uint8_t prevlen ;                                   // Number of chars that are equal to previous
  char    filespec[MAXFNLEN] ;                        // Full file spec, the divergent end part
} ;


#ifndef SDCARD
  #define mount_SDCARD()         false                   // Dummy mount
  #define scan_SDCARD()                                  // Dummy scan files
  //#define check_SDCARD()                               // Dummy check
  #define close_SDCARD()                                 // Dummy close
  #define read_SDCARD(a,b)       0                       // Dummy read file buffer
  #define selectnextSDnode(b)    String("")
  #define getSDfilename(a)       String("")
  #define listsdtracks(a,b,c)    0
  #define connecttofile_SD()      false                  // Dummy connect to file
  #define initSDTask()                                   // Dummy start of task

#else
  #include <SPI.h>
  #include <SD.h>
  #include <FS.h>
  #define SDSPEED   2000000                             // SPI speed of SD card
  #define TRACKLIST "/tracklist.dat"                    // File with tracklist on SD card

  bool            SD_okay = false ;                     // SD is okay
  bool            SD_mounted = false ;                  // SD is mounted
  int             SD_filecount = 0 ;                    // Number of filenames in SD_nodelist
  int             SD_curindex ;                         // Current index in mp3names
  RTC_NOINIT_ATTR mp3spec_t   mp3entry ;                // Entry with track spec
  RTC_NOINIT_ATTR char        SD_lastmp3spec[512] ;     // Previous full file spec
  File            mp3file ;                             // File containing mp3 on SD card
  int             mp3filelength = 0 ;                   // Length of file
  bool            randomplay = false ;                  // Switch for random play
  File            trackfile ;                           // File for tracknames
  bool            trackfile_isopen = false ;            // True if trackfile is open for read
  
  // Forward declaration
  void setdatamode ( datamode_t newmode ) ;
  void SDtask      ( void * parameter ) ;

  const char*      STAG = "SDcard" ;


  //**************************************************************************************************
  //                               C L O S E T R A C K F I L E                                       *
  //**************************************************************************************************
  void closeTrackfile()
  {
    if ( trackfile_isopen )                             // Trackfile still open?
    {
      trackfile.close() ;
    }
    trackfile_isopen = false ;                          // Reset open status
  }


  //**************************************************************************************************
  //                                  O P E N T R A C K F I L E                                       *
  //**************************************************************************************************
  bool openTrackfile ( const char* mode = FILE_READ )
  {
    closeTrackfile() ;                                  // Close if already open
    trackfile = SD.open ( TRACKLIST, mode ) ;           // Open the tracklist file on SD card
    ESP_LOGI ( STAG, "OpenTrackfile(%s) result is %d",
               mode, (int)trackfile ) ;
    trackfile_isopen = ( trackfile != 0 ) ;             // Check result
    return trackfile_isopen ;                           // Return open status
  }


  //**************************************************************************************************
  //                               G E T F I R S T S D F I L E N A M E                               *
  //**************************************************************************************************
  // Get the first filespec from track file on SD card.                                              *
  //**************************************************************************************************
  const char* getFirstSDFileName()
  {
    if ( ! openTrackfile() )                            // Try to open track file
    {
      return NULL ;                                     // File not available
    }
    uint8_t* p = (uint8_t*)&mp3entry ;                  // Point to entrylength of mp3entry
    trackfile.read ( p, sizeof(mp3entry.entrylen) ) ;   // Get total size of entry
    p += sizeof(mp3entry.entrylen) ;                    // Bump pointer
    trackfile.read ( p, mp3entry.entrylen -             // Read rest of entry
                        sizeof(mp3entry.entrylen) ) ;
    strcpy ( SD_lastmp3spec, mp3entry.filespec ) ;      // Copy filename into SD_last
    SD_curindex = 0 ;                                   // Set current index to 0
    return SD_lastmp3spec ;                             // Return pointer to filename
  }


  //**************************************************************************************************
  //                                   S E T S D F I L E N A M E                                     *
  //**************************************************************************************************
  // Set the current filespec.                                                                       *
  //**************************************************************************************************
  void setSDFileName ( const char* fnam )
  {
    if ( strlen ( fnam ) < MAXFNLEN )                   // Guard against long filenames
    {
      strcpy ( SD_lastmp3spec,  fnam ) ;                // Copy filename into SD_last
    }
  }


  //**************************************************************************************************
  //                                   G E T S D F I L E N A M E                                     *
  //**************************************************************************************************
  // Get a filespec from tracklist file at index.                                                    *
  // If index is negative, a random track is selected.                                               *
  //**************************************************************************************************
  char* getSDFileName ( int inx )
  {
    if ( ! trackfile_isopen )                             // Track file available?
    {
      return NULL ;                                       // No, return empty name
    }
    if ( inx < 0 )                                        // Negative track number?
    {
      inx = (int) random ( SD_filecount ) ;               // Yes, pick random track
      randomplay = true ;                                 // Set random play flag
    }
    else
    {
      randomplay = false ;                                // Not random, reset flag
    }
    if ( inx >= SD_filecount )                            // Protect against going beyond last track
    {
      inx = 0 ;                                           // Beyond last track: rewind to begin
    }
    if ( inx < SD_curindex )                              // Going backwards?
    {
      getFirstSDFileName() ;                              // Yes, start all over
    }
    while ( SD_curindex < inx )                           // Move forward until at required position
    {
      uint8_t* pm = (uint8_t*)&mp3entry ;                 // Point to entrylen of mp3entry
      trackfile.read ( pm, sizeof(mp3entry.entrylen) ) ;  // Get total size of entry
      pm += sizeof(mp3entry.entrylen) ;                   // Bump pointer
      trackfile.read ( pm, mp3entry.entrylen -            // Read rest of entry
                         sizeof ( mp3entry.entrylen ) ) ;
      strcpy ( SD_lastmp3spec + mp3entry.prevlen,         // Copy filename into SD_last
               mp3entry.filespec ) ;
      SD_curindex++ ;                                     // Set current index
    }
    return SD_lastmp3spec ;                               // Return pointer to filename
  }


  //**************************************************************************************************
  //                               G E T N E X T S D F I L E N A M E                                 *
  //**************************************************************************************************
  // Get next filespec from mp3names.                                                                *
  //**************************************************************************************************
  char* getNextSDFileName()
  {
    int inx = SD_curindex + 1 ;                          // By default next track

    if ( randomplay )                                    // Playing random tracks?
    {
      inx = -1 ;                                         // Yes, next track will be random too
    }
    return ( getSDFileName ( inx ) ) ;                   // Select the track
  }


  //**************************************************************************************************
  //                           G E T C U R R E N T S D F I L E N A M E                               *
  //**************************************************************************************************
  // Get current filespec from mp3names.                                                             *
  //**************************************************************************************************
  char* getCurrentSDFileName()
  {
    return SD_lastmp3spec ;                             // Return pointer to filename
  }


  //**************************************************************************************************
  //                      G E T C U R R E N T S H O R T S D F I L E N A M E                          *
  //**************************************************************************************************
  // Get last part of current filespec from mp3names.                                                *
  //**************************************************************************************************
  char* getCurrentShortSDFileName()
  {
    return strrchr ( SD_lastmp3spec, '/' ) + 1 ;        // Last part of filespec
  }


  //**************************************************************************************************
  //                                  A D D T O F I L E L I S T                                      *
  //**************************************************************************************************
  // Add a filename to the trackfile on SD card.                                                     *
  // Example:                                                                                        *
  // Entry entry prev   filespec                             Interpreted result                      *
  //  num   len   len                   
  // ----- ----- ----   --------------------------------     ------------------------------------    *
  // [0]      32    0   /Fleetwood Mac/Albatross.mp3         /Fleetwood Mac/Albatross.mp3            *
  // [1]      15   15   Hold Me.mp3                          /Fleetwood Mac/Hold Me.mp3              *
  //**************************************************************************************************
  bool addToFileList ( const char* newfnam )
  {
    char*             lnam = SD_lastmp3spec ;           // Set pointer to compare
    uint8_t           n = 0 ;                           // Counter for number of equal characters
    uint16_t          l = strlen ( newfnam ) ;          // Length of new file name
    uint8_t           entrylen ;                        // Length of entry
    bool              res = false ;                     // Function result

    //ESP_LOGI ( STAG, "Full filename is %s", newfnam ) ;
    if ( l >= ( sizeof ( SD_lastmp3spec ) - 1 ) )       // Block very long filenames
    {
      ESP_LOGE ( STAG, "SD filename too long (%d)!", l ) ;
      return false ;                                    // Filename too long: skip
    }
    while ( *lnam == *newfnam )                         // Compare next character of filename
    {
      if ( *lnam == '\0' )                              // End of name?
      {
        break ;                                         // Yes, stop
      }
      n++ ;                                             // Equal: count
      lnam++ ;                                          // Update pointers
      newfnam++ ;
    }
    mp3entry.prevlen = n ;                              // This part is equal to previous name
    l -= n ;                                            // Length of rest of filename
    if ( l >= ( sizeof ( mp3spec_t ) - 5 ) )            // Block very long filenames
    {
      ESP_LOGE ( STAG, "SD filename too long (%d)!", l ) ;
      return false ;                                    // Filename too long: skip
    }
    strcpy ( mp3entry.filespec, newfnam ) ;             // This is last part of new filename
    entrylen = sizeof(mp3entry.entrylen) +              // Size of entry including string delimeter
               sizeof(mp3entry.prevlen) +
               strlen (newfnam) + 1 ;
    mp3entry.entrylen = entrylen ;
    strcpy ( lnam, newfnam ) ;                          // Set a new lastmp3spec
    ESP_LOGI ( STAG, "Added %3u : %s", n,               // Show last part of filename
               getCurrentShortSDFileName() ) ;
    if ( trackfile )                                    // Outputfile open?
    {
      res = true ;                                      // Yes, positive result
      trackfile.write ( (uint8_t*)&mp3entry,            // Yes, add to list
                         entrylen ) ; 
      SD_filecount++ ;                                  // Count number of files in list
    }
    return res ;                                        // Return result of adding name
  }


  //**************************************************************************************************
  //                                      G E T S D T R A C K S                                      *
  //**************************************************************************************************
  // Search all MP3 files on directory of SD card.                                                   *
  // Will be called recursively.                                                                     *
  //**************************************************************************************************
  bool getsdtracks ( const char * dirname, uint8_t levels )
  {
    File       root ;                                     // Work directory
    File       file ;                                     // File in work directory

    //ESP_LOGI ( STAG, "getsdt dir is %s", dirname ) ;
    root = SD.open ( dirname ) ;                          // Open directory
    if ( !root )                                          // Check on open
    {
      ESP_LOGI ( STAG, "Failed to open directory" ) ;
      return false ;
    }
    if ( !root.isDirectory() )                            // Really a directory?
    {
      ESP_LOGI ( STAG, "Not a directory" ) ;
      return false ;
    }
    file = root.openNextFile() ;
    while ( file )
    {
      vTaskDelay ( 50 / portTICK_PERIOD_MS ) ;            // Allow others
      if ( file.isDirectory() )                           // Is it a directory?
      {
        //ESP_LOGI ( STAG, "  DIR : %s", file.path() ) ;
        if ( levels )                                     // Dig in subdirectory?
        {
          if ( strrchr ( file.path(), '/' )[1] != '.' )   // Skip hidden directories
          {
            if ( ! getsdtracks ( file.path(),             // Non hidden directory: call recursive
                                  levels -1 ) )
            {
              return false ;                              // File I/O error
            }
          }
        }
      }
      else                                                // It is a file
      {
        const char* ext = file.name() ;                   // Point to begin of name
        ext = ext + strlen ( ext ) - 4 ;                  // Point to extension
        if ( ( strcmp ( ext, ".MP3" ) == 0 ) ||           // It is a file, but is it an MP3?
            ( strcmp ( ext, ".mp3" ) == 0 ) )
        {
          if ( ! addToFileList ( file.path() ) )          // Add file to the list
          {
            break ;                                       // No need to continue
          }
        }
      }
      file = root.openNextFile() ;
    }
    return true ;
  }


  //**************************************************************************************************
  //                                  H A N D L E _ I D 3 _ S D                                      *
  //**************************************************************************************************
  // Check file on SD card for ID3 tags and use them to display some info.                           *
  // Extended headers are not parsed.                                                                *
  //**************************************************************************************************
  void handle_ID3_SD ( String &path )
  {
    char*  p ;                                                // Pointer to filename
    struct ID3head_t                                          // First part of ID3 info
    {
      char    fid[3] ;                                        // Should be filled with "ID3"
      uint8_t majV, minV ;                                    // Major and minor version
      uint8_t hflags ;                                        // Headerflags
      uint8_t ttagsize[4] ;                                   // Total tag size
    } ID3head ;
    uint8_t  exthsiz[4] ;                                     // Extended header size
    uint32_t stx ;                                            // Ext header size converted
    uint32_t sttg ;                                           // Total tagsize converted
    uint32_t stg ;                                            // Size of a single tag
    struct ID3tag_t                                           // Tag in ID3 info
    {
      char    tagid[4] ;                                      // Things like "TCON", "TYER", ...
      uint8_t tagsize[4] ;                                    // Size of the tag
      uint8_t tagflags[2] ;                                   // Tag flags
    } ID3tag ;
    uint8_t   tmpbuf[4] ;                                     // Scratch buffer
    uint8_t   tenc ;                                          // Text encoding
    String    albttl = String() ;                              // Album and title
    bool      talb ;                                          // Tag is TALB (album title)
    bool      tpe1 ;                                          // Tag is TPE1 (artist)

    tftset ( 2, "Playing from local file" ) ;                 // Assume no ID3
    p = (char*)path.c_str() + 1 ;                             // Point to filename (after the slash)
    showstreamtitle ( p, true ) ;                             // Show the filename as title (middle part)
    mp3file = SD.open ( path ) ;                              // Open the file
    mp3file.read ( (uint8_t*)&ID3head, sizeof(ID3head) ) ;    // Read first part of ID3 info
    if ( strncmp ( ID3head.fid, "ID3", 3 ) == 0 )
    {
      sttg = ssconv ( ID3head.ttagsize ) ;                    // Convert tagsize
      ESP_LOGI ( STAG, "Found ID3 info" ) ;
      if ( ID3head.hflags & 0x40 )                            // Extended header?
      {
        stx = ssconv ( exthsiz ) ;                            // Yes, get size of extended header
        while ( stx-- )
        {
          mp3file.read () ;                                   // Skip next byte of extended header
        }
      }
      while ( sttg > 10 )                                     // Now handle the tags
      {
        sttg -= mp3file.read ( (uint8_t*)&ID3tag,
                              sizeof(ID3tag) ) ;              // Read first part of a tag
        if ( ID3tag.tagid[0] == 0 )                           // Reached the end of the list?
        {
          break ;                                             // Yes, quit the loop
        }
        stg = ssconv ( ID3tag.tagsize ) ;                     // Convert size of tag
        if ( ID3tag.tagflags[1] & 0x08 )                      // Compressed?
        {
          sttg -= mp3file.read ( tmpbuf, 4 ) ;                // Yes, ignore 4 bytes
          stg -= 4 ;                                          // Reduce tag size
        }
        if ( ID3tag.tagflags[1] & 0x044 )                     // Encrypted or grouped?
        {
          sttg -= mp3file.read ( tmpbuf, 1 ) ;                // Yes, ignore 1 byte
          stg-- ;                                             // Reduce tagsize by 1
        }
        if ( stg > ( sizeof(metalinebf) + 2 ) )               // Room for tag?
        {
          break ;                                             // No, skip this and further tags
        }
        sttg -= mp3file.read ( (uint8_t*)metalinebf,
                               stg ) ;                        // Read tag contents
        metalinebf[stg] = '\0' ;                              // Add delimeter
        tenc = metalinebf[0] ;                                // First byte is encoding type
        if ( tenc == '\0' )                                   // Debug all tags with encoding 0
        {
          ESP_LOGI ( STAG, "ID3 %s = %s", ID3tag.tagid,
                    metalinebf + 1 ) ;
        }
        talb = ( strncmp ( ID3tag.tagid, "TALB", 4 ) == 0 ) ; // Album title
        tpe1 = ( strncmp ( ID3tag.tagid, "TPE1", 4 ) == 0 ) ; // Artist?
        if ( talb || tpe1 )                                   // Album title or artist?
        {
          albttl += String ( metalinebf + 1 ) ;               // Yes, add to string
          #ifdef T_NEXTION                                    // NEXTION display?
            albttl += String ( "\\r" ) ;                      // Add code for newline (2 characters)
          #else
            albttl += String ( "\n" ) ;                       // Add newline (1 character)
          #endif
          if ( tpe1 )                                         // Artist tag?
          {
            icyname = String ( metalinebf + 1 ) ;             // Yes, save for status in webinterface
          }
        }
        if ( strncmp ( ID3tag.tagid, "TIT2", 4 ) == 0 )       // Songtitle?
        {
          tftset ( 2, metalinebf + 1 ) ;                      // Yes, show title
          icystreamtitle = String ( metalinebf + 1 ) ;        // For status in webinterface
        }
      }
      tftset ( 1, albttl ) ;                                  // Show album and title
    }
    //mp3file.seek ( 0 ) ;                                      // Back to begin of file
  }


  //**************************************************************************************************
  //                                  C O N N E C T T O F I L E _ S D                                *
  //**************************************************************************************************
  // Open the local mp3-file.                                                                        *
  //**************************************************************************************************
  bool connecttofile_SD()
  {
    String path ;                                           // Full file spec

    stop_mp3client() ;                                      // Disconnect if still connected
    tftset ( 0, "MP3 Player" ) ;                            // Set screen segment top line
    displaytime ( "" ) ;                                    // Clear time on TFT screen
    setdatamode ( DATA ) ;                                  // Start in datamode 
    path = String ( getCurrentSDFileName() ) ;              // Set path to file to play
    icystreamtitle = path ;                                 // If no ID3 available
    icyname = String ( "" ) ;                               // If no ID3 available
    handle_ID3_SD ( path ) ;                                // See if there are ID3 tags in this file
    if ( !mp3file )
    {
      ESP_LOGI ( STAG, "Error opening file %s",             // No luck
                 path.c_str() ) ;
      return false ;
    }
    mp3filelength = mp3file.available() ;                   // Get length
    mqttpub.trigger ( MQTT_STREAMTITLE ) ;                  // Request publishing to MQTT
    chunked = false ;                                       // File not chunked
    metaint = 0 ;                                           // No metadata
    return true ;
  }


  //**************************************************************************************************
  //                                       M O U N T _ S D C A R D                                   *
  //**************************************************************************************************
  // Initialize the SD card.                                                                         *
  //**************************************************************************************************
  bool mount_SDCARD ( int8_t csPin )
  {
    bool       okay = false ;                              // True if SD card in place and readable

    if ( csPin >= 0 )                                      // SD configured?
    {
      SD_mounted = SD.begin ( csPin, SPI,                  // Yes, try to init SD card driver
                              SDSPEED, "/sd", 3 ) ;
      if ( !SD_mounted )                                   // Init (mount) okay?
      {
        //ESP_LOGE ( STAG, "SD Card Mount Failed!" ) ;     // No success, check formatting (FAT)
      }
      else
      {
        okay = ( SD.cardType() != CARD_NONE ) ;            // See if known card
      }
    }
    if ( !okay )
    {
      ESP_LOGI ( STAG, "No SD card attached" ) ;           // Card not readable
    }
    return okay ;
  }


  //**************************************************************************************************
  //                                       C L O S E _ S D C A R D                                   *
  //**************************************************************************************************
  // Close file on SD card.                                                                          *
  //**************************************************************************************************
  void close_SDCARD()
  {
    ESP_LOGI ( STAG, "Close SD file" ) ;
    mp3file.close() ;                                     // Close the file
  }


  //**************************************************************************************************
  //                                   S D I N S E R T C H E C K                                     *
  //**************************************************************************************************
  // Check if new SD card is inserted and can be read.                                               *
  //**************************************************************************************************
  bool SDInsertCheck()
  {
    static uint32_t nextCheckTime = 0 ;                     // To prevent checking too often
    uint32_t        newmillis ;                             // Current timestamp
    bool            sdinsNew ;                              // Result of insert check
    void*           p ;                                     // Pointer to item from ringbuffer
    size_t          f0 ;                                    // Length of item from ringbuffer
    static bool     sdInserted = false ;                    // Yes, flag for inserted SD
    int8_t          dpin = ini_block.sd_detect_pin ;        // SD inserted detect pin

    if ( ( newmillis = millis() ) < nextCheckTime )         // Time to check?
    {
      return false ;                                        // No, return "no new insert"
    }
    nextCheckTime = newmillis + 5000 ;                      // Yes, set new check time
    if ( dpin >= 0 )                                        // Hardware detection possible?
    {
      sdinsNew = ( digitalRead ( dpin ) == LOW ) ;          // Yes, see if card inserted
      if ( sdinsNew == sdInserted )                         // Situation changed?
      {
        return false ;                                      // No, return "no new insert"
      }
      else
      {
        sdInserted = sdinsNew ;                             // Remember status
        if ( ! sdInserted )                                 // Card out?
        {
          ESP_LOGI ( STAG, "SD card removed" ) ;
          if ( SD_mounted )                                 // Still mounted?
          {
            SD.end() ;                                      // Unmount SD card
            SD_okay = false ;                               // Not okay anymore
            SD_mounted = false ;                            // And not mounted anymore
          }
        }
        else                                                // Card inserted
        {
          ESP_LOGI ( STAG, "SD card inserted" ) ;
          SD_okay = mount_SDCARD ( ini_block.sd_cs_pin ) ;  // Try to mount
        }
      }
    }
    else                                                    // Handle SD without detect pin
    {
      ESP_LOGI ( STAG, "Try to mount SD card" ) ;
      SD_okay = mount_SDCARD ( ini_block.sd_cs_pin ) ;      // Try to mount
    }
    return SD_okay ;                                        // Return result
  }


  //**************************************************************************************************
  //                                    C O U N T F I L E S                                          *
  //**************************************************************************************************
  // Count number of tracks in the track list.                                                       *
  //**************************************************************************************************
  int countfiles()
  {
    int count = 0 ;                                         // Files found
  
    while ( trackfile.available() > 1 )
    {
      uint8_t* pm = (uint8_t*)&mp3entry ;                   // Point to entrylength of mp3entry
      trackfile.read ( pm, sizeof(mp3entry.entrylen) ) ;    // Get total size of entry
      pm += sizeof(mp3entry.entrylen) ;                     // Bump pointer
      trackfile.read ( pm, mp3entry.entrylen -              // Read rest of entry
                        sizeof ( mp3entry.entrylen ) ) ;
      count++ ;                                             // count entries
      vTaskDelay ( 10 / portTICK_PERIOD_MS  ) ;             // Allow other tasks
    }
    ESP_LOGI ( STAG, "%d files on SD card", count ) ;
    return count ;                                          // Return number of tracks
  }


  //**************************************************************************************************
  //                                       S D T A S K                                               *
  //**************************************************************************************************
  // This task will constantly try to fill the ringbuffer with filenames on SD.                      *
  // if the SD detect pin is defined, a test will on SD change will be performed every 5 seconds.    *
  // Otherwise, the check is made only once, after reset.                                            *
  //**************************************************************************************************
  void SDtask ( void * parameter )
  {
    const char* ffn ;                                     // First filename on SD card
    int8_t      dpin = ini_block.sd_detect_pin ;          // SD inserted detect pin
    bool        once = true ;                             // Always check once
  
    vTaskDelay ( 1000 / portTICK_PERIOD_MS ) ;            // Start delay
    while ( true )                                        // Endless task
    {
      vTaskDelay ( 200 / portTICK_PERIOD_MS ) ;           // Allow other tasks
      if ( ( dpin < 0 ) && ( once == false ) )            // Just one check if no detect pin
      {
        continue ;
      }
      once = false ;                                      // Stop detect without detect pin
      if ( SDInsertCheck() )                              // See if new card is inserted
      {
        SD_lastmp3spec[0] = '\0' ;                        // No last track
        if ( ( openTrackfile() ) &&                       // Try to open trackfile
             ( trackfile.size() > 0 ) )                   // Tracklist on this SD card?
        {
          ESP_LOGI ( STAG, "Track list is on SD card, "   // Yes, show it
                     "read tracks" ) ;
          SD_filecount = countfiles() ;                   // Count number of files on this card
          closeTrackfile() ;                              // Close the tracklist file
          ESP_LOGI ( STAG, "%d tracks on SD card",        // Yes, show it
                     SD_filecount ) ;
        }
        else
        {
          ESP_LOGI ( STAG, "Locate mp3 files on SD, "
                     "may take a while..." ) ;
          if ( openTrackfile ( FILE_WRITE ) )             // Try to open trackfile for write
          {
            SD_okay = getsdtracks ( "/", SD_MAXDEPTH ) ;  // Get filenames, store on the SD card
            closeTrackfile() ;                            // Close the tracklist file
          }
        }
        ffn = getFirstSDFileName() ;
        if ( ffn )
        {
          ESP_LOGI ( STAG, "First file on SD card is %s", // Show the first track name
                     ffn ) ;
        }
      }
    }
  }
#endif
