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
 * Volume changer processor plugin                                           *
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

#include "WaWECommon.h"    // Common defines and structures
#include "PVolume-Resources.h" // Resource IDs

// Global variables
// (Per process, because the DLL is initinstance/terminstance!)
HMODULE hmodOurDLLHandle;
WaWEPluginHelpers_t PluginHelpers;

char *WinStateSave_Name = "WaWE";
char *WinStateSave_Key  = "ProcessorPlugin_VolumeChanger_NormalWndPP";

char *pchPluginName = "Volume changer plugin";
char *pchAboutTitle = "About Volume changer plugin";
char *pchAboutMessage =
  "Volume changer plugin v1.0 for WaWE\n"
  "Written by Doodle\n"
  "\n"
  "Compilation date: "__DATE__" "__TIME__;


// Read 65536 bytes in one step
#define SAMPLE_BUFFER_SIZE  16384

// Helper functions, called from inside:

void ErrorMsg(char *Msg)
{
#ifdef DEBUG_BUILD
  printf("ErrorMsg: %s\n",Msg); fflush(stdout);
#endif
  WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                Msg, "Volume changer plugin error", 0, MB_OK | MB_MOVEABLE | MB_ICONEXCLAMATION);
}

static void SetSliderArmPosition(HWND hwnd)
{
  int iShaftSize;
  int iArmPos;
  char achBuf[128];
  WNDPARAMS wndparams;
  SLDCDATA SliderData;

  // We set the slider arm position, based on the value in entry field!

  // First get how many pixels wide is the slider's inner part!
  SliderData.cbSize = sizeof(SLDCDATA);
  SliderData.usScale1Increments = 0;
  SliderData.usScale1Spacing = 0;
  SliderData.usScale2Increments = 0;
  SliderData.usScale1Spacing = 0;
  wndparams.fsStatus = WPM_CTLDATA;
  wndparams.cchText = 0;
  wndparams.cbPresParams = 0;
  wndparams.cbCtlData = sizeof(SLDCDATA);
  wndparams.pCtlData = &SliderData;
  WinSendDlgItemMsg(hwnd, SLD_CHANGEAMOUNT, WM_QUERYWINDOWPARAMS, (MPARAM) &wndparams, (MPARAM) 0);

  iShaftSize = SliderData.usScale1Spacing * (SliderData.usScale1Increments-1);

  // Get value from entry field!
  memset(achBuf, 0, sizeof(achBuf));
  WinQueryDlgItemText(hwnd, EF_AMOUNT, sizeof(achBuf)-1, achBuf);
  iArmPos = atoi(achBuf);
  // We only can show 0..200 on the slider!
  if (iArmPos<0) iArmPos = 0;
  if (iArmPos>200) iArmPos = 200;

  iArmPos = iShaftSize * iArmPos / 200;

  WinSendDlgItemMsg(hwnd, SLD_CHANGEAMOUNT,
		    SLM_SETSLIDERINFO,
		    MPFROM2SHORT(SMA_SLIDERARMPOSITION, SMA_RANGEVALUE),
		    (MPARAM) iArmPos);
}

static void SetEntryFieldBySliderArmPosition(HWND hwnd, int iSliderPos)
{
  int iShaftSize;
  int iValue;
  char achBuf[128];
  WNDPARAMS wndparams;
  SLDCDATA SliderData;

  // We set entry field, based on the slider arm position!

  // First get how many pixels wide is the slider's inner part!
  SliderData.cbSize = sizeof(SLDCDATA);
  SliderData.usScale1Increments = 0;
  SliderData.usScale1Spacing = 0;
  SliderData.usScale2Increments = 0;
  SliderData.usScale1Spacing = 0;
  wndparams.fsStatus = WPM_CTLDATA;
  wndparams.cchText = 0;
  wndparams.cbPresParams = 0;
  wndparams.cbCtlData = sizeof(SLDCDATA);
  wndparams.pCtlData = &SliderData;
  WinSendDlgItemMsg(hwnd, SLD_CHANGEAMOUNT, WM_QUERYWINDOWPARAMS, (MPARAM) &wndparams, (MPARAM) 0);

  iShaftSize = SliderData.usScale1Spacing * (SliderData.usScale1Increments-1);

  // Calculate percentage value, based on Shaft size, and Arm position!
  if (iShaftSize==0) iValue = 0;
  else
    iValue = iSliderPos * 200 / iShaftSize;

  // Now set this string in entry field!
  sprintf(achBuf, "%d", iValue);
  WinSetDlgItemText(hwnd, EF_AMOUNT, achBuf);
}

