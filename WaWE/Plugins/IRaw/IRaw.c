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
 * RAW data import plugin                                                    *
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
#include <os2.h>

#include "WaWECommon.h"    // Common defines and structures
#include "IRaw-Resources.h"

// Global variables
// (Per process, because the DLL is initinstance/terminstance!)
HMODULE hmodOurDLLHandle;
WaWEPluginHelpers_t PluginHelpers;
HMTX hmtxUsePlugin;

char *WinStateSave_Name = "WaWE";
char *WinStateSave_Key  = "ImportPlugin_RAW_ImportPP";

char *pchPluginName = "~RAW Import plugin";
char *pchAboutTitle = "About RAW Import plugin";
char *pchAboutMessage =
  "RAW Import plugin v1.0 for WaWE\n"
  "Written by Doodle\n"
  "\n"
  "Compilation date: "__DATE__" "__TIME__;



// Helper functions, called from inside:

void ErrorMsg(char *Msg)
{
#ifdef DEBUG_BUILD
  printf("ErrorMsg: %s\n",Msg); fflush(stdout);
#endif
  WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                Msg, "RAW Import Plugin Error", 0, MB_OK | MB_MOVEABLE | MB_ICONEXCLAMATION);
}

void ResizeImportDialogControls(HWND hwnd)
{
  SWP Swp1, Swp2, Swp3;
  ULONG CXDLGFRAME, CYDLGFRAME, CYTITLEBAR;
  int iDisabledWindowUpdate=0;

  if (WinIsWindowVisible(hwnd))
  {
    WinEnableWindowUpdate(hwnd, FALSE);
    iDisabledWindowUpdate = 1;
  }

  CXDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
  CYDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);
  CYTITLEBAR = WinQuerySysValue(HWND_DESKTOP, SV_CYTITLEBAR);

  // Reposition/resize controls in window

  WinQueryWindowPos(hwnd, &Swp1);

  // First the push buttons
  WinSetWindowPos(WinWindowFromID(hwnd, PB_IMPORT), HWND_TOP,
                  3*CXDLGFRAME,
                  2*CYDLGFRAME,
		  0, 0,
                  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, PB_CANCEL), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, PB_CANCEL), HWND_TOP,
                  Swp1.cx - 3*CXDLGFRAME - Swp2.cx,
                  2*CYDLGFRAME,
		  0, 0,
                  SWP_MOVE);

  // The groupbox
  WinQueryWindowPos(WinWindowFromID(hwnd, PB_CANCEL), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, GB_IMPORTSETTINGS), HWND_TOP,
                  2*CXDLGFRAME,
                  Swp2.y + Swp2.cy + 2*CYDLGFRAME,
                  Swp1.cx - 4*CXDLGFRAME,
                  Swp1.cy - Swp2.y - Swp2.cy - 4*CYDLGFRAME - CYTITLEBAR,
                  SWP_MOVE | SWP_SIZE);

  // The texts in the groupbox
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_IMPORTSETTINGS), &Swp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_FREQUENCY), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_FREQUENCY), HWND_TOP,
                  Swp2.x + 2*CXDLGFRAME,
                  Swp2.y + Swp2.cy - 2*Swp3.cy - 2*CYDLGFRAME,
                  0,
                  0,
                  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_FREQUENCY), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_BITS), HWND_TOP,
                  Swp2.x,
                  Swp2.y - Swp2.cy - 2*CYDLGFRAME,
                  0,
                  0,
                  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_BITS), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_CHANNELS), HWND_TOP,
                  Swp2.x,
                  Swp2.y - Swp2.cy - 2*CYDLGFRAME,
                  0,
                  0,
                  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_CHANNELS), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_SIGNED), HWND_TOP,
                  Swp2.x,
                  Swp2.y - Swp2.cy - 2*CYDLGFRAME,
                  0,
                  0,
                  SWP_MOVE);

  // The entry fields
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_IMPORTSETTINGS), &Swp3);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_FREQUENCY), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, EF_FREQUENCY), HWND_TOP,
                  Swp2.x + Swp2.cx + 2*CXDLGFRAME,
                  Swp2.y,
                  Swp3.cx - (Swp2.x + Swp2.cx + 2*CXDLGFRAME) - CXDLGFRAME,
                  Swp2.cy,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_FREQUENCY), &Swp1);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_CHANNELS), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, EF_CHANNELS), HWND_TOP,
                  Swp1.x + Swp1.cx + 2*CXDLGFRAME,
                  Swp2.y,
                  Swp3.cx - (Swp1.x + Swp1.cx + 2*CXDLGFRAME) - CXDLGFRAME,
                  Swp2.cy,
                  SWP_MOVE | SWP_SIZE);

  // Combo boxes
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_FREQUENCY), &Swp1);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_BITS), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, CX_BITS), HWND_TOP,
                  Swp1.x + Swp1.cx + 2*CXDLGFRAME -CXDLGFRAME+1 ,
                  Swp2.y - Swp2.cy*6 + Swp2.cy +1,
                  Swp3.cx - (Swp1.x + Swp1.cx + 2*CXDLGFRAME) - CXDLGFRAME +2*CXDLGFRAME-2,
                  Swp2.cy*6 +2, // 4 lines
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_FREQUENCY), &Swp1);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_SIGNED), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, CX_SIGNED), HWND_TOP,
                  Swp1.x + Swp1.cx + 2*CXDLGFRAME -CXDLGFRAME+1 ,
                  Swp2.y - Swp2.cy*4 + Swp2.cy +1,
                  Swp3.cx - (Swp1.x + Swp1.cx + 2*CXDLGFRAME) - CXDLGFRAME +2*CXDLGFRAME-2,
                  Swp2.cy*4 +2, // 2 lines
                  SWP_MOVE | SWP_SIZE);

  if (iDisabledWindowUpdate)
    WinEnableWindowUpdate(hwnd, TRUE);
}

