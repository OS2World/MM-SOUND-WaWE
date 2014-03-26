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
 * Clipboard-like processor plugin                                           *
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <process.h>
#include <string.h>
#include <limits.h>

#define INCL_TYPES
#define INCL_WIN
#define INCL_GPI
#define INCL_DOSRESOURCES
#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>

#include "WaWECommon.h"         // Common defines and structures
#include "PClipbrd-Resources.h" // Window resources

#define USE_DOSALLOCMEM

// Global variables
// (Per process, because the DLL is initinstance/terminstance!)
HMODULE hmodOurDLLHandle;
WaWEPluginHelpers_t PluginHelpers;

char *pchPluginName = "~Clipboard plugin";
char *pchAboutTitle = "About Clipboard plugin";
char *pchAboutMessage =
  "Clipboard plugin v1.0 for WaWE\n"
  "Written by Doodle\n"
  "\n"
  "Compilation date: "__DATE__" "__TIME__;


#ifdef USE_DOSALLOCMEM
static void *huge_malloc(size_t size)
{
  APIRET rc;
  void *pResult;

  rc = DosAllocMem(&pResult,
                   size,
                   PAG_READ | PAG_WRITE | PAG_COMMIT | OBJ_ANY);
  if (rc != NO_ERROR)
    return NULL;
  else
    return pResult;
}

static void huge_free(void *pTemp)
{
  DosFreeMem(pTemp);
}
#else

/* We have no special function for huge array allocation, so use normal ones */
#define huge_malloc malloc
#define huge_free free

#endif


// The "Clipboard" itself:
typedef struct ClipboardChannel_t_struct
{
  WaWEFormat_t VirtualFormat;
  WaWESize_t   Length;
  WaWESample_p phSamples;

  void *pNext;
} ClipboardChannel_t, *ClipboardChannel_p;

int iNumOfChannelsOnClipboard = 0;
ClipboardChannel_p pClipboard = NULL;

// Helper functions, called from inside:

void ErrorMsg(char *Msg)
{
#ifdef DEBUG_BUILD
  printf("ErrorMsg: %s\n",Msg); fflush(stdout);
#endif
  WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                Msg, "Clipboard plugin error", 0, MB_OK | MB_MOVEABLE | MB_ICONEXCLAMATION);
}

void WarningMsg(char *Msg)
{
#ifdef DEBUG_BUILD
  printf("WarningMsg: %s\n",Msg); fflush(stdout);
#endif
  WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                Msg, "Clipboard plugin warning", 0, MB_OK | MB_MOVEABLE | MB_ICONEXCLAMATION);
}

void ClearClipboard()
{
  ClipboardChannel_p pToDelete;

  // Free allocated memory (clear clipboard)
  while (pClipboard)
  {
    pToDelete = pClipboard;
    if (pToDelete->phSamples)
      huge_free(pToDelete->phSamples);
    free(pToDelete);
    pClipboard = pClipboard->pNext;
  }
  iNumOfChannelsOnClipboard = 0;
}

typedef struct ProgressIndicator_ID_struct
{
  int iPITState;
  int iPIWState;
  HWND hwndOwner;
  HWND hwndDlg;
} ProgressIndicator_ID_t, *ProgressIndicator_ID;

#define TID_ANIMATIONTIMER 2003

MRESULT EXPENTRY ProgressIndicatorWindowProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  SWP swp;
  char achTemp[128];

  switch(msg)
  {
    case WM_INITDLG:
      {
        ULONG CXDLGFRAME, CYDLGFRAME, CYTITLEBAR;
        CXDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
        CYDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);
        CYTITLEBAR = WinQuerySysValue(HWND_DESKTOP, SV_CYTITLEBAR);

        WinQueryWindowPos(WinWindowFromID(hwnd, ST_WORKINGPLEASEWAIT), &swp);
        WinSetWindowPos(WinWindowFromID(hwnd, ST_PROGRESS),
                        HWND_TOP,
                        2*CXDLGFRAME,
                        2*CYDLGFRAME,
                        swp.cx, swp.cy,
                        SWP_MOVE | SWP_SIZE);
        WinSetWindowPos(WinWindowFromID(hwnd, ST_WORKINGPLEASEWAIT),
                        HWND_TOP,
                        2*CXDLGFRAME, swp.cy+3*CYDLGFRAME,
                        0, 0,
                        SWP_MOVE);
        WinSetWindowPos(hwnd,
                        HWND_TOP,
                        0, 0,
                        swp.cx+4*CXDLGFRAME,
                        2*swp.cy+4*CYDLGFRAME + 2*CYDLGFRAME + CYTITLEBAR,
                        SWP_SIZE);
        WinSetDlgItemText(hwnd, ST_PROGRESS, "....                        ");
        // Ok, stuffs set up!
        return (MRESULT) 0;
      }
    case WM_TIMER:
      {
        if (((int)mp1)==TID_ANIMATIONTIMER)
        {
          char chTemp;
          int i, l;
          WinQueryDlgItemText(hwnd, ST_PROGRESS, 128, &(achTemp[0]));
          l = strlen(achTemp);
          chTemp = achTemp[l-1];
          for (i=l-1; i>0; i--)
            achTemp[i] = achTemp[i-1];
          achTemp[0] = chTemp;
          WinSetDlgItemText(hwnd, ST_PROGRESS, &achTemp[0]);

          return (MRESULT) 0;
        }
        break;
      }
    case WM_CHAR:
      return (MRESULT) 0;
    default:
      break;
  }
  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}

