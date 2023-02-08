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
#define DEBUG	false

// Stuff you can change to suit your needs
#define MENU_AREA			0x80010000
#define MAX_TITLES			1024
#define MENU_SIZE			MAX_TITLES*128
#define MOD_AREA			0x80030000	// MUST BE MANUAL?
#define TEMP_AREA			0x80030000	
#define QLP_MAXSIZE			1024*128
#define LISTFILE_MAXSIZE	1024*8

#define TITLE_AREA			0x8003000C
#define CONT_AREA			0x80030800

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
#define MUSIC_XM 0xFFFFFF02
#define MUSIC_SEQ 0xFFFFFF03
#define MUSIC_SEP 0xFFFFFF04

#define SEP_MIN 0x01000000
#define SEP_MAX 0x0100FFFF

#define SEQ_MIN 0x01010000
#define SEQ_MAX 0x0101FFFF

#define MENU_DIRREAD 0xFFFFFFFB
#define MENU_VFSBYTE 0xFFFFFFFC //uses bytes instead of sectors in the vfs. possible but probably useless
#define MENU_TXTVFS 0xFFFFFFFFD
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

typedef struct {
	short SeqNum;
	short Version;
	short MvolL;
	short MvolR;
	short VolL;
	short VolR;
	short RvolL;
	short RvolR;
	short RdepthL;
	short RdepthR;
	short Rdelay;
	short Rmode;
	short Rfeedback;
	long TickMode;
	char SeqLoops;
	char SeqType;
} PARAMS_V1;

typedef struct {
	short SeqNum;
	short Version;
} PARAMS_HEADER;

// Struct to store title names and executable paths
typedef struct {
	char	Name[64];
	char	ExecFile[52];
	u_long	StackAddr;
	int 	SectorStart;
	int 	SectorLength;
} TITLESTRUCT;

//TITLESTRUCT Title[MAX_TITLES]={0};
TITLESTRUCT* Title=(TITLESTRUCT*)MENU_AREA;

int		NumTitles=0;
int 	SelTitle=0;
char	StringBuff[56]={0};
char	NameBuff[64]={0};

int		LSMI=MAX_TITLES + 1;

PARAMS_V1 p;
//short vol = 127;
short seq1;  /* SEQ data id */
short sep1;  /* SEP data id */
short vab1;  /* VAB data id */
short septrk;
short curtrk;

//char seq_table[SS_SEQ_TABSIZ * 4 * 5];
char seq_table[SS_SEQ_TABSIZ * 2 * 16];


// For launching an EXE
struct EXEC ExeParams;

PARAMS_HEADER ParamsNull;

// Function prototypes
int main();
void DoMenu();
int LoadEXEfile(char *FileName, struct EXEC *params,  u_long ssect, u_long nsect);

void fPrint(char *string, short x, short y, char opacity, GsOT *otptr, GsIMAGE font);
void SortBigImage (int x, int y, GsIMAGE TimImage);

void Init();
void LoadGraphics(char* gfxfile, u_long ssect, u_long nsect);
void InitTitles(char* titlefile, u_long ssect, u_long nsect);

float frand();
int hex2int(char *string);

int DoTransition();
int DoTransition2();

void PrepDisplay();
void Display();
void DisplayNoClear();

u_long StartMusic (TITLESTRUCT* file, u_long currenttype, int PadStatus);
int StopMusic (u_long filetype);
int UnloadMusic (u_long filetype);
int LoadMusic (u_long filetype);
int ChangeMusic (TITLESTRUCT* file, int PadStatus);

int PlayMusic (u_long filetype);
int PauseMusic (u_long filetype);

short ChangeTrack (short nowtrack, u_long filetype);
short ChangeVol (short nowvolL, short nowvolR, u_long filetype);

void InitVfs(char* vfsfile);
int CDRF(char* file, u_long *addr, u_long startsect, u_long nsect);
int LoadSep (char* name, u_long* addr, u_long ssect, u_long nsect, short ptrack);
short LoadSeq (u_long* addr, short ptrack, int extfiles);
int str_n_cmp (char* str1, char* str2, int len);
PARAMS_V1 ParamsV1ToDefault();

int SeqParamCount (u_long* addr);
short LoadPreParams(PARAMS_HEADER* addr);
short LoadPostParams(PARAMS_HEADER* addr);
PARAMS_HEADER* ParamFile(u_long stack, u_long *QLP_AREA);

short ChangeRevMode (short revmode, u_long filetype);
short IncVols (u_long MusType);
short DecVols (u_long MusType);
short IncRVols (u_long MusType);
short DecRVols (u_long MusType);
short ChangeRVol (short nowvolL, short nowvolR, u_long filetype);