MRESULT EXPENTRY ImportDialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  LONG lID, lDefaultID;
  switch (msg)
  {
    case WM_INITDLG:
      {
	// Clear entry fields
	WinSendDlgItemMsg(hwnd, EF_FREQUENCY,
			  EM_CLEAR,
			  (MPARAM) 0,
                          (MPARAM) 0);
	WinSendDlgItemMsg(hwnd, EF_CHANNELS,
			  EM_CLEAR,
			  (MPARAM) 0,
			  (MPARAM) 0);
	// Clear combo boxes
	WinSendDlgItemMsg(hwnd, CX_BITS,
			  LM_DELETEALL,
			  (MPARAM) 0,
                          (MPARAM) 0);
	WinSendDlgItemMsg(hwnd, CX_SIGNED,
			  LM_DELETEALL,
			  (MPARAM) 0,
			  (MPARAM) 0);
	// Set default stuffs in entry fields:
	WinSetDlgItemText(hwnd, EF_FREQUENCY, "44100");
	WinSetDlgItemText(hwnd, EF_CHANNELS, "2");
	// Set up combo boxes, and set their defaults:
        // First the Bits
	lID = (LONG) WinSendDlgItemMsg(hwnd, CX_BITS,
				       LM_INSERTITEM,
				       (MPARAM) LIT_END,
				       (MPARAM) "8");
	WinSendDlgItemMsg(hwnd, CX_BITS,
			  LM_SETITEMHANDLE,
			  (MPARAM) lID,
                          (MPARAM) 8);
        lDefaultID =
	lID = (LONG) WinSendDlgItemMsg(hwnd, CX_BITS,
				       LM_INSERTITEM,
				       (MPARAM) LIT_END,
				       (MPARAM) "16");
	WinSendDlgItemMsg(hwnd, CX_BITS,
			  LM_SETITEMHANDLE,
			  (MPARAM) lID,
			  (MPARAM) 16);

	lID = (LONG) WinSendDlgItemMsg(hwnd, CX_BITS,
				       LM_INSERTITEM,
				       (MPARAM) LIT_END,
				       (MPARAM) "24");
	WinSendDlgItemMsg(hwnd, CX_BITS,
			  LM_SETITEMHANDLE,
			  (MPARAM) lID,
                          (MPARAM) 24);
	lID = (LONG) WinSendDlgItemMsg(hwnd, CX_BITS,
				       LM_INSERTITEM,
				       (MPARAM) LIT_END,
				       (MPARAM) "32");
	WinSendDlgItemMsg(hwnd, CX_BITS,
			  LM_SETITEMHANDLE,
			  (MPARAM) lID,
                          (MPARAM) 32);

	WinSendDlgItemMsg(hwnd, CX_BITS,
			  LM_SELECTITEM,
			  (MPARAM) lDefaultID,
                          (MPARAM) TRUE);
        // Then the Signed
        lDefaultID =
	lID = (LONG) WinSendDlgItemMsg(hwnd, CX_SIGNED,
				       LM_INSERTITEM,
				       (MPARAM) LIT_END,
				       (MPARAM) "Yes");
	WinSendDlgItemMsg(hwnd, CX_SIGNED,
			  LM_SETITEMHANDLE,
			  (MPARAM) lID,
                          (MPARAM) 1);

	lID = (LONG) WinSendDlgItemMsg(hwnd, CX_SIGNED,
				       LM_INSERTITEM,
				       (MPARAM) LIT_END,
				       (MPARAM) "No");
	WinSendDlgItemMsg(hwnd, CX_SIGNED,
			  LM_SETITEMHANDLE,
			  (MPARAM) lID,
                          (MPARAM) 0);

	WinSendDlgItemMsg(hwnd, CX_SIGNED,
			  LM_SELECTITEM,
			  (MPARAM) lDefaultID,
                          (MPARAM) TRUE);

        // Ok, stuffs set up!

	return (MRESULT) 0;
      }
    case WM_FORMATFRAME:
      {
	ResizeImportDialogControls(hwnd);
        break;
      }
  } // End of switch msg
  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}


