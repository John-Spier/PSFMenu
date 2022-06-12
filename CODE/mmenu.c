/*	MeidoMenu v1.2 source code by Lameguy64
	(?) 2014 Meido-Tek Productions
	
	New in v1.2:
		- Fixed a MAJOR bug where the menu fails to execute any EXE (LoadExec() sucks).
		- You can now load EXEs of up to 1672KB in size!
		- Speed-up load times through clever background loading.
		- Added file not found notification.
		- Clears the VRAM once the EXE has been loaded.
		
	Usage Rules:
		- You can copy some bits of code and use it for your own menu as long as you put
			my namesomewhere in your menu. (ex. Based from Lameguy64's MeidoMenu)
	
	How this menu works:
		
		The menu loads itself high above the base memory area in address 0x801B2000 while
		other data (such as MOD music) reside temporarily in the base area. This is so that
		EXEs can be loaded safely in the base area and once the loaded EXE has taken over
		the system, it can safely overwrite the menu through memory allocation.
		
		Because LoadExec() can't seem to load any EXE on a real PlayStation, I made my own
		routine that does pretty much the same exact thing except that it doesn't need that
		useless _96_init() function and is much more stable.
		
		There is a possibility for a homebrew program to 'return' to this menu by loading it
		again and executing it. However, any part of the homebrew program's code must not
		reside in the same area where the menu is loaded otherwise, the load may fail.
	
*/

#include <sys/types.h>
#include <stdio.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include <libetc.h>
#include <libcd.h>
#include <libapi.h>
#include <kernel.h>
#include <rand.h>
#include <libsnd.h>


#include "hitmod.h"

#define true		1
#define false		0

#define CENTERED	0x7FFF


// Toggles debug mode
#define DEBUG	true

// Stuff you can change to suit your needs
#define MAX_TITLES			32
#define MOD_AREA			0x80010000
#define TEMP_AREA			0x80090000	// Just 512KB above MOD_AREA
#define QLP_MAXSIZE			1024*128
#define LISTFILE_MAXSIZE	1024*8

// Cosmetic stuff
#define MAX_BUBBLES	64

#define CLR_RED		0
#define CLR_GRN		68
#define CLR_BLU		170


// Some defines that don't need much changing
#define OT_LENGTH	9		// 512 sprites should be enough
#define PACKETMAX	2048
#define PACKETMAX2	PACKETMAX*24

//files playable
#define MUSIC_NONE 0xFFFFFF00
#define MUSIC_MOD 0xFFFFFF01

#define MUSIC_SEQ 0xFFFFFF03
#define MUSIC_SEP 0xFFFFFF04


//#define MUSIC_AUTOSEP 0xFFFFFF05
//#define MUSIC_SEPTRK 0xFFFFFF06
/*
SEP tracks will use a specific part of the address space.
0x01000000 to 0x0100FFFF will be the memory range,
if problems are found with the range of the first 64K of memory.
This should cause no problems since the SEP track is a short,
the first 64K of RAM is reserved for the BIOS,
and more than 65,536 tracks will not be needed.
Any additional tracks can be stored on CD.
LastSelectedMultitrackItem and strncmp will be used to check for reloads.

The VFS system will have two fields: sectorStart and sectorSize.
Files will be stored directly in the VFS file without header.
A modified titles.txt with those fields will be used for VFS files.
This titles.txt can itself be used as the first file of a VFS file.
sectorSize will be the file size in sectors.
*/
//#define MENU_TRACK 0xFFFFFFFE

#define MENU_VFSBYTE 0xFFFFFFFD //uses bytes instead of sectors in the vfs. possible but probably useless
#define MENU_VFS 0xFFFFFFFE
#define MENU_TXT 0xFFFFFFFF

// Ordering tables and packet buffers for the graphics system
GsOT 		myOT[2];
GsOT_TAG 	myOT_TAG[2][1<<OT_LENGTH];
PACKET		GPUPacketArea[2][PACKETMAX2];

// Screen stats
int			ActiveBuffer=0;
int			ScreenXres=0;
int			ScreenYres=0;
int			ScreenCenterX=0;
int			ScreenCenterY=0;
int			ClearRed=CLR_RED;
int			ClearGrn=CLR_GRN;
int			ClearBlu=CLR_BLU;
int			TransCount=0;


// TIM images
GsIMAGE	BannerTIM		={0};
GsIMAGE	FontTIM			={0};
GsIMAGE	BigCircleTIM	={0};
GsIMAGE SmallCircleTIM	={0};


// Character width table
u_char CharWidth[] = {
	14	,6	,12	,16	,12	,14	,14	,8	,10	,10	,14	,12	,8	,14	,6	,14	,
	14	,6	,12	,13	,14	,14	,14	,14	,14	,14	,6	,8	,12	,14	,12	,14	,
	14	,14	,14	,13	,14	,14	,14	,14	,14	,14	,14	,14	,12	,15	,14	,14	,
	14	,14	,14	,13	,14	,15	,15	,16	,16	,16	,16	,10	,14	,10	,14	,14	,
	8	,14	,13	,11	,13	,12	,13	,12	,13	,6	,14	,14	,8	,16	,14	,14	,
	14	,14	,13	,12	,12	,14	,14	,14	,13	,13	,14	,10	,6	,10	,16	,16
};