void ResizeNormalDialogControls(HWND hwnd)
{
  SWP swp1, swp2, swp3, swpLinePart1, swpLinePart2, swpLinePart3;
  ULONG CXDLGFRAME, CYDLGFRAME, CYTITLEBAR;
  int iYPos;
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

  WinQueryWindowPos(hwnd, &swp1);

  // First the push buttons
  WinSetWindowPos(WinWindowFromID(hwnd, PB_OK), HWND_TOP,
                  3*CXDLGFRAME,
                  2*CYDLGFRAME,
		  0, 0,
                  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, PB_CANCEL), &swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, PB_CANCEL), HWND_TOP,
                  swp1.cx - 3*CXDLGFRAME - swp2.cx,
                  2*CYDLGFRAME,
		  0, 0,
                  SWP_MOVE);

  // The first line: "Change volume for"
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_CHANGEVOLUMEFOR), &swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_CHANGEVOLUMEFOR), HWND_TOP,
                  (swp1.cx - swp2.cx)/2,
                  swp1.cy - CYTITLEBAR - CYDLGFRAME - 2*CYDLGFRAME - swp2.cy,
                  0, 0,
		  SWP_MOVE);

  // The second line: "Name of channel or channelset"
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_NAMEOFCHANNELORCHANNELSET), &swp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_CHANGEVOLUMEFOR), &swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_NAMEOFCHANNELORCHANNELSET), HWND_TOP,
                  2*CXDLGFRAME,
                  swp3.y - CYDLGFRAME - swp2.cy,
                  swp1.cx - 4*CXDLGFRAME,
                  swp2.cy,
                  SWP_MOVE | SWP_SIZE);

  // Then the following three lines will be centered to the remaining space
  WinQueryWindowPos(WinWindowFromID(hwnd, PB_OK), &swp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_NAMEOFCHANNELORCHANNELSET), &swp3);
  iYPos = swp2.y + swp2.cy + (swp3.y - (swp2.y + swp2.cy)) / 2;

  WinQueryWindowPos(WinWindowFromID(hwnd, SLD_CHANGEAMOUNT), &swp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, CB_MAXIMIZEVOLUME), &swpLinePart1);
  WinSetWindowPos(WinWindowFromID(hwnd, CB_MAXIMIZEVOLUME), HWND_TOP,
                  CXDLGFRAME + (swp1.cx - (CXDLGFRAME + swpLinePart1.cx + CXDLGFRAME))/2,
                  iYPos + swp2.cy/2 + CYDLGFRAME,
                  0, 0,
                  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, SLD_CHANGEAMOUNT), &swpLinePart1);
  WinSetWindowPos(WinWindowFromID(hwnd, SLD_CHANGEAMOUNT), HWND_TOP,
                  2*CXDLGFRAME,
                  iYPos - swpLinePart1.cy/2,
                  swp1.cx - 4*CXDLGFRAME,
                  swpLinePart1.cy,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, SLD_CHANGEAMOUNT), &swp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_SETVOLUMETO), &swpLinePart1);
  WinQueryWindowPos(WinWindowFromID(hwnd, EF_AMOUNT), &swpLinePart2);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_PERCENTAGE), &swpLinePart3);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_SETVOLUMETO), HWND_TOP,
                  CXDLGFRAME + (swp1.cx - (CXDLGFRAME + swpLinePart1.cx + 2*CXDLGFRAME + swpLinePart2.cx + 2*CXDLGFRAME + swpLinePart3.cx + CXDLGFRAME))/2,
                  iYPos - swp2.cy/2 - CYDLGFRAME - swpLinePart2.cy,
                  0, 0,
                  SWP_MOVE);
  WinSetWindowPos(WinWindowFromID(hwnd, EF_AMOUNT), HWND_TOP,
		  CXDLGFRAME + swpLinePart1.cx + 2*CXDLGFRAME + (swp1.cx - (CXDLGFRAME + swpLinePart1.cx + 2*CXDLGFRAME + swpLinePart2.cx + 2*CXDLGFRAME + swpLinePart3.cx + CXDLGFRAME))/2,
                  iYPos - swp2.cy/2 - CYDLGFRAME - swpLinePart2.cy,
                  0, 0,
                  SWP_MOVE);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_PERCENTAGE), HWND_TOP,
                  CXDLGFRAME + swpLinePart1.cx + 2*CXDLGFRAME + swpLinePart2.cx + 2*CXDLGFRAME + (swp1.cx - (CXDLGFRAME + swpLinePart1.cx + 2*CXDLGFRAME + swpLinePart2.cx + 2*CXDLGFRAME + swpLinePart3.cx + CXDLGFRAME))/2,
                  iYPos - swp2.cy/2 - CYDLGFRAME - swpLinePart2.cy,
                  0, 0,
		  SWP_MOVE);

  // Slider arm position has to be updated to show the new value.
  // For this, we take the number from the edit field,
  // and create a slider arm position from that.
  SetSliderArmPosition(hwnd);

  if (iDisabledWindowUpdate)
    WinEnableWindowUpdate(hwnd, TRUE);
}