void WindowServerThread(void *pParm)
{
  ProgressIndicator_ID PIID = pParm;
  HAB hab;
  HMQ hmq;
  SWP DlgSwap, ParentSwap;
  unsigned long ulTimerID;

  hab = WinInitialize(0);
  hmq = WinCreateMsgQueue(hab, 0);
  PIID->hwndDlg = WinLoadDlg(HWND_DESKTOP, PIID->hwndOwner,
                             ProgressIndicatorWindowProc, hmodOurDLLHandle,
                             DLG_CLIPBOARDPLUGIN, NULL);

  // Center window to owner
  if (WinQueryWindowPos(PIID->hwndDlg, &DlgSwap))
    if (WinQueryWindowPos(PIID->hwndOwner, &ParentSwap))
    {
      // Center dialog box within the parent window
      int ix, iy;
      ix = ParentSwap.x + (ParentSwap.cx - DlgSwap.cx)/2;
      iy = ParentSwap.y + (ParentSwap.cy - DlgSwap.cy)/2;
      WinSetWindowPos(PIID->hwndDlg, HWND_TOP, ix, iy, 0, 0,
                      SWP_MOVE);
    }

  // Start timer
  ulTimerID = WinStartTimer(hab, PIID->hwndDlg, TID_ANIMATIONTIMER, 100);

  PIID->iPIWState=1; // Note that we're running

  // Hm, WinProcessDlg() always makes window visible...
  //  WinProcessDlg(PIID->hwndDlg);
  {
    QMSG qmsgmsg;
    WinEnableWindow(PIID->hwndOwner, FALSE);
    while (WinGetMsg(hab, &qmsgmsg, 0, 0, 0))
      WinDispatchMsg(hab, &qmsgmsg);
    WinEnableWindow(PIID->hwndOwner, TRUE);
  }

//  // Set focus back to old window
//  WinSetFocus(HWND_DESKTOP, PIID->hwndOwner);

  // Stop timer
  WinStopTimer(hab, PIID->hwndDlg, ulTimerID);

  WinDestroyWindow(PIID->hwndDlg); // Destroy window

  // Uninitialize PM
  WinDestroyMsgQueue(hmq);
  WinTerminate(hab);

  // Indicate that we're stopped
  PIID->iPIWState=-1;
  _endthread();
}

void ProgressIndicatorThread(void *pParm)
{
  ProgressIndicator_ID PIID = pParm;
  TID tidWST; // WindowServerThread
  int i;

  if (PIID)
  {
    tidWST = _beginthread(&WindowServerThread, 0, 16384, pParm);
    DosSetPriority(PRTYS_THREAD, PRTYC_FOREGROUNDSERVER, +1, tidWST);
    // Wait until it reads everything into memory
    while (PIID->iPIWState==0) DosSleep(64);

    i=0;
    while ((PIID->iPITState==0) && (i<15))
    {
      DosSleep(32); i++;
    }
    if (PIID->iPITState==0)
    {
#ifdef DEBUG_BUILD
      printf("Showing progress window...\n"); fflush(stdout);
#endif
      WinSetWindowPos(PIID->hwndDlg, HWND_TOP, 0, 0, 0, 0,
                      SWP_SHOW | SWP_ACTIVATE | SWP_ZORDER);

      while (PIID->iPITState==0)
      {
        DosSleep(32);
      }
    }
    WinPostMsg(PIID->hwndDlg, WM_QUIT, 0, 0);
    // Wait until the Window thread dies
    while (PIID->iPIWState!=-1) DosSleep(32);
  }
  PIID->iPITState=-1;
  _endthread();
}

ProgressIndicator_ID ProgressIndicator_Start(HWND hwndOwner)
{
  ProgressIndicator_ID pResult = NULL;

  pResult = (ProgressIndicator_ID) malloc(sizeof(ProgressIndicator_ID_t));
  if (pResult)
  {
    TID tidPI;
    pResult->iPITState = 0;
    pResult->iPIWState = 0;
    pResult->hwndOwner = hwndOwner;
    tidPI = _beginthread(&ProgressIndicatorThread, 0, 16384, (void *)pResult);
    DosSetPriority(PRTYS_THREAD, PRTYC_FOREGROUNDSERVER, +1, tidPI);
  }
  return pResult;
}

void ProgressIndicator_Stop(ProgressIndicator_ID PIID)
{
  if (PIID)
  {
    if (PIID->iPITState == 0)
    {
      PIID->iPITState = 1;
      while (PIID->iPITState != -1) DosSleep(32);
    }
  }
}

int CopyOrCut_WholeChannel(WaWEState_p pWaWEState, HWND hwndOwner, int bCut)
{
  WaWEChannel_p pChannel;
  WaWESize_t    CouldRead, CouldCut;
  ProgressIndicator_ID  PIID;

  if (!pWaWEState) return 0;
  if (!pWaWEState->pCurrentChannel) return 0;

  ClearClipboard();
  pChannel = pWaWEState->pCurrentChannel;

  pClipboard = (ClipboardChannel_p) malloc(sizeof(ClipboardChannel_t));
  if (!pClipboard)
  {
    ErrorMsg("Out of memory!\nOperation cancelled.");
    return 0;
  }

  if (pChannel->Length<=0)
  {
    pClipboard->phSamples = NULL;
  } else
  {
    pClipboard->phSamples = (WaWESample_p) huge_malloc(pChannel->Length * sizeof(WaWESample_t));
    if (!(pClipboard->phSamples))
    {
      free(pClipboard); pClipboard = NULL;
      ErrorMsg("Out of memory!\nOperation cancelled.");
      return 0;
    }
    memset(pClipboard->phSamples, 0, pChannel->Length * sizeof(WaWESample_t));
  }

  memcpy(&(pClipboard->VirtualFormat), &(pChannel->VirtualFormat), sizeof(WaWEFormat_t));
  pClipboard->Length = pChannel->Length;
  pClipboard->pNext = NULL;
  iNumOfChannelsOnClipboard++;

  PIID = ProgressIndicator_Start(hwndOwner);

  if (pChannel->Length>0)
  {
    PluginHelpers.fn_WaWEHelper_Channel_ReadSample(pChannel,
                                                   0,
                                                   pClipboard->Length,
                                                   pClipboard->phSamples,
                                                   &CouldRead);
    if ((CouldRead>0) && (bCut))
      PluginHelpers.fn_WaWEHelper_Channel_DeleteSample(pChannel,
                                                       0,
                                                       pClipboard->Length,
                                                       &CouldCut);
  }

  ProgressIndicator_Stop(PIID);

  if (CouldRead<=0)
  {
    huge_free(pClipboard->phSamples);
    free(pClipboard); pClipboard = NULL;
    ErrorMsg("Error reading samples!\nOperation cancelled.");
    return 0;
  }

  return 1;
}