// Struct to store title names and executable paths
typedef struct {
	char	Name[64];
	char	ExecFile[32];
	u_long	StackAddr;
} TITLESTRUCT;

TITLESTRUCT Title[MAX_TITLES]={0};

int		NumTitles=0;
int 	SelTitle=0;
char	StringBuff[32]={0};

int		LastSelectedMultitrackItem=-1;


short seq1;  /* SEQ data id */
short sep1;  /* SEP data id */
short vab1;  /* VAB data id */
short septrk;
short curtrk;

//char seq_table[SS_SEQ_TABSIZ * 4 * 5];
char seq_table[SS_SEQ_TABSIZ * 10 * 10];


// For launching an EXE
struct EXEC ExeParams;


// Function prototypes
int main();
void DoMenu();
int LoadEXEfile(char *FileName, struct EXEC *params);

void fPrint(char *string, short x, short y, char opacity, GsOT *otptr, GsIMAGE font);
void SortBigImage (int x, int y, GsIMAGE TimImage);

void Init();
void LoadGraphics();
void InitTitles();

float frand();
int hex2int(char *string);

int DoTransition();
int DoTransition2();

void PrepDisplay();
void Display();
void DisplayNoClear();

u_long StartMusic (TITLESTRUCT file, u_long currenttype);
int StopMusic (u_long filetype);
int UnloadMusic (u_long filetype);
int LoadMusic (u_long filetype);
int ChangeMusic (TITLESTRUCT file);

int PlayMusic (u_long filetype);
int PauseMusic (u_long filetype);

short ChangeTrack (short nowtrack, u_long filetype);

// Include my custom little libraries
#include "timlib.c"
#include "qlplib.c"


