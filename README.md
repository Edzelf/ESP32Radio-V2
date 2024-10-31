# ESP32Radio-V2
New version of the well known ESP32 Radio.  Now optional I2S output and SP/DIF output!
- Compile time configuration in config.h.
- Do not forget to upload the data directory to the ESP32.
- SD cards supported, but still experimental.

![SAM_2922](https://github.com/Edzelf/ESP32Radio-V2/assets/18257026/a58f7b7e-cdd4-4d7f-96b8-62fa487de906)

Updates:
- 17-oct-2024: Experimental support for ESP32-S3. SP/DIF tested.
- 30-sep-2024: Enhancements to tiny OLEDs, OTA beautified (Trip5 Update).
- 05-jul-2024: Correction SPDIF output.
- 12-mar-2024: Add document with building instructions.  New PCB available! See [here at PCBWay](https://www.pcbway.com/project/shareproject/W652317AS2P1_Gerber_ESP32_Radio_PCB_ESP32_Radio_fab8a6d9.html).
- 19-feb-2024: SP/DIF (Toslink) output, fixed mono bug.
- 26-dec-2023: Correct crash with VS1053 and empty preferences.
- 14-dec-2023: Add "mqttrefresh" command to refresh all mqtt items.
- 09-oct-2023: Reduced error messages caused by uninitialized GPIO numbers.
- 16-may-2023: Added sleep command.
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
