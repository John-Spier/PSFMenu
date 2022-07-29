# PSFMenu
A menu application for PlayStation that loads both music files and EXE files. Based on LameGuy64's MeidoMenu.

[PSF2CSV](https://github.com/John-Spier/Psf2Csv) is recommended to create VFS files, which save space in the CD filesystem.

## Music controls

X = Select/play file

○ = Play/pause track

L1 = Previous track

R1 = Next track

L2 = Volume down

R2 = Volume up

Select = Stop music

Start = Return to main menu

## Menu creation

Stack addresses with specific values are used to load music file formats.

FFFFFFXX is the base "address", and XX is the type of file.

00 = Stop playing music.

01 = HITMOD mod file converted with modconv.

03 = SEQ, VH, VB files packed to [QLP](https://github.com/John-Spier/QLPTool) format.

04 = SEP, VH, VB, TRACKNUM files packed to [QLP](https://github.com/John-Spier/QLPTool) format.

FE = [VFS](https://github.com/John-Spier/VFSTool) submenu.

FF = TXT submenu.

01000000 - 0100FFFF is a packed SEP file, automatically selecting the track based on the stack location.