int CopyOrCut_WholeChannelSet(WaWEState_p pWaWEState, HWND hwndOwner, int bCut)
{
  ClipboardChannel_p pNewClipboardChn, pLastClipboardChn;
  WaWEChannel_p pChannel;
  WaWESize_t    CouldRead, CouldCut;
  ProgressIndicator_ID  PIID;

  if (!pWaWEState) return 0;
  if (!pWaWEState->pCurrentChannelSet) return 0;

  if (DosRequestMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList, SEM_INDEFINITE_WAIT)!=NO_ERROR)
    return 0;

  PIID = ProgressIndicator_Start(hwndOwner);

  ClearClipboard();

  pChannel = pWaWEState->pCurrentChannelSet->pChannelListHead;

  pLastClipboardChn = NULL;

  while (pChannel)
  {
    pNewClipboardChn = (ClipboardChannel_p) malloc(sizeof(ClipboardChannel_t));
    if (!pNewClipboardChn)
    {
      ProgressIndicator_Stop(PIID);
      ErrorMsg("Out of memory!\nOperation cancelled.");
      return 0;
    }
    if (pChannel->Length<=0)
    {
      pNewClipboardChn->phSamples = NULL;
    } else
    {
      pNewClipboardChn->phSamples = (WaWESample_p) huge_malloc(pChannel->Length * sizeof(WaWESample_t));
      if (!(pNewClipboardChn->phSamples))
      {
        DosReleaseMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList);
        free(pNewClipboardChn); pNewClipboardChn = NULL;
        ProgressIndicator_Stop(PIID);
        ErrorMsg("Out of memory!\nOperation cancelled.");
        return 0;
      }
      memset(pNewClipboardChn->phSamples, 0, pChannel->Length * sizeof(WaWESample_t));
    }
    memcpy(&(pNewClipboardChn->VirtualFormat), &(pChannel->VirtualFormat), sizeof(WaWEFormat_t));
    pNewClipboardChn->Length = pChannel->Length;
    pNewClipboardChn->pNext = NULL;

    if (pChannel->Length>0)
    {
      PluginHelpers.fn_WaWEHelper_Channel_ReadSample(pChannel,
                                                     0,
                                                     pNewClipboardChn->Length,
                                                     pNewClipboardChn->phSamples,
                                                     &CouldRead);
      if ((CouldRead>0) && (bCut))
        PluginHelpers.fn_WaWEHelper_Channel_DeleteSample(pChannel,
                                                         0,
                                                         pNewClipboardChn->Length,
                                                         &CouldCut);

    }

    if (CouldRead<=0)
    {
      DosReleaseMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList);
      huge_free(pNewClipboardChn->phSamples);
      free(pNewClipboardChn); pNewClipboardChn = NULL;
      ProgressIndicator_Stop(PIID);
      ErrorMsg("Error reading samples!\nOperation cancelled.");
      return 0;
    }

    iNumOfChannelsOnClipboard++;
    if (pLastClipboardChn)
      pLastClipboardChn->pNext = pNewClipboardChn;
    else
      pClipboard = pNewClipboardChn;
    pLastClipboardChn = pNewClipboardChn;

    pChannel = pChannel->pNext;
  }
  ProgressIndicator_Stop(PIID);

  DosReleaseMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList);
  return 1;
}

int CopyOrCut_SelectedChannelData(WaWEState_p pWaWEState, HWND hwndOwner, int bCut)
{
  WaWEChannel_p pChannel;
  WaWESize_t    CouldRead, CouldCut;
  ProgressIndicator_ID  PIID;

  if (!pWaWEState) return 0;
  if (!pWaWEState->pCurrentChannel) return 0;
  if (pWaWEState->pCurrentChannel->ShownSelectedLength<=0) return 0;

  ClearClipboard();
  pChannel = pWaWEState->pCurrentChannel;

  pClipboard = (ClipboardChannel_p) malloc(sizeof(ClipboardChannel_t));
  if (!pClipboard)
  {
    ErrorMsg("Out of memory!\nOperation cancelled.");
    return 0;
  }

  memcpy(&(pClipboard->VirtualFormat), &(pChannel->VirtualFormat), sizeof(WaWEFormat_t));
  pClipboard->Length = pChannel->ShownSelectedLength;
  pClipboard->pNext = NULL;
  iNumOfChannelsOnClipboard++;

  if (pChannel->ShownSelectedLength<=0)
  {
    pClipboard->phSamples = NULL;
  } else
  {
    pClipboard->phSamples = (WaWESample_p) huge_malloc(pChannel->ShownSelectedLength * sizeof(WaWESample_t));
    if (!(pClipboard->phSamples))
    {
      free(pClipboard); pClipboard = NULL;
      ErrorMsg("Out of memory!\nOperation cancelled.");
      return 0;
    }
    memset(pClipboard->phSamples, 0, pChannel->ShownSelectedLength * sizeof(WaWESample_t));
  }

  PIID = ProgressIndicator_Start(hwndOwner);

  if (pChannel->Length>0)
  {
    WaWESelectedRegion_p pSelectedRegion;
    WaWEPosition_t CurrPos=0;

    pSelectedRegion = pChannel->pShownSelectedRegionListHead;
    CouldRead=1;
    while ((pSelectedRegion) && (CouldRead>0))
    {
      PluginHelpers.fn_WaWEHelper_Channel_ReadSample(pChannel,
                                                     pSelectedRegion->Start,
                                                     pSelectedRegion->End - pSelectedRegion->Start + 1,
                                                     &(pClipboard->phSamples[CurrPos]),
                                                     &CouldRead);
      if ((CouldRead>0) && (bCut))
	PluginHelpers.fn_WaWEHelper_Channel_DeleteSample(pChannel,
                                                         pSelectedRegion->Start,
                                                         pSelectedRegion->End - pSelectedRegion->Start + 1,
                                                         &CouldCut);

      CurrPos+=CouldRead;
      pSelectedRegion = pSelectedRegion->pNext;
    }
  }

  ProgressIndicator_Stop(PIID);

  if (CouldRead<=0)
  {
    if (pClipboard->phSamples)
      huge_free(pClipboard->phSamples);
    free(pClipboard); pClipboard = NULL;
    ErrorMsg("Error reading samples!\nOperation cancelled.");
    return 0;
  }

  return 1;
}

int CopyOrCut_SelectedChannelSetData(WaWEState_p pWaWEState, HWND hwndOwner, int bCut)
{
  ClipboardChannel_p pNewClipboardChn, pLastClipboardChn;
  WaWEChannel_p pChannel;
  WaWESize_t    CouldRead, CouldCut;
  ProgressIndicator_ID  PIID;

  if (!pWaWEState) return 0;
  if (!pWaWEState->pCurrentChannelSet) return 0;

  if (DosRequestMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList, SEM_INDEFINITE_WAIT)!=NO_ERROR)
    return 0;

#ifdef DEBUG_BUILD
//  printf("[CopyOrCut_SelectedChannelSetData] : ProgressIndicator_Start()\n");
#endif
  PIID = ProgressIndicator_Start(hwndOwner);

