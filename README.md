# Boby Audio for VW Maps & More

Small Windows CE/ARM helper for the original VW/Garmin Maps & More system.

The helper does **not** launch MediaPlayer.exe directly and does **not** emulate touch input.
It loads the original `ACAppCom.dll` and asks AppCom to start the registered
`MMPMediaPlayer` application through the same application-management layer used by
the VW shell.

## Test version 0.1

This first build intentionally does only one thing:

1. wait for `VwUserShell`
2. wait a little longer for Bluetooth/A2DP reconnect
3. call AppCom `StartApplication("MMPMediaPlayer", "", true)`
4. verify the foreground application through AppCom
5. write diagnostics to `\\My Flash Disk\\boby_audio.log`

Standby/resume handling will only be added after this launch path is verified on the device.

## Device install for the test

Copy these two files from the GitHub Actions artifact to the root of the Garmin:

- `BobyAudio.exe`
- `boby_startup.mscr`

The existing `autorunce.mscr` already launches `boby_startup.mscr`.
