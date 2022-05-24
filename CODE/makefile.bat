cd /d %~dp0
ccpsx -O3 -Xo$801B2000 mmenu.c hitmod.obj -ommenu.cpe
cpe2x /CA mmenu.cpe
del mmenu.cpe
del mmenu.exe