#ifdef DEBUG_BUILD
//  printf("[CopyOrCut_SelectedChannelSetData] : ClearClipboard()\n");
#endif

  ClearClipboard();
  pChannel = pWaWEState->pCurrentChannelSet->pChannelListHead;

  pLastClipboardChn = NULL;

  while (pChannel)
  {
    pNewClipboardChn = (ClipboardChannel_p) malloc(sizeof(ClipboardChannel_t));
    if (!pNewClipboardChn)
    {
      DosReleaseMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList);
      ProgressIndicator_Stop(PIID);
      ErrorMsg("Out of memory!\nOperation cancelled.");
      return 0;
    }
    if (pChannel->ShownSelectedLength<=0)
    {
      pNewClipboardChn->phSamples = NULL;
    } else
    {
      pNewClipboardChn->phSamples = (WaWESample_p) huge_malloc(pChannel->ShownSelectedLength * sizeof(WaWESample_t));
      if (!(pNewClipboardChn->phSamples))
      {
        DosReleaseMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList);
        free(pNewClipboardChn); pNewClipboardChn = NULL;
        ProgressIndicator_Stop(PIID);
        ErrorMsg("Out of memory!\nOperation cancelled.");
        return 0;
      }
      memset(pNewClipboardChn->phSamples, 0, pChannel->ShownSelectedLength * sizeof(WaWESample_t));
    }
    memcpy(&(pNewClipboardChn->VirtualFormat), &(pChannel->VirtualFormat), sizeof(WaWEFormat_t));
    pNewClipboardChn->Length = pChannel->ShownSelectedLength;
    pNewClipboardChn->pNext = NULL;

    if (pChannel->ShownSelectedLength>0)
    {
      WaWEPosition_t CurrPos = 0;
      WaWESelectedRegion_p pSelectedRegion;

      pSelectedRegion = pChannel->pShownSelectedRegionListHead;
      CouldRead = 1;
      while ((pSelectedRegion) && (CouldRead>0))
      {
#ifdef DEBUG_BUILD
//        printf("[CopyOrCut_SelectedChannelSetData] : ReadSample() call\n");
#endif

	PluginHelpers.fn_WaWEHelper_Channel_ReadSample(pChannel,
                                                       pSelectedRegion->Start,
                                                       pSelectedRegion->End - pSelectedRegion->Start + 1,
                                                       &(pNewClipboardChn->phSamples[CurrPos]),
                                                       &CouldRead);
#ifdef DEBUG_BUILD
//        printf("[CopyOrCut_SelectedChannelSetData] : ReadSample() done\n");
#endif


	if ((CouldRead>0) && (bCut))
	  PluginHelpers.fn_WaWEHelper_Channel_DeleteSample(pChannel,
                                                           pSelectedRegion->Start,
                                                           pSelectedRegion->End - pSelectedRegion->Start + 1,
                                                           &CouldCut);

        CurrPos+=CouldRead;
        pSelectedRegion = pSelectedRegion->pNext;
      }
    }

    if (CouldRead<=0)
    {
      DosReleaseMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList);
      if (pNewClipboardChn->phSamples)
	huge_free(pNewClipboardChn->phSamples);
      free(pNewClipboardChn); pNewClipboardChn = NULL;
      ProgressIndicator_Stop(PIID);
      ErrorMsg("Error reading samples!\nOperation cancelled.");
      return 0;
    }

    iNumOfChannelsOnClipboard++;
    if (pLastClipboardChn)
      pLastClipboardChn->pNext = pNewClipboardChn;
    else
      pClipboard = pNewClipboardChn;
    pLastClipboardChn = pNewClipboardChn;

    pChannel = pChannel->pNext;
  }
#ifdef DEBUG_BUILD
//  printf("[CopyOrCut_SelectedChannelSetData] : ProgressIndicator_Stop()\n");
#endif
  ProgressIndicator_Stop(PIID);
#ifdef DEBUG_BUILD
//  printf("[CopyOrCut_SelectedChannelSetData] : Return 1\n");
#endif

  DosReleaseMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList);
  return 1;
}

int Delete_WholeChannel(WaWEState_p pWaWEState, HWND hwndOwner)
{
  WaWEChannel_p pChannel;
  WaWESize_t    CouldCut;
  ProgressIndicator_ID  PIID;

  if (!pWaWEState) return 0;
  if (!pWaWEState->pCurrentChannel) return 0;

  pChannel = pWaWEState->pCurrentChannel;

  PIID = ProgressIndicator_Start(hwndOwner);

  if (pChannel->Length>0)
  {
      PluginHelpers.fn_WaWEHelper_Channel_DeleteSample(pChannel,
                                                       0,
                                                       pChannel->Length,
                                                       &CouldCut);
  }

  ProgressIndicator_Stop(PIID);

  return 1;
}

int Delete_WholeChannelSet(WaWEState_p pWaWEState, HWND hwndOwner)
{
  WaWEChannel_p pChannel;
  WaWESize_t    CouldCut;
  ProgressIndicator_ID  PIID;

  if (!pWaWEState) return 0;
  if (!pWaWEState->pCurrentChannelSet) return 0;

  if (DosRequestMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList, SEM_INDEFINITE_WAIT)!=NO_ERROR)
    return 0;

  PIID = ProgressIndicator_Start(hwndOwner);

  pChannel = pWaWEState->pCurrentChannelSet->pChannelListHead;

  while (pChannel)
  {
    if (pChannel->Length>0)
    {
        PluginHelpers.fn_WaWEHelper_Channel_DeleteSample(pChannel,
                                                         0,
                                                         pChannel->Length,
                                                         &CouldCut);

    }

    pChannel = pChannel->pNext;
  }
  ProgressIndicator_Stop(PIID);

  DosReleaseMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList);
  return 1;
}

int Delete_SelectedChannelData(WaWEState_p pWaWEState, HWND hwndOwner)
{
  WaWEChannel_p pChannel;
  WaWESelectedRegion_p pSelectedRegion;
  WaWESize_t    CouldCut;
  ProgressIndicator_ID  PIID;

  if (!pWaWEState) return 0;
  if (!pWaWEState->pCurrentChannel) return 0;
  if (pWaWEState->pCurrentChannel->ShownSelectedLength<=0) return 0;

  pChannel = pWaWEState->pCurrentChannel;

  PIID = ProgressIndicator_Start(hwndOwner);

  pSelectedRegion = pChannel->pShownSelectedRegionListHead;
  while (pSelectedRegion)
  {
    PluginHelpers.fn_WaWEHelper_Channel_DeleteSample(pChannel,
                                                     pSelectedRegion->Start,
                                                     pSelectedRegion->End - pSelectedRegion->Start + 1,
                                                     &CouldCut);

    pSelectedRegion = pSelectedRegion->pNext;
  }

  ProgressIndicator_Stop(PIID);

  return 1;
}