MRESULT EXPENTRY NormalDialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  char achBuf[64];

  switch (msg)
  {
    case WM_CONTROL:
      {
	switch (SHORT1FROMMP(mp1)) // Check ID
	{
	  case CB_MAXIMIZEVOLUME:
	    {
	      // Something happened to our checkbox!

	      if (WinQueryButtonCheckstate(hwnd, CB_MAXIMIZEVOLUME))
	      {
		// Checkbox has been checked
		// Disable slider and others
		WinEnableWindow(WinWindowFromID(hwnd, SLD_CHANGEAMOUNT), FALSE);
		WinEnableWindow(WinWindowFromID(hwnd, ST_SETVOLUMETO), FALSE);
		WinEnableWindow(WinWindowFromID(hwnd, EF_AMOUNT), FALSE);
		WinEnableWindow(WinWindowFromID(hwnd, ST_PERCENTAGE), FALSE);
	      } else
	      {
		// Checkbox has been cleared
		// Enable slider and others
		WinEnableWindow(WinWindowFromID(hwnd, SLD_CHANGEAMOUNT), TRUE);
		WinEnableWindow(WinWindowFromID(hwnd, ST_SETVOLUMETO), TRUE);
		WinEnableWindow(WinWindowFromID(hwnd, EF_AMOUNT), TRUE);
		WinEnableWindow(WinWindowFromID(hwnd, ST_PERCENTAGE), TRUE);
	      }
              return (MRESULT) 1;
	    }
	  case SLD_CHANGEAMOUNT:
	    {
	      // Something happened to the slider!
	      if (
		  (SHORT2FROMMP(mp1)==SLN_CHANGE)
		  ||
		  (SHORT2FROMMP(mp1)==SLN_SLIDERTRACK)
		 )
	      {
		// If the slider has the focus, then we should
		// set new value in entry field, because the user has modified the
		// slider!
		if (WinWindowFromID(hwnd, SLD_CHANGEAMOUNT) == WinQueryFocus(HWND_DESKTOP))
		{
                  SetEntryFieldBySliderArmPosition(hwnd, (int) mp2);
		}
                return (MRESULT) 1;
	      }
              break;
	    }
	  case EF_AMOUNT:
	    {
	      // Something happened to the entry field!
	      switch (SHORT2FROMMP(mp1))
	      {
		case EN_SETFOCUS:
		  {
		    long lValue;
                    // Save current setting in window word!
		    memset(achBuf, 0, sizeof(achBuf));
		    WinQueryDlgItemText(hwnd, EF_AMOUNT, sizeof(achBuf)-1, achBuf);
		    lValue = atoi(achBuf);
                    WinSetWindowULong(WinWindowFromID(hwnd, EF_AMOUNT), QWL_USER, lValue);
		    return (MRESULT) 1;
		  }
		case EN_KILLFOCUS:
		  {
		    long lValue;
                    int iScanned;
                    // Check current text, and restore old, if it's not a valid number!
		    memset(achBuf, 0, sizeof(achBuf));
		    WinQueryDlgItemText(hwnd, EF_AMOUNT, sizeof(achBuf)-1, achBuf);
		    iScanned = sscanf(achBuf, "%ld", &lValue);
		    if (iScanned==1)
		    {
                      if ((lValue<0) || (lValue>1000)) iScanned = 0;
		    }
		    if (iScanned!=1)
		    {
                      // Not a valid thing entered, so restore old value!
		      lValue = (long) WinQueryWindowULong(WinWindowFromID(hwnd, EF_AMOUNT), QWL_USER);
		    }
                    // Make value string
                    sprintf(achBuf, "%d", lValue);
		    WinSetDlgItemText(hwnd, EF_AMOUNT, achBuf);

                    // Now set arm position
                    SetSliderArmPosition(hwnd);

		    return (MRESULT) 1;
		  }
		default:
                  break;
	      }
              break;
	    }
	  default:
            break;
	}
        break;
      }

    case WM_FORMATFRAME:
      {
	ResizeNormalDialogControls(hwnd);
        break;
      }
  } // End of switch msg
  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}