short IncRDepths (u_long MusType);
short DecRDepths (u_long MusType);
short ChangeRDepth (short nowvolL, short nowvolR, u_long filetype);
short ChangeDelay (short nowfbdel, u_long filetype);
short ChangeFeedback (short nowfbdel, u_long filetype);
//int TimeoutFrames (short revmode);

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

	int		Timeout=0;
	int		TimeoutStart=10;
	
	u_long		padPressed=0;
	int		padPressedCount=0;

	int		LoadError=false;
	
	u_long  MusType=MUSIC_NONE;
	int    MusPlaying=false;
	
	PARAMS_HEADER* ParamPtr = 0;
	
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
	p = ParamsV1ToDefault();
	ParamsNull.SeqNum = 0;
	ParamsNull.Version = 0;
	
	srand(1);
	while (1) {
		if (MusType == MUSIC_SEQ && (p.TickMode & SS_NOTICK || p.TickMode == SS_NOTICK0)) {
			SsSeqCalledTbyT();
		}
		PrepDisplay();
		PadStatus = PadRead(0);

		// Title selection controls
		if (TitleChosen == false) {

			//printf("PAD %i\n",PadStatus);
			//select nothing
			if (PadStatus == 0 || PadStatus == PADRleft || PadStatus == PADRup) {
				padPressed = 0;
				padPressedCount = 0;
			}
			// While holding triangle
			if (PadStatus & PADRup) {
				//#if DEBUG
				// Select Loops Up
				if (PadStatus & PADR2) {
				#if DEBUG
				printf("%i presses, %d loops\n", padPressedCount, p.SeqLoops);
				#endif
					if (p.SeqLoops < 255 && padPressed != PADR2 + PADRup) {
					p.SeqLoops++;
					padPressedCount = 0;
					}
					//if (padPressedCount >= 32) padPressedCount = 30;
					//if (padPressedCount == 30) IncVols(MusType);
					padPressedCount += 1;
					padPressed = PADR2 + PADRup;
				}
				
				// Select Loops Down
				if (PadStatus & PADL2) {
				#if DEBUG
				printf("%i presses, %d loops\n", padPressedCount, p.SeqLoops);
				#endif
					if (p.SeqLoops > 0 && padPressed != PADL2 + PADRup) {
					p.SeqLoops--;
					padPressedCount = 0;
					}
					//if (padPressedCount >= 32)	padPressedCount = 30;
					//if (padPressedCount == 30) DecVols(MusType);
					padPressedCount += 1;
					padPressed = PADL2 + PADRup;
				}
				
				if (PadStatus & PADselect) {
					if (padPressed != PADselect + PADRup) {
						padPressedCount=0;
						SsUtReverbOff();
					}
					padPressedCount += 1;
					padPressed = PADselect + PADRup;
				}
				if (PadStatus & PADstart) {
					if (padPressed != PADstart + PADRup) {
						padPressedCount=0;
						SsUtReverbOn();
					}
					padPressedCount += 1;
					padPressed = PADstart + PADRup;
				}
				if (PadStatus & PADRright) {
					if (padPressed != PADRright + PADRup) {
						padPressedCount=0;
						p = ParamsV1ToDefault();
						ChangeVol(p.VolL, p.VolR, MusType);
						ChangeFeedback(p.Rfeedback, MusType);
						ChangeDelay(p.Rdelay, MusType);
						ChangeRevMode(p.Rmode, MusType);
						Timeout = TimeoutStart;
					}
					padPressedCount += 1;
					padPressed = PADRright + PADRup;
					#if DEBUG
					printf("%i presses now\n", padPressedCount);
					#endif
				}
				if (PadStatus & PADLup) {
					if (TimeoutStart < 600) {
						if (padPressed != PADLup + PADRup) {	
						TimeoutStart++;
						padPressedCount = 0;
						}
						if (padPressedCount >= 32)	padPressedCount = 30;
						if (padPressedCount == 30)	TimeoutStart++;
						padPressedCount += 1;
					}
					padPressed = PADLup + PADRup;
					#if DEBUG
					printf("Timeout is now %i frames\n", TimeoutStart);
					#endif
				}
				if (PadStatus & PADLdown) {
					if (TimeoutStart > 1) {
						if (padPressed != PADLdown + PADRup) {	
						TimeoutStart--;
						padPressedCount = 0;
						}
						if (padPressedCount >= 32)	padPressedCount = 30;
						if (padPressedCount == 30)	TimeoutStart--;
						padPressedCount += 1;
					}
					padPressed = PADLdown + PADRup;
					#if DEBUG
					printf("Timeout is now %i frames\n", TimeoutStart);
					#endif
				}
				if (PadStatus & PADLright) {
					if (TimeoutStart < 590) {
						if (padPressed != PADLright + PADRup) {	
						TimeoutStart+=10;
						padPressedCount = 0;
						}
						if (padPressedCount >= 32)	padPressedCount = 30;
						if (padPressedCount == 30)	TimeoutStart+=10;
						padPressedCount += 1;
					} else if (TimeoutStart < 600) {
						TimeoutStart = 600;
						padPressedCount += 1;
					}
					padPressed = PADLright + PADRup;
					#if DEBUG
					printf("Timeout is now %i frames\n", TimeoutStart);
					#endif
				}
				if (PadStatus & PADLleft) {
					if (TimeoutStart > 11) {
						if (padPressed != PADLleft + PADRup) {	
						TimeoutStart-=10;
						padPressedCount = 0;
						}
						if (padPressedCount >= 32)	padPressedCount = 30;
						if (padPressedCount == 30)	TimeoutStart-=10;
						padPressedCount += 1;
					} else if (TimeoutStart > 1) {
						TimeoutStart = 1;
						padPressedCount += 1;
					}
					padPressed = PADLleft + PADRup;
					#if DEBUG
					printf("Timeout is now %i frames\n", TimeoutStart);
					#endif
				}
				
				// Select Track Up
				if (PadStatus & PADR1) {
				#if DEBUG
				printf("%i presses, %hi current track, %hi tracks total\n", padPressedCount, curtrk, septrk);
				#endif
					if (curtrk < (septrk - 1)) {
						if (padPressed != PADL1 + PADRup) {
							ParamPtr = ParamFile((u_long)curtrk - 1, (u_long*)MOD_AREA);
							if (ParamPtr->Version != 0) {
								LoadPreParams(ParamPtr);
							}
							curtrk = ChangeTrack(curtrk+1, MusType);
							if (ParamPtr->Version != 0) {
								ChangeFeedback(p.Rfeedback, MusType);
								ChangeDelay(p.Rdelay, MusType);
								if (LoadPostParams(ParamPtr) > 0) {
									ChangeRevMode(p.Rmode, MusType);
									Timeout = TimeoutStart;
								} else {
									ChangeRVol(p.RvolL, p.RvolR, MusType);
									ChangeRDepth(p.RdepthL, p.RdepthR, MusType);
								}
							}
						padPressedCount = 0;
						}
						if (padPressedCount >= 32)	padPressedCount = 30;
						padPressedCount += 1;
					}
					padPressed = PADR1 + PADRup;
				}
				
				// Select Track Down
				if (PadStatus & PADL1) {
				#if DEBUG
				printf("%i presses, %hi current track, %hi tracks total\n", padPressedCount, curtrk, septrk);
				#endif
					if (curtrk > 0) {
						if (padPressed != PADL1 + PADRup) {
							ParamPtr = ParamFile((u_long)curtrk - 1, (u_long*)MOD_AREA);
							if (ParamPtr->Version != 0) {
								LoadPreParams(ParamPtr);
							}
							curtrk = ChangeTrack(curtrk-1, MusType);
							if (ParamPtr->Version != 0) {
								ChangeFeedback(p.Rfeedback, MusType);
								ChangeDelay(p.Rdelay, MusType);
								if (LoadPostParams(ParamPtr) > 0) {
									ChangeRevMode(p.Rmode, MusType);
									Timeout = TimeoutStart;
								} else {
									ChangeRVol(p.RvolL, p.RvolR, MusType);
									ChangeRDepth(p.RdepthL, p.RdepthR, MusType);
								}
							}
						padPressedCount = 0;
						}
						if (padPressedCount >= 32)	padPressedCount = 30;
						padPressedCount += 1;
					}
					padPressed = PADL1 + PADRup;
				}
				//#endif
			} else if (PadStatus & PADRleft) {
				// Select Delay Up
				if (PadStatus & PADLup) {
				#if DEBUG
				printf("%i presses, %hi delay\n", padPressedCount, p.Rdelay);
				#endif
					if (p.Rdelay < 127 && Timeout == 0) {
						if (padPressed != PADLup + PADRleft)	{
						p.Rdelay = ChangeDelay(p.Rdelay + 1, MusType);
						padPressedCount = 0;
						}
						if (padPressedCount >= 32) padPressedCount = 30;
						if (padPressedCount == 30) p.Rdelay = ChangeDelay(p.Rdelay + 1, MusType);
						padPressedCount += 1;
					}
					padPressed = PADLup + PADRleft;
				}
				
				// Select Delay Down
				if (PadStatus & PADLdown) {
				#if DEBUG
				printf("%i presses, %hi delay\n", padPressedCount, p.Rdelay);
				#endif
					if (p.Rdelay > 0 && Timeout == 0) {
						if (padPressed != PADLdown + PADRleft) {	
						p.Rdelay = ChangeDelay(p.Rdelay - 1, MusType);
						padPressedCount = 0;
						}
						if (padPressedCount >= 32)	padPressedCount = 30;
						if (padPressedCount == 30)	p.Rdelay = ChangeDelay(p.Rdelay - 1, MusType);
						padPressedCount += 1;
					}
					padPressed = PADLdown + PADRleft;
				}
			
				// Select Feedback Up
				if (PadStatus & PADLright) {
				#if DEBUG
				printf("%i presses, %hi feedback\n", padPressedCount, p.Rfeedback);
				#endif
					if (p.Rfeedback < 127 && Timeout == 0) {
						if (padPressed != PADLright + PADRleft)	{
						p.Rfeedback = ChangeFeedback(p.Rfeedback + 1, MusType);
						padPressedCount = 0;
						}
						if (padPressedCount >= 32) padPressedCount = 30;
						if (padPressedCount == 30) p.Rfeedback = ChangeFeedback(p.Rfeedback + 1, MusType);
						padPressedCount += 1;
					}
					padPressed = PADLright + PADRleft;
				}
				
				// Select Feedback Down
				if (PadStatus & PADLleft) {
				#if DEBUG
				printf("%i presses, %hi feedback\n", padPressedCount, p.Rfeedback);
				#endif
					if (p.Rfeedback > 0 && Timeout == 0) {
						if (padPressed != PADLright + PADRleft) {	
						p.Rfeedback = ChangeFeedback(p.Rfeedback + 1, MusType);
						padPressedCount = 0;
						}
						if (padPressedCount >= 32)	padPressedCount = 30;
						if (padPressedCount == 30)	p.Rfeedback = ChangeFeedback(p.Rfeedback - 1, MusType);
						padPressedCount += 1;
					}
					padPressed = PADLright + PADRleft;
				}
			
				// Select Reverb Type
				if (PadStatus & PADRright) {
					if (padPressed != PADRright + PADRleft) {
						padPressedCount=0;
						p.Rmode = ChangeRevMode((p.Rmode + 1) % 10, MusType);
						//printf("%hi %i\n", p.Rmode, p.Rmode);
						Timeout = TimeoutStart;
					}
					padPressedCount += 1;
					padPressed = PADRright + PADRleft;
					#if DEBUG
					printf("%hi reverb type set\n", p.Rmode);
					#endif
				}
				
			
				// Select Rev Volume Up
				if (PadStatus & PADR2 && Timeout == 0) {
				#if DEBUG
				printf("%i presses, %hi-%hi current volume\n", padPressedCount, p.RvolL, p.RvolR);
				#endif
					if (padPressed != PADR2 + PADRleft)	{
					IncRVols(MusType);
					padPressedCount = 0;
					}
					if (padPressedCount >= 32) padPressedCount = 30;
					if (padPressedCount == 30) IncRVols(MusType);
					padPressedCount += 1;
					padPressed = PADR2 + PADRleft;
				}
				
				// Select Rev Volume Down
				if (PadStatus & PADL2 && Timeout == 0) {
				#if DEBUG
				printf("%i presses, %hi-%hi current volume\n", padPressedCount, p.RvolL, p.RvolR);
				#endif
					if (padPressed != PADL2 + PADRleft) {	
					DecRVols(MusType);
					padPressedCount = 0;
					}
					if (padPressedCount >= 32)	padPressedCount = 30;
					if (padPressedCount == 30)	DecRVols(MusType);
					padPressedCount += 1;
					padPressed = PADL2 + PADRleft;
				}
				
				// Select Rev Depth Up
				if (PadStatus & PADR1 && Timeout == 0) {
				#if DEBUG
				printf("%i presses, %hi-%hi current depth\n", padPressedCount, p.RdepthL, p.RdepthR);
				#endif
					if (padPressed != PADR1 + PADRleft)	{
					IncRDepths(MusType);
					padPressedCount = 0;
					}
					if (padPressedCount >= 32) padPressedCount = 30;
					if (padPressedCount == 30) IncRDepths(MusType);
					padPressedCount += 1;
					padPressed = PADR1 + PADRleft;
				}
				
				// Select Rev Depth Down
				if (PadStatus & PADL1 && Timeout == 0) {
				#if DEBUG
				printf("%i presses, %hi-%hi current depth\n", padPressedCount, p.RdepthL, p.RdepthR);
				#endif
					if (padPressed != PADL1 + PADRleft) {	
					DecRDepths(MusType);
					padPressedCount = 0;
					}
					if (padPressedCount >= 32)	padPressedCount = 30;
					if (padPressedCount == 30)	DecRDepths(MusType);
					padPressedCount += 1;
					padPressed = PADL1 + PADRleft;
				}
				
				if (PadStatus & PADselect) {
					if (padPressed != PADselect + PADRleft) {
						padPressedCount=0;
						SsSetMono();
					}
					padPressedCount += 1;
					padPressed = PADselect + PADRleft;
				}
				if (PadStatus & PADstart) {
					if (padPressed != PADstart + PADRleft) {
						padPressedCount=0;
						SsSetStereo();
					}
					padPressedCount += 1;
					padPressed = PADstart + PADRleft;
				}
			
			} else {
				
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
				
				// Select up 10
				if (PadStatus & PADLleft) {
					if (SelTitle > 9) {
						if (padPressed != PADLleft)	{
						SelTitle -= 10;
						padPressedCount = 0;
						}
						if (padPressedCount >= 32) padPressedCount = 30;
						if (padPressedCount == 30) SelTitle -= 10;
						padPressedCount += 1;
					} else {
						padPressedCount = 0;
						SelTitle = 0;
					}
					padPressed = PADLleft;
				}
				
				// Select down 10
				if (PadStatus & PADLright) {
					if (SelTitle < (NumTitles - 10)) {
						if (padPressed != PADLright) {	
						SelTitle += 10;
						padPressedCount = 0;
						}
						if (padPressedCount >= 32)	padPressedCount = 30;
						if (padPressedCount == 30)	SelTitle += 10;
						padPressedCount += 1;
					} else {
						padPressedCount = 0;
						SelTitle = NumTitles - 1;
					}
					padPressed = PADLright;
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
				
				// Select Volume Up
				if (PadStatus & PADR2) {
				#if DEBUG
				printf("%i presses, %hi-%hi current volume\n", padPressedCount, p.VolL, p.VolR);
				#endif
					if (padPressed != PADR2)	{
					IncVols(MusType);
					padPressedCount = 0;
					}
					if (padPressedCount >= 32) padPressedCount = 30;
					if (padPressedCount == 30) IncVols(MusType);
					padPressedCount += 1;
					padPressed = PADR2;
				}
				
				// Select Volume Down
				if (PadStatus & PADL2) {
				#if DEBUG
				printf("%i presses, %hi-%hi current volume\n", padPressedCount, p.VolL, p.VolR);
				#endif
					if (padPressed != PADL2) {	
					DecVols(MusType);
					padPressedCount = 0;
					}
					if (padPressedCount >= 32)	padPressedCount = 30;
					if (padPressedCount == 30)	DecVols(MusType);
					padPressedCount += 1;
					padPressed = PADL2;
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
					padPressedCount += 1;
					padPressed = PADRright;
					#if DEBUG
					printf("%i presses now\n", padPressedCount);
					#endif
				}
				
				if (PadStatus & PADselect) {
					if (padPressed != PADselect) {
						padPressedCount=0;
						StopMusic(MusType);
						UnloadMusic(MusType);
						LoadMusic(MusType); //clear out spu ram
						UnloadMusic(MusType);
						LSMI = MAX_TITLES + 1;
						MusType = MUSIC_NONE;
					}
					padPressedCount += 1;
					padPressed = PADselect;
					#if DEBUG
					printf("%i presses now\n", padPressedCount);
					#endif
				}
				
				if (PadStatus & PADstart) {
					if (padPressed != PADstart) {
						padPressedCount=0;
						StopMusic(MusType);
						UnloadMusic(MusType);
						MusType = MUSIC_NONE;
						SelTitle = 0;
						LSMI = MAX_TITLES + 1;
						InitTitles("\\PSFMENU\\TITLES.TXT", 0, 0);
					}
					padPressedCount += 1;
					padPressed = PADstart;
					#if DEBUG
					printf("%i presses now\n", padPressedCount);
					#endif
				}
			
			}
			// Start game
			if (PadStatus & PADRdown) {
				if (padPressed != PADRdown) {
					if (PadStatus & PADRup) {
						if (Title[SelTitle].StackAddr == MUSIC_SEQ || ((Title[SelTitle].StackAddr >= SEQ_MIN) && (Title[SelTitle].StackAddr <= SEQ_MAX))) {
							if (Title[SelTitle].StackAddr == MUSIC_SEQ) {
								CDRF(Title[SelTitle].ExecFile, (u_long*)MOD_AREA, Title[SelTitle].SectorStart, Title[SelTitle].SectorLength);
								CdReadSync(0, 0);
								ParamPtr = ParamFile(0, (u_long*)MOD_AREA);
								LSMI = MAX_TITLES + 1;
							} else if (LSMI <= MAX_TITLES && Title[LSMI].SectorStart == Title[SelTitle].SectorStart && Title[LSMI].SectorLength == Title[SelTitle].SectorLength && str_n_cmp(Title[LSMI].ExecFile, Title[SelTitle].ExecFile, 52) == 0) {
								ParamPtr = ParamFile(Title[SelTitle].StackAddr - SEQ_MIN, (u_long*)MOD_AREA);
								LSMI = SelTitle;
							} else {
								CDRF(Title[SelTitle].ExecFile, (u_long*)MOD_AREA, Title[SelTitle].SectorStart, Title[SelTitle].SectorLength);
								CdReadSync(0, 0);
								ParamPtr = ParamFile(Title[SelTitle].StackAddr - SEQ_MIN, (u_long*)MOD_AREA);
								LSMI = SelTitle;
							}
							if (ParamPtr->Version != 0) {
								LoadPreParams(ParamPtr);
							}
							if (ParamPtr->Version != 0) {
								ChangeFeedback(p.Rfeedback, MusType);
								ChangeDelay(p.Rdelay, MusType);
								if (LoadPostParams(ParamPtr) > 0) {
									ChangeRevMode(p.Rmode, MusType);
									Timeout = TimeoutStart;
								} else {
									ChangeRVol(p.RvolL, p.RvolR, MusType);
									ChangeRDepth(p.RdepthL, p.RdepthR, MusType);
								}
							}
						}
					} else {
						if ((Title[SelTitle].StackAddr >= SEP_MIN) && (Title[SelTitle].StackAddr <= SEP_MAX)) {
							if (LSMI <= MAX_TITLES && Title[LSMI].SectorStart == Title[SelTitle].SectorStart && Title[LSMI].SectorLength == Title[SelTitle].SectorLength && str_n_cmp(Title[LSMI].ExecFile, Title[SelTitle].ExecFile, 52) == 0) {
								#if DEBUG
								printf("Multitrack correct, switching track to %i\n", Title[SelTitle].StackAddr - SEP_MIN);
								#endif
								curtrk = ChangeTrack(Title[SelTitle].StackAddr - SEP_MIN, MUSIC_SEP);
							} else {
								#if DEBUG
								printf("Multitrack incorrect, switching track to %i\n", Title[SelTitle].StackAddr - SEP_MIN);
								#endif
								StopMusic(MusType);
								if (MusType != MUSIC_SEP) {
									#if DEBUG
									printf("Changing music type to SEP\n");
									#endif
									UnloadMusic(MusType);
									LoadMusic(MUSIC_SEP);
									MusType = MUSIC_SEP;
									
								}
								LoadSep(Title[SelTitle].ExecFile, (u_long*)MOD_AREA, Title[SelTitle].SectorStart, Title[SelTitle].SectorLength, (short)(Title[SelTitle].StackAddr - SEP_MIN));
								LSMI = SelTitle;
							}
						} else if ((Title[SelTitle].StackAddr >= SEQ_MIN) && (Title[SelTitle].StackAddr <= SEQ_MAX)) {
							if (LSMI <= MAX_TITLES && Title[LSMI].SectorStart == Title[SelTitle].SectorStart && Title[LSMI].SectorLength == Title[SelTitle].SectorLength && str_n_cmp(Title[LSMI].ExecFile, Title[SelTitle].ExecFile, 52) == 0) {
								#if DEBUG
								printf("Multitrack correct. Switching track to %i\n", Title[SelTitle].StackAddr - SEQ_MIN);
								#endif
								ParamPtr = ParamFile(Title[SelTitle].StackAddr - SEQ_MIN, (u_long*)MOD_AREA);
								if (ParamPtr->Version != 0 && !(PadStatus & PADRleft)) {
									LoadPreParams(ParamPtr);
								}
								curtrk = ChangeTrack(Title[SelTitle].StackAddr - SEQ_MIN, MUSIC_SEQ);
								if (ParamPtr->Version != 0 && !(PadStatus & PADRleft)) {
									ChangeFeedback(p.Rfeedback, MusType);
									ChangeDelay(p.Rdelay, MusType);
									if (LoadPostParams(ParamPtr) > 0) {
										ChangeRevMode(p.Rmode, MusType);
										Timeout = TimeoutStart;
									} else {
										ChangeRVol(p.RvolL, p.RvolR, MusType);
										ChangeRDepth(p.RdepthL, p.RdepthR, MusType);
									}
								}
							} else {
								#if DEBUG
								printf("Multitrack incorrect. Switching track to %i\n", Title[SelTitle].StackAddr - SEQ_MIN);
								#endif
								StopMusic(MusType);
								CDRF(Title[SelTitle].ExecFile, (u_long*)MOD_AREA, Title[SelTitle].SectorStart, Title[SelTitle].SectorLength);
								CdReadSync(0, 0);
								ParamPtr = ParamFile(Title[SelTitle].StackAddr - SEQ_MIN, (u_long*)MOD_AREA);
								#if DEBUG
									printf("Paramater Pointer: %p\n", ParamPtr);
								#endif
								if (ParamPtr->Version != 0 && !(PadStatus & PADRleft)) {
									LoadPreParams(ParamPtr);
								}
								if (MusType != MUSIC_SEQ) {
									#if DEBUG
									printf("Changing music type to SEQ\n");
									#endif
									UnloadMusic(MusType);
									LoadMusic(MUSIC_SEQ);
									MusType = MUSIC_SEQ;
									
								}
								
								LoadSeq((u_long*)MOD_AREA, (short)(Title[SelTitle].StackAddr - SEQ_MIN), SeqParamCount((u_long*)MOD_AREA));
								if (ParamPtr->Version != 0 && !(PadStatus & PADRleft)) {
									ChangeFeedback(p.Rfeedback, MusType);
									ChangeDelay(p.Rdelay, MusType);
									if (LoadPostParams(ParamPtr) > 0) {
										ChangeRevMode(p.Rmode, MusType);
										Timeout = TimeoutStart;
									} else {
										ChangeRVol(p.RvolL, p.RvolR, MusType);
										ChangeRDepth(p.RdepthL, p.RdepthR, MusType);
									}
								}
								//Timeout = TimeoutStart;
								LSMI = SelTitle;
							}
						} else switch (Title[SelTitle].StackAddr) {
							case MUSIC_NONE:
							case MUSIC_MOD:
							case MUSIC_SEQ:
							case MUSIC_SEP:
								#if DEBUG
								printf("chosen: %s %i\n", Title[SelTitle].ExecFile, Title[SelTitle].StackAddr);
								#endif
								if (PadStatus & PADRleft) {
									MusType = StartMusic(&Title[SelTitle], MusType, 0);
								} else { 
									MusType = StartMusic(&Title[SelTitle], MusType, 1);
								}
								MusPlaying = true;
								LSMI = MAX_TITLES + 1;
								Timeout = TimeoutStart;
								break;
							case MENU_TXT:
								StopMusic(MusType);
								UnloadMusic(MusType);
								MusType = MUSIC_NONE;
								sprintf(StringBuff, "%s", Title[SelTitle].ExecFile);
								SelTitle = 0;
								LSMI = MAX_TITLES + 1;
								InitTitles(StringBuff, Title[SelTitle].SectorStart, Title[SelTitle].SectorLength);
								break;
							case MENU_VFS:
								StopMusic(MusType);
								UnloadMusic(MusType);
								MusType = MUSIC_NONE;
								sprintf(StringBuff, "%s", Title[SelTitle].ExecFile);
								SelTitle = 0;
								LSMI = MAX_TITLES + 1;
								InitVfs(StringBuff);
								break;
							default:
								TitleChosen = true;
								TransCount = 0;
								break;
						}
						padPressed = PADRdown;
						padPressedCount = 0;
					}
				} else {
					padPressedCount++;
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
		
		if (Timeout == 1) {
			//SsUtReverbOn();
			#if DEBUG
			printf("Reverb timeout done\n");
			#endif
			ChangeRVol(p.RvolL, p.RvolR, MusType);
			ChangeRDepth(p.RdepthL, p.RdepthR, MusType);
		}
		if (Timeout > 0) Timeout--;
		
		
		
		// Display everything
		Display();
		
		
		// Break once transition is completed when a title is chosen
		if ((TitleChosen) && (TransState == 0)) break;
		
	}
	
	
	// Stop the music
	//MOD_Stop();
	//MOD_Free();
	StopMusic(MusType);
	UnloadMusic(MusType);
	MusType = MUSIC_NONE;
	
	// Begin loading the EXE file in the background and get its parameters
	sprintf(StringBuff, "%s;1", Title[SelTitle].ExecFile);
	sprintf(NameBuff, "%s", Title[SelTitle].Name);
	if (LoadEXEfile(StringBuff, &ExeParams, Title[SelTitle].SectorStart, Title[SelTitle].SectorLength) == 0) LoadError=true;
	
	
	// Display the title of the game being loaded
	ClearRed = ClearGrn = ClearBlu = 0;
	for (i=0; i<127; i+=2) {
		PrepDisplay();
		if (LoadError == false) {
			fPrint("Now Playing", CENTERED, 220, i, &myOT[ActiveBuffer], FontTIM);
			fPrint(NameBuff, CENTERED, 240, i, &myOT[ActiveBuffer], FontTIM);
		} else {
			fPrint("ERROR! Cannot find file:", CENTERED, 220, i, &myOT[ActiveBuffer], FontTIM);
			fPrint(StringBuff, CENTERED, 240, i, &myOT[ActiveBuffer], FontTIM);
		}
		Display();
	}
	
	// This is to prevent the text from flickering on interlaced displays
	for (i=0; i<2; i+=1) {
		PrepDisplay();
		if (LoadError == false) {
			fPrint("Now Playing", CENTERED, 220, 128, &myOT[ActiveBuffer], FontTIM);
			fPrint(NameBuff, CENTERED, 240, 128, &myOT[ActiveBuffer], FontTIM);
		} else {
			fPrint("ERROR! Cannot find file:", CENTERED, 220, 128, &myOT[ActiveBuffer], FontTIM);
			fPrint(StringBuff, CENTERED, 240, 128, &myOT[ActiveBuffer], FontTIM);
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
int LoadEXEfile(char *FileName, struct EXEC *Params, u_long ssect, u_long nsect) {
	
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
		printf("File not found: %s\n", FileName);
		#endif
		return(0);
	}
	
	#if DEBUG
	printf("Found it!\n");
	#endif
	if (ssect > 0) {
		CdIntToPos(CdPosToInt(&File.pos)+ssect, &File.pos);
	}
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

	if (nsect > 1) {
		#if DEBUG
		printf("CD Position set, loading %i sectors...",nsect);
		#endif
		CdRead(nsect - 1, (void*)Params->t_addr, Mode);
	} else {
		#if DEBUG
		printf("CD Position set, loading rest of file...");
		#endif
		CdRead(((File.size - 2048)+2047)/2048, (void*)Params->t_addr, Mode);
	}
	#if DEBUG
	printf("CdRead started...");
	#endif
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
	//CdReadSync(0, 0);
	
	
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
	#if DEBUG
	printf("Title array size: %i Title array location: %X\n", MENU_SIZE, Title);
	#endif
	// Load the menu graphics and title entries
	LoadGraphics("\\PSFMENU\\GRAPHICS.QLP", 0, 0);
	InitTitles("\\PSFMENU\\TITLES.TXT", 0, 0);
	
	// Init controller
	PadInit(0);
	
}
void LoadGraphics(char* gfxfile, u_long ssect, u_long nsect) {
	
	// Load the QLP pack and upload all the TIMs inside it onto VRAM
	#if DEBUG
	printf("Loading %s...", gfxfile);
	#endif
	//CdReadFile(gfxfile, (u_long*)TEMP_AREA, 0);
	CDRF(gfxfile, (u_long*)TEMP_AREA, ssect, nsect);
	CdReadSync(0, 0);
	
	FontTIM			= LoadTIM(QLPfilePtr((u_long*)TEMP_AREA, 0));
	BannerTIM		= LoadTIM(QLPfilePtr((u_long*)TEMP_AREA, 1));
	BigCircleTIM	= LoadTIM(QLPfilePtr((u_long*)TEMP_AREA, 2));
	SmallCircleTIM	= LoadTIM(QLPfilePtr((u_long*)TEMP_AREA, 3));
	
	#if DEBUG
	printf("Done.\n");
	printf("FONT ADDR: %X\n",QLPfilePtr((u_long*)TEMP_AREA, 0));
	#endif
	
}

void InitVfs(char* vfsfile) {
	typedef struct {
		char	name[64];
		u_long	size;
		u_long	addr;
		u_long	sector_size;
		u_long	byte_addr;
		u_long	stack;
	} VFSFILE;
	CdlFILE cdf;
	int sect, titlenum;
	int i;
	VFSFILE* titles = (VFSFILE*)TITLE_AREA;
	sprintf(NameBuff, "%s;1", vfsfile);
	//CDRF(vfsfile, (u_long*)TEMP_AREA, 0, 1);
	if (CdSearchFile(&cdf, NameBuff) == 0) {
		printf("VFS not found: %s\n", NameBuff);
		return;
	}
	CdControl(CdlSetloc, (u_char*)&cdf.pos, 0);
	CdRead(1, (u_long*)TEMP_AREA, CdlModeSpeed);
	CdReadSync(0, 0);
	sect = *((u_long*)TEMP_AREA + 2);
	titlenum = *((u_long*)TEMP_AREA + 1);
	#if DEBUG
	printf("VFS: %s Load Location: %x\n",vfsfile,TEMP_AREA);
	printf("VFS Sectors to load: %i Titles in VFS:%i\n", sect, titlenum);
	#endif
	if (sect > 1) {
		CdIntToPos(CdPosToInt(&cdf.pos) + 1, &cdf.pos);
		CdControl(CdlSetloc, (u_char*)&cdf.pos, 0);
		CdRead(sect - 1, (u_long*)CONT_AREA, CdlModeSpeed);
		CdReadSync(0, 0);
	}
	for (i=0; i<titlenum; i++) {
		Title[i].StackAddr = titles[i].stack;
		Title[i].SectorStart = titles[i].addr;
		Title[i].SectorLength = titles[i].sector_size;
		sprintf(Title[i].Name, "%s", titles[i].name);
		sprintf(Title[i].ExecFile, "%s", vfsfile);
		//#if DEBUG
		//printf("Title Area: %X\n", titles);
		//printf("Title %i - Stack %x - Start Sector %i - Size %i\nName %s\nFilename%s\n",i,Title[i].StackAddr,Title[i].SectorStart,Title[i].SectorLength,Title[i].Name,Title[i].ExecFile);
		//#endif
	}
	NumTitles = titlenum;
	
}

void InitTitles(char* titlefile, u_long ssect, u_long nsect) {
	
	
	int		i=0,InQuote=0,TitleNum=0,GrabStep=0,Separator=0,CharNum=0,j=0;
	
	char	AddrText[16]={0};
	//char	TextBuff[LISTFILE_MAXSIZE]={0};
	char*	TextBuff=(char*)TEMP_AREA;
	
	#if DEBUG
	printf("Loading %s...", titlefile);
	#endif
	
	// Load LIST.TXT
	//CdReadFile(titlefile, (u_long*)TextBuff, 0);
	CDRF(titlefile, (u_long*)TextBuff, ssect, nsect);
	CdReadSync(0, 0);
	
	for (j=0;j<52;j++) {
		Title[0].ExecFile[j] = '\0';
	}
	for (j=0;j<64;j++) {
		Title[0].Name[j] = '\0';
	}

	
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
			/*
				if (GrabStep == 0) {
					GrabStep = 1;
				} else if (GrabStep == 1) {
					GrabStep = 2;
				} else {
					GrabStep = 0;
					Title[TitleNum].StackAddr = hex2int(AddrText);
					TitleNum += 1;
				}
			*/	
				switch (GrabStep) {
					case 0:
						GrabStep = 1;
						break;
					case 1:
						GrabStep = 2;
						break;
					case 2:
						GrabStep = 0;
						Title[TitleNum].StackAddr = hex2int(AddrText);
						TitleNum += 1;
						for (j=0;j<52;j++) {
							Title[TitleNum].ExecFile[j] = '\0';
						}
						for (j=0;j<64;j++) {
							Title[TitleNum].Name[j] = '\0';
						}
						Title[TitleNum].SectorStart = 0;
						Title[TitleNum].SectorLength = 0;
						break;
					default:
						break;
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

u_long StartMusic (TITLESTRUCT* file, u_long currenttype, int PadStatus) {

	StopMusic (currenttype);
	if (file->StackAddr != currenttype) {
		UnloadMusic(currenttype);
		LoadMusic(file->StackAddr);
	}
	ChangeMusic(file, PadStatus);
	return file->StackAddr;

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

int ChangeMusic (TITLESTRUCT* file, int PadStatus) {
	//int strackvol;
	PARAMS_HEADER* ParamPtr = &ParamsNull;	
		switch (file->StackAddr) {
		case MUSIC_NONE:
			return 0;
			break;
		case MUSIC_MOD:
			CDRF(file->ExecFile, (u_long*)MOD_AREA, file->SectorStart, file->SectorLength);
			CdReadSync(0, 0);
			MOD_Load((u_char*)MOD_AREA);
			MOD_Start();
			return 0;
			break;
		case MUSIC_SEQ:
			CDRF(file->ExecFile, (u_long*)MOD_AREA, file->SectorStart, file->SectorLength);
			CdReadSync(0, 0);
			ParamPtr = ParamFile(0, (u_long*)MOD_AREA);
			#if DEBUG
			printf("params load ver %hi\n", ParamPtr->Version);
			#endif
			if (ParamPtr->Version != 0 && PadStatus == 1) {
				LoadPreParams(ParamPtr);
			}
			LoadSeq((u_long*)MOD_AREA, 0, SeqParamCount((u_long*)MOD_AREA));
			if (ParamPtr->Version != 0 && PadStatus == 1) {
				ChangeFeedback(p.Rfeedback, MUSIC_SEQ);
				ChangeDelay(p.Rdelay, MUSIC_SEQ);
				if (LoadPostParams(ParamPtr) > 0) {
					ChangeRevMode(p.Rmode, MUSIC_SEQ);
					//Timeout = TimeoutStart;
				} else {
					ChangeRVol(p.RvolL, p.RvolR, MUSIC_SEQ);
					ChangeRDepth(p.RdepthL, p.RdepthR, MUSIC_SEQ);
				}			}
			return 0;
			break;
		case MUSIC_SEP:
			return LoadSep(file->ExecFile, (u_long*)MOD_AREA, file->SectorStart, file->SectorLength, 0);
			break;
		default:
			return 1;
		}
	return -1;
}

int LoadSep (char* name, u_long* addr, u_long ssect, u_long nsect, short ptrack) {
	CDRF(name, addr, ssect, nsect);
	CdReadSync(0, 0);
	vab1 = SsVabOpenHead ((unsigned char*)QLPfilePtr(addr, 1), -1);
	#if DEBUG
		if( vab1 == -1 ) {
			printf("Failed to open VH\n");
		}
	#endif
	vab1 = SsVabTransBody ((unsigned char*)QLPfilePtr(addr, 2), vab1);
	#if DEBUG
		if( vab1 == -1 ) {
			printf("Failed to open VB\n");
		}
	#endif
	SsVabTransCompleted (SS_WAIT_COMPLETED);
	SsStart();
	septrk = ((short)*QLPfilePtr(addr, 3));
	#if DEBUG
	printf("SEP Track Number Loaded to %X with %hi tracks\n",QLPfilePtr(addr, 3),septrk);
	//septrk=3;
	#endif
	sep1 = SsSepOpen ((unsigned long*)QLPfilePtr(addr, 0), vab1, septrk);
	#if DEBUG
	printf("SEP Opened at %X\n",QLPfilePtr(addr, 0));
	#endif
	SsSetMVol (p.MvolL, p.MvolR);
	for (curtrk = 0; curtrk < septrk; curtrk++) {
		SsSepSetVol (sep1, curtrk, p.VolL, p.VolR);
		#if DEBUG
		printf("SEP Track %hi Volume Set\n",curtrk);
		#endif
	}
	curtrk = ptrack;
	SsSepPlay(sep1, curtrk, SSPLAY_PLAY, (short)p.SeqLoops);
	return 0;
}

short LoadSeq (u_long* addr, short ptrack, int extfiles) {
	//SeqParamCount (addr);
	vab1 = SsVabOpenHead ((unsigned char*)QLPfilePtr(addr, (QLPfileCount(addr) - 2) - extfiles), -1);
	#if DEBUG
		if( vab1 == -1 ) {
		printf("Failed to open VH!\n");
		}
	#endif
	vab1 = SsVabTransBody ((unsigned char*)QLPfilePtr(addr, (QLPfileCount(addr) - 1) - extfiles), vab1);
	#if DEBUG
		if( vab1 == -1 ) {
		printf("Failed to open VB!\n");
		}
	#endif
	SsVabTransCompleted (SS_WAIT_COMPLETED);
	SsStart2();
	seq1 = SsSeqOpen ((unsigned long*)QLPfilePtr(addr, ptrack), vab1);
	SsSetMVol (p.MvolL, p.MvolR);
	SsSeqSetVol (seq1, p.VolL, p.VolR);
	SsSeqPlay(seq1, SSPLAY_PLAY, (short)p.SeqLoops);
	SsUtReverbOn();
	septrk = (QLPfileCount(addr) - 2) - extfiles;
	curtrk = ptrack;
	#if DEBUG
		printf("Tracks Total: %hi Track Selected: %hi File Count: %i\n", septrk, curtrk, QLPfileCount(addr));
	#endif
	return 0;
}

int str_n_cmp (char* str1, char* str2, int len) {
	int i;
	for (i = 0; i < len; i++) {
		if (str1[i] != str2[i]) {
			return i;
		} else if (str1[i] == 0x00) {
			return 0;
		}
	}
	return 0;
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
			SsSetTableSize (seq_table, 2, 16);
			SsSetTickMode (p.TickMode);
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
			//SsUtReverbOn();
			return 1;
			break;
		case MUSIC_SEP:
			SsSepReplay(sep1, curtrk);
			//SsUtReverbOn();
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
			//SsUtReverbOff();
			return 0;
			break;
		case MUSIC_SEP:
			SsSepPause(sep1, curtrk);
			//SsUtReverbOff();
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
			return nowtrack;
			break;
		case MUSIC_SEQ:
			SsSeqClose (seq1);
			seq1 = SsSeqOpen ((unsigned long*)QLPfilePtr((u_long*)MOD_AREA, nowtrack), vab1);
			SsSetMVol (p.MvolL, p.MvolR);
			SsSeqSetVol (seq1, p.VolL, p.VolR);
			SsSeqPlay(seq1, SSPLAY_PLAY, (short)p.SeqLoops);
			return nowtrack;
			break;
		case MUSIC_SEP:
			SsSepStop(sep1, curtrk);
			SsSepPlay(sep1, nowtrack, SSPLAY_PLAY, (short)p.SeqLoops);
			return nowtrack;
			break;
		default:
			return nowtrack;
			break;
		}
	return nowtrack;
}

short ChangeVol (short nowvolL, short nowvolR, u_long filetype) {
	switch (filetype) {
		case MUSIC_NONE:
		case MUSIC_MOD:
			return (nowvolL + nowvolR) / 2;
			break;
		case MUSIC_SEQ:
			SsSeqSetVol (seq1, nowvolL, nowvolR);
			return (nowvolL + nowvolR) / 2;
			break;
		case MUSIC_SEP:
			SsSepSetVol (sep1, curtrk, nowvolL, nowvolR);
			return (nowvolL + nowvolR) / 2;
			break;
		default:
			return (nowvolL + nowvolR) / 2;
			break;
		}
	return (nowvolL + nowvolR) / 2;
}

short ChangeRVol (short nowvolL, short nowvolR, u_long filetype) {
	switch (filetype) {
		case MUSIC_NONE:
		case MUSIC_MOD:
			return (nowvolL + nowvolR) / 2;
			break;
		case MUSIC_SEQ:
		case MUSIC_SEP:
			SsSetRVol (nowvolL, nowvolR);
			return (nowvolL + nowvolR) / 2;
			break;
		default:
			return (nowvolL + nowvolR) / 2;
			break;
		}
	return (nowvolL + nowvolR) / 2;
}

int CDRF(char* file, u_long *addr, u_long startsect, u_long nsect) {
	CdlFILE cdlf;
	if (startsect == 0) {
		return CdReadFile(file, addr, nsect * 2048);
	} else {
		sprintf(StringBuff, "%s;1", file);
		if (CdSearchFile(&cdlf, StringBuff) == 0) {
			printf("Subfile not found: %s\n", file);
			return -1;
		}
		CdIntToPos(CdPosToInt(&cdlf.pos) + startsect, &cdlf.pos);
		CdControl(CdlSetloc, (u_char*)&cdlf.pos, 0);
		return CdRead(nsect, addr, CdlModeSpeed);
	}
}

PARAMS_V1 ParamsV1ToDefault() {
	PARAMS_V1 par;
	par.SeqNum = 0;
	par.Version = 1;
	par.MvolL = 127;
	par.MvolR = 127;
	par.VolL = 127;
	par.VolR = 127;
	par.RvolL = 64;
	par.RvolR = 64;
	par.RdepthL = 64;
	par.RdepthR = 64;
	par.Rdelay = 64;
	par.Rmode = 0; //SPU_REV_MODE_OFF
	par.Rfeedback = 64;
	par.TickMode = SS_TICK240;
	par.SeqLoops = 1; //looping songs will loop forever even with infinite play off
	par.SeqType = 0;
	return par;
}

short IncVols (u_long MusType) {
	if (p.VolL < 127) p.VolL++;
	if (p.VolR < 127) p.VolR++;
	return ChangeVol(p.VolL, p.VolR, MusType);
}

short DecVols (u_long MusType) {
	if (p.VolL > 0) p.VolL--;
	if (p.VolR > 0) p.VolR--;
	return ChangeVol(p.VolL, p.VolR, MusType);
}

short IncRVols (u_long MusType) {
	if (p.RvolL < 127) p.RvolL++;
	if (p.RvolR < 127) p.RvolR++;
	return ChangeRVol(p.RvolL, p.RvolR, MusType);
}

short DecRVols (u_long MusType) {
	if (p.RvolL > 0) p.RvolL--;
	if (p.RvolR > 0) p.RvolR--;
	return ChangeRVol(p.RvolL, p.RvolR, MusType);
}

short IncRDepths (u_long MusType) {
	if (p.RdepthL < 127) p.RdepthL++;
	if (p.RdepthR < 127) p.RdepthR++;
	return ChangeRDepth(p.RdepthL, p.RdepthR, MusType);
}

short DecRDepths (u_long MusType) {
	if (p.RdepthL > 0) p.RdepthL--;
	if (p.RdepthR > 0) p.RdepthR--;
	return ChangeRDepth(p.RdepthL, p.RdepthR, MusType);
}


short ChangeRevMode (short revmode, u_long filetype) {	
	switch (filetype) {
		case MUSIC_NONE:
			return revmode;
			break;
		case MUSIC_MOD:
			return revmode;
			break;
		case MUSIC_SEP:
		case MUSIC_SEQ:
			SsUtSetReverbType(revmode);
			return revmode;
			break;
		default:
			return 0;
		}
	return -1;
}

short ChangeRDepth (short nowvolL, short nowvolR, u_long filetype) {
	switch (filetype) {
		case MUSIC_NONE:
		case MUSIC_MOD:
			return (nowvolL + nowvolR) / 2;
			break;
		case MUSIC_SEQ:
		case MUSIC_SEP:
			SsUtSetReverbDepth(nowvolL, nowvolR);
			return (nowvolL + nowvolR) / 2;
			break;
		default:
			return (nowvolL + nowvolR) / 2;
			break;
		}
	return (nowvolL + nowvolR) / 2;
}

short ChangeDelay (short nowfbdel, u_long filetype) {
	switch (filetype) {
		case MUSIC_NONE:
		case MUSIC_MOD:
			return nowfbdel;
			break;
		case MUSIC_SEQ:
		case MUSIC_SEP:
			SsUtSetReverbDelay(nowfbdel);
			return nowfbdel;
			break;
		default:
			return nowfbdel;
			break;
		}
	return nowfbdel;
}

short ChangeFeedback (short nowfbdel, u_long filetype) {
	switch (filetype) {
		case MUSIC_NONE:
		case MUSIC_MOD:
			return nowfbdel;
			break;
		case MUSIC_SEQ:
		case MUSIC_SEP:
			SsUtSetReverbFeedback(nowfbdel);
			return nowfbdel;
			break;
		default:
			return nowfbdel;
			break;
		}
	return nowfbdel;
}

int SeqParamCount (u_long* addr) {
	#if DEBUG
		printf("pBAV = 56414270\nFiles in QLP: %i(%i)\nVH guess for multiparam:%i(%X - %i)\nVH guess for singleparam:%i(%X - %i)\n",
		QLPfileCount(addr), QLPfileCount(addr) & 1,
		(QLPfileCount(addr)/2)-1, *QLPfilePtr(addr, (QLPfileCount(addr)/2)-1), *QLPfilePtr(addr, (QLPfileCount(addr)/2)-1)-0x56414270,
		QLPfileCount(addr)-3, *QLPfilePtr(addr, QLPfileCount(addr)-3), *QLPfilePtr(addr, QLPfileCount(addr)-3)-0x56414270);
	#endif
	if (*QLPfilePtr(addr, QLPfileCount(addr)-3) == 0x56414270) {
		return 1; //single parameter file can mean an odd number of files
	}
	if (QLPfileCount(addr) & 1) {
		return 0; //avoid integer division errors
	}
	if (*QLPfilePtr(addr, (QLPfileCount(addr)/2)-1) == 0x56414270) {
		return (QLPfileCount(addr)/2)-1; //multi parameter will always be even (one parameter per seq + vh and vb)
	}
	return 0;
}

short LoadPreParams(PARAMS_HEADER* addr) {
	switch (addr->Version) {
		case 0:
			return 0;
		case 1:
			p.MvolL = ((PARAMS_V1*)addr)->MvolL;
			p.MvolR = ((PARAMS_V1*)addr)->MvolR;
			p.SeqType = ((PARAMS_V1*)addr)->SeqType;
			//p.TickMode = ((PARAMS_V1*)addr)->TickMode;
			p.SeqNum = ((PARAMS_V1*)addr)->SeqNum;
			p.Version = ((PARAMS_V1*)addr)->Version;
			p.SeqLoops = ((PARAMS_V1*)addr)->SeqLoops;
			return 1;
		default:
			return 0;
	}
	return 0;
}


short LoadPostParams(PARAMS_HEADER* addr) {

	switch (addr->Version) {
		case 0:
			return -1;
		case 1:
		
			p.Version = ((PARAMS_V1*)addr)->Version;	
			p.RvolL = ((PARAMS_V1*)addr)->RvolL;
			p.RvolR = ((PARAMS_V1*)addr)->MvolR;
			p.RdepthL = ((PARAMS_V1*)addr)->RdepthL;
			p.RdepthR = ((PARAMS_V1*)addr)->RdepthR;
			p.Rdelay = ((PARAMS_V1*)addr)->Rdelay;
			p.Rfeedback = ((PARAMS_V1*)addr)->Rfeedback;
			//printf("reverb: %hi new reverb: %hi\n",p.Rmode,((PARAMS_V1*)addr)->Rmode);
			if (((PARAMS_V1*)addr)->Rmode == p.Rmode) {
				p.Rmode = ((PARAMS_V1*)addr)->Rmode;
				return 0;
			} else {
				p.Rmode = ((PARAMS_V1*)addr)->Rmode;
			}
			return 1;
		default:
			return -1;
	}
	return -1;
}

PARAMS_HEADER* ParamFile(u_long stack, u_long *QLP_AREA) {
	int ExtTracks = 0;
	ExtTracks = SeqParamCount(QLP_AREA);
	if (ExtTracks == 1) {
		ExtTracks = QLPfileCount(QLP_AREA) - 1;
	} else if (ExtTracks != 0) {
		ExtTracks += (stack + 2);
		//printf("QLP File Selected: %i\n", ExtTracks);
	} else {
	return &ParamsNull;
	}
	return (PARAMS_HEADER*)QLPfilePtr(QLP_AREA, ExtTracks);
}