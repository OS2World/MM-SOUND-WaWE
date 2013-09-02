/*
 * The Warped Wave Editor - an audio editor for OS/2 and eComStation systems
 * Copyright (C) 2004 Doodle
 *
 * Contact: doodle@dont.spam.me.scenergy.dfmk.hu
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 */

/*****************************************************************************
 * Warped Wave Editor                                                        *
 *                                                                           *
 * MPEG Audio import plugin                                                  *
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <process.h>
#include <string.h>

#define INCL_TYPES
#define INCL_WIN
#define INCL_DOSRESOURCES
#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_GPI
#include <os2.h>

#include "interface.h"     // mpglib interface
#include "WaWECommon.h"    // Common defines and structures
#include "IMPA-Resources.h" // Dlg window resources
#define DID_PROGRESSINDICATOR   1000
#define WM_SETPERCENTAGE (WM_USER + 50)

#define DECODEBUFFER_SIZE (1152 * 2 * sizeof(double))
#define ALLOCATEDFRAMESPACE_STEP  128

// There is a bit of hacking implemented here, for quickly parsing the
// mpeg audio file.
// We know the number of samples in audio frames for mpeg versions, so
// we don't have to decode all the audio frames at parsing, but we can guess
// the number of samples we'll get from mpglib.
// The problem is that mpglib doesn't decode the first frame for some resason,
// and sometimes also skips the second.
// Right now, we use mpglib for the first 10 frames, and "guess" for the others!
#define OPTIMIZE_PARSING_FOR_MPGLIB

// It seems that the decoder adds some samples of silence to the beginning
// of the audio stream. We cut there.
// The value is by experience.
// To turn off the feature, define this value to be 0!
#define FIRST_SAMPLES_TO_CUT   1105

typedef struct MP3PluginFrameDesc_t_struct {
  int            iFilePos;   // Where to read from
  int            iFrameSize; // How much to read from there

  WaWEPosition_t StartPCMPos;
  WaWESize_t     PCMBytes;
} MP3PluginFrameDesc_t, *MP3PluginFrameDesc_p;

typedef struct MP3PluginHandle_t_struct {
  FILE *hFile; // File handle from fopen()
  MPSTR mpstrHandle; // MPSTR structure of mpglib

  int iNumOfFrames; // Number of MPEG Audio frames in file
  int iAllocatedFrameSpace; // Allocated space for frames in next pointer
  MP3PluginFrameDesc_p pIndexTable;

  int bID3v1Found;   //
  int bID3v11Found;  //
  char achID3v1_SongTitle[31];
  char achID3v1_Artist[31];
  char achID3v1_Album[31];
  char achID3v1_Year[5];
  char achID3v1_Comment[30];
  char chID3v11_Track;
  char chID3v1_Genre;

  WaWEPosition_t CurrPCMPos;   // Current PCM position (In WaWESample_t's)

  WaWESize_t     FullPCMSize_d;  // Full PCM size (In Double-typed samples)
  WaWESize_t     FullPCMSize_w;  // Full PCM size (In WaWESample_t samples)
} MP3PluginHandle_t, *MP3PluginHandle_p;

typedef struct MP3PluginImporting_t_struct
{
  TID tidWindowThread;
  HWND hwndDlg;
  HWND hwndOwner;
  int iWindowThreadState;
  int iShutdownRequest;
  int iShownPercentage;
} MP3PluginImporting_t, *MP3PluginImporting_p;

// Global variables
// (Per process, because the DLL is initinstance/terminstance!)
HMODULE hmodOurDLLHandle;
WaWEPluginHelpers_t PluginHelpers;
HMTX hmtxUsePlugin; // Mutex semaphore to serialize access to plugin!

#define PLUGIN_NAME "MPEG Audio Import plugin"
char *pchPluginName = PLUGIN_NAME;
char *pchAboutTitle = PLUGIN_NAME;
char *pchAboutMessage =
  "MPEG Audio Import plugin v1.0 for WaWE\n"
  "Written by Doodle\n"
  "\n"
  "Using mpglib for decoding.\n"
  "\n"
  "Compilation date: "__DATE__" "__TIME__;
char *pchProgressIndicatorClassName = "MPAPluginProgressIndicatorClass";

// Some tables from mpglib
const int tabsel_123 [2] [3] [16];

const int tabsel_123 [2] [3] [16] = {
   { {0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,},
     {0,32,48,56, 64, 80, 96,112,128,160,192,224,256,320,384,},
     {0,32,40,48, 56, 64, 80, 96,112,128,160,192,224,256,320,} },

   { {0,32,48,56,64,80,96,112,128,144,160,176,192,224,256,},
     {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,},
     {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,} }
};

const long freqs[9] = { 44100, 48000, 32000,
                        22050, 24000, 16000,
                        11025, 12000,  8000 };

const char *modes[4] = { "Stereo", "Joint-Stereo", "Dual-Channel", "Single-Channel" };
const char *layers[4] = { "Unknown" , "I", "II", "III" };


// Tables from libmp3lame
static const char *const genre_names[] =
{
    /*
     * NOTE: The spelling of these genre names is identical to those found in
     * Winamp and mp3info.
     */
    "Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge",
    "Hip-Hop", "Jazz", "Metal", "New Age", "Oldies", "Other", "Pop", "R&B",
    "Rap", "Reggae", "Rock", "Techno", "Industrial", "Alternative", "Ska",
    "Death Metal", "Pranks", "Soundtrack", "Euro-Techno", "Ambient", "Trip-Hop",
    "Vocal", "Jazz+Funk", "Fusion", "Trance", "Classical", "Instrumental",
    "Acid", "House", "Game", "Sound Clip", "Gospel", "Noise", "Alt. Rock",
    "Bass", "Soul", "Punk", "Space", "Meditative", "Instrumental Pop",
    "Instrumental Rock", "Ethnic", "Gothic", "Darkwave", "Techno-Industrial",
    "Electronic", "Pop-Folk", "Eurodance", "Dream", "Southern Rock", "Comedy",
    "Cult", "Gangsta Rap", "Top 40", "Christian Rap", "Pop/Funk", "Jungle",
    "Native American", "Cabaret", "New Wave", "Psychedelic", "Rave",
    "Showtunes", "Trailer", "Lo-Fi", "Tribal", "Acid Punk", "Acid Jazz",
    "Polka", "Retro", "Musical", "Rock & Roll", "Hard Rock", "Folk",
    "Folk/Rock", "National Folk", "Swing", "Fast-Fusion", "Bebob", "Latin",
    "Revival", "Celtic", "Bluegrass", "Avantgarde", "Gothic Rock",
    "Progressive Rock", "Psychedelic Rock", "Symphonic Rock", "Slow Rock",
    "Big Band", "Chorus", "Easy Listening", "Acoustic", "Humour", "Speech",
    "Chanson", "Opera", "Chamber Music", "Sonata", "Symphony", "Booty Bass",
    "Primus", "Porn Groove", "Satire", "Slow Jam", "Club", "Tango", "Samba",
    "Folklore", "Ballad", "Power Ballad", "Rhythmic Soul", "Freestyle", "Duet",
    "Punk Rock", "Drum Solo", "A Cappella", "Euro-House", "Dance Hall",
    "Goa", "Drum & Bass", "Club-House", "Hardcore", "Terror", "Indie",
    "BritPop", "Negerpunk", "Polsk Punk", "Beat", "Christian Gangsta Rap",
    "Heavy Metal", "Black Metal", "Crossover", "Contemporary Christian",
    "Christian Rock", "Merengue", "Salsa", "Thrash Metal", "Anime", "JPop",
    "Synthpop"
};

#define GENRE_NAME_COUNT \
    ((int)(sizeof genre_names / sizeof (const char *const)))


// Helper functions, called from inside:

void ErrorMsg(char *Msg)
{
#ifdef DEBUG_BUILD
  printf("ErrorMsg: %s\n",Msg); fflush(stdout);
#endif
  WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                Msg, PLUGIN_NAME" Error", 0, MB_OK | MB_MOVEABLE | MB_ICONEXCLAMATION);
}