int main() {
	
	RECT ClearRect={640, 0, 385, 513};
	
	// Init everything
	Init();
	DoMenu();	
	

	// Set stack
	ExeParams.s_addr = Title[SelTitle].StackAddr;
	ExeParams.s_size = 0;
	
	// Clear the entire framebuffer to avoid possible graphical glitches
	ClearImage2(&ClearRect, 0, 0, 0);
	DrawSync(0);
	
	#if DEBUG
	printf("Stack set to: %x\n", ExeParams.s_addr);
	printf("Execute!\n");
	#endif
	
	// Terminate a bunch of things then execute the loaded EXE
	SpuQuit();
	ResetGraph(3);
	PadStop();
	StopCallback();
	EnterCriticalSection();
	
	// Execute!
	Exec(&ExeParams, 1, 0);
	
}
void DoMenu() {
	
	int		i=0,rnum=0,ba=0;
	int		ListDrawY=0,ItemY=0;
	int		StartListY=0,EndListY=0,MaxListLength=0;
	
	int		fBannerY=0;
	int		ListY=0,fListY=0;
	int		TitleChosen=0;
	int		TransState=0;
	
	int		PadStatus=0;
	/*
	int		uPressed=0;
	int		dPressed=0;
	int		uPressedCount=0;
	int		dPressedCount=0;
	
	int		pausePressed=0;
	int		nextPressed=0;
	int		pausePressedCount=0;
	int		nextPressedCount=0;
	int		prevPressed=0;
	int		prevPressedCount=0;
	*/
	u_long		padPressed=0;
	int		padPressedCount=0;
	
	
	int		LoadError=false;
	
	u_long  MusType=MUSIC_NONE;
	int    MusPlaying=false;
	
	typedef struct {
		int	x,y;
		int fx,fy;
		int xMove,yMove;
		int Active,Size;
	} BUBBLESTRUCT;
	
	BUBBLESTRUCT	Bubble[MAX_BUBBLES]={0};
	
	GsSPRITE 		FontSprite			={0};
	GsSPRITE		BigCircleSprite		={0};
	GsSPRITE		SmallCircleSprite	={0};
	GsBOXF			SelectionBox		={0};
	
	
	// Prepare the sprite structs
	FontSprite			= PrepSprite(FontTIM);
	BigCircleSprite		= PrepSprite(BigCircleTIM);
	SmallCircleSprite	= PrepSprite(SmallCircleTIM);
	
	BigCircleSprite.attribute	= (0<<28)|(1<<30);
	SmallCircleSprite.attribute = (0<<28)|(1<<30);
	
	
	// Prepare the selection bar
	SelectionBox.x = 0;
	SelectionBox.w = ScreenXres;
	SelectionBox.h = 18;
	SelectionBox.attribute = (1<<28)|(1<<30);
	SelectionBox.r = 0;
	SelectionBox.g = 0;
	SelectionBox.b = 127;
	
	
	// Start the banner and list menu off screen for a nice pop-in effect
	ListDrawY = (ScreenYres - (18 * 16)) + 1;
	MaxListLength = ((ScreenYres - ListDrawY) / 18) - 1;
	fListY = -((ONE * ((18 * (MaxListLength)))) + 15);
	fBannerY = -(ONE * 200);
	
	
	// Play the MOD music
	/*
	MOD_Init();
	MOD_Load((u_char*)MOD_AREA);
	MOD_Start();
	*/
	
	
	srand(1);
	while (1) {
		
		PrepDisplay();
		PadStatus = PadRead(0);
		
		
		// Title selection controls
		if (TitleChosen == false) {

			//printf("PAD %i\n",PadStatus);
			//select nothing
			if (PadStatus == 0) {
				padPressed = 0;
				padPressedCount = 0;
			}
			
			// Select up
			if (PadStatus & PADLup) {
				if (SelTitle > 0) {
					if (padPressed != PADLup)	{
					SelTitle -= 1;
					padPressedCount = 0;
					}
					if (padPressedCount >= 32) padPressedCount = 30;
					if (padPressedCount == 30) SelTitle -= 1;
					padPressedCount += 1;
				}
				padPressed = PADLup;
			}
			
			// Select down
			if (PadStatus & PADLdown) {
				if (SelTitle < (NumTitles - 1)) {
					if (padPressed != PADLdown) {	
					SelTitle += 1;
					padPressedCount = 0;
					}
					if (padPressedCount >= 32)	padPressedCount = 30;
					if (padPressedCount == 30)	SelTitle += 1;
					padPressedCount += 1;
				}
				padPressed = PADLdown;
			}
			
			// Select Track Up
			if (PadStatus & PADR1) {
			#if DEBUG
			printf("%i presses, %hi current track, %hi tracks total\n", padPressedCount, curtrk, septrk);
			#endif
				if (curtrk < (septrk - 1)) {
					if (padPressed != PADR1)	{
					curtrk = ChangeTrack(curtrk+1, MusType);
					padPressedCount = 0;
					}
					if (padPressedCount >= 32) padPressedCount = 30;
					if (padPressedCount == 30) curtrk = ChangeTrack(curtrk+1, MusType);
					padPressedCount += 1;
				}
				padPressed = PADR1;
			}
			
			// Select Track Down
			if (PadStatus & PADL1) {
			#if DEBUG
			printf("%i presses, %hi current track, %hi tracks total\n", padPressedCount, curtrk, septrk);
			#endif
				if (curtrk > 0) {
					if (padPressed != PADL1) {	
					curtrk = ChangeTrack(curtrk-1, MusType);
					padPressedCount = 0;
					}
					if (padPressedCount >= 32)	padPressedCount = 30;
					if (padPressedCount == 30)	curtrk = ChangeTrack(curtrk-1, MusType);
					padPressedCount += 1;
				}
				padPressed = PADL1;
			}
			
			if (PadStatus & PADRright) {

				if (padPressed != PADRright) {
					padPressedCount=0;
					if (MusPlaying) {
						MusPlaying=PauseMusic(MusType);
					} else {
						MusPlaying=PlayMusic(MusType);
					}
				}
				/*
				if (padPressedCount >= 32)	padPressedCount = 0;
				if (padPressedCount == 30)	{
				#if DEBUG
				printf("30 presses, pausing again\n");
				#endif
					if (MusPlaying) {
						MusPlaying=PauseMusic(MusType);
					} else {
						MusPlaying=PlayMusic(MusType);
					}
				}
				*/
				padPressedCount += 1;
				padPressed = PADRright;
				#if DEBUG
				printf("%i presses now\n", padPressedCount);
				#endif
			}
			
			// Start game
			if ((PadStatus & PADRdown) || (PadStatus & PADstart)) {
				switch (Title[SelTitle].StackAddr) {
					case MUSIC_NONE:
					case MUSIC_MOD:
					case MUSIC_SEQ:
					case MUSIC_SEP:
						#if DEBUG
						printf("chosen: %s %i\n", Title[SelTitle].ExecFile, Title[SelTitle].StackAddr);
						#endif
						MusType = StartMusic(Title[SelTitle], MusType);
						MusPlaying = true;
						break;
					default:
						TitleChosen = true;
						TransCount = 0;
				}
			}
			
		}
		
		
		// Transition to black if a title was selected
		if (TitleChosen) TransState = DoTransition2();
		
		
		// Draw banner
		fBannerY += ((ONE * 40) - fBannerY) / 8;
		SortBigImage(63, (fBannerY + (csin(ba) * 8)) / ONE, BannerTIM);
		ba = (ba + 8) % 4096;
		
		
		// Calculate coordinates of the list
		fListY	+= (((ONE * ((18 * (SelTitle - 7)))) + 9) - fListY) / 8;
		ListY	= (fListY + (ONE / 2)) / ONE;
		
		if (ListY < 0) {
			StartListY	= ((ListY + 17) - 18) / 18;
		} else {
			StartListY	= (ListY + 17) / 18;
		}
		EndListY	= StartListY + MaxListLength;
		
		if (StartListY < 0)			StartListY = 0;
		if (EndListY > NumTitles)	EndListY = NumTitles;
		
		
		// Draw the list
		for (i=StartListY; i<EndListY; i+=1) {
			
			ItemY = (ListDrawY + (18 * i)) - ListY;
			
			if (ItemY < (ListDrawY + 72)) {
				fPrint(Title[i].Name, CENTERED, ItemY, 127 * ((float)((ItemY + 1) - ListDrawY)  / 72), &myOT[ActiveBuffer], FontTIM);
			} else if ((ItemY + 18) >= (ScreenYres - 108)) {
				fPrint(Title[i].Name, CENTERED, ItemY, 127 - (127 * ((float)(ItemY - (ScreenYres - 108)) / 72)), &myOT[ActiveBuffer], FontTIM);
			} else {
				fPrint(Title[i].Name, CENTERED, ItemY, 127, &myOT[ActiveBuffer], FontTIM);
			}
			
			if ((i == SelTitle) && (TitleChosen == false)) {
				SelectionBox.y = ((ListDrawY + (18 * i)) - ListY) - 1;
				GsSortBoxFill(&SelectionBox, &myOT[ActiveBuffer], 0);
			}
			
		}
		
		
		// Process bubbles in the background
		for (i=0; i<MAX_BUBBLES; i+=1) {
			
			if (Bubble[i].Active) {
			
				Bubble[i].x = Bubble[i].fx / ONE;
				Bubble[i].y = Bubble[i].fy / ONE;
				Bubble[i].fx += Bubble[i].xMove;
				Bubble[i].fy += Bubble[i].yMove;
				if (Bubble[i].Size >= 5) {
					BigCircleSprite.x = Bubble[i].x;
					BigCircleSprite.y = Bubble[i].y;
					GsSortFastSprite(&BigCircleSprite, &myOT[ActiveBuffer], 0);
				} else {
					SmallCircleSprite.x = Bubble[i].x;
					SmallCircleSprite.y = Bubble[i].y;
					GsSortFastSprite(&SmallCircleSprite, &myOT[ActiveBuffer], 0);
				}
				if (Bubble[i].y < -64) Bubble[i].Active = false;
				
			} else {
			
				rnum = 30 * frand();
				if ((rnum >= 12) && (rnum <=18)) {
					Bubble[i].Active = true;
					Bubble[i].x = (672 * frand()) - 32;
					Bubble[i].y = 480 + (100 * frand());
					Bubble[i].fx = ONE * Bubble[i].x;
					Bubble[i].fy = ONE * Bubble[i].y;
					Bubble[i].xMove = 0;
					Bubble[i].yMove = -(ONE * (1.0f + (4.0f * frand())));
					Bubble[i].Size = 10 * frand();
				}
				
			}
			
		}
		
		
		// Display everything
		Display();
		
		
		// Break once transition is completed when a title is chosen
		if ((TitleChosen) && (TransState == 0)) break;
		
	}
	
	
	// Stop the music
	MOD_Stop();
	MOD_Free();
	
	
	// Begin loading the EXE file in the background and get its parameters
	sprintf(StringBuff, "%s;1", Title[SelTitle].ExecFile);
	if (LoadEXEfile(StringBuff, &ExeParams) == 0) LoadError=true;
	
	
	// Display the title of the game being loaded
	ClearRed = ClearGrn = ClearBlu = 0;
	for (i=0; i<127; i+=2) {
		PrepDisplay();
		if (LoadError == false) {
			fPrint("Now Loading", CENTERED, 220, i, &myOT[ActiveBuffer], FontTIM);
			fPrint(Title[SelTitle].Name, CENTERED, 240, i, &myOT[ActiveBuffer], FontTIM);
		} else {
			fPrint("ERROR! Cannot find file:", CENTERED, 220, i, &myOT[ActiveBuffer], FontTIM);
			fPrint(Title[SelTitle].ExecFile, CENTERED, 240, i, &myOT[ActiveBuffer], FontTIM);
		}
		Display();
	}
	
	// This is to prevent the text from flickering on interlaced displays
	for (i=0; i<2; i+=1) {
		PrepDisplay();
		if (LoadError == false) {
			fPrint("Now Loading", CENTERED, 220, 128, &myOT[ActiveBuffer], FontTIM);
			fPrint(Title[SelTitle].Name, CENTERED, 240, 128, &myOT[ActiveBuffer], FontTIM);
		} else {
			fPrint("ERROR! Cannot find file:", CENTERED, 220, 128, &myOT[ActiveBuffer], FontTIM);
			fPrint(Title[SelTitle].ExecFile, CENTERED, 240, 128, &myOT[ActiveBuffer], FontTIM);
		}
		DisplayNoClear();
	}
	
	
	// Suspend the menu if selected game does not exist
	if (LoadError) {
		#if DEBUG
		printf("Menu suspended.\n");
		#endif
		while (1) VSync(0);
	}
	
	// Wait for the EXE to finally finish loading
	CdReadSync(0, 0);
	#if DEBUG
	printf("EXE loaded successfully.\n");
	#endif
	
}
int LoadEXEfile(char *FileName, struct EXEC *Params) {
	
	typedef struct {
		u_char		Pad[16];
		struct EXEC	Params;
		u_char		Pad2[1972];
	} EXEHEADER;
	
	u_char		Mode=CdlModeSpeed;
	CdlFILE		File;
	EXEHEADER	Header;
	
	
	#if DEBUG
	printf("Searching for file %s...", FileName);
	#endif
	
	// Search for the file to load
	if (CdSearchFile(&File, FileName) == 0) {
		#if DEBUG
		printf("File not found.\n");
		#endif
		return(0);
	}
	
	#if DEBUG
	printf("Found it!\n");
	#endif
	
	
	// Seek to the EXE file and read its header
	CdControl(CdlSetloc, (u_char*)&File.pos, 0);
	CdRead(1, (u_long*)&Header, Mode);
	CdReadSync(0, 0);
	
	
	// Get the EXE header data from the sector buffer
	*Params = Header.Params;
	
	#if DEBUG
	printf("EXE parameters: %d\n", sizeof(EXEHEADER));
	printf(" pc0   :%X\n", Params->pc0);
	printf(" t_addr:%X\n", Params->t_addr);
	printf(" t_size:%X\n", Params->t_size);
	printf("Now loading EXE to t_addr...");
	#endif
	
	// Seek to next sector and read the rest of the EXE file
	CdIntToPos(CdPosToInt(&File.pos)+1, &File.pos);
	CdControl(CdlSetloc, (u_char*)&File.pos, 0);
	CdRead(((File.size - 2048)+2047)/2048, (void*)Params->t_addr, Mode);
	
	return(1);
	
}

