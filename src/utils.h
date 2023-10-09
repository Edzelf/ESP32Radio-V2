// utils.h
// Some useful utilities.
//
#ifndef UTILS_H
#define UTILS_H
#include <esp_wifi_types.h>

char        utf8ascii ( char ascii ) ;                             // Convert UTF to Ascii
void        utf8ascii_ip ( char* s ) ;                             // Convert UTF to Ascii in place
String      utf8ascii ( const char* s ) ;                          // Convert UTF to Ascii as String
const char* getEncryptionType ( wifi_auth_mode_t thisType ) ;      // Get encryption type voor WiFi networks
String      getContentType ( String filename ) ;
bool        pin_exists ( uint8_t pin ) ;                           // Check GPIO pin number

#endif
