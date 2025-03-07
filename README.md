# Tiny Screenshoter
![icon](res/ts-tray32.png)

A small Windows 9x compatible screenshot tool that runs in background and saves PNG screenshots at the selected directory by pressing PrScr keyboard button. It was made by me to simplify making screenshots while making various experiments or playing games on various retro-computers that running old Windows like XP or even Windows 98.

## How to use it
- Unpack an executable at any directory (suggested to make a new one, as the config file will be made at the same directory as the executable).
- Run the executable.
- Find a new icon at the system tray, and double-click it, or click by right mouse button and select "Settings" item.
- Choice the directory where to save screenshots (otherwise, by default they will be saved at the same directory as the executable file).
- Optionally, set up FTP credentials.
- Try to make screenshot by pressing button, and see the result. If FTP credentials incorrect, please adjust them and try again.
- Once you done with setup, feel free to close the window, and just do what you are going to do, and press PrScr button to let program save the screen shot at the moment.
- The "Ding" system sound will be played when screenshot has been captured, and the "Exclamation" system sound will be played after the screenshot had been written to the disk.
- You can add the shortcut into Start / Programs / Autorun menu if you want make the program to start automatically.
- If you want to install update, please close the already-running program by opening the context menu on system tray icon and selecting "Quit" action. Otherwise, you will be unable to replace executable while it running (it's a general limitation of the Windows systems).

## There are two variants of program available
- Qt version (Built with Qt 4.4.3, the last supported by Windows 9x), it also can be built with newer Qt 5 and can be built for Linux or macOS. It also supports the FTP upload of done screenshots (primarily to quickly send them to my main PC and share them somewhere also).
- The experimental pure-WinAPI version that supposed to have the tiny filesize, take few amount of RAM, and start very quickly even on very old PCs like Pentium MMX 133 Mhz and older. It's planned to implement the same functionality that Qt version does including FTP uploads.