void fPrint(char *string, short x, short y, char opacity, GsOT *otptr, GsIMAGE font) {
	
	// Draws characters as sprites
	
	int i=0,strwidth=0;
	GsSPRITE CharSprite=PrepSprite(font);
	
	CharSprite.attribute = (1<<28)|(1<<30);
	
	if (x == CENTERED) {
		for (i=0; *(string+i) != 0x00; i+=1) {
			strwidth += CharWidth[*(string+i)-32];;
		}
		strwidth += 32;
		CharSprite.x = ScreenCenterX - (strwidth / 2);
	} else {
		CharSprite.x = x;
	}
	
	CharSprite.y = y;
	CharSprite.w = 16;
	CharSprite.h = 16;
	CharSprite.r = CharSprite.g = CharSprite.b = opacity;
	
	for (i=0; *(string+i) != 0x00; i+=1) {
		
		if ((*(string+i) >= 32) && (*(string+i) <= 127)) {
			if (*(string+i) >= 33) {
				CharSprite.u = 16 * ((*(string+i) - 32) % 16);
				CharSprite.v = 16 * ((*(string+i) - 32) / 16);
				GsSortFastSprite(&CharSprite, otptr, 0);
			}
			CharSprite.x += CharWidth[*(string+i)-32];
		}
		
	}
	
}
void SortBigImage (int x, int y, GsIMAGE TimImage) {

	// Returns a GsSPRITE structure so that TimImage can be displayed using GsSortSprite.

	GsSPRITE tSprite;
	
	int ClipNeeded=0;
	int ImageWidth=0;
	
	// Set texture size and coordinates
	switch (TimImage.pmode & 3) {
		case 0: // 4-bit
			tSprite.w = TimImage.pw << 2;
			tSprite.u = (TimImage.px & 0x3f) * 4;
			break;
			
		case 1: // 8-bit
			tSprite.w = TimImage.pw << 1;
			
			if (tSprite.w > 256) {
				tSprite.w = 256;
				ClipNeeded = 1;
			}
			
			tSprite.u = (TimImage.px & 0x3f) * 2;
			break;
			
		default: // 16-bit
			tSprite.w = TimImage.pw;
			tSprite.u = TimImage.px & 0x3f; 
			
    };
	
	tSprite.h = TimImage.ph;
	tSprite.v = TimImage.py & 0xff;

	// Set texture page and attributes
	tSprite.tpage 		= GetTPage((TimImage.pmode & 3), 0, TimImage.px, TimImage.py);
	tSprite.attribute 	= ((TimImage.pmode & 3)<<24)|(1<<30);
	
	// CLUT coords
	tSprite.cx 			= TimImage.cx;
	tSprite.cy 			= TimImage.cy;
	
	// Default position, color intensity, and scale/rotation values
	tSprite.mx = tSprite.my				= 0;
	tSprite.r = tSprite.g = tSprite.b 	= 128;
	tSprite.scalex = tSprite.scaley 	= ONE;
	tSprite.rotate 						= 0;

	tSprite.x 			 				= x;
	tSprite.y							= y;
	
	GsSortFastSprite(&tSprite, &myOT[ActiveBuffer], 0);
	
	if (ClipNeeded) {
	
		switch (TimImage.pmode & 3) {
			case 0: // 4-bit
				tSprite.w = TimImage.pw << 2;
				tSprite.u = (TimImage.px & 0x3f) * 4;
				break;
				
			case 1: // 8-bit
				tSprite.w = (TimImage.pw << 1) - 256;
				tSprite.u = ((TimImage.px & 0x3f) * 2) + 256;
				break;
				
			default: // 16-bit
				tSprite.w = ImageWidth;
				tSprite.u = TimImage.px & 0x3f; 
		};
	
		tSprite.x 		= x + 256;
		tSprite.tpage 	= GetTPage((TimImage.pmode & 3), 0, TimImage.px + 128, TimImage.py);
		
		GsSortFastSprite(&tSprite, &myOT[ActiveBuffer], 0);
	
	}
	
}