MRESULT EXPENTRY ProgressIndicatorClassProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  switch (msg)
  {
    case WM_DESTROY:                   // --- Window destroying:
      {
        break;
      }
    case WM_CREATE:                    // --- Window creation:
      {
#ifdef DEBUG_BUILD
//	printf("[ProgressIndicatorClassProc] : WM_CREATE\n"); fflush(stdout);
#endif
        // Initialize storage!
        WinSetWindowULong(hwnd, 0, 0); // Current iPercentage value is 0
        break;
      }
    case WM_SETPERCENTAGE:
      {
#ifdef DEBUG_BUILD
//	printf("[ProgressIndicatorClassProc] : WM_SETPERCENTAGE\n"); fflush(stdout);
#endif
	WinSetWindowULong(hwnd, 0, (ULONG) mp1);
	WinInvalidateRect(hwnd, NULL, FALSE); // Redraw!
        return (MRESULT) TRUE;
      }
    case WM_PAINT:                     // --- Repainting parts of the window
      {
        HPS hpsBeginPaint;
        RECTL rclChangedRect;

#ifdef DEBUG_BUILD
//	printf("[ProgressIndicatorClassProc] : WM_PAINT\n"); fflush(stdout);
#endif

        hpsBeginPaint = WinBeginPaint(hwnd, NULL, &rclChangedRect);
        if (!WinIsRectEmpty(WinQueryAnchorBlock(hwnd), &rclChangedRect))
        {
          RECTL rclFullRect;
          RECTL rclInnerRect;
          int iPercentage;
          LONG rgbColor1, rgbColor2;
          PRGB2 pRGB2;

          // Switch to RGB mode
          GpiCreateLogColorTable(hpsBeginPaint, 0, LCOLF_RGB, 0, 0, NULL);

          // Fill variables
          WinQueryWindowRect(hwnd, &rclFullRect);

          iPercentage = WinQueryWindowULong(hwnd, 0);

          // Redraw the whole control, based on the values we have!
          pRGB2 = (RGB2 *)(&rgbColor1);
          pRGB2->bRed = 80;
          pRGB2->bGreen = 80;
          pRGB2->bBlue =100;
          pRGB2 = (RGB2 *)(&rgbColor2);
          pRGB2->bRed = 200;
          pRGB2->bGreen = 200;
          pRGB2->bBlue = 220;
          WinDrawBorder(hpsBeginPaint, &rclFullRect, 1, 1,
                        (LONG) rgbColor1, (LONG) rgbColor2, DB_INTERIOR | DB_AREAMIXMODE);

          rclInnerRect.xLeft = rclFullRect.xLeft + 1;
          rclInnerRect.yBottom = rclFullRect.yBottom+1;
          rclInnerRect.xRight = rclFullRect.xLeft + 1 +
            iPercentage * (rclFullRect.xRight-rclFullRect.xLeft-2) / 100;
          rclInnerRect.yTop = rclFullRect.yTop-1;

          pRGB2 = (RGB2 *)(&rgbColor1);
          pRGB2->bRed = 10;
          pRGB2->bGreen = 10;
          pRGB2->bBlue = 80;
          pRGB2 = (RGB2 *)(&rgbColor2);
          pRGB2->bRed = 150;
          pRGB2->bGreen = 150;
          pRGB2->bBlue = 200;
          WinDrawBorder(hpsBeginPaint, &rclInnerRect, 1, 1,
                        (LONG) rgbColor1, (LONG) rgbColor2, DB_INTERIOR | DB_AREAMIXMODE);
        }
	WinEndPaint(hpsBeginPaint);
#ifdef DEBUG_BUILD
//	printf("[ProgressIndicatorClassProc] : WM_PAINT done.\n"); fflush(stdout);
#endif
	return (MRESULT) FALSE;
      }
    default:
      break;
  }
  return WinDefWindowProc(hwnd, msg, mp1, mp2);
}

MRESULT EXPENTRY ImportWindowProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  MP3PluginImporting_p pImportWindow;

  switch (msg)
  {
    case WM_INITDLG:
#ifdef DEBUG_BUILD
//      printf("[ImportWindowProc] : Setting QWL_USER to 0x%p\n", (void *) mp2); fflush(stdout);
#endif
      WinSetWindowULong(hwnd, QWL_USER, (ULONG) mp2);
#ifdef DEBUG_BUILD
//      printf("[ImportWindowProc] : Returning.\n"); fflush(stdout);
#endif
      break;
    case WM_COMMAND:
      // Check who sent the WM_COMMAND message!
      switch (SHORT1FROMMP(mp2)) // usSource parameter of WM_COMMAND
      {
        case CMDSRC_PUSHBUTTON:
          // Hmm, it's a pushbutton! Let's see which one!
          switch ((ULONG) mp1)
          {
	    case PB_CANCEL:
	      pImportWindow = (MP3PluginImporting_p) WinQueryWindowULong(hwnd, QWL_USER);
	      if (pImportWindow)
                pImportWindow->iShutdownRequest = 1;
	      return (MPARAM) 1;
	    default:
	      break;
	  }
	  break;
      }

    default:
      break;
  }
  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}


void ImportWindowThreadFunc(void *p)
{
  HAB hab;
  HMQ hmq;
  MP3PluginImporting_p pImportWindow = (MP3PluginImporting_p) p;
  SWP swpDlg, swpParent, swpStatic, swpButton;
  POINTL ptlTemp;

#ifdef DEBUG_BUILD
//  printf("[ImportWindowThreadFunc] : Starting (0x%p).\n", pImportWindow); fflush(stdout);
#endif
  DosSetPriority(PRTYS_THREAD, PRTYC_FOREGROUNDSERVER, +5, 0);
  // Setup this thread for using PM (sending messages etc...)
  hab = WinInitialize(0);
  hmq = WinCreateMsgQueue(hab, 0);

#ifdef DEBUG_BUILD
//  printf("[ImportWindowThreadFunc] : Register class\n"); fflush(stdout);
#endif

  // Create main window
  WinRegisterClass(hab, pchProgressIndicatorClassName, // Register a private class for
		   ProgressIndicatorClassProc,         // progress indicator
		   0, 16); // 16 bytes of private storage should be enough...


#ifdef DEBUG_BUILD
//  printf("[ImportWindowThreadFunc] : Load Dlg window\n"); fflush(stdout);
#endif

  pImportWindow->hwndDlg = WinLoadDlg(HWND_DESKTOP,
                                      pImportWindow->hwndOwner,
                                      ImportWindowProc,
                                      hmodOurDLLHandle,
                                      DLG_PARSINGFILE,
                                      pImportWindow); // Application-specific data to window


  if (!pImportWindow->hwndDlg)
  {
    ErrorMsg("Could not create progress indicator window!");

  } else
  {
    ULONG CXDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
    ULONG CYDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);

#ifdef DEBUG_BUILD
//    printf("[ImportWindowThreadFunc] : WinQueryWindowPos()\n"); fflush(stdout);
#endif

    // Create custom control in window!
    WinQueryWindowPos(WinWindowFromID(pImportWindow->hwndDlg, ST_IMPORTINGMPEGAUDIOFILEPLEASEWAIT), &swpStatic);
    WinQueryWindowPos(WinWindowFromID(pImportWindow->hwndDlg, PB_CANCEL), &swpButton);
    WinQueryWindowPos(pImportWindow->hwndDlg, &swpDlg);

#ifdef DEBUG_BUILD
//    printf("[ImportWindowThreadFunc] : Create custom control\n"); fflush(stdout);
#endif

    WinCreateWindow(pImportWindow->hwndDlg,                    // hwndParent
		    pchProgressIndicatorClassName,   // pszClass
		    "Progress Indicator", // pszName
		    WS_VISIBLE,                 // flStyle
		    CXDLGFRAME*4,                              // x
		    swpButton.y + swpButton.cy + CYDLGFRAME*2, // y
		    swpDlg.cx - CXDLGFRAME*8, // cx
                    swpStatic.y - swpButton.y - swpButton.cy - CYDLGFRAME*4, // cy
		    pImportWindow->hwndDlg,                    // hwndOwner
		    HWND_TOP,                   // hwndInsertBehind
		    DID_PROGRESSINDICATOR,       // id
		    NULL,                       // pCtlData
		    NULL);                      // pPresParams

#ifdef DEBUG_BUILD
//    printf("[ImportWindowThreadFunc] : Centering window\n"); fflush(stdout);
#endif

    // Center dialog in parent
    WinQueryWindowPos(pImportWindow->hwndOwner, &swpParent);
    ptlTemp.x = swpParent.x;
    ptlTemp.y = swpParent.y;
    WinMapWindowPoints(pImportWindow->hwndOwner, HWND_DESKTOP, &ptlTemp, 1);
    {
      // Center dialog box within the parent window
      int ix, iy;
      ix = ptlTemp.x + (swpParent.cx - swpDlg.cx)/2;
      iy = ptlTemp.y + (swpParent.cy - swpDlg.cy)/2;
      WinSetWindowPos(pImportWindow->hwndDlg, HWND_TOP, ix, iy, 0, 0,
		      SWP_MOVE);
    }

    WinSetWindowPos(pImportWindow->hwndDlg, HWND_TOP, 0, 0, 0, 0,
                    SWP_SHOW | SWP_ACTIVATE | SWP_ZORDER);
    WinSetFocus(HWND_DESKTOP, pImportWindow->hwndDlg);

#ifdef DEBUG_BUILD
//    printf("[ImportWindowThreadFunc] : Notification\n"); fflush(stdout);
#endif

    pImportWindow->iWindowThreadState = 1; // Notify other thread that we're ready and running!

#ifdef DEBUG_BUILD
    printf("[ImportWindowThreadFunc] : WinProcessDlg()\n"); fflush(stdout);
#endif

    // Process the dialog!
    WinProcessDlg(pImportWindow->hwndDlg);

#ifdef DEBUG_BUILD
    printf("[ImportWindowThreadFunc] : WinDestroyWindow()\n"); fflush(stdout);
#endif

    WinDestroyWindow(pImportWindow->hwndDlg);

  }

#ifdef DEBUG_BUILD
  printf("[ImportWindowThreadFunc] : Leaving.\n"); fflush(stdout);
#endif

  pImportWindow->iWindowThreadState = -1; // Notify other thread that we're stopped!

  _endthread();
}