static void SetNameOfChannelOrChannelSet(HWND hwndDlg, WaWEState_p pWaWEState, int iChangeChannelSet)
{
  char *pchText;

  pchText = (char *) malloc(WAWE_CHANNEL_NAME_LEN + WAWE_CHANNELSET_NAME_LEN + 512);
  if (pchText)
  {
    if (!iChangeChannelSet)
    {
      sprintf(pchText,"%s (in %s)", pWaWEState->pCurrentChannel->achName, pWaWEState->pCurrentChannel->pParentChannelSet->achName);
      WinSetDlgItemText(hwndDlg, ST_NAMEOFCHANNELORCHANNELSET, pchText);
    } else
    {
      if (pWaWEState->pCurrentChannelSet)
	sprintf(pchText,"%s", pWaWEState->pCurrentChannelSet->achName);
      else
      if (pWaWEState->pCurrentChannel)
        sprintf(pchText,"%s", pWaWEState->pCurrentChannel->pParentChannelSet->achName);
      else
        sprintf(pchText,"Unknown");
      WinSetDlgItemText(hwndDlg, ST_NAMEOFCHANNELORCHANNELSET, pchText);
    }

    free(pchText);
  }
}

static int SetVolumeOfChannelChunk(WaWEState_p pWaWEState, WaWEChannel_p pChannel, WaWESample_p pSampleBuf, WaWEPosition_t FromPos, WaWEPosition_t ToPos, int iVolumePercentage)
{
  WaWEPosition_t Pos, BufPos;
  WaWESize_t CouldRead, CouldWrite;
  WaWESize_t ReadSize;

  Pos = FromPos;
  while (Pos<=ToPos)
  {
    ReadSize = SAMPLE_BUFFER_SIZE;
    if (ReadSize>(ToPos - Pos + 1))
      ReadSize = ToPos - Pos + 1;

    PluginHelpers.fn_WaWEHelper_Channel_ReadSample(pChannel, Pos, ReadSize, pSampleBuf, &CouldRead);
    if (CouldRead<=0)
    {
      ErrorMsg("Error reading channel data, could not set channel volume!");
      return 0;
      break;
    }
    for (BufPos=0; BufPos<CouldRead; BufPos++)
    {
      __int64 i64Temp;
      i64Temp = pSampleBuf[BufPos];
      i64Temp = i64Temp * iVolumePercentage / 100;
      // Cut sample to limits
      if (i64Temp < MIN_WAWESAMPLE)
	i64Temp = MIN_WAWESAMPLE;
      if (i64Temp > MAX_WAWESAMPLE)
	i64Temp = MAX_WAWESAMPLE;
      pSampleBuf[BufPos] = (WaWESample_t) i64Temp;
    }
    PluginHelpers.fn_WaWEHelper_Channel_WriteSample(pChannel, Pos, CouldRead, pSampleBuf, &CouldWrite);
    if (CouldRead!=CouldWrite)
    {
      ErrorMsg("Error writing channel data, could not set channel volume!");
      return 0;
      break;
    }
    Pos += CouldRead;
  }
  return 1;
}

int ChangeChannelVolume(WaWEState_p pWaWEState, 
			WaWEChannel_p pChannel,
			int iPercentage,
			HWND hwndOwner)
{
  int iResult = 0; // Failure
  WaWESample_p pSampleBuf;

#ifdef DEBUG_BUILD
  printf("About to change volume of channel [%s] to %d\n",
	 pChannel->achName, iPercentage);
  fflush(stdout);
#endif

  if (iPercentage == 100)
  {
#ifdef DEBUG_BUILD
    printf("Setting volume to 100%%, so doing nothing!\n"); fflush(stdout);
#endif
    return 1;
  }

  // Allocate memory for sample buffer
  pSampleBuf = (WaWESample_p) malloc(SAMPLE_BUFFER_SIZE * sizeof(WaWESample_t));
  if (!pSampleBuf)
  {
    ErrorMsg("Not enough memory to set channel volume!");
    return iResult;
  }

  if (pChannel->pLastValidSelectedRegionListHead)
  {
    WaWESelectedRegion_p pRegion;
    // There is selection in channel, so use that!
#ifdef DEBUG_BUILD
    printf("[ChangeChannelVolume] : Using selected region list of channel!\n"); fflush(stdout);
#endif
    pRegion = pChannel->pLastValidSelectedRegionListHead;
    iResult = 1;
    while ((pRegion) && (iResult))
    {
      iResult = SetVolumeOfChannelChunk(pWaWEState,
					pChannel,
					pSampleBuf,
					pRegion->Start, pRegion->End,
					iPercentage);
      pRegion = pRegion->pNext;
    }
  } else
  {
    // There is no selection in channel, so use all the channel!
#ifdef DEBUG_BUILD
    printf("[ChangeChannelVolume] : Using all the channel!\n"); fflush(stdout);
    printf("   Length: %d samples\n", (int) (pChannel->Length)); fflush(stdout);
#endif
    iResult = SetVolumeOfChannelChunk(pWaWEState,
				      pChannel,
				      pSampleBuf,
				      0, pChannel->Length-1,
				      iPercentage);
  }

  // Free the sample buffer!
  free(pSampleBuf);

#ifdef DEBUG_BUILD
  printf("[ChangeChannelVolume] : Returning result: %d\n", iResult); fflush(stdout);
#endif
  return iResult;
}