int Delete_SelectedChannelSetData(WaWEState_p pWaWEState, HWND hwndOwner)
{
  WaWEChannel_p pChannel;
  WaWESize_t    CouldCut;
  ProgressIndicator_ID  PIID;

  if (!pWaWEState) return 0;
  if (!pWaWEState->pCurrentChannelSet) return 0;

  if (DosRequestMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList, SEM_INDEFINITE_WAIT)!=NO_ERROR)
    return 0;

  PIID = ProgressIndicator_Start(hwndOwner);

  pChannel = pWaWEState->pCurrentChannelSet->pChannelListHead;

  while (pChannel)
  {
    WaWESelectedRegion_p pSelectedRegion;

    pSelectedRegion = pChannel->pShownSelectedRegionListHead;
    while (pSelectedRegion)
    {
      PluginHelpers.fn_WaWEHelper_Channel_DeleteSample(pChannel,
                                                       pSelectedRegion->Start,
                                                       pSelectedRegion->End - pSelectedRegion->Start + 1,
                                                       &CouldCut);

      pSelectedRegion = pSelectedRegion->pNext;
    }

    pChannel = pChannel->pNext;
  }

  ProgressIndicator_Stop(PIID);

  DosReleaseMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList);
  return 1;
}

int Paste(WaWEState_p pWaWEState, HWND hwndOwner)
{
  WaWEChannel_p pChannel;
  WaWESize_t    CouldInsert;
  ProgressIndicator_ID  PIID;
  int           rc;
  ClipboardChannel_p pClipboardChannel;

  if (!pWaWEState) return 0;
  if (!pClipboard)
  {
    ErrorMsg("Clipboard is empty!\nOperation cancelled.");
    return 0;
  }

  // Ok, now let's see if we should paste into channel or channel-set!
  if (iNumOfChannelsOnClipboard==1)
  {
    // Paste one channel only!
    if (!pWaWEState->pCurrentChannel)
    {
      ErrorMsg("There is no current channel to paste into!\nOperation cancelled.");
      return 0;
    }
    pChannel = pWaWEState->pCurrentChannel;

    PIID = ProgressIndicator_Start(hwndOwner);

    rc = PluginHelpers.fn_WaWEHelper_Channel_InsertSample(pChannel,
                                                          pChannel->CurrentPosition,
                                                          pClipboard->Length,
                                                          pClipboard->phSamples,
                                                          &CouldInsert);
    ProgressIndicator_Stop(PIID);

    if (rc!=WAWEHELPER_NOERROR)
    {
      ErrorMsg("Could not insert samples into current channel!");
      return 0;
    }
  } else
  {
    // Paste more than one channels!
    if (!pWaWEState->pCurrentChannelSet)
    {
      ErrorMsg("There is no current channel-set to paste into!\nOperation cancelled.");
      return 0;
    }

    if (DosRequestMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList, SEM_INDEFINITE_WAIT)!=NO_ERROR)
    {
      ErrorMsg("Internal error!\nOperation cancelled.");
      return 0;
    }

    pChannel = pWaWEState->pCurrentChannelSet->pChannelListHead;
    pClipboardChannel = pClipboard;

    PIID = ProgressIndicator_Start(hwndOwner);

    while ((pClipboardChannel) && (pChannel))
    {
      if (pClipboardChannel->phSamples)
	rc = PluginHelpers.fn_WaWEHelper_Channel_InsertSample(pChannel,
                                                              pChannel->CurrentPosition,
                                                              pClipboardChannel->Length,
                                                              pClipboardChannel->phSamples,
                                                              &CouldInsert);
      else
	rc = WAWEHELPER_NOERROR;

      if (rc!=WAWEHELPER_NOERROR)
      {
        ProgressIndicator_Stop(PIID);

        DosReleaseMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList);
        ErrorMsg("Could not insert samples into channel!");
        return 0;
      }

      pClipboardChannel = pClipboardChannel->pNext;
      pChannel = pChannel->pNext;
    }

    if ((pClipboardChannel) && (!pChannel))
    {
      ProgressIndicator_Stop(PIID);

      DosReleaseMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList);

      WarningMsg("There are less channels in current channel-set than in clipboard.\nOperation only partially done.");
      return 1;
    }

    ProgressIndicator_Stop(PIID);

    DosReleaseMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList);
  }
  return 1;
}

// Functions called from outside:

// Define menu IDs. Note, that the ID of 0 is reserved for WAWE_POPUPMENU_ID_ROOT!
#define POPUPMENU_ID_EDIT                             1

#define POPUPMENU_ID_COPY                             2
#define POPUPMENU_ID_COPY_SELECTEDCHANNELDATA         3
#define POPUPMENU_ID_COPY_SELECTEDCHANNELSETDATA      4
#define POPUPMENU_ID_COPY_WHOLECHANNEL                5
#define POPUPMENU_ID_COPY_WHOLECHANNELSET             6

#define POPUPMENU_ID_CUT                              7
#define POPUPMENU_ID_CUT_SELECTEDCHANNELDATA          8
#define POPUPMENU_ID_CUT_SELECTEDCHANNELSETDATA       9
#define POPUPMENU_ID_CUT_WHOLECHANNEL                10
#define POPUPMENU_ID_CUT_WHOLECHANNELSET             11

#define POPUPMENU_ID_DELETE                          12
#define POPUPMENU_ID_DELETE_SELECTEDCHANNELDATA      13
#define POPUPMENU_ID_DELETE_SELECTEDCHANNELSETDATA   14
#define POPUPMENU_ID_DELETE_WHOLECHANNEL             15
#define POPUPMENU_ID_DELETE_WHOLECHANNELSET          16

#define POPUPMENU_ID_PASTE                           17


static int NumOfChannelsWithSelectedAreasInChannelSet(WaWEState_p pWaWEState, WaWEChannelSet_p pChannelSet)
{
  int iResult = 0;
  WaWEChannel_p pChannel;

  if ((!pWaWEState) || (!pChannelSet)) return 0;

  if (DosRequestMutexSem(pChannelSet->hmtxUseChannelList, SEM_INDEFINITE_WAIT)!=NO_ERROR)
    return 0;

  pChannel = pChannelSet->pChannelListHead;
  while (pChannel)
  {
    if (pChannel->pLastValidSelectedRegionListHead)
      iResult++;
    pChannel = pChannel->pNext;
  }

  DosReleaseMutexSem(pChannelSet->hmtxUseChannelList);

  return iResult;
}