static MP3PluginImporting_p internal_CreateImportWindow(HWND hwndOwner)
{
  MP3PluginImporting_p pImportWindow;

  pImportWindow = (MP3PluginImporting_p) malloc(sizeof(MP3PluginImporting_t));
  if (pImportWindow)
  {
    memset(pImportWindow, 0, sizeof(MP3PluginImporting_t));
    pImportWindow->hwndOwner = hwndOwner;
    pImportWindow->tidWindowThread = _beginthread(ImportWindowThreadFunc,
						  NULL,
						  1024*1024,
						  pImportWindow);
    if (pImportWindow->tidWindowThread>0)
    {
      // Thread could be started
      while (!(pImportWindow->iWindowThreadState)) DosSleep(32);
      if (pImportWindow->iWindowThreadState<0)
      {
	// Started, and got an error at the very same time.
	// So, don't create the structure then!
	DosWaitThread(&(pImportWindow->tidWindowThread), DCWW_WAIT);
	memset(pImportWindow, 0, sizeof(MP3PluginImporting_t));
	free(pImportWindow); pImportWindow = NULL;
      }
    }
  }
  return pImportWindow;
}

static void internal_SetImportWindowPercentage(MP3PluginImporting_p pImportWindow, int iPercentage)
{
  if (!pImportWindow) return;

  if (pImportWindow->tidWindowThread)
  {
    if (iPercentage<0) iPercentage = 0;
    if (iPercentage>100) iPercentage = 100;
    if (pImportWindow->iShownPercentage != iPercentage)
    {
#ifdef DEBUG_BUILD
//      printf("[ImportWindowThreadFunc] : Setting percentage to %d\n", iPercentage); fflush(stdout);
#endif

      WinSendDlgItemMsg(pImportWindow->hwndDlg, DID_PROGRESSINDICATOR, WM_SETPERCENTAGE, (MPARAM) iPercentage, (MPARAM) NULL);
      pImportWindow->iShownPercentage = iPercentage;
    }
  }
}

static void internal_DestroyImportWindow(MP3PluginImporting_p pImportWindow)
{
  if (!pImportWindow) return;
  if (pImportWindow->tidWindowThread)
  {
    WinPostMsg(pImportWindow->hwndDlg, WM_QUIT, (MPARAM) NULL, (MPARAM) NULL);
    DosWaitThread(&(pImportWindow->tidWindowThread), DCWW_WAIT);
    memset(pImportWindow, 0, sizeof(MP3PluginImporting_t));
  }
  free(pImportWindow);
}


static int internal_TestIfSupportedFormat(FILE *hFile)
{
  int iRead;
  int iNeightbourousFrames;
  long lBuffer;
  long lIntelizedBuffer;
  char *pchBuffer;
  int iLastRead;
  struct frame Frame;

  // We should check if this file is really an MP3 file.
  // To do so, we'll read through one quarter megabyte of file, and
  // search for MPEG Audio frames. If we reach the end of the file,
  // or the end of the area we check, and there are
  // neightbourous frames there, it's an MPEG Audio Stream.

#ifdef DEBUG_BUILD
//  printf("[internal_TestIfSupportedFormat] : Enter\n"); fflush(stdout);
#endif
  iRead = 0;
  iNeightbourousFrames = 0;
  // Fill buffer
  pchBuffer = (char *) (&lBuffer);
  iLastRead = fread(&lBuffer, 1, 4, hFile);
  if (iLastRead!=4)
  {
#ifdef DEBUG_BUILD
    printf("[internal_TestIfSupportedFormat] : Could not fill buffer!\n"); fflush(stdout);
#endif
    return 0;
  }
  iRead+=iLastRead;

  while ((iLastRead>0) && (iRead<(256*1024)))
  {
    // Ok, there are four bytes in buffer. Let's see if that's an MPEG Audio
    // frame, or not!
    lIntelizedBuffer = (((unsigned int) pchBuffer[0]) << 24) |
                       (((unsigned int) pchBuffer[1]) << 16) |
                       (((unsigned int) pchBuffer[2]) << 8) |
                       (((unsigned int) pchBuffer[3]) );

#ifdef DEBUG_BUILD
//    printf(" IntelizedBuffer: 0x%x\n", lIntelizedBuffer);
#endif

    if ((head_check(lIntelizedBuffer, 0)) && (decode_header(&Frame, lIntelizedBuffer)))
    {
      if (Frame.framesize>0)
      {
#ifdef DEBUG_BUILD
//        printf("[internal_TestIfSupportedFormat] : Found an MPEG Audio frame\n"); fflush(stdout);
//        printf("  Frame size is %d\n", Frame.framesize);
#endif
        // Cool, this is an MPEG Audio frame!
        iNeightbourousFrames++;
        // Skip this MPEG Audio frame
        fseek(hFile, Frame.framesize, SEEK_CUR);
        iRead+=Frame.framesize;
      } else
      {
        iNeightbourousFrames=0;
      }

      // Fill buffer
      iLastRead = fread(&lBuffer, 1, 4, hFile);
      iRead+=iLastRead;
    } else
    if ((pchBuffer[0]=='I') &&
        (pchBuffer[1]=='D') &&
        (pchBuffer[2]=='3'))
    {
      long lSize;
#ifdef DEBUG_BUILD
//      printf("[internal_TestIfSupportedFormat] : ID3v2 tag found\n");
#endif
      // pchBuffer[3] contains the ID3v2 version Hi.
      // We read 2 more bytes to skip the version Lo and the flags.
      iLastRead = fread(&lBuffer, 1, 2, hFile);
      iRead+=iLastRead;
      // Read 4 more bytes, to get ID3v2 tag size
      lBuffer = 0;
      iLastRead = fread(&lBuffer, 1, 4, hFile);
      iRead+=iLastRead;
      // Unsync size int!
      lSize = ((unsigned int) pchBuffer[3]) |
              (((unsigned int) pchBuffer[2]) << 7) |
              (((unsigned int) pchBuffer[1]) << 14) |
              (((unsigned int) pchBuffer[0]) << 21);
#ifdef DEBUG_BUILD
//      printf("[internal_TestIfSupportedFormat] : Not yet supported, skipping %d bytes!\n", lSize);
#endif
      fseek(hFile, lSize, SEEK_CUR);
      iRead+=lSize;

      // Cool, we can threat this an MPEG Audio frame (it's not that, but we can treat it that here)!
      iNeightbourousFrames++;

      // Fill buffer
      iLastRead = fread(&lBuffer, 1, 4, hFile);
      iRead+=iLastRead;
    } else
    if ((pchBuffer[0]=='3') &&
        (pchBuffer[1]=='D') &&
        (pchBuffer[2]=='I'))
    {
#ifdef DEBUG_BUILD
//      printf("[internal_TestIfSupportedFormat] : ID3v2 footer tag found\n");
//      printf("[internal_TestIfSupportedFormat] : Not yet supported, skipping 6 bytes!\n");
#endif
      fseek(hFile, 6, SEEK_CUR);
      iRead+=6;

      // Cool, we can threat this an MPEG Audio frame (it's not that, but we can treat it that here)!
      iNeightbourousFrames++;

      // Fill buffer
      iLastRead = fread(&lBuffer, 1, 4, hFile);
      iRead+=iLastRead;
    } else
    if ((pchBuffer[0]=='T') &&
        (pchBuffer[1]=='A') &&
        (pchBuffer[2]=='G'))
    {
#ifdef DEBUG_BUILD
//      printf("[internal_TestIfSupportedFormat] : ID3v1 or ID2v1.1 tag found\n");
#endif
      fseek(hFile, 124, SEEK_CUR); // 128 bytes alltogether
      iRead+=124;

      // Cool, we can threat this an MPEG Audio frame (it's not that, but we can treat it that here)!
      iNeightbourousFrames++;

      // Fill buffer
      iLastRead = fread(&lBuffer, 1, 4, hFile);
      iRead+=iLastRead;
    } else
    {
#ifdef DEBUG_BUILD
//      if (iNeightbourousFrames)
//        printf("[internal_TestIfSupportedFormat] : Unknown data: 0x%x  Looking for known one...\n", lBuffer);
#endif

      // This is not an MPEG Audio frame, so skip one byte!
      lBuffer>>=8; // Intel's endianness...
      // Read next byte!
      iLastRead = fread(pchBuffer+3, 1, 1, hFile);
      // Some housekeeping...
      iNeightbourousFrames = 0;
      iRead+=iLastRead;
    }
  }
#ifdef DEBUG_BUILD
//  printf("[internal_TestIfSupportedFormat] : Return, iNeightbourousFrames = %d\n", iNeightbourousFrames); fflush(stdout);
#endif

  return (iNeightbourousFrames>=2);
}

#ifdef DEBUG_BUILD
void print_header(struct frame *fr)
{
  printf("MPEG %s, Layer: %s, Freq: %ld, mode: %s, modext: %d, BPF : %d\n",
         fr->mpeg25 ? "2.5" : (fr->lsf ? "2.0" : "1.0"),
         layers[fr->lay],freqs[fr->sampling_frequency],
         modes[fr->mode],fr->mode_ext,fr->framesize+4);
  printf("Channels: %d, copyright: %s, original: %s, CRC: %s, emphasis: %d.\n",
         fr->stereo,fr->copyright?"Yes":"No",
         fr->original?"Yes":"No",fr->error_protection?"Yes":"No",
         fr->emphasis);
  printf("Bitrate: %d Kbits/s, Extension value: %d\n",
         tabsel_123[fr->lsf][fr->lay-1][fr->bitrate_index],fr->extension);
}
#endif