static WaWESample_t GreatestSampleOfChannelChunk(WaWEState_p pWaWEState, WaWEChannel_p pChannel, WaWESample_p pSampleBuf, WaWEPosition_t FromPos, WaWEPosition_t ToPos)
{
  WaWESample_t GreatestSample;
  WaWEPosition_t Pos, BufPos;
  WaWESize_t CouldRead;
  WaWESize_t ReadSize;

  GreatestSample = 0;
  Pos = FromPos;
  while (Pos<=ToPos)
  {
    ReadSize = SAMPLE_BUFFER_SIZE;
    if (ReadSize>(ToPos - Pos + 1))
      ReadSize = ToPos - Pos + 1;

    PluginHelpers.fn_WaWEHelper_Channel_ReadSample(pChannel, Pos, ReadSize, pSampleBuf, &CouldRead);
    if (CouldRead<=0)
    {
      ErrorMsg("Error reading channel data, could not calculate maximum channel volume!");
      break;
    }
    for (BufPos=0; BufPos<CouldRead; BufPos++)
    {
      if (pSampleBuf[BufPos]>=0)
      {
        if (GreatestSample<pSampleBuf[BufPos])
          GreatestSample = pSampleBuf[BufPos];
      } else
      {
        if (GreatestSample<-pSampleBuf[BufPos])
          GreatestSample = -pSampleBuf[BufPos];
      }
    }
    Pos += CouldRead;
  }
#ifdef DEBUG_BUILD
  printf("[GSOCC] : Greatest is %d\n", GreatestSample); fflush(stdout);
#endif

  return GreatestSample;
}

int CalculateMaxChannelVolume(WaWEState_p pWaWEState, WaWEChannel_p pChannel)
{
  int iResult = 100;
  WaWESample_p pSampleBuf;
  WaWESample_t GreatestSample;
  __int64 i64Temp;

  // Allocate memory for sample buffer
  pSampleBuf = (WaWESample_p) malloc(SAMPLE_BUFFER_SIZE * sizeof(WaWESample_t));
  if (!pSampleBuf)
  {
    ErrorMsg("Not enough memory to calculate maximum channel volume!");
    return iResult;
  }

  // Go through all the samples, and check the maximum volume we can do!
  if (pChannel->pLastValidSelectedRegionListHead)
  {
    WaWESelectedRegion_p pRegion;
    // There is selection in channel, so use that!
#ifdef DEBUG_BUILD
    printf("[CalculateMaxChannelVolume] : Using selected region list of channel!\n"); fflush(stdout);
#endif
    GreatestSample = 0;
    pRegion = pChannel->pLastValidSelectedRegionListHead;
    while (pRegion)
    {
      WaWESample_t LocalGreatestSample;
      LocalGreatestSample = GreatestSampleOfChannelChunk(pWaWEState,
							 pChannel,
							 pSampleBuf,
							 pRegion->Start, pRegion->End);
      if (GreatestSample<LocalGreatestSample)
        GreatestSample = LocalGreatestSample;
      pRegion = pRegion->pNext;
    }
  } else
  {
    // There is no selection in channel, so use all the channel!
#ifdef DEBUG_BUILD
    printf("[CalculateMaxChannelVolume] : Using all the channel!\n"); fflush(stdout);
    printf("   Length: %d samples\n", (int) (pChannel->Length)); fflush(stdout);
#endif
    GreatestSample = GreatestSampleOfChannelChunk(pWaWEState,
						  pChannel,
						  pSampleBuf,
						  0, pChannel->Length-1);
  }

  // Free the sample buffer!
  free(pSampleBuf);

#ifdef DEBUG_BUILD
  printf("[CalculateMaxChannelVolume] : Greatest: %d (from %d)\n", GreatestSample, MAX_WAWESAMPLE); fflush(stdout);
#endif

  // Now calculate maximum volume from greatest and smallest sample!
  if (GreatestSample==0)
    iResult = INT_MAX;
  else
  {
    i64Temp = MAX_WAWESAMPLE;
    i64Temp *= 100;
    i64Temp /= GreatestSample;
    iResult = i64Temp;
  }

#ifdef DEBUG_BUILD
  printf("[CalculateMaxChannelVolume] : Returning result: %d\n", iResult); fflush(stdout);
#endif

  return iResult;
}