// Functions called from outside:

WaWEImP_Open_Handle_p WAWECALL plugin_Open(char *pchFileName, int iOpenMode, HWND hwndOwner)
{
  HWND dlg;
  SWP DlgSwap, ParentSwap;
  FILE *hFile;
  WaWEImP_Open_Handle_p pResult;

  pResult = NULL;

  if (DosRequestMutexSem(hmtxUsePlugin, SEM_INDEFINITE_WAIT) != NO_ERROR)
  {
#ifdef DEBUG_BUILD
    printf("[plugin_Open] : IRAW : Could not get mutex semaphore!\n"); fflush(stdout);
#endif
    return 0;
  }

  // First try to open the file
  hFile = fopen(pchFileName, "rb");
  if (!hFile)
  {
    DosReleaseMutexSem(hmtxUsePlugin);
    ErrorMsg("Could not open file!\n");
    return NULL;
  }

  if (iOpenMode==WAWE_OPENMODE_TEST)
  {
    // If we only had to test if we can handle it, report success!
    pResult = (WaWEImP_Open_Handle_p) malloc(sizeof(WaWEImP_Open_Handle_t));
  
    if (pResult)
    {
      // Make it empty first!
      memset(pResult, 0, sizeof(WaWEImP_Open_Handle_p));

      // Set default stuffs
      pResult->iFrequency = 44100;
      pResult->iChannels = 2;
      pResult->iBits = 16;
      pResult->iIsSigned = 1;

      // We don't propose any channel or chanelset names.
      pResult->pchProposedChannelSetName = NULL;
      pResult->apchProposedChannelName = NULL;

      // We only set the only-one mandatory format spec, the FourCC value.
      PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat), "FourCC", "RAW");

      pResult->pPluginSpecific = hFile;

      pResult->bTempClosed = 0;
    }
  } else
  {
    // If we have to open it really, ask user for open settings!

    // Then let the dialog window come, and ask for the settings,
    // how to load the file!

    dlg = WinLoadDlg(HWND_DESKTOP,
                     hwndOwner, // Owner will be the WaWE app itself
                     ImportDialogProc, // Dialog proc
                     hmodOurDLLHandle, // Source module handle
                     DLG_RAWIMPORT,
                     NULL);
    if (dlg)
    {
      // Center dialog in screen
      if (WinQueryWindowPos(dlg, &DlgSwap))
        if (WinQueryWindowPos(HWND_DESKTOP, &ParentSwap))
        {
          // Center dialog box within the parent window
          int ix, iy;
          ix = ParentSwap.x + (ParentSwap.cx - DlgSwap.cx)/2;
          iy = ParentSwap.y + (ParentSwap.cy - DlgSwap.cy)/2;
          WinSetWindowPos(dlg, HWND_TOP, ix, iy, 0, 0,
                          SWP_MOVE);
        }
      // Now try to reload size and position information from INI file
      WinRestoreWindowPos(WinStateSave_Name, WinStateSave_Key, dlg);
      // Now that we have the size and position of the dialog,
      // we can reposition and resize the controls inside it, so it
      // will look nice in every resolution.
      ResizeImportDialogControls(dlg);

      WinSetWindowPos(dlg, HWND_TOP, 0, 0, 0, 0,
                      SWP_SHOW | SWP_ACTIVATE | SWP_ZORDER);
      WinSetFocus(HWND_DESKTOP, dlg);
  
      // Process the dialog!
      if (WinProcessDlg(dlg)==PB_IMPORT)
      {
        pResult = (WaWEImP_Open_Handle_p) malloc(sizeof(WaWEImP_Open_Handle_t));
  
        if (pResult)
        {
          char achEntered[128];
          ULONG ulSelected;

          memset(pResult, 0, sizeof(WaWEImP_Open_Handle_t));
  
          WinQueryDlgItemText(dlg, EF_FREQUENCY, 128, achEntered);
          pResult->iFrequency = atoi(achEntered);
          WinQueryDlgItemText(dlg, EF_CHANNELS, 128, achEntered);
          pResult->iChannels = atoi(achEntered);
          ulSelected = (LONG) WinSendDlgItemMsg(dlg, CX_BITS, LM_QUERYSELECTION, (MPARAM) 0, (MPARAM) 0);
          pResult->iBits = (int) WinSendDlgItemMsg(dlg, CX_BITS, LM_QUERYITEMHANDLE, (MPARAM) ulSelected, (MPARAM) 0);
          ulSelected = (ULONG) WinSendDlgItemMsg(dlg, CX_SIGNED, LM_QUERYSELECTION, (MPARAM) 0, (MPARAM) 0);
          pResult->iIsSigned = (int) WinSendDlgItemMsg(dlg, CX_SIGNED, LM_QUERYITEMHANDLE, (MPARAM) ulSelected, (MPARAM) 0);

          // We don't propose any channel or chanelset names.
          pResult->pchProposedChannelSetName = NULL;
          pResult->apchProposedChannelName = NULL;

          
          // We only set the only-one mandatory format spec, the FourCC value.
          PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat), "FourCC", "RAW");

          pResult->bTempClosed = 0;

          if ((pResult->iFrequency<100) ||
              (pResult->iBits<8) ||
              (pResult->iBits>32) ||
              (pResult->iBits%8 != 0) ||
              (pResult->iChannels<=0) ||
              (pResult->iChannels>64))
          {
            ErrorMsg("Invalid settings!");
            free(pResult); pResult = NULL;
          } else
          {
            pResult->pPluginSpecific = (void *) hFile;
          }
        }
      }
      // Now save size and position info into INI file
      WinStoreWindowPos(WinStateSave_Name, WinStateSave_Key, dlg);
      WinDestroyWindow(dlg);
    }
  }

  if (!pResult)
    fclose(hFile);

  DosReleaseMutexSem(hmtxUsePlugin);

  return pResult;
}

