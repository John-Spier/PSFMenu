# MeidoModMenu
A menu application for PlayStation that loads both music files and EXE files. Based on LameGuy64's MeidoMenu.

Music controls:
○ = Play/pause track
L1 = Previous track
R1 = Next track

Stack addresses with specific values are used to load music file formats.
FFFFFFXX is the base "address", and XX is the type of file.
00 = Stop playing music.
01 = HITMOD mod file converted with modconv.

03 = SEQ, VH, VB files packed to QLP format.
04 = SEP, VH, VB, TRACKNUM files packed to QLP format.