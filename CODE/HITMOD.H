/**************************************************************************** 
 *                      HITMOD PlayStation MODPlayer                        * 
 *                              version 1.5                                 * 
 *                           by Silpheed/HITMEN                             *  
 *                                                                          *  
 ****************************************************************************/

void MOD_Init(void);
/**************************************************************************** 
 * MOD_Init(): Call this first to initialise the player and the SPU etc..   * 
 *                                                                          * 
 ****************************************************************************/
 
int MOD_Load(unsigned char*);
/**************************************************************************** 
 * MOD_Load(): Call this with the address of a HITMOD module to prepare it  * 
 *             for playback. Returns 1 for success, 0 for failure           * 
 ****************************************************************************/
 
void MOD_Start(void);
/**************************************************************************** 
 * MOD_Start(): Starts the module playing.                                  * 
 *                                                                          * 
 ****************************************************************************/

void MOD_Stop(void);
/**************************************************************************** 
 * MOD_Stop(): Stops playback. Can resume playing again with MOD_Start().   * 
 *                                                                          * 
 ****************************************************************************/

void MOD_Free(void);
/**************************************************************************** 
 * MOD_Free(): Frees up memory used by MOD_Load, allowing you to load       * 
 *             another module.                                              * 
 ****************************************************************************/

 
extern int *MOD_CurrentPattern;     /* Current pattern being played */
extern int *MOD_CurrentRow;         /* Current row being played */
extern int *MOD_CurrentOrder;       /* Current order being played */

/* the following three are arrays - [0 to No.Channels-1] */
extern int *MOD_Volume;            /* Current sample volume of each channel */
extern int *MOD_Freq;              /* Current frequency (note) of each channel */            
extern int *MOD_LastSample;        /* Last sample played on each channel */