WaWEPosition_t        WAWECALL plugin_Seek(WaWEImP_Open_Handle_p pHandle,
                                           WaWEPosition_t Offset,
                                           int iOrigin)
{
  FILE *hFile;
  int iTranslatedOrigin;
  WaWEPosition_t Result;

  if (!pHandle) return 0;

  if (DosRequestMutexSem(hmtxUsePlugin, SEM_INDEFINITE_WAIT) != NO_ERROR)
  {
#ifdef DEBUG_BUILD
    printf("[plugin_Seek] : IRAW : Could not get mutex semaphore!\n"); fflush(stdout);
#endif
    return 0;
  }

  if (pHandle->bTempClosed)
  {
    DosReleaseMutexSem(hmtxUsePlugin);
    return 0; // Hey, it's TempClosed!
  }

  hFile = (FILE *)(pHandle->pPluginSpecific);
  if (!hFile)
  {
    DosReleaseMutexSem(hmtxUsePlugin);
    return 0;
  }

  iTranslatedOrigin = SEEK_SET;
  if (iOrigin == WAWE_SEEK_CUR) iTranslatedOrigin = SEEK_CUR;
  if (iOrigin == WAWE_SEEK_END) iTranslatedOrigin = SEEK_END;

  fseek(hFile, Offset, iTranslatedOrigin);

  Result = ftell(hFile);

  DosReleaseMutexSem(hmtxUsePlugin);

  return Result;
}