int ChangeVolume_Normal(WaWEState_p pWaWEState, HWND hwndOwner, int iChangeChannelSet)
{
  HWND hwndDlg;
  SLDCDATA SliderData;
  WNDPARAMS wprm;

  // We cannot do anything if there is not channel-set!
  if (!(pWaWEState->pCurrentChannelSet)) return 0;
  // Also cannot do anything if there is no channel in channel-set!
  if (!(pWaWEState->pCurrentChannelSet->pChannelListHead)) return 0;

  // Then let the dialog window come, and ask for the settings,
  // how to load the file!

  hwndDlg = WinLoadDlg(HWND_DESKTOP,
		       hwndOwner, // Owner will be the WaWE app itself
		       NormalDialogProc, // Dialog proc
		       hmodOurDLLHandle, // Source module handle
		       DLG_CHANGEVOLUME,
		       NULL);
  if (hwndDlg)
  {
    SWP swpDlg, swpParent;
    int iResult;

    // Set default values
    WinSendDlgItemMsg(hwndDlg, EF_AMOUNT,
		      EM_CLEAR,
		      (MPARAM) 0,
		      (MPARAM) 0);
    WinSetDlgItemText(hwndDlg, EF_AMOUNT, "100");


    SetNameOfChannelOrChannelSet(hwndDlg, pWaWEState, iChangeChannelSet);

    // Set Slider parameters

    SliderData.cbSize = sizeof(SLDCDATA);
    SliderData.usScale1Increments = 21;
    SliderData.usScale1Spacing = 0;
    SliderData.usScale2Increments = 21;
    SliderData.usScale1Spacing = 0;
    wprm.fsStatus = WPM_CTLDATA;
    wprm.cchText = 0;
    wprm.cbPresParams = 0;
    wprm.cbCtlData = 0;
    wprm.pCtlData = &SliderData;
    WinSendDlgItemMsg(hwndDlg, SLD_CHANGEAMOUNT,
		      WM_SETWINDOWPARAMS, (MPARAM) &wprm, (MPARAM) 0);

    WinSendDlgItemMsg(hwndDlg, SLD_CHANGEAMOUNT,
		      SLM_SETSLIDERINFO,
		      MPFROM2SHORT(SMA_SLIDERARMPOSITION, SMA_INCREMENTVALUE),
		      (MPARAM) 10);

    WinSendDlgItemMsg(hwndDlg, SLD_CHANGEAMOUNT,
		      SLM_SETTICKSIZE,
		      MPFROM2SHORT(SMA_SETALLTICKS, 5),
		      (MPARAM) 0);

    // Set 'Maximize volume' checkbox
    WinSendDlgItemMsg(hwndDlg, CB_MAXIMIZEVOLUME,
		      BM_SETCHECK,
		      (MPARAM) 1,
		      (MPARAM) 0);
    // Disable slider and others
    WinEnableWindow(WinWindowFromID(hwndDlg, SLD_CHANGEAMOUNT), FALSE);
    WinEnableWindow(WinWindowFromID(hwndDlg, ST_SETVOLUMETO), FALSE);
    WinEnableWindow(WinWindowFromID(hwndDlg, EF_AMOUNT), FALSE);
    WinEnableWindow(WinWindowFromID(hwndDlg, ST_PERCENTAGE), FALSE);

    // Center dialog in screen
    if (WinQueryWindowPos(hwndDlg, &swpDlg))
      if (WinQueryWindowPos(HWND_DESKTOP, &swpParent))
      {
	// Center dialog box within the parent window
	int ix, iy;
	ix = swpParent.x + (swpParent.cx - swpDlg.cx)/2;
	iy = swpParent.y + (swpParent.cy - swpDlg.cy)/2;
	WinSetWindowPos(hwndDlg, HWND_TOP, ix, iy, 0, 0,
			SWP_MOVE);
      }
    // Now try to reload size and position information from INI file
    WinRestoreWindowPos(WinStateSave_Name, WinStateSave_Key, hwndDlg);
    // Now that we have the size and position of the dialog,
    // we can reposition and resize the controls inside it, so it
    // will look nice in every resolution.
    ResizeNormalDialogControls(hwndDlg);

    WinSetWindowPos(hwndDlg, HWND_TOP, 0, 0, 0, 0,
		    SWP_SHOW | SWP_ACTIVATE | SWP_ZORDER);
    WinSetFocus(HWND_DESKTOP, hwndDlg);

    // Process the dialog!
    if (WinProcessDlg(hwndDlg)==PB_OK)
    {
      char achBuf[64];
      int iScanned;
      int iPercentage;
      int iMaximizeVolume;

      WinQueryDlgItemText(hwndDlg, EF_AMOUNT, sizeof(achBuf)-1, achBuf);
      iScanned = sscanf(achBuf, "%d", &iPercentage);
      if (iScanned!=1)
	iPercentage = 100;

      iMaximizeVolume = WinQueryButtonCheckstate(hwndDlg, CB_MAXIMIZEVOLUME);

#ifdef DEBUG_BUILD
      printf("OK button pressed, Vol = %d, MaxVol = %d\n", iPercentage, iMaximizeVolume); fflush(stdout);
#endif

      if (!iChangeChannelSet)
      {
	// Change volume of current channel

	// If we have to maximize the volume, then first we get the maximum
        // percentage we can increase the volume to!
	if (iMaximizeVolume)
	{
#ifdef DEBUG_BUILD
	  printf("Calculating maximum volume we can apply for channel!\n"); fflush(stdout);
#endif
	  iPercentage = CalculateMaxChannelVolume(pWaWEState, pWaWEState->pCurrentChannel);
	}

        // Now do the real volume changing!
	iResult = ChangeChannelVolume(pWaWEState, pWaWEState->pCurrentChannel, iPercentage, hwndOwner);
      } else
      {
        // Change volume of channels in current channel-set
	WaWEChannel_p pChannel;

	// If we have to maximize the volume, then first we get the maximum
        // percentage we can increase the volume to!
	if (iMaximizeVolume)
	{
#ifdef DEBUG_BUILD
	  printf("Calculating maximum volume we can apply for channels!\n"); fflush(stdout);
#endif
          if (DosRequestMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList, SEM_INDEFINITE_WAIT)!=NO_ERROR)
          {
            // Internal error!
            iPercentage = 100;
          } else
          {
            pChannel = pWaWEState->pCurrentChannelSet->pChannelListHead;
            if (!pChannel)
              iPercentage = 100;
            else
            {
              iPercentage = CalculateMaxChannelVolume(pWaWEState, pChannel);
              pChannel = pChannel->pNext;
              while (pChannel)
              {
                int iThisPercentage;

                iThisPercentage = CalculateMaxChannelVolume(pWaWEState, pChannel);
                if (iThisPercentage<iPercentage) iPercentage = iThisPercentage;
                pChannel = pChannel->pNext;
              }
            }
            DosReleaseMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList);
          }
	}

        // Now do the real volume changing!
        if (DosRequestMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList, SEM_INDEFINITE_WAIT)==NO_ERROR)
        {
          pChannel = pWaWEState->pCurrentChannelSet->pChannelListHead;
          while (pChannel)
          {
            iResult = ChangeChannelVolume(pWaWEState, pChannel, iPercentage, hwndOwner);
            // Exit loop if there was an error!
            if (!iResult) break;
            pChannel = pChannel->pNext;
          }
          DosReleaseMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList);
        }
      }
    }
    else
      iResult = 0;
    // Now save size and position info into INI file
    WinStoreWindowPos(WinStateSave_Name, WinStateSave_Key, hwndDlg);
    WinDestroyWindow(hwndDlg);

    return iResult;
  }
  else
    // Could not create window!
    return 0;
}