static int internal_ParseMPEGAudioFile(WaWEImP_Open_Handle_p pResult, MP3PluginHandle_p pInternalHandle, HWND hwndOwner)
{
  long lFileSize;
  long lFilePos;
  long lBuffer;
  long lIntelizedBuffer;
  char *pchBuffer;
  int iLastRead;
  struct frame Frame;
  int   bFrameFound;
  char *pchDecodeBuffer;
  int iDecodedBytes;
  int iCurrFilePos;
  int iResult = 1;
  MP3PluginImporting_p pImportWindow;
  int iPercentage;

  long long llBitrateSum;
  int        iNumOfFrames;
  int        iPrevBitrate, iCurrBitrate;
  int        iMinBitrate, iMaxBitrate;
  int        bVBR;

  // This function should examine the file, read tags, index the mpeg audio frames, and
  // keep track of decoded PCM sample numbers...
  // This should also set proposed channel-set name, and write
  // info from tags and bitrate info into formatdesc!

  // First of all, go to the beginning of the file!
  fseek(pInternalHandle->hFile, 0, SEEK_END);
  lFileSize = ftell(pInternalHandle->hFile);
  if (!lFileSize)
  {
#ifdef DEBUG_BUILD
    printf("[internal_ParseMPEGAudioFile] : Empty file? (filesize is zero)!\n"); fflush(stdout);
#endif
    ErrorMsg("Size of file is zero!\nOpening aborted!");
    return 0;
  }
  fseek(pInternalHandle->hFile, 0, SEEK_SET);
  pInternalHandle->CurrPCMPos = 0;

  bVBR = 0;
  iPrevBitrate = 0;
  iNumOfFrames = 0;
  llBitrateSum = 0;
  iMinBitrate = 0;
  iMaxBitrate = 0;

  // Allocate memory for decode buffer!
  pchDecodeBuffer = (char *) malloc(DECODEBUFFER_SIZE);
  if (!pchDecodeBuffer)
  {
#ifdef DEBUG_BUILD
    printf("[internal_ParseMPEGAudioFile] : Out of memory (@pchDecodeBuffer)!\n"); fflush(stdout);
#endif
    ErrorMsg("Could not allocate memory for decode buffer!\n");
    return 0;
  }

  // Now parse the file until EOF!
  bFrameFound = 0;
  pInternalHandle->bID3v1Found = 0;
  pInternalHandle->bID3v11Found = 0;
  // Fill buffer
  pchBuffer = (char *) (&lBuffer);
  iLastRead = fread(&lBuffer, 1, 4, pInternalHandle->hFile);
  if (iLastRead!=4)
  {
#ifdef DEBUG_BUILD
    printf("[internal_ParseMPEGAudioFile] : Could not fill buffer!\n"); fflush(stdout);
#endif
    return 0;
  }

  // Create and initialize an Importing window to be able to show progress!
  pImportWindow = internal_CreateImportWindow(hwndOwner);
#ifdef DEBUG_BUILD
//  printf(" pImportWindow = 0x%p\n", pImportWindow); fflush(stdout);
#endif

  while ((iLastRead>0) && ((pImportWindow) && (!(pImportWindow->iShutdownRequest))))
  {
#ifdef DEBUG_BUILD
//    printf(" GetFilePos\n"); fflush(stdout);
#endif

    lFilePos = ftell(pInternalHandle->hFile);
    if (lFileSize)
      iPercentage = (lFilePos * 100)/lFileSize;
    else
      iPercentage = 100;
#ifdef DEBUG_BUILD
//    printf(" iPercentage = %d\n", iPercentage); fflush(stdout);
#endif

    internal_SetImportWindowPercentage(pImportWindow, iPercentage);

    // Ok, there are four bytes in buffer. Let's see if that's an MPEG Audio
    // frame, or not!
    lIntelizedBuffer = (((unsigned int) pchBuffer[0]) << 24) |
                       (((unsigned int) pchBuffer[1]) << 16) |
                       (((unsigned int) pchBuffer[2]) << 8) |
                       (((unsigned int) pchBuffer[3]) );

#ifdef DEBUG_BUILD
//    printf(" IntelizedBuffer: 0x%x\n", lIntelizedBuffer);
#endif

    if ((head_check(lIntelizedBuffer, 0)) && (decode_header(&Frame, lIntelizedBuffer)))
    {
#ifdef DEBUG_BUILD
//      printf("[internal_ParseMPEGAudioFile] : Found an MPEG Audio frame\n"); fflush(stdout);
//      printf("  Frame size is %d\n", Frame.framesize);
#endif

      if (bFrameFound<1024)
        bFrameFound++;

      if (Frame.framesize>4)
      {
        char *pchFrameBuffer;
        int iRet;

        // Process this MPEG Audio frame!

        iCurrBitrate = tabsel_123[Frame.lsf][Frame.lay-1][Frame.bitrate_index];
        if (iNumOfFrames==0)
        {
          // Initialize min and max bitrates from first frame
          iMinBitrate = iCurrBitrate;
          iMaxBitrate = iCurrBitrate;
        } else
        {
          if (iCurrBitrate<iMinBitrate)
            iMinBitrate = iCurrBitrate;
          if (iCurrBitrate>iMaxBitrate)
            iMaxBitrate = iCurrBitrate;
        }

        if ((iNumOfFrames!=0) && (iPrevBitrate!=iCurrBitrate))
          bVBR = 1; // This is a VBR file!

        iNumOfFrames++;
        llBitrateSum += iCurrBitrate;
        iPrevBitrate = iCurrBitrate;

        // Get the current file pos first!
        iCurrFilePos = ftell(pInternalHandle->hFile);

        pchFrameBuffer = (char *) malloc(Frame.framesize+4);
        if (!pchFrameBuffer)
        {
          // Yikes, could not allocate memory for this buffer, skipping frame!
          fseek(pInternalHandle->hFile, Frame.framesize, SEEK_CUR);
        } else
        {
          memcpy(pchFrameBuffer, pchBuffer, 4);
          iLastRead = fread(pchFrameBuffer+4, 1, Frame.framesize, pInternalHandle->hFile);
          // Ok, frame read, now decode it, and modify the index table!

#ifdef OPTIMIZE_PARSING_FOR_MPGLIB
          if (bFrameFound<=10)
          {
            iRet = decodeMP3_unclipped(&(pInternalHandle->mpstrHandle),
                                       pchFrameBuffer, Frame.framesize+4,
                                       pchDecodeBuffer, DECODEBUFFER_SIZE,
                                       &iDecodedBytes);
          } else
          {
            iRet = MP3_OK;
            if (Frame.lsf)
              iDecodedBytes = 576 * Frame.stereo * sizeof(double); // MPEG 2 and MPEG 2.5
            else
              iDecodedBytes = 1152 * Frame.stereo * sizeof(double); // MPEG 1
          }

#else
          iRet = decodeMP3_unclipped(&(pInternalHandle->mpstrHandle),
                                     pchFrameBuffer, Frame.framesize+4,
                                     pchDecodeBuffer, DECODEBUFFER_SIZE,
                                     &iDecodedBytes);

#ifdef DEBUG_BUILD
          // Check our theory!
          if (bFrameFound<=1)
          {
            if (iRet!=MP3_NEED_MORE)
            {
              printf("[Frame %d] : iRet is %d, not MP3_NEED_MORE!!\n", bFrameFound, iRet);
            }
          } else
          {
            if (iRet!=MP3_OK)
            {
              printf("[Frame %d] : iRet is %d, not MP3_OK!!\n", bFrameFound, iRet);

            }
            else
            {
              // iRet is Ok. Now check the returned number of samples!
              if (Frame.lsf)
              {
                if (iDecodedBytes != 576 * Frame.stereo * sizeof(double))
                {
                  printf("[Frame %d] : Returned %d samples, not %d (MPEG2)!!\n", iDecodedBytes/sizeof(double), 576*Frame.stereo);
                }
              }
              else
              {
                if (iDecodedBytes != 1152 * Frame.stereo * sizeof(double))
                {
                  printf("[Frame %d] : Returned %d samples, not %d (MPEG1)!!\n", iDecodedBytes/sizeof(double), 1152*Frame.stereo);
                }
              }
            }
          }

#endif

#endif

          if (iRet>=MP3_OK)
          {
            // Fine we could decode it!
            if (iRet==MP3_NEED_MORE)
            {
              iDecodedBytes = 0;
	    }
#ifdef DEBUG_BUILD
//            if (bFrameFound<10)
//              printf("[Dec %d]", iDecodedBytes/2/sizeof(double));
#endif

            // So, it's a good frame, put it into index table!

            // Let's see if we have to increase the buffer space!
            if (pInternalHandle->iAllocatedFrameSpace<=pInternalHandle->iNumOfFrames)
            {
              // We have to increase the buffer space!
              if (pInternalHandle->iAllocatedFrameSpace==0)
              {
#ifdef DEBUG_BUILD
//                printf("AllocBuffer()\n");
#endif

                // We have to initially allocate it!
                pInternalHandle->pIndexTable = (MP3PluginFrameDesc_p) malloc(sizeof(MP3PluginFrameDesc_t) * ALLOCATEDFRAMESPACE_STEP);
                if (pInternalHandle->pIndexTable)
                  pInternalHandle->iAllocatedFrameSpace = ALLOCATEDFRAMESPACE_STEP;
              } else
              {
                MP3PluginFrameDesc_p pNew;
                // We have to reallocate it!
#ifdef DEBUG_BUILD
//                printf("ReallocBuffer()\n");
#endif

                pInternalHandle->iAllocatedFrameSpace+=ALLOCATEDFRAMESPACE_STEP;
                pNew = (MP3PluginFrameDesc_p) realloc(pInternalHandle->pIndexTable,
                                                      sizeof(MP3PluginFrameDesc_t) * pInternalHandle->iAllocatedFrameSpace);
                if (pNew)
                {
                  pInternalHandle->pIndexTable = pNew;
                } else
                {
                  // Yikes, could not increase it!
                  pInternalHandle->iAllocatedFrameSpace -= ALLOCATEDFRAMESPACE_STEP;
                }
              }
            }
            // If all goes well, we have enough space in index table now. Let's
            // add an entry if it's the case!
            if (pInternalHandle->iAllocatedFrameSpace>pInternalHandle->iNumOfFrames)
            {
#ifdef DEBUG_BUILD
//              printf("StoreData() (pcmpos is %I64d)\n", pInternalHandle->CurrPCMPos);
//              printf("  0x%p", pInternalHandle);
//              printf(" 0x%p", pInternalHandle->pIndexTable);
//              printf(" %d", pInternalHandle->iNumOfFrames);
//              printf("/%d", pInternalHandle->iAllocatedFrameSpace);
//              printf(" 0x%x\n", &(pInternalHandle->pIndexTable[pInternalHandle->iNumOfFrames].iFilePos));
#endif

              pInternalHandle->pIndexTable[pInternalHandle->iNumOfFrames].iFilePos =
                iCurrFilePos-4; // Header!
              pInternalHandle->pIndexTable[pInternalHandle->iNumOfFrames].iFrameSize =
                Frame.framesize+4; // Header!
              pInternalHandle->pIndexTable[pInternalHandle->iNumOfFrames].StartPCMPos =
                pInternalHandle->CurrPCMPos;
              pInternalHandle->pIndexTable[pInternalHandle->iNumOfFrames].PCMBytes =
                iDecodedBytes;

              pInternalHandle->iNumOfFrames++;

              pInternalHandle->CurrPCMPos += iDecodedBytes;
            }

          }
#ifdef DEBUG_BUILD
//          printf("FreeFrameBuffer()\n"); fflush(stdout);
#endif

          free(pchFrameBuffer);
        }
      }

      // Fill buffer
      iLastRead = fread(&lBuffer, 1, 4, pInternalHandle->hFile);
    } else
    if ((pchBuffer[0]=='I') &&
        (pchBuffer[1]=='D') &&
        (pchBuffer[2]=='3'))
    {
      long lSize;
#ifdef DEBUG_BUILD
      printf("[internal_ParseMPEGAudioFile] : ID3v2 tag found\n");
#endif
      // pchBuffer[3] contains the ID3v2 version Hi.
      // We read 2 more bytes to skip the version Lo and the flags.
      iLastRead = fread(&lBuffer, 1, 2, pInternalHandle->hFile);
      // Read 4 more bytes, to get ID3v2 tag size
      lBuffer = 0;
      iLastRead = fread(&lBuffer, 1, 4, pInternalHandle->hFile);
      // Unsync size int!
      lSize = ((unsigned int) pchBuffer[3]) |
              (((unsigned int) pchBuffer[2]) << 7) |
              (((unsigned int) pchBuffer[1]) << 14) |
              (((unsigned int) pchBuffer[0]) << 21);
#ifdef DEBUG_BUILD
      printf("[internal_ParseMPEGAudioFile] : Not yet supported, skipping %d bytes!\n", lSize);
#endif
      fseek(pInternalHandle->hFile, lSize, SEEK_CUR);

      // Fill buffer
      iLastRead = fread(&lBuffer, 1, 4, pInternalHandle->hFile);
    } else
    if ((pchBuffer[0]=='3') &&
        (pchBuffer[1]=='D') &&
        (pchBuffer[2]=='I'))
    {
#ifdef DEBUG_BUILD
      printf("[internal_ParseMPEGAudioFile] : ID3v2 footer tag found\n");
      printf("[internal_ParseMPEGAudioFile] : Not yet supported, skipping 6 bytes!\n");
#endif
      fseek(pInternalHandle->hFile, 6, SEEK_CUR);

      // Fill buffer
      iLastRead = fread(&lBuffer, 1, 4, pInternalHandle->hFile);
    } else
    if ((pchBuffer[0]=='T') &&
        (pchBuffer[1]=='A') &&
        (pchBuffer[2]=='G'))
    {
      char achBuf[130];
      char *pchTemp;
      int i;

#ifdef DEBUG_BUILD
      printf("[internal_ParseMPEGAudioFile] : ID3v1 or ID2v1.1 tag found\n");
#endif
      memset(achBuf, 0, sizeof(achBuf));
      memcpy(achBuf, pchBuffer, 4);
      iLastRead = fread(&(achBuf[4]), 1, 124, pInternalHandle->hFile); // 128 bytes alltogether
      if (iLastRead == 124)
      {
        pInternalHandle->bID3v1Found = 1;

        memset(pInternalHandle->achID3v1_SongTitle, 0,
               sizeof(pInternalHandle->achID3v1_SongTitle));
        strncpy(pInternalHandle->achID3v1_SongTitle,
                &(achBuf[3]),
                sizeof(pInternalHandle->achID3v1_SongTitle)-1);
        // Trim spaces from the end
        pchTemp = pInternalHandle->achID3v1_SongTitle;
        i = sizeof(pInternalHandle->achID3v1_SongTitle)-2;
        while ((i>=0) &&
               (pchTemp[i]==0x20))
        {
          pchTemp[i]=0; i--;
        }


        memset(pInternalHandle->achID3v1_Artist, 0,
               sizeof(pInternalHandle->achID3v1_Artist));
        strncpy(pInternalHandle->achID3v1_Artist,
                &(achBuf[33]),
                sizeof(pInternalHandle->achID3v1_Artist)-1);
        // Trim spaces from the end
        pchTemp = pInternalHandle->achID3v1_Artist;
        i = sizeof(pInternalHandle->achID3v1_Artist)-2;
        while ((i>=0) &&
               (pchTemp[i]==0x20))
        {
          pchTemp[i]=0; i--;
        }

        memset(pInternalHandle->achID3v1_Album, 0,
               sizeof(pInternalHandle->achID3v1_Album));
        strncpy(pInternalHandle->achID3v1_Album,
                &(achBuf[63]),
                sizeof(pInternalHandle->achID3v1_Album)-1);
        // Trim spaces from the end
        pchTemp = pInternalHandle->achID3v1_Album;
        i = sizeof(pInternalHandle->achID3v1_Album)-2;
        while ((i>=0) &&
               (pchTemp[i]==0x20))
        {
          pchTemp[i]=0; i--;
        }

        memset(pInternalHandle->achID3v1_Year, 0,
               sizeof(pInternalHandle->achID3v1_Year));
        strncpy(pInternalHandle->achID3v1_Year,
                &(achBuf[93]),
                sizeof(pInternalHandle->achID3v1_Year)-1);
        // Trim spaces from the end
        pchTemp = pInternalHandle->achID3v1_Year;
        i = sizeof(pInternalHandle->achID3v1_Year)-2;
        while ((i>=0) &&
               (pchTemp[i]==0x20))
        {
          pchTemp[i]=0; i--;
        }

        memset(pInternalHandle->achID3v1_Comment, 0,
               sizeof(pInternalHandle->achID3v1_Comment));
        strncpy(pInternalHandle->achID3v1_Comment,
                &(achBuf[97]),
                sizeof(pInternalHandle->achID3v1_Comment)-1);
        // Trim spaces from the end
        pchTemp = pInternalHandle->achID3v1_Comment;
        i = sizeof(pInternalHandle->achID3v1_Comment)-2;
        while ((i>=0) &&
               (pchTemp[i]==0x20))
        {
          pchTemp[i]=0; i--;
        }

        pInternalHandle->chID3v1_Genre = achBuf[127];

        if (achBuf[125]==0)
        {
          // It might be an ID3v1.1 tag!
          if (achBuf[126]!=0)
          {
            pInternalHandle->bID3v11Found=1;
            pInternalHandle->chID3v11_Track = achBuf[126];
          }
        }
      }

      // Fill buffer
      iLastRead = fread(&lBuffer, 1, 4, pInternalHandle->hFile);
    } else
    {
#ifdef DEBUG_BUILD
//      printf("[internal_ParseMPEGAudioFile] : Unknown data: 0x%x Skipping one byte.\n", lBuffer);
#endif

      // This is not an MPEG Audio frame, so skip one byte!
      lBuffer>>=8; // Intel's endianness...
      // Read next byte!
      iLastRead = fread(pchBuffer+3, 1, 1, pInternalHandle->hFile);
    }
  }

  free(pchDecodeBuffer);

  // All is done, process the results and return!

#ifdef DEBUG_BUILD
  printf("[internal_ParseMPEGAudioFile] : Number of frames found: %d\n",
         pInternalHandle->iNumOfFrames);
  printf("[internal_ParseMPEGAudioFile] : Size of PCM stream (in Doubles): %I64d\n",
         pInternalHandle->CurrPCMPos);
  if (!bVBR)
    printf("[internal_ParseMPEGAudioFile] : Bitrate: %d\n",
           (int) (llBitrateSum / iNumOfFrames));
  else
    printf("[internal_ParseMPEGAudioFile] : Average bitrate: %d (VBR, Bitrate from %d to %d)\n",
           (int) (llBitrateSum / iNumOfFrames), iMinBitrate, iMaxBitrate);

#endif
  pInternalHandle->FullPCMSize_d = pInternalHandle->CurrPCMPos;
  pInternalHandle->FullPCMSize_w = (pInternalHandle->CurrPCMPos / sizeof(double) * sizeof(WaWESample_t));
  pInternalHandle->CurrPCMPos = 0;
  if ((bFrameFound) && ((pImportWindow) && (!(pImportWindow->iShutdownRequest))))
  {
    char achTemp[32];
#ifdef DEBUG_BUILD
    print_header(&Frame);
#endif
    // Frame contains the last MPEG Audio frame from the stream.
    // Fill the audio file properties in file handle!
    pResult->iFrequency = freqs[Frame.sampling_frequency];
    pResult->iBits = sizeof(WaWESample_t)*8;
    pResult->iIsSigned = 1;
    pResult->iChannels = Frame.stereo;

    // Add MPEG Audio specific info to format-spec, to be able to encode
    // it later into the very same format!

    PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                          "MPEG Audio/Standard",
                                                          Frame.mpeg25 ? "2.5" : (Frame.lsf ? "2.0" : "1.0"));
    PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                          "MPEG Audio/Layer",
                                                          (char *) (layers[Frame.lay]));
    PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                          "MPEG Audio/Mode",
                                                          (char *) (modes[Frame.mode]));

    PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                          "MPEG Audio/Error Protection",
                                                          Frame.error_protection?"Yes":"No");

    PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                          "MPEG Audio/Private Extension",
                                                          Frame.extension?"Yes":"No");
    if (Frame.copyright)
      PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                            "copyright",
                                                            "Copyrighted");
    if (Frame.original)
      PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                            "original",
                                                            "Yes");

    // Also save in common format key, so others can use it (e.g. OGG Vorbis)
    sprintf(achTemp, "%d", (int) (llBitrateSum*1000/iNumOfFrames));
    PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                          "bitrate",
                                                          achTemp);
    if (bVBR)
    {
      // Add detailed bitrate info
      PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                            "bitrate/nominal bitrate", achTemp);
      sprintf(achTemp, "%d", iMaxBitrate*1000);
      PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                            "bitrate/upper bitrate", achTemp);
      sprintf(achTemp, "%d", iMinBitrate*1000);
      PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                            "bitrate/lower bitrate", achTemp);
    }

    PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                          "VBR",
                                                          bVBR?"Yes":"No");

    if (pInternalHandle->bID3v1Found)
    {
      int iCHSetNameLen;

      // Prepare channel-set name
      iCHSetNameLen = 0;
      iCHSetNameLen+=strlen(pInternalHandle->achID3v1_SongTitle) + 10;
      iCHSetNameLen+=strlen(pInternalHandle->achID3v1_Artist) + 10;
      iCHSetNameLen+=strlen(pInternalHandle->achID3v1_Album) + 10;

      if (iCHSetNameLen)
      {
        pResult->pchProposedChannelSetName = (char *) malloc(iCHSetNameLen);
        if (pResult->pchProposedChannelSetName)
        {
          strcpy(pResult->pchProposedChannelSetName, "");
          if (strlen(pInternalHandle->achID3v1_Artist))
            strcat(pResult->pchProposedChannelSetName, pInternalHandle->achID3v1_Artist);
          if (strlen(pInternalHandle->achID3v1_Album))
          {
            if (strlen(pInternalHandle->achID3v1_Artist))
              strcat(pResult->pchProposedChannelSetName, " - ");
            strcat(pResult->pchProposedChannelSetName, pInternalHandle->achID3v1_Album);
          }
          if (strlen(pInternalHandle->achID3v1_SongTitle))
          {
            if (strlen(pInternalHandle->achID3v1_Artist) || strlen(pInternalHandle->achID3v1_Album))
              strcat(pResult->pchProposedChannelSetName, " - ");
            strcat(pResult->pchProposedChannelSetName, pInternalHandle->achID3v1_SongTitle);
          }
        }
      }

#ifdef DEBUG_BUILD
      printf("[internal_ParseMPEGAudioFile] : Writing ID3 info into FormatDesc.\n");
#endif

      PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                            "title",
                                                            pInternalHandle->achID3v1_SongTitle);

      PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                            "artist",
                                                            pInternalHandle->achID3v1_Artist);
      PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                            "album",
                                                            pInternalHandle->achID3v1_Album);
      PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                            "date",
                                                            pInternalHandle->achID3v1_Year);
      PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                            "comment",
                                                            pInternalHandle->achID3v1_Comment);

      if (pInternalHandle->chID3v1_Genre<GENRE_NAME_COUNT)
      {
        PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                              "genre",
                                                              (char *) (genre_names[pInternalHandle->chID3v1_Genre]));
      }

      if (pInternalHandle->bID3v11Found)
      {
        sprintf(achTemp, "%d", pInternalHandle->chID3v11_Track);
        PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                              "track number",
                                                              achTemp);
      }
    }
  } else
  {
#ifdef DEBUG_BUILD
    printf("[internal_ParseMPEGAudioFile] : No audio frame found in stream!\n");
#endif
    iResult = 0;
  }

  // Destroy import window
  internal_DestroyImportWindow(pImportWindow);

  return iResult;
}