WaWESize_t            WAWECALL plugin_Read(WaWEImP_Open_Handle_p pHandle,
                                           char *pchBuffer,
                                           WaWESize_t cBytes)
{
  FILE *hFile;
  WaWESize_t Result;

  if (!pHandle) return 0;

  if (DosRequestMutexSem(hmtxUsePlugin, SEM_INDEFINITE_WAIT) != NO_ERROR)
  {
#ifdef DEBUG_BUILD
    printf("[plugin_Read] : IRAW : Could not get mutex semaphore!\n"); fflush(stdout);
#endif
    return 0;
  }

  if (pHandle->bTempClosed)
  {
    DosReleaseMutexSem(hmtxUsePlugin);
    return 0; // Hey, it's TempClosed!
  }

  hFile = (FILE *)(pHandle->pPluginSpecific);
  if (!hFile)
  {
    DosReleaseMutexSem(hmtxUsePlugin);
    return 0;
  }

  Result = fread(pchBuffer, 1, cBytes, hFile);
  DosReleaseMutexSem(hmtxUsePlugin);

  return Result;
}

long                  WAWECALL plugin_Close(WaWEImP_Open_Handle_p pHandle)
{
  FILE *hFile;

  if (!pHandle) return 0;

  if (DosRequestMutexSem(hmtxUsePlugin, SEM_INDEFINITE_WAIT) != NO_ERROR)
  {
#ifdef DEBUG_BUILD
    printf("[plugin_Close] : IRAW : Could not get mutex semaphore!\n"); fflush(stdout);
#endif
    return 0;
  }

  if (!(pHandle->bTempClosed))
  {
    hFile = (FILE *)(pHandle->pPluginSpecific);

    if (hFile)
      fclose(hFile);
  }

  // We don't have to free other stuffs in pHandle, because
  // all the pointers we used there were pointing to static data.

  free(pHandle);

  DosReleaseMutexSem(hmtxUsePlugin);
  return 1;
}

long                  WAWECALL plugin_TempClose(WaWEImP_Open_Handle_p pHandle)
{
  FILE *hFile;

  if (!pHandle) return 0;

  if (DosRequestMutexSem(hmtxUsePlugin, SEM_INDEFINITE_WAIT) != NO_ERROR)
  {
#ifdef DEBUG_BUILD
    printf("[plugin_TempClose] : IRAW : Could not get mutex semaphore!\n"); fflush(stdout);
#endif
    return 0;
  }

  if (pHandle->bTempClosed)
  {
    DosReleaseMutexSem(hmtxUsePlugin);
    return 1; // No need to do anything, it's already TempClosed.
  }

  hFile = (FILE *)(pHandle->pPluginSpecific);
  if (!hFile)
  {
    DosReleaseMutexSem(hmtxUsePlugin);
    return 0;
  }

  fclose(hFile);
  pHandle->bTempClosed = 1; // Note that it's temporarily closed

  DosReleaseMutexSem(hmtxUsePlugin);
  
  return 1;
}

long                  WAWECALL plugin_ReOpen(char *pchFileName, WaWEImP_Open_Handle_p pHandle)
{
  FILE *hFile;

  if (!pHandle) return 0;

  if (DosRequestMutexSem(hmtxUsePlugin, SEM_INDEFINITE_WAIT) != NO_ERROR)
  {
#ifdef DEBUG_BUILD
    printf("[plugin_ReOpen] : IRAW : Could not get mutex semaphore!\n"); fflush(stdout);
#endif
    return 0;
  }

  if (!(pHandle->bTempClosed))
  {
    DosReleaseMutexSem(hmtxUsePlugin);
    return 1; // No need to do anything, it's already opened.
  }

  hFile = fopen(pchFileName, "rb");
  pHandle->pPluginSpecific =  hFile;

  // We should update the format now.
  // We only set the only-one mandatory format spec, the FourCC value.
  PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pHandle->ChannelSetFormat), "FourCC", "RAW");

  pHandle->bTempClosed = 0; // Note that it's not closed temporarily anymore!

  DosReleaseMutexSem(hmtxUsePlugin);

  if (!hFile) return 0;
  else
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

  // Use this plugin to open files if no other plugin accepted the file:
  pPluginDesc->iPluginImportance = WAWE_PLUGIN_IMPORTANCE_MIN;

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