void Init() {
	
	// Reset GPU without clearing the VRAM for a nice transition effect
	ResetGraph(3);	
	
	// Init CD
	CdInit();
	
	
	// Start loading the MOD music while transitioning
	//CdReadFile("\\MUSIC.HIT", (u_long*)MOD_AREA, 0);
	
	// Do a cool transition effect
	VSync(0);
	while (DoTransition()) {
		DrawSync(0);
		VSync(0);
	}
	
	// Wait for the MOD to finally finish loading
	CdReadSync(0, 0);
	
	
	// Check whether if system is an NTSC or PAL region unit
	if (*(char *)0xbfc7ff52=='E') {
		SetVideoMode(MODE_PAL);
	} else {
		SetVideoMode(MODE_NTSC);
	}
	
	
	// Init video
	ScreenXres = 640;
	ScreenYres = 480;
	ScreenCenterX = ScreenXres / 2;
	ScreenCenterY = ScreenYres / 2;
	GsInitGraph(ScreenXres, ScreenYres, GsINTER|GsOFSGPU|GsRESET3, 0, 0);	
	
	// Set the Display buffers
	GsDefDispBuff(0, 0, 0, 0);

	// Setup the ordering tables
	myOT[0].length = OT_LENGTH;
	myOT[1].length = OT_LENGTH;
	myOT[0].org = myOT_TAG[0];
	myOT[1].org = myOT_TAG[1];
	
	GsClearOt(0, 0, &myOT[0]);
	GsClearOt(0, 0, &myOT[1]);
	
	
	// Load the menu graphics and title entries
	LoadGraphics();
	InitTitles();
	
	// Init controller
	PadInit(0);
	
}
void LoadGraphics() {
	
	// Load the QLP pack and upload all the TIMs inside it onto VRAM
	#if DEBUG
	printf("Loading GRAPHICS.QLP...");
	#endif
	CdReadFile("\\GRAPHICS.QLP", (u_long*)TEMP_AREA, 0);
	CdReadSync(0, 0);
	
	FontTIM			= LoadTIM(QLPfilePtr((u_long*)TEMP_AREA, 0));
	BannerTIM		= LoadTIM(QLPfilePtr((u_long*)TEMP_AREA, 1));
	BigCircleTIM	= LoadTIM(QLPfilePtr((u_long*)TEMP_AREA, 2));
	SmallCircleTIM	= LoadTIM(QLPfilePtr((u_long*)TEMP_AREA, 3));
	
	#if DEBUG
	printf("Done.\n");
	printf("FONT ADDR: %X\n",QLPfilePtr((u_long*)TEMP_AREA, 3));
	#endif
	
}
void InitTitles() {
	
	
	int		i=0,InQuote=0,TitleNum=0,GrabStep=0,Separator=0,CharNum=0;
	
	char	AddrText[16]={0};
	char	TextBuff[LISTFILE_MAXSIZE]={0};
	
	#if DEBUG
	printf("Loading TITLES.TXT...");
	#endif
	
	// Load LIST.TXT
	CdReadFile("\\TITLES.TXT", (u_long*)TextBuff, 0);
	CdReadSync(0, 0);
	
	
	// Scan the file's text
	for (i=0; TextBuff[i] != 128; i+=1) {
		
		if (InQuote == false) {
			
			if (TextBuff[i] == '"') {	// If quote detected, start grabbing characters
				CharNum = 0;
				InQuote = true;
				Separator = false;
			}
			
		} else {
			
			if (TextBuff[i] == '"') {	// If quote detected, end character grabbing
			
				if (GrabStep == 0) {
					GrabStep = 1;
				} else if (GrabStep == 1) {
					GrabStep = 2;
				} else {
					GrabStep = 0;
					Title[TitleNum].StackAddr = hex2int(AddrText);
					TitleNum += 1;
				}
				
				InQuote = false;
				
			} else {
			
				switch (GrabStep) {
					case 0:	// Grab title name
						Title[TitleNum].Name[CharNum] = TextBuff[i];
						CharNum += 1;
						break;
					case 1:	// Grab executable file name
						Title[TitleNum].ExecFile[CharNum] = TextBuff[i];
						CharNum += 1;
						break;
					case 2:	// Grab stack address in hex
						AddrText[CharNum] = TextBuff[i];
						CharNum += 1;
						AddrText[CharNum] = 0x00;	// Add null byte just in case
						break;
				}
				
			}
			
		}
		
	}
	
	NumTitles = TitleNum;
	
	#if DEBUG
	printf("Done.\n");
	#endif
	
}