// Functions called from outside:

long                  WAWECALL plugin_Close(WaWEImP_Open_Handle_p pHandle)
{
  MP3PluginHandle_p pInternalHandle;

  if (!pHandle) return 0;

  if (DosRequestMutexSem(hmtxUsePlugin, SEM_INDEFINITE_WAIT) != NO_ERROR)
  {
#ifdef DEBUG_BUILD
    printf("[plugin_Close] : IMPA : Could not get mutex semaphore!\n"); fflush(stdout);
#endif
    return NULL;
  } else
  {

    if (!(pHandle->bTempClosed))
    {
      // Free our internal, plugin-specific stuffs

      pInternalHandle = (MP3PluginHandle_p) (pHandle->pPluginSpecific);
      if (pInternalHandle)
      {
        ExitMP3(&(pInternalHandle->mpstrHandle));
#ifdef DEBUG_BUILD
        printf("[plugin_Close] : IMPA : Closing file handle.\n"); fflush(stdout);
#endif
	fclose(pInternalHandle->hFile);

	if (pInternalHandle->pIndexTable)
	  free(pInternalHandle->pIndexTable);

	free(pInternalHandle);
      }

    }

    // We might have proposed a channel-set name, free that if it's so...
    if (pHandle->pchProposedChannelSetName)
      free(pHandle->pchProposedChannelSetName);

    PluginHelpers.fn_WaWEHelper_ChannelSetFormat_DeleteAllKeys(&(pHandle->ChannelSetFormat));
    // Free handle
    free(pHandle);
    DosReleaseMutexSem(hmtxUsePlugin);
  }

  return 1;
}