// Functions called from outside:

// Define menu IDs. Note, that the ID of 0 is reserved for WAWE_POPUPMENU_ID_ROOT!
#define POPUPMENU_ID_CHANGEVOLUME             1
#define POPUPMENU_ID_CHANGEVOLUME_CHANNEL     2
#define POPUPMENU_ID_CHANGEVOLUME_CHANNELSET  3
#define POPUPMENU_ID_CHANGEVOLUME_SPECIAL     4

int  WAWECALL plugin_GetPopupMenuEntries(int iPopupMenuID,
                                         WaWEState_p pWaWEState,
                                         int *piNumOfPopupMenuEntries,
                                         WaWEPopupMenuEntries_p *ppPopupMenuEntries)
{
#ifdef DEBUG_BUILD
//  printf("[GetPopupMenuEntries] : Enter, id = %d\n", iPopupMenuID); fflush(stdout);
#endif
  // Check parameters!
  if ((!pWaWEState) || (!piNumOfPopupMenuEntries) ||
      (!ppPopupMenuEntries) || (!(pWaWEState->pCurrentChannelSet)))
    return 0;

  switch (iPopupMenuID)
  {
    case WAWE_POPUPMENU_ID_ROOT:
      {
        // Ok, WaWE asks what menu items we want into first popup menu!
        // We'll have only one
        *piNumOfPopupMenuEntries = 1;
        *ppPopupMenuEntries = (WaWEPopupMenuEntries_p) malloc((*piNumOfPopupMenuEntries) * sizeof(WaWEPopupMenuEntries_t));
        if (!(*ppPopupMenuEntries))
        {
#ifdef DEBUG_BUILD
          printf("[GetPopupMenuEntries] : Warning, out of memory!\n"); fflush(stdout);
#endif
          return 0;
        }

        (*ppPopupMenuEntries)[0].pchEntryText = "~Change volume";
        (*ppPopupMenuEntries)[0].iEntryID     = POPUPMENU_ID_CHANGEVOLUME;
        (*ppPopupMenuEntries)[0].iEntryStyle  = WAWE_POPUPMENU_STYLE_SUBMENU;
        break;
      }
    case POPUPMENU_ID_CHANGEVOLUME:
      {
        // Ok, WaWE asks what menu items we want into our 'Change volume' menu!
        // Let's see!
        // If there is a current channel, then we offer a menu item of
        // change volume for current channel, otherwise we won't!
        if (pWaWEState->pCurrentChannel)
        {
          // We'll have three items
          *piNumOfPopupMenuEntries = 3;
        } else
        {
          // We'll have two items
          *piNumOfPopupMenuEntries = 2;
        }
        // Now allocate memory for the menu item descriptors!
        *ppPopupMenuEntries = (WaWEPopupMenuEntries_p) malloc((*piNumOfPopupMenuEntries) * sizeof(WaWEPopupMenuEntries_t));
        if (!(*ppPopupMenuEntries))
        {
#ifdef DEBUG_BUILD
          printf("[GetPopupMenuEntries] : Warning, out of memory!\n"); fflush(stdout);
#endif
          return 0;
        }
        if (*piNumOfPopupMenuEntries == 3)
        {
          // So, we'll have three items!
          (*ppPopupMenuEntries)[0].pchEntryText = "of ~channel";
          (*ppPopupMenuEntries)[0].iEntryID     = POPUPMENU_ID_CHANGEVOLUME_CHANNEL;
          (*ppPopupMenuEntries)[0].iEntryStyle  = WAWE_POPUPMENU_STYLE_MENUITEM;

          (*ppPopupMenuEntries)[1].pchEntryText = "of channel-~set";
          (*ppPopupMenuEntries)[1].iEntryID     = POPUPMENU_ID_CHANGEVOLUME_CHANNELSET;
          (*ppPopupMenuEntries)[1].iEntryStyle  = WAWE_POPUPMENU_STYLE_MENUITEM;

          (*ppPopupMenuEntries)[2].pchEntryText = "s~pecial";
          (*ppPopupMenuEntries)[2].iEntryID     = POPUPMENU_ID_CHANGEVOLUME_SPECIAL;
          (*ppPopupMenuEntries)[2].iEntryStyle  = WAWE_POPUPMENU_STYLE_MENUITEM;
        } else
        {
          // We'll have two items (no current channel!)
          (*ppPopupMenuEntries)[0].pchEntryText = "of channel-~set";
          (*ppPopupMenuEntries)[0].iEntryID     = POPUPMENU_ID_CHANGEVOLUME_CHANNELSET;
          (*ppPopupMenuEntries)[0].iEntryStyle  = WAWE_POPUPMENU_STYLE_MENUITEM;

          (*ppPopupMenuEntries)[1].pchEntryText = "s~pecial";
          (*ppPopupMenuEntries)[1].iEntryID     = POPUPMENU_ID_CHANGEVOLUME_SPECIAL;
          (*ppPopupMenuEntries)[1].iEntryStyle  = WAWE_POPUPMENU_STYLE_MENUITEM;
        }
        break;
      }
    default:
#ifdef DEBUG_BUILD
      printf("[GetPopupMenuEntries] : Invalid Menu ID!\n"); fflush(stdout);
#endif
      return 0;
  }
  return 1;
}