int hex2int(char *string) {

	// A tiny little function to convert a string of 8 hex characters
	// into a 32-bit integer.
	
	int		i=0,r=0;
	char	c=0;
	
	for (i=0; *(string+i) != 0x00; i+=1) {
		
		c=*(string+i);
		if ((c >= 48) && (c <= 57)) {
			r |= (c-48)<<(4*(7-i));
		} else if ((c >= 65) && (c <= 70)) {
			r |= (10+(c-65))<<(4*(7-i));
		}
		
	}
	
	return(r);

}
float frand() {
	
	// Simplified random number generator
	
	return((float)rand() / 32767);
	
}

int DoTransition() {
	
	// Simple transition effect routine (framebuffer based)
	
	RECT Box = { 0, 0, 640, 1 };
	int BoxHeight=0;
	int i=0;

	if (TransCount) {
		
		for (i=0;i<15;i+=1) {
			
			if (TransCount > (2 * i)) {
			
				BoxHeight = TransCount - (2 * i);
				
				if (BoxHeight < 0)	BoxHeight = 0;
				if (BoxHeight > 32)	BoxHeight = 32;
				
				if (BoxHeight > 0) {
					Box.y = (32 * i) + BoxHeight;
					ClearImage2(&Box, CLR_RED, CLR_GRN, CLR_BLU);
				}
			
			}
			
		}
		
	}
	
	if (TransCount < 96) {
		TransCount += 1;
		return(1);
	} else {
		return(0);
	}
	
}
int DoTransition2() {
	
	// Simple transition effect routine
	
	GsBOXF Box = { 0, 0, 0, 640, 0, 0, 0, 0 };
	int BoxHeight=0;
	int i=0;

	if (TransCount) {
		
		for (i=0;i<15;i+=1) {
			
			if (TransCount > (2 * i)) {
			
				BoxHeight = TransCount - (2 * i);
				
				if (BoxHeight < 0)	BoxHeight = 0;
				if (BoxHeight > 32)	BoxHeight = 32;
				
				if (BoxHeight > 0) {
					Box.y = (32 * i);
					Box.h = BoxHeight;
					GsSortBoxFill(&Box, &myOT[ActiveBuffer], 0);
				}
			
			}
			
		}
		
	}
	
	if (TransCount < 96) {
		TransCount += 1;
		return(1);
	} else {
		return(0);
	}
	
}