WaWEImP_Open_Handle_p WAWECALL plugin_Open(char *pchFileName, int iOpenMode, HWND hwndOwner)
{
  FILE *hFile;
  WaWEImP_Open_Handle_p pResult;
  MP3PluginHandle_p pInternalHandle;

  pResult = NULL;

#ifdef DEBUG_BUILD
//  printf("[plugin_Open] : IMPA : Called for [%s]\n", pchFileName); fflush(stdout);
#endif

  if (DosRequestMutexSem(hmtxUsePlugin, SEM_INDEFINITE_WAIT) != NO_ERROR)
  {
#ifdef DEBUG_BUILD
    printf("[plugin_Open] : IMPA : Could not get mutex semaphore!\n"); fflush(stdout);
#endif
    return NULL;
  } else
  {

    // First try to open the file
    hFile = fopen(pchFileName, "rb");
    if (!hFile)
    {
      DosReleaseMutexSem(hmtxUsePlugin);
      ErrorMsg("Could not open file!\n");
      return NULL;
    }

#ifdef DEBUG_BUILD
//    printf("[plugin_Open] : IMPA : Opened!\n"); fflush(stdout);
#endif

    // We should test if we can handle this file format!
    if (!internal_TestIfSupportedFormat(hFile))
    {
      // This file format is not supported.
#ifdef DEBUG_BUILD
      printf("[plugin_Open] : IMPA : Closing file handle.\n"); fflush(stdout);
#endif
      fclose(hFile);
      DosReleaseMutexSem(hmtxUsePlugin);
      return NULL;
    }
#ifdef DEBUG_BUILD
//    printf("[plugin_Open] : IMPA : Supported format!\n");
#endif

    pInternalHandle = (MP3PluginHandle_p) malloc(sizeof(MP3PluginHandle_t));
    if (!pInternalHandle)
    {
#ifdef DEBUG_BUILD
      printf("[plugin_Open] : IMPA : Closing file handle.\n"); fflush(stdout);
#endif
      fclose(hFile);
      DosReleaseMutexSem(hmtxUsePlugin);
      ErrorMsg("Out of memory!\n");
      return NULL;
    }

    // Initialize internal handle structure
    memset(pInternalHandle, 0, sizeof(MP3PluginHandle_t));

    pInternalHandle->hFile = hFile;
#ifdef DEBUG_BUILD
//    printf("[plugin_Open] : IMPA : DEBUG1\n"); fflush(stdout);
#endif

    InitMP3(&(pInternalHandle->mpstrHandle));

#ifdef DEBUG_BUILD
//    printf("[plugin_Open] : IMPA : DEBUG2\n"); fflush(stdout);
#endif

    pInternalHandle->iNumOfFrames = 0;
    pInternalHandle->iAllocatedFrameSpace = 0;
    pInternalHandle->pIndexTable = NULL;

    pInternalHandle->CurrPCMPos = 0;

#ifdef DEBUG_BUILD
//    printf("[plugin_Open] : IMPA : DEBUG3\n"); fflush(stdout);
#endif


    pResult = (WaWEImP_Open_Handle_p) malloc(sizeof(WaWEImP_Open_Handle_t));
    if (!pResult)
    {
      ExitMP3(&(pInternalHandle->mpstrHandle));
      free(pInternalHandle);
#ifdef DEBUG_BUILD
      printf("[plugin_Open] : IMPA : Closing file handle.\n"); fflush(stdout);
#endif
      fclose(hFile);
      DosReleaseMutexSem(hmtxUsePlugin);
      ErrorMsg("Out of memory!\n");
      return NULL;
    }

#ifdef DEBUG_BUILD
//    printf("[plugin_Open] : IMPA : DEBUG4\n"); fflush(stdout);
#endif

    // Set default stuffs, we'll fill these later, at real open!
    memset(pResult, 0, sizeof(WaWEImP_Open_Handle_t));
    pResult->iFrequency = 44100;
    pResult->iChannels = 2;
    pResult->iBits = 16;
    pResult->iIsSigned = 1;

    // We don't propose any channel or chanelset names.
    pResult->pchProposedChannelSetName = NULL;
    pResult->apchProposedChannelName = NULL;

#ifdef DEBUG_BUILD
//    printf("[plugin_Open] : IMPA : DEBUG5\n"); fflush(stdout);
#endif

    PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                          "FourCC", "MPA");  // MPEG Audio

#ifdef DEBUG_BUILD
//    printf("[plugin_Open] : IMPA : DEBUG6\n"); fflush(stdout);
#endif

    pResult->pPluginSpecific = pInternalHandle;

    pResult->bTempClosed = 0;

    if (iOpenMode==WAWE_OPENMODE_NORMAL)
    {
#ifdef DEBUG_BUILD
//      printf("[plugin_Open] : IMPA : Open mode is Normal mode.\n"); fflush(stdout);
#endif

      // Now, examine the file, read tags, index the mpeg audio frames, and
      // keep track of decoded PCM sample numbers...
      // This should also set proposed channel-set name, and write
      // info from tags and bitrate info into formatdesc!

      if (!internal_ParseMPEGAudioFile(pResult, pInternalHandle, hwndOwner))
      {
#ifdef DEBUG_BUILD
	printf("[plugin_Open] : IMPA : Could not parse file, cleaning up!\n"); fflush(stdout);
#endif
	plugin_Close(pResult);
	pResult = NULL;
      }
    }
    DosReleaseMutexSem(hmtxUsePlugin);
  }