void WAWECALL plugin_FreePopupMenuEntries(int iNumOfPopupMenuEntries,
                                          WaWEPopupMenuEntries_p pPopupMenuEntries)
{
#ifdef DEBUG_BUILD
//  printf("[FreePopupMenuEntries] : Enter\n"); fflush(stdout);
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
  printf("[DoPopupMenuAction] : Enter, id = %d\n", iPopupMenuID); fflush(stdout);
#endif
  // Check parameters
  if (!pWaWEState) return 0;

  switch (iPopupMenuID)
  {
    case POPUPMENU_ID_CHANGEVOLUME_CHANNEL:
      return ChangeVolume_Normal(pWaWEState, hwndOwner, 0);
    case POPUPMENU_ID_CHANGEVOLUME_CHANNELSET:
      return ChangeVolume_Normal(pWaWEState, hwndOwner, 1);
    // TODO : Special volume change

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

  // Return success!
  return 1;
}

void WAWECALL plugin_Uninitialize(HWND hwndOwner)
{
  // Nothing to do here
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

  // Average importance
  pPluginDesc->iPluginImportance = (WAWE_PLUGIN_IMPORTANCE_MIN + WAWE_PLUGIN_IMPORTANCE_MAX)/2;

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
  // (We will need it when we want to load some resources from DLL)
  hmodOurDLLHandle = pPluginDesc->hmodDLL;

  // Return success!
  return 1;
}
