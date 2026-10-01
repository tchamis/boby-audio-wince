# Boby Audio for VW Maps & More

Windows CE/ARM helper for the original VW/Garmin Maps & More system.

## Verified AppCom behavior

The helper must be stored and launched from:

`\\My Flash Disk\\VwUserShell\\BobyAudio.exe`

This is required because AppCom only sees the stock application registry (`NgAppCom.xml`) correctly in the VwUserShell context.

Version 0.3:

1. waits for the original VW shell
2. loads the stock `ACAppCom.dll`
3. waits for the stock Bluetooth application
4. calls AppCom `StartApplication("MMPMediaPlayer", "", true)`
5. retries until the stock media player is running/foreground
6. does not emulate touch and does not send a fake Play command

The existing `autorunce.mscr` should launch:

`Run("\\My Flash Disk\\VwUserShell\\BobyAudio.exe")`

Diagnostics are written to:

`\\My Flash Disk\\boby_audio.log`

Standby/resume handling is the next step after the cold-start path is confirmed.