#ifdef DEBUG_BUILD
//  printf("[plugin_Open] : IMPA : Return\n"); fflush(stdout);
#endif


  return pResult;
}

WaWEPosition_t        WAWECALL plugin_Seek(WaWEImP_Open_Handle_p pHandle,
                                           WaWEPosition_t Offset,
                                           int iOrigin)
{
  MP3PluginHandle_p pInternalHandle;
  WaWEPosition_t RequiredPos;
  WaWEPosition_t PCMPosToCut;

  if (!pHandle) return 0;
  if (pHandle->bTempClosed) return 0; // Hey, it's TempClosed!

  pInternalHandle = (MP3PluginHandle_p)(pHandle->pPluginSpecific);
  if (!pInternalHandle) return 0;

  PCMPosToCut = FIRST_SAMPLES_TO_CUT * sizeof(WaWESample_t) * pHandle->iChannels;

  if (DosRequestMutexSem(hmtxUsePlugin, SEM_INDEFINITE_WAIT) == NO_ERROR)
  {

    switch (iOrigin)
    {
      default:
      case WAWE_SEEK_SET:
	RequiredPos = PCMPosToCut + Offset;
	break;
      case WAWE_SEEK_CUR:
	RequiredPos = pInternalHandle->CurrPCMPos + Offset;
	break;
      case WAWE_SEEK_END:
	RequiredPos = pInternalHandle->FullPCMSize_w + Offset;
	break;
    }

    if (RequiredPos<PCMPosToCut)
      RequiredPos = PCMPosToCut;
    if (RequiredPos>pInternalHandle->FullPCMSize_w)
      RequiredPos = pInternalHandle->FullPCMSize_w;

    pInternalHandle->CurrPCMPos = RequiredPos;

    DosReleaseMutexSem(hmtxUsePlugin);
    return RequiredPos - PCMPosToCut;
  } else
  {
#ifdef DEBUG_BUILD
    printf("[plugin_Seek] : Error, could not get mutex semaphore!\n");
#endif
    if (pInternalHandle->CurrPCMPos<PCMPosToCut)
      return 0;
    else
      return pInternalHandle->CurrPCMPos - PCMPosToCut;
  }
}

WaWESize_t            WAWECALL plugin_Read(WaWEImP_Open_Handle_p pHandle,
                                           char *pchBuffer,
                                           WaWESize_t cBytes)
{
  MP3PluginHandle_p pInternalHandle;
  char *pchDecodeBuffer;
  WaWESize_t CouldRead;
  WaWEPosition_t PCMPosStartDecodeFrom;
  int iFrameToStartDecodeFrom;
  int iCouldRead;
  int i;
  int iFrameSizeSum;

  if (!pHandle) return 0;
  if (pHandle->bTempClosed) return 0; // Hey, it's TempClosed!

  pInternalHandle = (MP3PluginHandle_p)(pHandle->pPluginSpecific);
  if (!pInternalHandle) return 0;

  pchDecodeBuffer = (char *) malloc(DECODEBUFFER_SIZE);
  if (!pchDecodeBuffer)
  {
#ifdef DEBUG_BUILD
    printf("[plugin_Read] : MPA : Not enough memory for decode buffer!\n");
#endif
    return 0;
  }

  if (DosRequestMutexSem(hmtxUsePlugin, SEM_INDEFINITE_WAIT) == NO_ERROR)
  {

    CouldRead = 0;

    // Tell Decoder that we're seeked, so flush internal buffers!
    ExitMP3(&(pInternalHandle->mpstrHandle));
    InitMP3(&(pInternalHandle->mpstrHandle));
  
    PCMPosStartDecodeFrom = (pInternalHandle->CurrPCMPos / sizeof(WaWESample_t) * sizeof(double));
  
#ifdef DEBUG_BUILD
//    printf("[plugin_Read] : MPA : PCMPosStartDecodeFrom = %I64d\n", PCMPosStartDecodeFrom);
#endif
  
    // The nature of MP3 frames is that they can have references to previous frames.
    // They have 512bytes of internal buffer for these references, so we have to start
    // decoding a bit earlier, to make sure that this buffer is filled!

    // First, find the MP3 frame which contains this PCM position, then go back
    // some bytes from the beginning of that frame, and start decoding from that frame!


    iFrameToStartDecodeFrom = 0;
    i=0;
    while (i<pInternalHandle->iNumOfFrames)
    {
      if (pInternalHandle->pIndexTable[i].StartPCMPos<PCMPosStartDecodeFrom)
        iFrameToStartDecodeFrom = i;
      else
        break;
      i++;
    }

#ifdef DEBUG_BUILD
//    printf("[plugin_Read] : MPA : iFrameToStartDecodeFrom (@1) = %d\n", iFrameToStartDecodeFrom);
#endif

    // Ok, we have the frame to start decoding from, to get the
    // decoded data which contains the starting position of the bytes we want.
    // Now go back some frames, to make sure that when the decoding gets this
    // frame, the decoder's buffers will be full!

    iFrameSizeSum=0;
    while ((iFrameToStartDecodeFrom>0) && (iFrameSizeSum<MAXFRAMESIZE))
    {
      iFrameToStartDecodeFrom--;
      iFrameSizeSum += pInternalHandle->pIndexTable[iFrameToStartDecodeFrom].iFrameSize;
    }

#ifdef DEBUG_BUILD
//    printf("[plugin_Read] : MPA : iFrameToStartDecodeFrom (@2) = %d\n", iFrameToStartDecodeFrom); fflush(stdout);
#endif

    // Gooood. Now start decoding from this frame, and copy the data if we got it!
    iCouldRead = 1;
    while ((CouldRead<cBytes) &&
           (iFrameToStartDecodeFrom < pInternalHandle->iNumOfFrames) &&
           (iCouldRead)) // Also stop if end of file is reached!
    {
      char *pchFrameBuffer;
      int iDecodedBytes, iRet;
  
#ifdef DEBUG_BUILD
  //    printf("Go(%I64d/%I64d) ", CouldRead, cBytes); fflush(stdout);
#endif
  
      // Read the MPEG Audio frame
#ifdef DEBUG_BUILD
  //    printf("Seek(%d) ", pInternalHandle->pIndexTable[iFrameToStartDecodeFrom].iFilePos); fflush(stdout);
#endif
  
      fseek(pInternalHandle->hFile, pInternalHandle->pIndexTable[iFrameToStartDecodeFrom].iFilePos, SEEK_SET);
      pchFrameBuffer = (char *) malloc(pInternalHandle->pIndexTable[iFrameToStartDecodeFrom].iFrameSize);
      if (pchFrameBuffer)
      {
#ifdef DEBUG_BUILD
  //      printf("Read%d(%d bytes) ", iFrameToStartDecodeFrom, pInternalHandle->pIndexTable[iFrameToStartDecodeFrom].iFrameSize); fflush(stdout);
#endif
        // Buffer allocated, so read data into it, and decode it!
        iCouldRead = fread(pchFrameBuffer, 1, pInternalHandle->pIndexTable[iFrameToStartDecodeFrom].iFrameSize, pInternalHandle->hFile);
        if (iCouldRead)
        {
          // Ok, frame read, now decode it.
#ifdef DEBUG_BUILD
  //        printf("Dec%d(iCouldRead=%d) ", iFrameToStartDecodeFrom, iCouldRead); fflush(stdout);
#endif

          iRet = decodeMP3_unclipped(&(pInternalHandle->mpstrHandle),
                                     pchFrameBuffer,
                                     iCouldRead,
                                     pchDecodeBuffer, DECODEBUFFER_SIZE,
                                     &iDecodedBytes);

#ifdef DEBUG_BUILD
  //        printf("Ret(%d) ", iRet); fflush(stdout);
#endif
  
          if (iRet>=MP3_OK)
          {
            // Fine we could decode it!
            if (iRet==MP3_NEED_MORE)
              iDecodedBytes = 0;

#ifdef DEBUG_BUILD
  //          printf("Check(%d) ", iDecodedBytes); fflush(stdout);
#endif
  
            if (iDecodedBytes)
            {
              // Nice, decoded it.
              // Now use some data from it, if we can!
              if (pInternalHandle->pIndexTable[iFrameToStartDecodeFrom].StartPCMPos+iDecodedBytes>
                  PCMPosStartDecodeFrom)
              {
                // We can use bytes from this buffer!
                // Copy out and convert the doubles into the output buffer!
                int iCopyFrom;
                double *pdblBuffer;
                int iCopyBytes;
                int iPos;
                WaWESample_t OneSample;

                if (pInternalHandle->pIndexTable[iFrameToStartDecodeFrom].StartPCMPos<PCMPosStartDecodeFrom)
                  iCopyFrom = PCMPosStartDecodeFrom - pInternalHandle->pIndexTable[iFrameToStartDecodeFrom].StartPCMPos;
                else
                  iCopyFrom = 0;

                iCopyBytes = iDecodedBytes - iCopyFrom;
                if (iCopyBytes<0) iCopyBytes = 0;
                if (iCopyBytes>iDecodedBytes) iCopyBytes = iDecodedBytes;

#ifdef DEBUG_BUILD
  //              printf("Use(%d from %d) ", iCopyBytes, iCopyFrom); fflush(stdout);
#endif

                iPos = iCopyFrom;
                while ((iPos < iCopyFrom+iCopyBytes) && (CouldRead<cBytes))
                {
                  pdblBuffer = (double *) (pchDecodeBuffer + iPos);
#ifdef DEBUG_BUILD
  //                if (iPos<iCopyFrom+10)
  //                  printf("%f ", (float) (*pdblBuffer));
#endif
                  // Clip
                  if (*pdblBuffer>32768) *pdblBuffer = 32768;
                  if (*pdblBuffer<-32767) *pdblBuffer = -32767;
                  // Convert to WaWESample_t
                  OneSample = (WaWESample_t) ((*pdblBuffer) * MAX_WAWESAMPLE / 32768);
                  iPos+=sizeof(double);
                  memcpy(pchBuffer, &OneSample, sizeof(OneSample));
                  pchBuffer += sizeof(OneSample);
                  CouldRead += sizeof(OneSample);
                }
              }

            }

          }
        }
        free(pchFrameBuffer);
      }
#ifdef DEBUG_BUILD
  //    printf("Next\n"); fflush(stdout);
#endif

      // Go for next frame!
      iFrameToStartDecodeFrom++;
    }

    free(pchDecodeBuffer);

#ifdef DEBUG_BUILD
  //  printf("[plugin_Read] : MPA : CouldRead = %I64d\n", CouldRead); fflush(stdout);
#endif

    DosReleaseMutexSem(hmtxUsePlugin);
    return CouldRead;
  } else
  {
#ifdef DEBUG_BUILD
    printf("[plugin_Read] : MPA : Could ot get mutex semaphore!\n"); fflush(stdout);
#endif
    return 0;
  }
}

