# ESP32Radio-V2
New version of the well known ESP32 Radio.  Now optional I2S output!
- Compile time configuration in config.h.
- Do not forget to upload the data directory to the ESP32.
- SD cards supported, but still experimental.

Updates:
- 05-may-2023: SD card file stored on SD card, mute/unmute, better mutex.
- 24-mar-2023: Code clean-up
- 03-nov-2022: Added AI Thinker Audio kit V2.1 suport.
- 05-oct-2021: Fixed internal DAC output, fixed OTA upload.
- 06-oct-2021: Fixed AP mode.
- 26-mar-2022: Fixed NEXTION bug.
- 12-apr-2022: Fixed queue bug (NEXT function).
- 13-apr-2022: Fixed redirect bug (preset was reset), fixed playlist bug.
- 14-apr-2022: Add FIXEDWIFI in config.h to simplify WiFi set-up.
- 15-apr-2022: Redesigned station selection.
- 25-apr-2022: Add support for WT32-ETH0 (wired Ethernet).
- 04-may-2022: OLED with Wire library, should work with Heltec-WIfi board.
- 16-may-2023: Added sleep command.
