# PSFMenu
A menu application for PlayStation that loads both music files and EXE files. Based on LameGuy64's MeidoMenu.

[PSF2CSV](https://github.com/John-Spier/Psf2Csv) is recommended to create VFS files, which save space in the CD filesystem.

## Music controls

### No buttons held

Up = Cursor up

Down = Cursor down

Left = Cursor up 10

Right = Cursor down 10

⨯ = Select/play file

○ = Play/pause track

L1 = Previous track

R1 = Next track

L2 = Volume down

R2 = Volume up

Select = Stop music

Start = Return to main menu

### While holding □

Up = Reverb delay up

Down = Reverb delay down

Left = Reverb feedback down

Right = Reverb feedback up

⨯ = Select/play file (without loading parameters)

○ = Change reverb type

L1 = Previous track

R1 = Next track

L2 = Reverb volume down

R2 = Reverb volume up

Select = Set audio to mono

Start = Set audio to stereo

### While holding △

Up = Reverb buffer timeout frames up

Down = Reverb buffer timeout frames down

Left = Reverb buffer timeout frames up 10

Right = Reverb buffer timeout frames down 10

⨯ = Select file parameters

○ = Reset parameters

L1 = Previous track (load parameters)

R1 = Next track (load parameters)

L2 = Track loops down

R2 = Track loops up

Select = Reverb off

Start = Reverb on

## Menu creation

Stack addresses with specific values are used to load music file formats.

FFFFFFXX is the base "address", and XX is the type of file.

00 = Stop playing music.

01 = HITMOD mod file converted with modconv.

03 = SEQ, (SEQ, SEQ, SEQ,...) VH, VB, (PARAM, PARAM, PARAM,...) files packed to [QLP](https://github.com/John-Spier/QLPTool) format.

04 = SEP, VH, VB, TRACKNUM files packed to [QLP](https://github.com/John-Spier/QLPTool) format.

FE = [VFS](https://github.com/John-Spier/VFSTool) submenu.

FF = TXT submenu.

01000000 - 0100FFFF is a packed SEP file, automatically selecting the track based on the stack location.

01010000 - 0101FFFF is a packed SEQ file, automatically selecting the track based on the stack location.