void PrepDisplay() {
	
	ActiveBuffer = GsGetActiveBuff();
	GsSetWorkBase((PACKET*)GPUPacketArea[ActiveBuffer]);
	GsClearOt(0, 0, &myOT[ActiveBuffer]);
	
}
void Display() {
	
	VSync(0);
	GsSwapDispBuff();
	GsSortClear(ClearRed, ClearGrn, ClearBlu, &myOT[ActiveBuffer]);
	GsDrawOt(&myOT[ActiveBuffer]);
	
}
void DisplayNoClear() {

	VSync(0);
	GsSwapDispBuff();
	GsDrawOt(&myOT[ActiveBuffer]);
	
}

u_long StartMusic (TITLESTRUCT file, u_long currenttype) {

	StopMusic (currenttype);
	if (file.StackAddr != currenttype) {
		UnloadMusic(currenttype);
		LoadMusic(file.StackAddr);
	}
	ChangeMusic(file);
	return file.StackAddr;

}

int StopMusic (u_long filetype) {
	#if DEBUG
	printf("Stopping music type %i\n", filetype);
	#endif
	switch (filetype) {

		case MUSIC_NONE:
			return 0;
			break;
		case MUSIC_MOD:
			MOD_Stop();
			MOD_Free();
			return 0;
			break;
		case MUSIC_SEQ:
			SsSeqClose (seq1);
			SsVabClose (vab1);
			return 0;
			break;
		case MUSIC_SEP:
			SsSepClose (sep1);
			SsVabClose (vab1);
			return 0;
			break;
		default:
			return 1;
	}
	return 2;

}