#define ALLOCATE_MEMORY_FOR_NEW_MENUENTRY()                                                                                                   \
          (*piNumOfPopupMenuEntries)++;                                                                                                   \
          if ((*piNumOfPopupMenuEntries)==1)                                                                                              \
          {                                                                                                                               \
            /* First entry, so allocate memory */                                                                                         \
            pEntries = (WaWEPopupMenuEntries_p) malloc((*piNumOfPopupMenuEntries) * sizeof(WaWEPopupMenuEntries_t));                      \
            if (!pEntries)                                                                                                                \
            {                                                                                                                             \
              *ppPopupMenuEntries = pEntries;                                                                                             \
              *piNumOfPopupMenuEntries = 0;                                                                                               \
              return 0;                                                                                                                   \
            }                                                                                                                             \
            *ppPopupMenuEntries = pEntries;                                                                                               \
          } else                                                                                                                          \
          {                                                                                                                               \
            /* Not the first entry, so reallocate memory */                                                                               \
            pEntries = (WaWEPopupMenuEntries_p) realloc(*ppPopupMenuEntries, (*piNumOfPopupMenuEntries) * sizeof(WaWEPopupMenuEntries_t));\
            if (!pEntries)                                                                                                                \
            {                                                                                                                             \
              free(*ppPopupMenuEntries); *ppPopupMenuEntries = NULL;                                                                      \
              *piNumOfPopupMenuEntries = 0;                                                                                               \
              return 0;                                                                                                                   \
            }                                                                                                                             \
            *ppPopupMenuEntries = pEntries;                                                                                               \
                                                                                                                                          \
          }