long                  WAWECALL plugin_TempClose(WaWEImP_Open_Handle_p pHandle)
{
  MP3PluginHandle_p pInternalHandle;

  if (!pHandle) return 0;
  if (pHandle->bTempClosed) return 1; // No need to do anything, it's already TempClosed.

  pInternalHandle = (MP3PluginHandle_p)(pHandle->pPluginSpecific);
  if (!pInternalHandle) return 0;

  if (DosRequestMutexSem(hmtxUsePlugin, SEM_INDEFINITE_WAIT) == NO_ERROR)
  {
    // Okay, release the file!
    // Free our internal, plugin-specific stuffs

    ExitMP3(&(pInternalHandle->mpstrHandle));
#ifdef DEBUG_BUILD
    printf("[plugin_TempClose] : IMPA : Closing file handle.\n"); fflush(stdout);
#endif
    fclose(pInternalHandle->hFile);

    if (pInternalHandle->pIndexTable)
      free(pInternalHandle->pIndexTable);

    free(pInternalHandle);

    pHandle->pPluginSpecific = NULL;
    pHandle->bTempClosed = 1; // Note that it's temporarily closed

    DosReleaseMutexSem(hmtxUsePlugin);
    return 1;
  } else
  {
#ifdef DEBUG_BUILD
    printf("[plugin_TempClose] : Error, could not get mutex semaphore!\n");
#endif
    return 0;
  }
}

long                  WAWECALL plugin_ReOpen(char *pchFileName, WaWEImP_Open_Handle_p pHandle)
{
  WaWEImP_Open_Handle_p pNewHandle;

  if (!pHandle) return 0;
  if (!(pHandle->bTempClosed)) return 1; // No need to do anything, it's already opened.

  pNewHandle = plugin_Open(pchFileName, WAWE_OPENMODE_NORMAL, HWND_DESKTOP);

  if (!pNewHandle) return 0;
  memcpy(pHandle, pNewHandle, sizeof(WaWEImP_Open_Handle_t));
  free(pNewHandle);

  return 1;
}


void WAWECALL plugin_About(HWND hwndOwner)
{
  WinMessageBox(HWND_DESKTOP, hwndOwner,
                pchAboutMessage, pchAboutTitle, 0, MB_OK | MB_MOVEABLE | MB_INFORMATION);
}

int WAWECALL plugin_Initialize(HWND hwndOwner,
                               WaWEPluginHelpers_p pWaWEPluginHelpers)
{
  if (!pWaWEPluginHelpers) return 0;

  // Save the helper functions for later usage!
  memcpy(&PluginHelpers, pWaWEPluginHelpers, sizeof(PluginHelpers));

  DosCreateMutexSem(NULL, // Name
                    &hmtxUsePlugin,
                    0L, // Non-shared
                    FALSE); // Unowned

  // Return success!
  return 1;
}

void WAWECALL plugin_Uninitialize(HWND hwndOwner)
{
  DosRequestMutexSem(hmtxUsePlugin, SEM_INDEFINITE_WAIT); // Make sure that nobody uses the plugin now.
  DosCloseMutexSem(hmtxUsePlugin);
}


// Exported functions:

WAWEDECLSPEC long WAWECALL WaWE_GetPluginInfo(long lPluginDescSize,
                                              WaWEPlugin_p pPluginDesc,
                                              int  iWaWEMajorVersion,
                                              int  iWaWEMinorVersion)
{
  // Check parameters and do version check:
  if ((!pPluginDesc) || (sizeof(WaWEPlugin_t)!=lPluginDescSize)) return 0;
  if ((iWaWEMajorVersion!=WAWE_APPVERSIONMAJOR) ||
      (iWaWEMinorVersion!=WAWE_APPVERSIONMINOR)) return 0;

  // Version seems to be Ok.
  // So, report info about us!
  strcpy(pPluginDesc->achName, pchPluginName);

  // Make the importance of this plugin high, so if we report that we can handle
  // a file, we'll be used. Hey, who else would handle MP3/MP2 files if not us?
  pPluginDesc->iPluginImportance = WAWE_PLUGIN_IMPORTANCE_MAX - 5;

  pPluginDesc->iPluginType = WAWE_PLUGIN_TYPE_IMPORT;

  // Plugin functions:
  pPluginDesc->fnAboutPlugin = plugin_About;
  pPluginDesc->fnConfigurePlugin = NULL;
  pPluginDesc->fnInitializePlugin = plugin_Initialize;
  pPluginDesc->fnPrepareForShutdown = NULL;
  pPluginDesc->fnUninitializePlugin = plugin_Uninitialize;

  pPluginDesc->TypeSpecificInfo.ImportPluginInfo.fnOpen = plugin_Open;
  pPluginDesc->TypeSpecificInfo.ImportPluginInfo.fnSeek = plugin_Seek;
  pPluginDesc->TypeSpecificInfo.ImportPluginInfo.fnRead = plugin_Read;
  pPluginDesc->TypeSpecificInfo.ImportPluginInfo.fnClose = plugin_Close;
  pPluginDesc->TypeSpecificInfo.ImportPluginInfo.fnTempClose = plugin_TempClose;
  pPluginDesc->TypeSpecificInfo.ImportPluginInfo.fnReOpen = plugin_ReOpen;

  // Save module handle for later use
  // (We will need it when we want to load some resources from DLL)
  hmodOurDLLHandle = pPluginDesc->hmodDLL;

  // Return success!
  return 1;
}