int ChangeMusic (TITLESTRUCT file) {
	//int strackvol;
		switch (file.StackAddr) {
		case MUSIC_NONE:
			return 0;
			break;
		case MUSIC_MOD:
			CdReadFile(file.ExecFile, (u_long*)MOD_AREA, 0);
			CdReadSync(0, 0);
			MOD_Load((u_char*)MOD_AREA);
			MOD_Start();
			return 0;
			break;
		case MUSIC_SEQ:
			CdReadFile(file.ExecFile, (u_long*)MOD_AREA, 0);
			CdReadSync(0, 0);
			vab1 = SsVabOpenHead ((unsigned char*)QLPfilePtr((u_long*)MOD_AREA, 1), -1);
			#if DEBUG
				if( vab1 == -1 ) {
				printf("Failed to open VH!\n");
				}
			#endif
			vab1 = SsVabTransBody ((unsigned char*)QLPfilePtr((u_long*)MOD_AREA, 2), vab1);
			#if DEBUG
				if( vab1 == -1 ) {
				printf("Failed to open VB!\n");
				}
			#endif
			SsVabTransCompleted (SS_WAIT_COMPLETED);
			SsStart();
			seq1 = SsSeqOpen ((unsigned long*)QLPfilePtr((u_long*)MOD_AREA, 0), vab1);
			SsSetMVol (127, 127);
			SsSeqSetVol (seq1, 127, 127);
			SsSeqPlay(seq1, SSPLAY_PLAY, SSPLAY_INFINITY);
			return 0;
			break;
		case MUSIC_SEP:
			CdReadFile(file.ExecFile, (u_long*)MOD_AREA, 0);
			CdReadSync(0, 0);
			vab1 = SsVabOpenHead ((unsigned char*)QLPfilePtr((u_long*)MOD_AREA, 1), -1);
			#if DEBUG
				if( vab1 == -1 ) {
				printf("Failed to open VH\n");
				}
			#endif
			vab1 = SsVabTransBody ((unsigned char*)QLPfilePtr((u_long*)MOD_AREA, 2), vab1);
			#if DEBUG
				if( vab1 == -1 ) {
				printf("Failed to open VB\n");
				}
			#endif
			SsVabTransCompleted (SS_WAIT_COMPLETED);
			SsStart();
			septrk = ((short)*QLPfilePtr((u_long*)MOD_AREA, 3));
			#if DEBUG
			printf("SEP Track Number Loaded to %X with %hi tracks\n",QLPfilePtr((u_long*)MOD_AREA, 3),septrk);
			//septrk=3;
			#endif
			sep1 = SsSepOpen ((unsigned long*)QLPfilePtr((u_long*)MOD_AREA, 0), vab1, septrk);
			#if DEBUG
			printf("SEP Opened at %X\n",QLPfilePtr((u_long*)MOD_AREA, 0));
			#endif
			SsSetMVol (127, 127);
			for (curtrk = 0; curtrk < septrk; curtrk++) {
			SsSepSetVol (sep1, curtrk, 127, 127); //indenting didnt work dont know why
			#if DEBUG
			printf("SEP Track %hi Volume Set\n",curtrk);
			#endif
			}
			curtrk = 0;
			SsSepPlay(sep1, curtrk, SSPLAY_PLAY, SSPLAY_INFINITY);
			return 0;
			break;
		default:
			return 1;
		}
	
}

int UnloadMusic (u_long filetype) {
	
	switch (filetype) {
		case MUSIC_NONE:
			return 0;
			break;
		case MUSIC_MOD:
			return 0;
			break;
		case MUSIC_SEP:
		case MUSIC_SEQ:
			SsEnd();
			SsQuit();
			return 0;
			break;
		default:
			return 1;
		}
	return 2;

}

int LoadMusic (u_long filetype) {

		switch (filetype) {
		case MUSIC_NONE:
			return 0;
			break;
		case MUSIC_MOD:
			MOD_Init();
			return 0;
			break;
		case MUSIC_SEQ:
		case MUSIC_SEP:
			SsInit();
			//SsSetTableSize (seq_table, 4, 5);
			SsSetTableSize (seq_table, 10, 10);
			SsSetTickMode (SS_TICK240);
			return 0;
			break;
		default:
			return 1;
		}
	return 2;

}

int PlayMusic (u_long filetype) {
	
	switch (filetype) {
		case MUSIC_NONE:
			return 1;
			break;
		case MUSIC_MOD:
			MOD_Start();
			return 1;
			break;
		case MUSIC_SEQ:
			SsSeqReplay(seq1);
			return 1;
			break;
		case MUSIC_SEP:
			SsSepReplay(sep1, curtrk);
			return 1;
			break;
		default:
			return 1;
		}
	return 0;

}

int PauseMusic (u_long filetype) {
	
	switch (filetype) {
		case MUSIC_NONE:
			return 0;
			break;
		case MUSIC_MOD:
			MOD_Stop();
			return 0;
			break;
		case MUSIC_SEQ:
			SsSeqPause(seq1);
			return 0;
			break;
		case MUSIC_SEP:
			SsSepPause(sep1, curtrk);
			return 0;
			break;
		default:
			return 0;
		}
	return 1;

}

short ChangeTrack (short nowtrack, u_long filetype) {
	switch (filetype) {
		case MUSIC_NONE:
		case MUSIC_MOD:
		case MUSIC_SEQ:
			return nowtrack;
			break;
		case MUSIC_SEP:
			SsSepStop(sep1, curtrk);
			SsSepPlay(sep1, nowtrack, SSPLAY_PLAY, SSPLAY_INFINITY);
			return nowtrack;
			break;
		default:
			return nowtrack;
			break;
		}
	return nowtrack;
}