int  WAWECALL plugin_GetPopupMenuEntries(int iPopupMenuID,
                                         WaWEState_p pWaWEState,
                                         int *piNumOfPopupMenuEntries,
                                         WaWEPopupMenuEntries_p *ppPopupMenuEntries)
{
  int iEntryNum;
#ifdef DEBUG_BUILD
//  printf("[GetPopupMenuEntries] : PClipbrd: Enter, id = %d\n", iPopupMenuID); fflush(stdout);
#endif
  // Check parameters!
  if ((!pWaWEState) || (!piNumOfPopupMenuEntries) ||
      (!ppPopupMenuEntries))
    return 0;

    // We cannot do anything if there is no current channel or channelset!
  if ((!(pWaWEState->pCurrentChannel)) &&
      (!(pWaWEState->pCurrentChannelSet)))
    return 0;

  switch (iPopupMenuID)
  {
    case WAWE_POPUPMENU_ID_ROOT:
      {
        // Ok, WaWE asks what menu items we want into first popup menu!
        (*piNumOfPopupMenuEntries) = 1;
        *ppPopupMenuEntries = (WaWEPopupMenuEntries_p) malloc((*piNumOfPopupMenuEntries) * sizeof(WaWEPopupMenuEntries_t));
        if (!(*ppPopupMenuEntries))
        {
#ifdef DEBUG_BUILD
          printf("[GetPopupMenuEntries] : Warning, out of memory!\n"); fflush(stdout);
#endif
          return 0;
        }

        (*ppPopupMenuEntries)[0].pchEntryText = "~Edit";
        (*ppPopupMenuEntries)[0].iEntryID     = POPUPMENU_ID_EDIT;
        (*ppPopupMenuEntries)[0].iEntryStyle  = WAWE_POPUPMENU_STYLE_SUBMENU;

        break;
      }
    case POPUPMENU_ID_EDIT:
      {
        // Ok, WaWE asks what menu items we want into 'EDIT' popup menu!
        (*piNumOfPopupMenuEntries) = 0;
        // If there is a current channel-set, then
        // we'll have at least Copy and Cut and Delete, so minimum 3
        if (pWaWEState->pCurrentChannelSet)
          (*piNumOfPopupMenuEntries)+=3;
        // We'll also have Paste menu if there is something on clipboard
        if (iNumOfChannelsOnClipboard>0)
          (*piNumOfPopupMenuEntries)++;

        // If there is nothing we want to insert to root, then return with that result!
        if ((*piNumOfPopupMenuEntries) == 0)
        {
          *ppPopupMenuEntries = NULL;
          return 0;
        }

        *ppPopupMenuEntries = (WaWEPopupMenuEntries_p) malloc((*piNumOfPopupMenuEntries) * sizeof(WaWEPopupMenuEntries_t));
        if (!(*ppPopupMenuEntries))
        {
#ifdef DEBUG_BUILD
          printf("[GetPopupMenuEntries] : Warning, out of memory!\n"); fflush(stdout);
#endif
          return 0;
        }

        iEntryNum = 0;

        if (pWaWEState->pCurrentChannelSet)
        {
          (*ppPopupMenuEntries)[iEntryNum].pchEntryText = "~Copy";
          (*ppPopupMenuEntries)[iEntryNum].iEntryID     = POPUPMENU_ID_COPY;
          (*ppPopupMenuEntries)[iEntryNum].iEntryStyle  = WAWE_POPUPMENU_STYLE_SUBMENU;
          iEntryNum++;

          (*ppPopupMenuEntries)[iEntryNum].pchEntryText = "C~ut";
          (*ppPopupMenuEntries)[iEntryNum].iEntryID     = POPUPMENU_ID_CUT;
          (*ppPopupMenuEntries)[iEntryNum].iEntryStyle  = WAWE_POPUPMENU_STYLE_SUBMENU;
          iEntryNum++;

          (*ppPopupMenuEntries)[iEntryNum].pchEntryText = "~Delete";
          (*ppPopupMenuEntries)[iEntryNum].iEntryID     = POPUPMENU_ID_DELETE;
          (*ppPopupMenuEntries)[iEntryNum].iEntryStyle  = WAWE_POPUPMENU_STYLE_SUBMENU;
          iEntryNum++;
        }

        if (iNumOfChannelsOnClipboard>0)
        {
          // There is something on clipboard
          (*ppPopupMenuEntries)[iEntryNum].pchEntryText = "~Paste";
          (*ppPopupMenuEntries)[iEntryNum].iEntryID     = POPUPMENU_ID_PASTE;
          (*ppPopupMenuEntries)[iEntryNum].iEntryStyle  = WAWE_POPUPMENU_STYLE_MENUITEM;
          iEntryNum++;
        }
        break;
      }
    case POPUPMENU_ID_COPY:
      {
        WaWEPopupMenuEntries_p pEntries;
        // Ok, WaWE asks what menu items we want into our 'Copy' menu!
        // Let's see!

        *piNumOfPopupMenuEntries = 0;

        // If there is a current channel, and there is a selected area in there, then we
        // offer a menu item of copy of selected area from current channel
        if ((pWaWEState->pCurrentChannel) && (pWaWEState->pCurrentChannel->pLastValidSelectedRegionListHead))
        {
          ALLOCATE_MEMORY_FOR_NEW_MENUENTRY();
          // Ok, memory is available, now fill entries!
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].pchEntryText = "~selected area of Channel";
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryID     = POPUPMENU_ID_COPY_SELECTEDCHANNELDATA;
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryStyle  = WAWE_POPUPMENU_STYLE_MENUITEM;
        }
        // If there is a current channel, then we
        // offer a menu item of copy of the whole current channel
        if (pWaWEState->pCurrentChannel)
        {
          ALLOCATE_MEMORY_FOR_NEW_MENUENTRY();
          // Ok, memory is available, now fill entries!
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].pchEntryText = "~whole Channel";
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryID     = POPUPMENU_ID_COPY_WHOLECHANNEL;
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryStyle  = WAWE_POPUPMENU_STYLE_MENUITEM;
        }
        // If there is a current channelset, and there is a selected area in there, then we
        // offer a menu item of copy of selected area from current channel
        if ((pWaWEState->pCurrentChannelSet) && (NumOfChannelsWithSelectedAreasInChannelSet(pWaWEState, pWaWEState->pCurrentChannelSet)>0))
        {
          ALLOCATE_MEMORY_FOR_NEW_MENUENTRY();
          // Ok, memory is available, now fill entries!
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].pchEntryText = "~selected area of Channel-Set";
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryID     = POPUPMENU_ID_COPY_SELECTEDCHANNELSETDATA;
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryStyle  = WAWE_POPUPMENU_STYLE_MENUITEM;
        }
        // If there is a current channelset, then we
        // offer a menu item of copy of the whole current channelset
        if (pWaWEState->pCurrentChannelSet)
        {
          ALLOCATE_MEMORY_FOR_NEW_MENUENTRY();
          // Ok, memory is available, now fill entries!
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].pchEntryText = "~whole Channel-Set";
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryID     = POPUPMENU_ID_COPY_WHOLECHANNELSET;
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryStyle  = WAWE_POPUPMENU_STYLE_MENUITEM;
        }
        break;
      }
    case POPUPMENU_ID_CUT:
      {
        WaWEPopupMenuEntries_p pEntries;
        // Ok, WaWE asks what menu items we want into our 'Cut' menu!
        // Let's see!

        *piNumOfPopupMenuEntries = 0;

        // If there is a current channel, and there is a selected area in there, then we
        // offer a menu item of cut of selected area from current channel
        if ((pWaWEState->pCurrentChannel) && (pWaWEState->pCurrentChannel->pLastValidSelectedRegionListHead))
        {
          ALLOCATE_MEMORY_FOR_NEW_MENUENTRY();
          // Ok, memory is available, now fill entries!
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].pchEntryText = "~selected area of Channel";
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryID     = POPUPMENU_ID_CUT_SELECTEDCHANNELDATA;
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryStyle  = WAWE_POPUPMENU_STYLE_MENUITEM;
        }
        // If there is a current channel, then we
        // offer a menu item of cut of the whole current channel
        if (pWaWEState->pCurrentChannel)
        {
          ALLOCATE_MEMORY_FOR_NEW_MENUENTRY();
          // Ok, memory is available, now fill entries!
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].pchEntryText = "~whole Channel";
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryID     = POPUPMENU_ID_CUT_WHOLECHANNEL;
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryStyle  = WAWE_POPUPMENU_STYLE_MENUITEM;
        }
        // If there is a current channelset, and there is a selected area in there, then we
        // offer a menu item of cut of selected area from current channel
        if ((pWaWEState->pCurrentChannelSet) && (NumOfChannelsWithSelectedAreasInChannelSet(pWaWEState, pWaWEState->pCurrentChannelSet)>0))
        {
          ALLOCATE_MEMORY_FOR_NEW_MENUENTRY();
          // Ok, memory is available, now fill entries!
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].pchEntryText = "~selected area of Channel-Set";
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryID     = POPUPMENU_ID_CUT_SELECTEDCHANNELSETDATA;
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryStyle  = WAWE_POPUPMENU_STYLE_MENUITEM;
        }
        // If there is a current channelset, then we
        // offer a menu item of cut of the whole current channelset
        if (pWaWEState->pCurrentChannelSet)
        {
          ALLOCATE_MEMORY_FOR_NEW_MENUENTRY();
          // Ok, memory is available, now fill entries!
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].pchEntryText = "~whole Channel-Set";
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryID     = POPUPMENU_ID_CUT_WHOLECHANNELSET;
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryStyle  = WAWE_POPUPMENU_STYLE_MENUITEM;
        }
        break;
      }
    case POPUPMENU_ID_DELETE:
      {
        WaWEPopupMenuEntries_p pEntries;
        // Ok, WaWE asks what menu items we want into our 'Delete' menu!
        // Let's see!

        *piNumOfPopupMenuEntries = 0;

        // If there is a current channel, and there is a selected area in there, then we
        // offer a menu item of deletion of selected area from current channel
        if ((pWaWEState->pCurrentChannel) && (pWaWEState->pCurrentChannel->pLastValidSelectedRegionListHead))
        {
          ALLOCATE_MEMORY_FOR_NEW_MENUENTRY();
          // Ok, memory is available, now fill entries!
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].pchEntryText = "~selected area of Channel";
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryID     = POPUPMENU_ID_DELETE_SELECTEDCHANNELDATA;
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryStyle  = WAWE_POPUPMENU_STYLE_MENUITEM;
        }
        // If there is a current channel, then we
        // offer a menu item of cut of the whole current channel
        if (pWaWEState->pCurrentChannel)
        {
          ALLOCATE_MEMORY_FOR_NEW_MENUENTRY();
          // Ok, memory is available, now fill entries!
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].pchEntryText = "~whole Channel";
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryID     = POPUPMENU_ID_DELETE_WHOLECHANNEL;
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryStyle  = WAWE_POPUPMENU_STYLE_MENUITEM;
        }
        // If there is a current channelset, and there is a selected area in there, then we
        // offer a menu item of cut of selected area from current channel
        if ((pWaWEState->pCurrentChannelSet) && (NumOfChannelsWithSelectedAreasInChannelSet(pWaWEState, pWaWEState->pCurrentChannelSet)>0))
        {
          ALLOCATE_MEMORY_FOR_NEW_MENUENTRY();
          // Ok, memory is available, now fill entries!
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].pchEntryText = "~selected area of Channel-Set";
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryID     = POPUPMENU_ID_DELETE_SELECTEDCHANNELSETDATA;
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryStyle  = WAWE_POPUPMENU_STYLE_MENUITEM;
        }
        // If there is a current channelset, then we
        // offer a menu item of cut of the whole current channelset
        if (pWaWEState->pCurrentChannelSet)
        {
          ALLOCATE_MEMORY_FOR_NEW_MENUENTRY();
          // Ok, memory is available, now fill entries!
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].pchEntryText = "~whole Channel-Set";
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryID     = POPUPMENU_ID_DELETE_WHOLECHANNELSET;
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryStyle  = WAWE_POPUPMENU_STYLE_MENUITEM;
        }
        break;
      }
    default:
#ifdef DEBUG_BUILD
      printf("[GetPopupMenuEntries] : Invalid Menu ID!\n"); fflush(stdout);
#endif
      return 0;
  }
#ifdef DEBUG_BUILD
//  printf("[GetPopupMenuEntries] : PClipbrd: Leave\n"); fflush(stdout);
#endif

  return 1;
}

void WAWECALL plugin_FreePopupMenuEntries(int iNumOfPopupMenuEntries,
                                          WaWEPopupMenuEntries_p pPopupMenuEntries)
{
#ifdef DEBUG_BUILD
//  printf("[FreePopupMenuEntries] : PClipbrd : Enter\n"); fflush(stdout);
#endif
  if (pPopupMenuEntries)
  {
    // No need to free stuffs inside the descriptors, because all the
    // strings in there were constants.
    free(pPopupMenuEntries);
  }
  return;
}

int  WAWECALL plugin_DoPopupMenuAction(int iPopupMenuID,
                                       WaWEState_p pWaWEState,
                                       HWND hwndOwner)
{
#ifdef DEBUG_BUILD
//  printf("[DoPopupMenuAction] : PClipbrd : Enter, id = %d\n", iPopupMenuID); fflush(stdout);
#endif
  // Check parameters
  if (!pWaWEState) return 0;

  switch (iPopupMenuID)
  {
    case POPUPMENU_ID_COPY_SELECTEDCHANNELDATA:
      return CopyOrCut_SelectedChannelData(pWaWEState, hwndOwner, 0);
    case POPUPMENU_ID_COPY_SELECTEDCHANNELSETDATA:
      return CopyOrCut_SelectedChannelSetData(pWaWEState, hwndOwner, 0);
    case POPUPMENU_ID_COPY_WHOLECHANNEL:
      return CopyOrCut_WholeChannel(pWaWEState, hwndOwner, 0);
    case POPUPMENU_ID_COPY_WHOLECHANNELSET:
      return CopyOrCut_WholeChannelSet(pWaWEState, hwndOwner, 0);

    case POPUPMENU_ID_CUT_SELECTEDCHANNELDATA:
      return CopyOrCut_SelectedChannelData(pWaWEState, hwndOwner, 1);
    case POPUPMENU_ID_CUT_SELECTEDCHANNELSETDATA:
      return CopyOrCut_SelectedChannelSetData(pWaWEState, hwndOwner, 1);
    case POPUPMENU_ID_CUT_WHOLECHANNEL:
      return CopyOrCut_WholeChannel(pWaWEState, hwndOwner, 1);
    case POPUPMENU_ID_CUT_WHOLECHANNELSET:
      return CopyOrCut_WholeChannelSet(pWaWEState, hwndOwner, 1);

    case POPUPMENU_ID_DELETE_SELECTEDCHANNELDATA:
      return Delete_SelectedChannelData(pWaWEState, hwndOwner);
    case POPUPMENU_ID_DELETE_SELECTEDCHANNELSETDATA:
      return Delete_SelectedChannelSetData(pWaWEState, hwndOwner);
    case POPUPMENU_ID_DELETE_WHOLECHANNEL:
      return Delete_WholeChannel(pWaWEState, hwndOwner);
    case POPUPMENU_ID_DELETE_WHOLECHANNELSET:
      return Delete_WholeChannelSet(pWaWEState, hwndOwner);

    case POPUPMENU_ID_PASTE:
      return Paste(pWaWEState, hwndOwner);

    default:
      return 0;
  }
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

  // Initialize variables
  iNumOfChannelsOnClipboard = 0;
  pClipboard = NULL;

  // Return success!
  return 1;
}

void WAWECALL plugin_Uninitialize(HWND hwndOwner)
{
  // Free allocated memory (clear clipboard)
  ClearClipboard();
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

  // Quite high importance, so it'll be around the top of the popup menu
  pPluginDesc->iPluginImportance = (WAWE_PLUGIN_IMPORTANCE_MIN + WAWE_PLUGIN_IMPORTANCE_MAX)/8*7;

  pPluginDesc->iPluginType = WAWE_PLUGIN_TYPE_PROCESSOR;

  // Plugin functions:
  pPluginDesc->fnAboutPlugin = plugin_About;
  pPluginDesc->fnConfigurePlugin = NULL;
  pPluginDesc->fnInitializePlugin = plugin_Initialize;
  pPluginDesc->fnPrepareForShutdown = NULL;
  pPluginDesc->fnUninitializePlugin = plugin_Uninitialize;

  pPluginDesc->TypeSpecificInfo.ProcessorPluginInfo.fnGetPopupMenuEntries =
    plugin_GetPopupMenuEntries;
  pPluginDesc->TypeSpecificInfo.ProcessorPluginInfo.fnFreePopupMenuEntries =
    plugin_FreePopupMenuEntries;
  pPluginDesc->TypeSpecificInfo.ProcessorPluginInfo.fnDoPopupMenuAction =
    plugin_DoPopupMenuAction;

  // Save module handle for later use
  // (We might need it when we want to load some resources from DLL)
  hmodOurDLLHandle = pPluginDesc->hmodDLL;

  // Return success!
  return 1;
}


//---------------------------------------------------------------------
// LibMain
//
// This gets called at DLL initialization and termination.
//---------------------------------------------------------------------
unsigned _System LibMain( unsigned hmod, unsigned termination )
{
  if (termination)
  {
    // Cleanup!

    // Just to make sure that there will be no memory leak in case of a crash,
    // we do the same here what we do in plugin_Uninitialize()

    // Free allocated memory (clear clipboard)
    ClearClipboard();
  } else
  {
    // Startup!
    // All will be done in plugin_Initialize()
  }
  return 1;
}
