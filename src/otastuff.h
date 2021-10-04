// otastuff.h
//
#include <AsyncTCP.h>

enum updatetype_e { UIDLE, UBIN, UTFT } ;              // Filetype of update

AsyncClient* otaclient ;                               // Client for OTA update
bool         ota_host_connected = false ;              // True if host connected
bool         ota_host_error = false ;                  // True if error
int          currentlen ;
bool         headermode ;                              // True if receiving header
String       lstmod ;                                  // Last modified timestamp in NVS
String       newlstmod ;                               // Last modified from host
updatetype_e updatetype ;                              // Update file type

//**************************************************************************************************
//                                    C B _ O T A C O N N E C T                                    *
//**************************************************************************************************
// Event callback on connect.                                                                      *
//**************************************************************************************************
void cb_otaConnect ( void* arg, AsyncClient* client )
{
  dbgprint ( "Connected to update server" ) ;
  currentlen = 0 ;
  headermode = true ;
  ota_host_connected = true ;
}


//**************************************************************************************************
//                               C B _ O T A D I S C O N N E C T                                   *
//**************************************************************************************************
// Event callback on disconnect                                                                    *
//**************************************************************************************************
void cb_otaDisconnect ( void* arg, AsyncClient* client )
{
  dbgprint ( "Disconnected from update server" ) ;
  dbgprint ( "Total data is %d bytes", currentlen ) ;
  ota_host_connected = false ;
}




//**************************************************************************************************
//                                     C B _ O T A E R R O R                                       *
//**************************************************************************************************
// Event callback on error.                                                                        *
//**************************************************************************************************
void cb_otaError ( void* arg, AsyncClient* client, int error )
{
  dbgprint ( "TCP connect to update server error!" ) ;
  ota_host_error = true ;
  dbgprint ( "Total data is %d bytes", currentlen ) ;
  ota_host_connected = false ;
}


//**************************************************************************************************
//                                     C B _ O T A D A T A                                         *
//**************************************************************************************************
// Event callback on received data from host.                                                      *
// Assumption: all header lines are in the first chunk.                                            *
// Header looks like this:                                                                         *
//  HTTP/1.1 200 OK                                                                                *
//  Date: Tue, 10 Aug 2021 12:57:57 GMT                                                            *
//  Server: Apache/2                                                                               *
//  Last-Modified: Sat, 07 Aug 2021 18:42:17 GMT                                                   *
//  ETag: "11db00-5c8fc82e65440"                                                                   *
//  Accept-Ranges: bytes                                                                           *
//  Content-Length: 1170176                                                                        *
//  Vary: Accept-Encoding,User-Agent                                                               *
//  Keep-Alive: timeout=2, max=100                                                                 *
//  Connection: Keep-Alive                                                                         *
//  Content-Type: application/octet-stream                                                         *
//**************************************************************************************************
void cb_otaData ( void* arg, AsyncClient* client, void *data, size_t len )
{
  char*       pdata = (char*)data ;                   // Point to begin of block of new data

  if ( headermode )                                   // In header mode?
  {
    char*       p ;                                   // Will point in header line
    size_t      hdrlen ;                              // Header length
    if ( ! strstr ( pdata, "200 OK" ) )               // Scan for HTML OK (file found)
    {
      dbgprint ( "Got a non 200 status code from server!" ) ;
      otaclient->close() ;                            // Stop stream
      return ;
    }
    p = strstr ( pdata, "\r\n\r\n" ) + 4 ;
    hdrlen = p - pdata ;                              // Compute headerlength
    len -= hdrlen ;                                   // Adjust length of data read
    data = (void*)(pdata + hdrlen) ;                  // Adjust pointer to data
    p = strstr ( pdata, "Content-Length: " ) + 16 ;   // Postion of file length
    clength = atoi ( p ) ;                            // Get length
    p = strstr ( pdata, "Last-Modified: " ) + 15 ;    // Position of last modified string
    *(p + 29) = '\0' ;                                // Delimit timestamp string
    newlstmod = String ( p ) ;                        // Get timestamp
    if ( newlstmod == lstmod )                        // Need for update?
    {
      dbgprint ( "No new version available" ) ;       // No, show reason
      otaclient->close() ;
      return ;    
    }
    if ( updatetype == UBIN )                         // Is it a sketch update?
    {
      if ( Update.begin ( clength ) )                 // Update possible?
      {
        dbgprint ( "Begin OTA update, length is %ld",
                   clength ) ;
      }
      else
      {
        dbgprint ( "OTA sketch size error" ) ;
        updatetype = UIDLE ;
      }
    }
    headermode = false ;                              // End of header
  }
  // We are receiving pure data for update
  if ( updatetype == UBIN )                           // Working for the sketch?
  {
    Update.write ( (uint8_t*)data, len ) ;            // Yes, write to flash
    currentlen += len ;                               // Update current length
    if ( currentlen == clength )                      // End of sketch?
    {
      if ( Update.end() )                             // Check for successful flash
      {
        dbgprint( "OTA done" ) ;
        if ( Update.isFinished() )
        {
          dbgprint ( "Update successfully completed" ) ;
        }
        else
        {
          dbgprint ( "Update not finished!" ) ;
        }
      }
      else
      {
        dbgprint ( "Error Occurred. Error %s", Update.getError() ) ;
      }
    }
  }
  //dbgprint ( "ota data len is %d total %d", len, lseen ) ;
}


//**************************************************************************************************
//                                     C B _ O T A T I M E O U T                                   *
//**************************************************************************************************
// Event callback on time-out.                                                                     *
//**************************************************************************************************
void cb_otaTimeout ( void* arg, AsyncClient* client, uint32_t time )
{
  dbgprint ( "Time-out on update server, t is %d!", time ) ;
  dbgprint ( "Total data is %d bytes", currentlen ) ;
  ota_host_connected = false ;
}


//**************************************************************************************************
//                                  O T A _ C L I E N T _ C O N N E C T                            *
//**************************************************************************************************
// Connect to host running WP-port.                                                                *
//**************************************************************************************************
bool ota_client_connect ( const char* updatehost )
{
  if ( ota_host_connected )                              // Still connected?
  {
    return true ;                                        // Yes, quick return
  }
  ota_host_error = false ;                               // Assume no error
  otaclient->connect ( updatehost, 80 ) ;                // Connect to host on port 80
  for ( int i = 0 ; i < 200 ; i++ )                      // Wait for 200 loops, 20 seconds
  {
    if ( ota_host_connected )                            // Connected?
    {
      return true ;                                      // Yes, positive exit
    }
    if ( ota_host_error )                                // Error encountered?
    {
      break ;                                            // Yes, end loop
    }
    vTaskDelay ( 100 / portTICK_PERIOD_MS ) ;            // Pause until connected or error
  }
  return false ;                                         // Time-out or error
}


void setupota()
{
  otaclient = new AsyncClient ;                           // Create object for OTA update
  otaclient->onError      ( &cb_otaError, NULL ) ;        // Set callback on received data
  otaclient->onData       ( &cb_otaData, NULL ) ;         // Set callback on received data
  otaclient->onConnect    ( &cb_otaConnect, NULL ) ;      // Set callback on connect
  otaclient->onDisconnect ( &cb_otaDisconnect, NULL ) ;   // Set callback on disconnect
  otaclient->onTimeout    ( &cb_otaTimeout, NULL ) ;      // Set callback on time-out
}