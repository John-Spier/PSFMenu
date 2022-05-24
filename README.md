# MeidoModMenu
A menu application for PlayStation that loads both music files and EXE files. Based on LameGuy64's MeidoMenu.

Music controls:
□ = Pause song
△ = Play paused song

Stack addresses with specific values are used to load music file formats.
FFFFFFXX is the base "address", and XX is the type of file.
00 = Stop playing music.
01 = HITMOD mod file converted with modconv.
