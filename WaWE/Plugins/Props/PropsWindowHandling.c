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
 * Channel-Set Properties plugin                                             *
 * Module for handling the Properties windows                                *
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <process.h>
#include <string.h>
#include <limits.h>

#define INCL_TYPES
#define INCL_WIN
#define INCL_DOSRESOURCES
#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_GPI
#include <os2.h>

#include "WaWECommon.h"         // Common defines and structures
#include "PropsWindowHandling.h" // Functions about handling Properties windows
#include "Props-Resources.h"    // Resources

#define DID_PROGRESSINDICATOR   1000
#define WM_SETPERCENTAGE (WM_USER + 50)


typedef struct InitDlgAppData_t_struct {
  USHORT usAreaSize; // This is required, according to the docs about WM_INITDLG!
  void *pRealData;
} InitDlgAppData_t, *InitDlgAppData_p;

char *WinStateSave_AppName                   = "WaWE";
char *WinStateSave_PropertiesWindowKey       = "ProcessorPlugin_Properties_MainWindowPP";
char *WinStateSave_ChangeSamplerateWindowKey = "ProcessorPlugin_Properties_ChangeSamplerateWindowPP";
char *WinStateSave_ConvertSamplerateProgressWindowKey = "ProcessorPlugin_Properties_ConvertSamplerateProgressWindowPP";
char *WinStateSave_AddNewFormatStringWindowKey        = "ProcessorPlugin_Properties_AddNewFormatStringWindowPP";
char *WinStateSave_ModifyFormatStringWindowKey        = "ProcessorPlugin_Properties_ModifyFormatStringWindowPP";
char *pchProgressIndicatorClassName = "PropsPluginProgressIndicatorClass";

HMTX hmtxUseWindowList; // Mutex semaphore to protect window list from concurrent access!
PropertiesWindowList_p pWindowListHead;

// Some prototypes and variables from Props.c
extern WaWEPluginHelpers_t PluginHelpers; // From Props.c!
void ErrorMsg(char *Msg);
void WarningMsg(char *Msg);

// Now our stuffs:

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
          pRGB2->bGreen = 100;
          pRGB2->bBlue =80;
          pRGB2 = (RGB2 *)(&rgbColor2);
          pRGB2->bRed = 200;
          pRGB2->bGreen = 220;
          pRGB2->bBlue = 200;
          WinDrawBorder(hpsBeginPaint, &rclFullRect, 1, 1,
                        (LONG) rgbColor1, (LONG) rgbColor2, DB_INTERIOR | DB_AREAMIXMODE);

          rclInnerRect.xLeft = rclFullRect.xLeft + 1;
          rclInnerRect.yBottom = rclFullRect.yBottom+1;
          rclInnerRect.xRight = rclFullRect.xLeft + 1 +
            iPercentage * (rclFullRect.xRight-rclFullRect.xLeft-2) / 100;
          rclInnerRect.yTop = rclFullRect.yTop-1;

          pRGB2 = (RGB2 *)(&rgbColor1);
          pRGB2->bRed = 10;
          pRGB2->bGreen = 80;
          pRGB2->bBlue = 10;
          pRGB2 = (RGB2 *)(&rgbColor2);
          pRGB2->bRed = 150;
          pRGB2->bGreen = 200;
          pRGB2->bBlue = 150;
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


static void AddNotebookTab( HWND hwndNotebook, HWND hwndNotebookPage, USHORT usType, char *szTabText, char *szStatusText,
                            ULONG *pulPageID, unsigned short *pusTabTextLength, unsigned short *pusTabTextHeight )
{
  HPS hps;
  FONTMETRICS fmFontMetrics;
  POINTL aptlTabText[TXTBOX_COUNT];

  // Query dimensions of this tab text:
  hps = WinGetPS(hwndNotebook);
  memset(&fmFontMetrics, 0, sizeof(FONTMETRICS));
  if ((GpiQueryFontMetrics(hps, sizeof(FONTMETRICS), &fmFontMetrics)) &&
      (*pusTabTextHeight < fmFontMetrics.lMaxBaselineExt*2))
    *pusTabTextHeight = fmFontMetrics.lMaxBaselineExt*2;

  if ((GpiQueryTextBox(hps, strlen(szTabText), szTabText, TXTBOX_COUNT, aptlTabText)) &&
      (*pusTabTextLength < aptlTabText[TXTBOX_CONCAT].x))
    *pusTabTextLength = aptlTabText[TXTBOX_CONCAT].x;

  // Now add page
  *pulPageID = (ULONG) WinSendMsg( hwndNotebook,
                                 BKM_INSERTPAGE,
                                 0L,
                                 MPFROM2SHORT( (BKA_STATUSTEXTON | BKA_AUTOPAGESIZE | usType ),
					       BKA_LAST ) );
  // And set text for it
  WinSendMsg( hwndNotebook,
	      BKM_SETTABTEXT,
	      MPFROMLONG( *pulPageID ),
	      MPFROMP( szTabText ) );
  WinSendMsg( hwndNotebook,
	      BKM_SETSTATUSLINETEXT,
	      MPFROMLONG( *pulPageID ),
	      MPFROMP( szStatusText ) );
  WinSendMsg(hwndNotebook,
             BKM_SETPAGEWINDOWHWND,
             MPFROMP(*pulPageID),
             MPFROMP(hwndNotebookPage));
}

static void AdjustNewFormatStringDlgControls(HWND hwnd)
{
  SWP swpTemp, swpTemp2, swpWindow;
  int staticheight;
  ULONG CXDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
  ULONG CYDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);
  int iDisabledWindowUpdate=0;

  if (WinIsWindowVisible(hwnd))
  {
    WinEnableWindowUpdate(hwnd, FALSE);
    iDisabledWindowUpdate = 1;
  }

  // Query static text height in pixels
  if (WinQueryWindowPos(WinWindowFromID(hwnd, ST_KEYNAME), &swpTemp))
    staticheight = swpTemp.cy;
  else
    staticheight = 0;

  WinQueryWindowPos(hwnd, &swpWindow);

  // Move the cancel button to the right at the bottom!
  WinQueryWindowPos(WinWindowFromID(hwnd, PB_CANCEL), &swpTemp);
  swpTemp.y = CYDLGFRAME+2;
  swpTemp.x = swpWindow.cx/4*3-swpTemp.cx/2;
  WinSetWindowPos(WinWindowFromID(hwnd, PB_CANCEL), HWND_TOP,
		  swpTemp.x, swpTemp.y,
                  0, 0,
		  SWP_MOVE);
  // Move the create button to the left at the bottom!
  WinQueryWindowPos(WinWindowFromID(hwnd, PB_CREATE), &swpTemp);
  swpTemp.y = CYDLGFRAME+2;
  swpTemp.x = swpWindow.cx/4-swpTemp.cx/2;
  WinSetWindowPos(WinWindowFromID(hwnd, PB_CREATE), HWND_TOP,
		  swpTemp.x, swpTemp.y,
                  0, 0,
		  SWP_MOVE);

  // The Key name text to the top!
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_KEYNAME), &swpTemp);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_KEYNAME), HWND_TOP,
		  CXDLGFRAME*2,
		  swpWindow.cy - WinQuerySysValue(HWND_DESKTOP, SV_CYTITLEBAR) - CYDLGFRAME - 5 - staticheight,
                  0, 0,
		  SWP_MOVE);

  // The Key value text next to it!
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_KEYNAME), &swpTemp);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_KEYVALUE), &swpTemp2);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_KEYVALUE), HWND_TOP,
		  swpTemp.x,
		  swpTemp.y - 2*CYDLGFRAME - staticheight,
                  0, 0,
		  SWP_MOVE);

  // Position the two entry fields next to the texts!
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_KEYNAME), &swpTemp);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_KEYVALUE), &swpTemp2);
  WinSetWindowPos(WinWindowFromID(hwnd, EF_KEYNAME), HWND_TOP,
		  swpTemp2.x + swpTemp2.cx + 2*CXDLGFRAME,
		  swpTemp.y,
		  swpWindow.cx - 2*CXDLGFRAME - (swpTemp2.x + swpTemp2.cx + 2*CXDLGFRAME),
		  staticheight,
		  SWP_MOVE | SWP_SIZE);
  WinSetWindowPos(WinWindowFromID(hwnd, EF_KEYVALUE), HWND_TOP,
		  swpTemp2.x + swpTemp2.cx + 2*CXDLGFRAME,
		  swpTemp2.y,
		  swpWindow.cx - 2*CXDLGFRAME - (swpTemp2.x + swpTemp2.cx + 2*CXDLGFRAME),
		  staticheight,
		  SWP_MOVE | SWP_SIZE);

  // That's all!
  if (iDisabledWindowUpdate)
    WinEnableWindowUpdate(hwnd, TRUE);
}


static void AdjustChangeSamplerateDlgControls(HWND hwnd)
{
  SWP swpTemp, swpTemp2, swpWindow;
  int staticheight;
  ULONG CXDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
  ULONG CYDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);
  int iDisabledWindowUpdate=0;

  if (WinIsWindowVisible(hwnd))
  {
    WinEnableWindowUpdate(hwnd, FALSE);
    iDisabledWindowUpdate = 1;
  }

  // Query static text height in pixels
  if (WinQueryWindowPos(WinWindowFromID(hwnd, ST_SHOWNORIGINALSAMPLERATE), &swpTemp))
    staticheight = swpTemp.cy;
  else
    staticheight = 0;

  WinQueryWindowPos(hwnd, &swpWindow);

  // Move the cancel button to the middle at the bottom!
  WinQueryWindowPos(WinWindowFromID(hwnd, PB_CANCELCHANGESAMPLERATE), &swpTemp);
  swpTemp.y = CYDLGFRAME+2;
  swpTemp.x = (swpWindow.cx-swpTemp.cx)/2;
  WinSetWindowPos(WinWindowFromID(hwnd, PB_CANCELCHANGESAMPLERATE), HWND_TOP,
		  swpTemp.x, swpTemp.y,
                  0, 0,
		  SWP_MOVE);

  // The Original samplerate text to the top!
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_ORIGINALSAMPLERATE), &swpTemp);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_ORIGINALSAMPLERATE), HWND_TOP,
		  CXDLGFRAME*2,
		  swpWindow.cy - WinQuerySysValue(HWND_DESKTOP, SV_CYTITLEBAR) - CYDLGFRAME - 5 - staticheight,
		  swpTemp.cx, staticheight,
		  SWP_MOVE | SWP_SIZE);

  // The ShownOriginalSamplerate next to it!
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_ORIGINALSAMPLERATE), &swpTemp);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_SHOWNORIGINALSAMPLERATE), HWND_TOP,
		  swpTemp.x + swpTemp.cx + CXDLGFRAME*2,
		  swpTemp.y,
		  swpWindow.cx - 2*CXDLGFRAME - (swpTemp.x + swpTemp.cx + CXDLGFRAME*2),
		  staticheight,
		  SWP_MOVE | SWP_SIZE);

  // Position the two pushbuttons under the samplerate text!
  WinQueryWindowPos(WinWindowFromID(hwnd, PB_CHANGESAMPLERATE), &swpTemp);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_ORIGINALSAMPLERATE), &swpTemp2);
  WinSetWindowPos(WinWindowFromID(hwnd, PB_CHANGESAMPLERATE), HWND_TOP,
		  swpWindow.cx/3 - swpTemp.cx/2,
		  swpTemp2.y - 2*CYDLGFRAME - swpTemp.cy,
                  0, 0,
		  SWP_MOVE);
  WinSetWindowPos(WinWindowFromID(hwnd, PB_CONVERTSAMPLERATETO), HWND_TOP,
		  swpWindow.cx/3*2 - swpTemp.cx/2,
		  swpTemp2.y - 2*CYDLGFRAME - swpTemp.cy,
                  0, 0,
		  SWP_MOVE);

  // The "New samplerate" text
  WinQueryWindowPos(WinWindowFromID(hwnd, PB_CHANGESAMPLERATE), &swpTemp);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_NEWSAMPLERATE), HWND_TOP,
		  CXDLGFRAME*2,
		  swpTemp.y - 2*CYDLGFRAME - staticheight,
                  0, 0,
		  SWP_MOVE);

  // The new samplerate edit field (combo box)
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_NEWSAMPLERATE), &swpTemp);
  WinSetWindowPos(WinWindowFromID(hwnd, CX_NEWSAMPLERATE), HWND_TOP,
		  swpTemp.x + swpTemp.cx + 2*CXDLGFRAME,
		  swpTemp.y - 4*swpTemp.cy + CYDLGFRAME,
                  swpTemp.cx, swpTemp.cy*4 +3*CYDLGFRAME,
		  SWP_MOVE | SWP_SIZE);
  // The Hz text
  WinSetWindowPos(WinWindowFromID(hwnd, ST_CHANGESAMPLERATEHZ), HWND_TOP,
		  swpTemp.x + swpTemp.cx + 2*CXDLGFRAME + swpTemp.cx + 2*CXDLGFRAME,
		  swpTemp.y,
                  0, 0,
		  SWP_MOVE);

  // That's all!
  if (iDisabledWindowUpdate)
    WinEnableWindowUpdate(hwnd, TRUE);
}


static void AdjustPropertiesDlgControls(PropertiesWindowList_p pWindowDesc)
{
  SWP swpTemp, swpWindow;
  int buttonheight;
  ULONG CXDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
  ULONG CYDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);
  int iDisabledWindowUpdate=0;

  if (WinIsWindowVisible(pWindowDesc->hwndPropertiesWindow))
  {
    WinEnableWindowUpdate(pWindowDesc->hwndPropertiesWindow, FALSE);
    iDisabledWindowUpdate = 1;
  }

  // Query button height in pixels
  if (WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndPropertiesWindow, PB_HELP), &swpTemp))
    buttonheight = swpTemp.cy;
  else
    buttonheight = 0;

  WinQueryWindowPos(pWindowDesc->hwndPropertiesWindow, &swpWindow);

  // Notebook position is above the buttons, which is some pixels above the bottom of window:
  swpTemp.x = CXDLGFRAME;
  swpTemp.y = CYDLGFRAME + buttonheight + 5;
  // Notebook size is just calculated:
  swpTemp.cx = swpWindow.cx - 2*CXDLGFRAME;
  swpTemp.cy = swpWindow.cy - swpTemp.y - WinQuerySysValue(HWND_DESKTOP, SV_CYTITLEBAR) - CYDLGFRAME - 5;
  WinSetWindowPos(pWindowDesc->hwndPropertiesNB, HWND_TOP,
		  swpTemp.x, swpTemp.y,
		  swpTemp.cx, swpTemp.cy,
		  SWP_MOVE | SWP_SIZE);

  // Set the position of button(s)

  //  Help Button
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndPropertiesWindow, PB_HELP), &swpTemp);
  swpTemp.y = CYDLGFRAME+2;
  swpTemp.x = (swpWindow.cx/4)*2-swpTemp.cx/2;
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndPropertiesWindow, PB_HELP), HWND_TOP,
		  swpTemp.x, swpTemp.y,
		  swpTemp.cx, swpTemp.cy,
		  SWP_MOVE);

  // That's all!
  if (iDisabledWindowUpdate)
    WinEnableWindowUpdate(pWindowDesc->hwndPropertiesWindow, TRUE);

}

static void AdjustGeneralNotebookControls(PropertiesWindowList_p pWindowDesc)
{
  SWP swpTemp, swpWindow;
  int staticheight;
  ULONG CXDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
  ULONG CYDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);
  int iDisabledWindowUpdate=0;

  if (WinIsWindowVisible(pWindowDesc->hwndGeneralNBP))
  {
    WinEnableWindowUpdate(pWindowDesc->hwndGeneralNBP, FALSE);
    iDisabledWindowUpdate = 1;
  }

  // Query static height in pixels
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndGeneralNBP, ST_CHSETNAME), &swpTemp);
  staticheight = swpTemp.cy;

  // Get window size!
  WinQueryWindowPos(pWindowDesc->hwndGeneralNBP, &swpWindow);

  // Set the position of "Length:" static control
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndGeneralNBP, ST_CHSETLENGTH), &swpTemp);
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndGeneralNBP, ST_CHSETLENGTH),
		  HWND_TOP,
		  CXDLGFRAME,
		  3*CYDLGFRAME + (staticheight+CYDLGFRAME)*2, // pos
		  0, 0,                                                 // size
		  SWP_MOVE);
  // Set position and size of length static controls
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndGeneralNBP, ST_CHSETLENGTHINMSEC),
		  HWND_TOP,
		  CXDLGFRAME+swpTemp.cx+CXDLGFRAME,
		  3*CYDLGFRAME + (staticheight+CYDLGFRAME)*2, // pos
		  swpWindow.cx - CXDLGFRAME - CXDLGFRAME - swpTemp.cx - CXDLGFRAME,
		  staticheight,                                      // size
		  SWP_MOVE | SWP_SIZE);
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndGeneralNBP, ST_CHSETLENGTHINTIME),
		  HWND_TOP,
		  CXDLGFRAME+swpTemp.cx+CXDLGFRAME,
		  3*CYDLGFRAME + (staticheight+CYDLGFRAME)*1, // pos
		  swpWindow.cx - CXDLGFRAME - CXDLGFRAME - swpTemp.cx - CXDLGFRAME,
		  staticheight,                                      // size
		  SWP_MOVE | SWP_SIZE);
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndGeneralNBP, ST_CHSETLENGTHINSAMPLES),
		  HWND_TOP,
		  CXDLGFRAME+swpTemp.cx+CXDLGFRAME,
		  3*CYDLGFRAME, // pos
		  swpWindow.cx - CXDLGFRAME - CXDLGFRAME - swpTemp.cx - CXDLGFRAME,
		  staticheight,                                      // size
		  SWP_MOVE | SWP_SIZE);

  // Now the filename edit field!
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndGeneralNBP, EF_CHSETFILENAME),
		  HWND_TOP,
		  2*CXDLGFRAME,
		  3*CYDLGFRAME + (staticheight+CYDLGFRAME)*3 + 2*CXDLGFRAME, // pos
		  swpWindow.cx - 2*CXDLGFRAME - 2*CXDLGFRAME,
		  staticheight,                                      // size
		  SWP_MOVE | SWP_SIZE);

  // Static text of filename
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndGeneralNBP, ST_CHSETFILENAME),
		  HWND_TOP,
		  CXDLGFRAME,
		  3*CYDLGFRAME + (staticheight+CYDLGFRAME)*4 + 2*CXDLGFRAME, // pos
                  0, 0,
		  SWP_MOVE);

  // Now the name edit field!
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndGeneralNBP, EF_CHSETNAME),
		  HWND_TOP,
		  2*CXDLGFRAME,
		  3*CYDLGFRAME + (staticheight+CYDLGFRAME)*5 + 6*CXDLGFRAME, // pos
		  swpWindow.cx - 2*CXDLGFRAME - 2*CXDLGFRAME,
		  staticheight,                                      // size
		  SWP_MOVE | SWP_SIZE);

  // Static text of filename
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndGeneralNBP, ST_CHSETNAME),
		  HWND_TOP,
		  CXDLGFRAME,
		  3*CYDLGFRAME + (staticheight+CYDLGFRAME)*6 + 6*CXDLGFRAME, // pos
                  0, 0,
		  SWP_MOVE);
  // That's all!
  if (iDisabledWindowUpdate)
    WinEnableWindowUpdate(pWindowDesc->hwndGeneralNBP, TRUE);

}

static void UpdateGeneralNotebookControlValues(HWND hwnd, PropertiesWindowList_p pWindowDesc)
{
  char *pchBuffer;
  int iBufferSize;
  __int64 i64HH, i64MM, i64SS, i64msec;

#ifdef DEBUG_BUILD
//  printf("[UpdateGeneralNotebookControlValues] : Enter.\n"); fflush(stdout);
//  printf("[UpdateGeneralNotebookControlValues] : name: [%s]\n", pWindowDesc->pChannelSet->achName); fflush(stdout);
#endif

  iBufferSize = 2048;
  pchBuffer = (char *) malloc(iBufferSize);
  if (pchBuffer)
  {

    // Only update the entry fields if they differ! This resolves recursive updating.
    // (updating it means changing it, meaning calling back to wawe, meaning its updating, etc...)
    WinQueryWindowText(WinWindowFromID(hwnd, EF_CHSETNAME),
		       iBufferSize,
		       pchBuffer);
    if (strcmp(pchBuffer, pWindowDesc->pChannelSet->achName))
    {
      WinSetWindowText(WinWindowFromID(hwnd, EF_CHSETNAME),
		       pWindowDesc->pChannelSet->achName);
      WinSendDlgItemMsg(hwnd, EF_CHSETNAME, EM_QUERYCHANGED, (MPARAM) 0, (MPARAM) 0); // Clear "Changed" flag!
    }

    WinQueryWindowText(WinWindowFromID(hwnd, EF_CHSETFILENAME),
		       iBufferSize,
		       pchBuffer);
    if (strcmp(pchBuffer, pWindowDesc->pChannelSet->achFileName))
    {
      WinSetWindowText(WinWindowFromID(hwnd, EF_CHSETFILENAME),
		       pWindowDesc->pChannelSet->achFileName);
      WinSendDlgItemMsg(hwnd, EF_CHSETFILENAME, EM_QUERYCHANGED, (MPARAM) 0, (MPARAM) 0); // Clear "Changed" flag!
    }

    // Samples
    snprintf(pchBuffer, iBufferSize, "%I64d sample(s)",
	     pWindowDesc->pChannelSet->Length);
    WinSetWindowText(WinWindowFromID(hwnd, ST_CHSETLENGTHINSAMPLES),
		     pchBuffer);

    // Time
    i64msec = pWindowDesc->pChannelSet->Length*1000/pWindowDesc->pChannelSet->OriginalFormat.iFrequency;

    i64SS = i64msec / 1000;
    i64msec = i64msec % 1000;

    i64MM = i64SS / 60;
    i64SS = i64SS % 60;

    i64HH = i64MM / 60;
    i64MM = i64MM % 60;

    snprintf(pchBuffer, iBufferSize, "%I64d:%02I64d:%02I64d.%03I64d",
	     i64HH, i64MM, i64SS, i64msec);
    WinSetWindowText(WinWindowFromID(hwnd, ST_CHSETLENGTHINTIME),
                     pchBuffer);

    // msec
    snprintf(pchBuffer, iBufferSize, "%I64d msec",
	     (pWindowDesc->pChannelSet->Length*1000/pWindowDesc->pChannelSet->OriginalFormat.iFrequency));
    WinSetWindowText(WinWindowFromID(hwnd, ST_CHSETLENGTHINMSEC),
                     pchBuffer);

    // done
    free(pchBuffer);
  }

}

static void AdjustFormatCommonNotebookControls(PropertiesWindowList_p pWindowDesc)
{
  SWP swpTemp, swpTemp2, swpTemp3, swpWindow;
  int staticheight;
  ULONG CXDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
  ULONG CYDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);
  int iDisabledWindowUpdate=0;

  if (WinIsWindowVisible(pWindowDesc->hwndFormatCommonNBP))
  {
    WinEnableWindowUpdate(pWindowDesc->hwndFormatCommonNBP, FALSE);
    iDisabledWindowUpdate = 1;
  }

  // Query static height in pixels. We'll use the ShowNumOfChannels static, as it's the highest. (9.WarpSans Bold)
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndFormatCommonNBP, ST_SHOWNUMOFCHANNELS), &swpTemp);
  staticheight = swpTemp.cy;

  // Get window size!
  WinQueryWindowPos(pWindowDesc->hwndFormatCommonNBP, &swpWindow);

  // Set the position of "Number of channels:" static control to top of window
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndFormatCommonNBP, ST_NUMBEROFCHANNELS),
		  HWND_TOP,
		  CXDLGFRAME,
                  swpWindow.cy - staticheight - 2*CYDLGFRAME,
		  0, 0,                                                 // size
		  SWP_MOVE);
  // Set position and size of Number of channels static control
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndFormatCommonNBP, ST_NUMBEROFCHANNELS), &swpTemp);
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndFormatCommonNBP, ST_SHOWNUMOFCHANNELS),
		  HWND_TOP,
		  swpTemp.x+swpTemp.cx+CXDLGFRAME,
		  swpTemp.y, // pos
		  swpWindow.cx - CXDLGFRAME - CXDLGFRAME - swpTemp.cx - CXDLGFRAME,
		  staticheight,                                      // size
		  SWP_MOVE | SWP_SIZE);

  // Set the position of "Samplerate:" static control
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndFormatCommonNBP, ST_SAMPLERATE),
		  HWND_TOP,
		  CXDLGFRAME,
                  swpWindow.cy - 2*(staticheight + 2*CYDLGFRAME),
		  0, 0,                                                 // size
		  SWP_MOVE);
  // Set position of "Change" button
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndFormatCommonNBP, PB_SAMPLERATECHANGE), &swpTemp);
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndFormatCommonNBP, ST_SAMPLERATE), &swpTemp2);
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndFormatCommonNBP, PB_SAMPLERATECHANGE),
		  HWND_TOP,
		  swpWindow.cx - CXDLGFRAME - swpTemp.cx,
		  swpTemp2.y + staticheight/2 - swpTemp.cy/2,
		  0, 0,
		  SWP_MOVE);

  // Set size and position of Real Samplerate static control
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndFormatCommonNBP, PB_SAMPLERATECHANGE), &swpTemp);
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndFormatCommonNBP, ST_NUMBEROFCHANNELS), &swpTemp2);
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndFormatCommonNBP, ST_SAMPLERATE), &swpTemp3);
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndFormatCommonNBP, ST_SAMPLERATEHZ),
		  HWND_TOP,
		  swpTemp2.x + swpTemp2.cx + CXDLGFRAME,
		  swpTemp3.y,
		  swpTemp.x - CXDLGFRAME - (swpTemp2.x + swpTemp2.cx + CXDLGFRAME),
		  staticheight,                                      // size
		  SWP_MOVE | SWP_SIZE);

  // Set position of "Bits per sample:" static control
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndFormatCommonNBP, ST_BITSPERSAMPLE),
		  HWND_TOP,
		  CXDLGFRAME,
                  swpWindow.cy - 3*(staticheight + 2*CYDLGFRAME),
		  0, 0,                                                 // size
		  SWP_MOVE);

  // Set size and position of Bits per sample edit field
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndFormatCommonNBP, ST_BITSPERSAMPLE), &swpTemp);
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndFormatCommonNBP, ST_NUMBEROFCHANNELS), &swpTemp2);
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndFormatCommonNBP, ST_SAMPLERATE), &swpTemp3);
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndFormatCommonNBP, EF_BITSPERSAMPLE),
		  HWND_TOP,
		  swpTemp2.x + swpTemp2.cx + 2*CXDLGFRAME,
                  swpTemp.y,
		  swpTemp3.cx / 2,
		  swpTemp3.cy,                                                 // size
		  SWP_MOVE | SWP_SIZE);

  // Set position of "Signed samples:" static control
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndFormatCommonNBP, ST_SIGNEDSAMPLES),
		  HWND_TOP,
		  CXDLGFRAME,
                  swpWindow.cy - 4*(staticheight + 2*CYDLGFRAME),
		  0, 0,                                                 // size
		  SWP_MOVE);

  // Set position of Signed/Unsigned combo box
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndFormatCommonNBP, ST_SIGNEDSAMPLES), &swpTemp);
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndFormatCommonNBP, ST_NUMBEROFCHANNELS), &swpTemp2);
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndFormatCommonNBP, CX_SIGNEDUNSIGNED), &swpTemp3);
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndFormatCommonNBP, CX_SIGNEDUNSIGNED),
		  HWND_TOP,
		  swpTemp2.x + swpTemp2.cx + CXDLGFRAME,
                  swpTemp.y + swpTemp.cy + CYDLGFRAME - swpTemp3.cy,
		  0, 0,                                                 // size
		  SWP_MOVE);


  // Set size and position of info field
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndFormatCommonNBP, ST_SIGNEDSAMPLES), &swpTemp);
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndFormatCommonNBP, ST_NOTE),
		  HWND_TOP,
		  CXDLGFRAME,
                  CYDLGFRAME,
		  swpWindow.cx - 2*CXDLGFRAME,
		  swpTemp.y - 6*CYDLGFRAME,                            // size
		  SWP_MOVE | SWP_SIZE);

  // That's all!
  if (iDisabledWindowUpdate)
    WinEnableWindowUpdate(pWindowDesc->hwndFormatCommonNBP, TRUE);

}

static void UpdateFormatCommonNotebookControlValues(HWND hwnd, PropertiesWindowList_p pWindowDesc)
{
  char *pchBuffer, *pchBuffer2;
  int iBufferSize, iBuffer2Size;
  int i, iCount;

  iBuffer2Size = 128;
  pchBuffer2 = (char *) malloc(iBuffer2Size);
  if (!pchBuffer2)
    iBuffer2Size = 0;

  iBufferSize = 128;
  pchBuffer = (char *) malloc(iBufferSize);
  if (pchBuffer)
  {
    // Number of channels
    switch (pWindowDesc->pChannelSet->iNumOfChannels)
    {
      case 0:
	sprintf(pchBuffer, "0 (No channels inside)");
	break;
      case 1:
        sprintf(pchBuffer, "1 (Mono)");
	break;
      case 2:
        sprintf(pchBuffer, "2 (Stereo)");
	break;
      default:
	snprintf(pchBuffer, iBufferSize, "%d",
		 pWindowDesc->pChannelSet->iNumOfChannels);
	break;
    }
    WinSetWindowText(WinWindowFromID(hwnd, ST_SHOWNUMOFCHANNELS),
		     pchBuffer);

    // Samplerate:
    switch (pWindowDesc->pChannelSet->OriginalFormat.iFrequency)
    {
      case 44100:
        sprintf(pchBuffer, "44100 Hz (CD quality)");
	break;
      case 11025:
        sprintf(pchBuffer, "11025 Hz (Radio quality)");
	break;
      default:
	snprintf(pchBuffer, iBufferSize, "%d Hz",
		 pWindowDesc->pChannelSet->OriginalFormat.iFrequency);
	break;
    }
    WinSetWindowText(WinWindowFromID(hwnd, ST_SAMPLERATEHZ),
		     pchBuffer);

    // Bits per sample
    snprintf(pchBuffer, iBufferSize, "%d",
	     pWindowDesc->pChannelSet->OriginalFormat.iBits);

    if (pchBuffer2)
    {
      WinQueryWindowText(WinWindowFromID(hwnd, EF_BITSPERSAMPLE),
			 iBuffer2Size,
			 pchBuffer2);
      if (strcmp(pchBuffer, pchBuffer2))
      {
	WinSetWindowText(WinWindowFromID(hwnd, EF_BITSPERSAMPLE),
			 pchBuffer);
	WinSendDlgItemMsg(hwnd, EF_BITSPERSAMPLE, EM_QUERYCHANGED, (MPARAM) 0, (MPARAM) 0); // Clear "Changed" flag!
      }
    } else
    {
      WinSetWindowText(WinWindowFromID(hwnd, EF_BITSPERSAMPLE),
		       pchBuffer);
      WinSendDlgItemMsg(hwnd, EF_BITSPERSAMPLE, EM_QUERYCHANGED, (MPARAM) 0, (MPARAM) 0); // Clear "Changed" flag!
    }

    // Signed/Unsigned samples
    iCount = (int) WinSendDlgItemMsg(hwnd, CX_SIGNEDUNSIGNED, LM_QUERYITEMCOUNT, (MPARAM) 0, (MPARAM) 0);
    for (i=0; i<iCount; i++)
    {
      if (((int) WinSendDlgItemMsg(hwnd, CX_SIGNEDUNSIGNED, LM_QUERYITEMHANDLE, (MPARAM) i, (MPARAM) 0)) ==
	  pWindowDesc->pChannelSet->OriginalFormat.iIsSigned)
      {
        // Select this item!
        WinSendDlgItemMsg(hwnd, CX_SIGNEDUNSIGNED, LM_SELECTITEM, (MPARAM) i, (MPARAM) TRUE);
      }
    }

    // done
    free(pchBuffer);
  }

  if (pchBuffer2) free(pchBuffer2);
}

static void AdjustFormatSpecificNotebookControls(PropertiesWindowList_p pWindowDesc)
{
  SWP swpTemp, swpWindow;
  ULONG CXDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
  ULONG CYDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);
  int iDisabledWindowUpdate=0;

  if (WinIsWindowVisible(pWindowDesc->hwndFormatSpecificNBP))
  {
    WinEnableWindowUpdate(pWindowDesc->hwndFormatSpecificNBP, FALSE);
    iDisabledWindowUpdate = 1;
  }

  // Get window size!
  WinQueryWindowPos(pWindowDesc->hwndFormatSpecificNBP, &swpWindow);

  // Set position of "Modify selected" pushbutton to middle
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndFormatSpecificNBP, PB_MODIFYSELECTED), &swpTemp);
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndFormatSpecificNBP, PB_MODIFYSELECTED),
		  HWND_TOP,
		  swpWindow.cx - CXDLGFRAME - swpTemp.cx,
                  (swpWindow.cy - swpTemp.cy)/2,
		  0, 0,                                                 // size
		  SWP_MOVE);

  // Set position of "Add new" pushbutton to just above the Modify button
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndFormatSpecificNBP, PB_MODIFYSELECTED), &swpTemp);
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndFormatSpecificNBP, PB_ADDNEW),
		  HWND_TOP,
		  swpTemp.x,
                  swpTemp.y + swpTemp.cy + CYDLGFRAME,
		  0, 0,                                                 // size
		  SWP_MOVE);

  // Set position of "Delete selected" pushbutton to just above the Modify button
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndFormatSpecificNBP, PB_DELETESELECTED),
		  HWND_TOP,
		  swpTemp.x,
                  swpTemp.y - CYDLGFRAME - swpTemp.cy,
		  0, 0,                                                 // size
		  SWP_MOVE);

  // Set position of "Move up" button to top of window
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndFormatSpecificNBP, PB_MOVEUP),
		  HWND_TOP,
		  swpTemp.x,
                  swpWindow.cy - CYDLGFRAME - swpTemp.cy,
		  0, 0,                                                 // size
		  SWP_MOVE);

  // Set position of "Move down" button to bottom of window
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndFormatSpecificNBP, PB_MOVEDOWN),
		  HWND_TOP,
		  swpTemp.x,
                  CYDLGFRAME,
		  0, 0,                                                 // size
		  SWP_MOVE);

  // Set size and position of listbox
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndFormatSpecificNBP, LB_FORMATSPECSTRINGS),
		  HWND_TOP,
		  CXDLGFRAME,
                  CYDLGFRAME,
		  swpTemp.x - 2*CXDLGFRAME,
		  swpWindow.cy - 2*CYDLGFRAME,                            // size
		  SWP_MOVE | SWP_SIZE);

  // That's all!
  if (iDisabledWindowUpdate)
    WinEnableWindowUpdate(pWindowDesc->hwndFormatSpecificNBP, TRUE);

}

static void UpdateFormatSpecificNotebookControlValues(HWND hwnd, PropertiesWindowList_p pWindowDesc)
{
  char *pchBuffer;
  long lID, lOriginalID;
  int iBufferSize, iKeyNameSize, iKeyValueSize;
  int i, iCount;

  // Get the currently selected item in listbox
  lOriginalID = (long) WinSendDlgItemMsg(hwnd, LB_FORMATSPECSTRINGS, LM_QUERYSELECTION,
                                         (MPARAM) LIT_CURSOR, (MPARAM) 0);

  // Now rebuild the list!
  PluginHelpers.fn_WaWEHelper_ChannelSet_StartChSetFormatUsage(pWindowDesc->pChannelSet);


  if (PluginHelpers.fn_WaWEHelper_ChannelSetFormat_GetNumOfKeys(&(pWindowDesc->pChannelSet->ChannelSetFormat), &iCount) == WAWEHELPER_NOERROR)
  {
    WinSendDlgItemMsg(hwnd, LB_FORMATSPECSTRINGS, LM_DELETEALL,
                      (MPARAM) 0, (MPARAM) 0);
  
    for (i=0; i<iCount; i++)
    {
      if (PluginHelpers.fn_WaWEHelper_ChannelSetFormat_GetKeySizeByPosition(&(pWindowDesc->pChannelSet->ChannelSetFormat), i, &iKeyNameSize, &iKeyValueSize) == WAWEHELPER_NOERROR)
      {
        // We'll have the key+value pair in this format:
        // Key = Value, so we'll need some more bytes. Make it 10, that will be enough.
        iBufferSize = iKeyNameSize + iKeyValueSize + 10;
        if (iBufferSize>10)
        {
          pchBuffer = (char *) malloc(iBufferSize);
          if (pchBuffer)
          {
            if (PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKeyByPosition(&(pWindowDesc->pChannelSet->ChannelSetFormat),
                                                                               i,
                                                                               pchBuffer,
                                                                               iBufferSize,
                                                                               pchBuffer+iKeyNameSize+2,
                                                                               iKeyValueSize) == WAWEHELPER_NOERROR)
            {
              // Fine, got the key by position!
              // Prepare it to be in Key = Value form!
              pchBuffer[iKeyNameSize-1] = ' ';
              pchBuffer[iKeyNameSize]   = '=';
              pchBuffer[iKeyNameSize+1] = ' ';
              // Add it to the listbox!
              lID = (long) WinSendDlgItemMsg(hwnd, LB_FORMATSPECSTRINGS, LM_INSERTITEM,
                                             (MPARAM) LIT_END, (MPARAM) pchBuffer);
              WinSendDlgItemMsg(hwnd, LB_FORMATSPECSTRINGS, LM_SETITEMHANDLE,
                                (MPARAM) lID, (MPARAM) i);
            }
            free(pchBuffer);
          }
        }
      }
    }

    // Set again the last active selection!
    if (lOriginalID!=LIT_NONE)
      WinSendDlgItemMsg(hwnd, LB_FORMATSPECSTRINGS, LM_SELECTITEM,
                        (MPARAM) lOriginalID, (MPARAM) TRUE);
  }
  PluginHelpers.fn_WaWEHelper_ChannelSet_StopChSetFormatUsage(pWindowDesc->pChannelSet, FALSE);
  // done
}

static void AdjustChannelsNotebookControls(PropertiesWindowList_p pWindowDesc)
{
  SWP swpTemp, swpTemp2, swpTemp3, swpWindow;
  ULONG CXDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
  ULONG CYDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);
  int staticheight;
  int iDisabledWindowUpdate=0;

  if (WinIsWindowVisible(pWindowDesc->hwndChannelsNBP))
  {
    WinEnableWindowUpdate(pWindowDesc->hwndChannelsNBP, FALSE);
    iDisabledWindowUpdate = 1;
  }

  // Query static height in pixels. 
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndChannelsNBP, ST_CHANNEL), &swpTemp);
  staticheight = swpTemp.cy;

  // Get window size!
  WinQueryWindowPos(pWindowDesc->hwndChannelsNBP, &swpWindow);

  // Set position of "Channel:" static control to top-left corner!
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndChannelsNBP, ST_CHANNEL),
		  HWND_TOP,
		  CXDLGFRAME,
                  swpWindow.cy - staticheight - 2*CYDLGFRAME,
		  0, 0,                                                 // size
		  SWP_MOVE);

  // Set position of list of channels combobox next to channel static control
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndChannelsNBP, ST_CHANNEL), &swpTemp);
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndChannelsNBP, CX_LISTOFCHANNELS), &swpTemp2);
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndChannelsNBP, CX_LISTOFCHANNELS),
		  HWND_TOP,
		  swpTemp.x + swpTemp.cx + CXDLGFRAME,
                  swpTemp.y + staticheight + CYDLGFRAME - swpTemp2.cy,
		  swpWindow.cx - (swpTemp.x + swpTemp.cx + CXDLGFRAME) - CXDLGFRAME,
		  swpTemp2.cy,                                                 // size
		  SWP_MOVE | SWP_SIZE);

  // Set position and size of Properties groupbox to fill remaining space
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndChannelsNBP, GB_PROPERTIESOFSELECTEDCHANNEL),
		  HWND_TOP,
		  2*CXDLGFRAME,
                  2*CYDLGFRAME,
		  swpWindow.cx - 4*CXDLGFRAME,
		  swpTemp.y - 6*CYDLGFRAME,                                 // size
		  SWP_MOVE | SWP_SIZE);

  // Set position of "Name:" static control to top left corner of groupbox
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndChannelsNBP, GB_PROPERTIESOFSELECTEDCHANNEL), &swpTemp);
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndChannelsNBP, ST_CHNAME),
		  HWND_TOP,
		  swpTemp.x + 3*CXDLGFRAME,
                  swpTemp.y + swpTemp.cy - staticheight - CYDLGFRAME - staticheight - CYDLGFRAME,
		  0, 0,
		  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndChannelsNBP, ST_CHNAME), &swpTemp2);
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndChannelsNBP, EF_CHANNELNAME),
		  HWND_TOP,
		  swpTemp2.x + swpTemp2.cx + 2*CXDLGFRAME,
                  swpTemp2.y,
		  swpTemp.cx + swpTemp.x - (swpTemp2.x + swpTemp2.cx + 2*CXDLGFRAME) - 4*CXDLGFRAME,
		  staticheight,
		  SWP_MOVE | SWP_SIZE);

  // Move Up/Down buttons to bottom-right corner of groupbox
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndChannelsNBP, PB_CHMOVEDOWN), &swpTemp2);
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndChannelsNBP, PB_CHMOVEDOWN),
		  HWND_TOP,
		  swpTemp.x + swpTemp.cx - 3*CXDLGFRAME - swpTemp2.cx,
                  swpTemp.y + 3*CYDLGFRAME,
		  0, 0,
		  SWP_MOVE);
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndChannelsNBP, PB_CHMOVEDOWN), &swpTemp2);
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndChannelsNBP, PB_CHMOVEUP),
		  HWND_TOP,
		  swpTemp2.x,
                  swpTemp2.y + swpTemp2.cy + CYDLGFRAME/2,
		  0, 0,
		  SWP_MOVE);

  // "Position:" static text
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndChannelsNBP, ST_CHNAME), &swpTemp2);
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndChannelsNBP, PB_CHMOVEDOWN), &swpTemp3);
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndChannelsNBP, ST_POSITION),
		  HWND_TOP,
		  swpTemp2.x,
                  swpTemp3.y + (2*swpTemp3.cy + CYDLGFRAME/2)/2 - staticheight/2,
		  0, 0,
		  SWP_MOVE);

  // "ChannelPosition" static text
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndChannelsNBP, ST_POSITION), &swpTemp2);
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndChannelsNBP, PB_CHMOVEDOWN), &swpTemp3);
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndChannelsNBP, ST_CHANNELPOSITION),
		  HWND_TOP,
		  swpTemp2.x + swpTemp2.cx + CXDLGFRAME,
                  swpTemp2.y,
		  swpTemp3.x - CXDLGFRAME - (swpTemp2.x + swpTemp2.cx + CXDLGFRAME),
		  staticheight,
		  SWP_MOVE | SWP_SIZE);

  // Now the channel length stuff
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndChannelsNBP, ST_CHNAME), &swpTemp2);
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndChannelsNBP, ST_CHLENGTH),
		  HWND_TOP,
		  swpTemp2.x,
		  swpTemp2.y - 2*CYDLGFRAME - staticheight,
                  0, 0,
		  SWP_MOVE);

  // The length values in different units
  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndChannelsNBP, ST_CHLENGTH), &swpTemp2);
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndChannelsNBP, ST_CHLENGTHINMSEC),
		  HWND_TOP,
		  swpTemp2.x + swpTemp2.cx + CXDLGFRAME,
		  swpTemp2.y,
		  swpTemp.x + swpTemp.cx - 3*CXDLGFRAME - (swpTemp2.x + swpTemp2.cx + CXDLGFRAME),
		  staticheight,
		  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndChannelsNBP, ST_CHLENGTHINMSEC), &swpTemp2);
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndChannelsNBP, ST_CHLENGTHINTIME),
		  HWND_TOP,
		  swpTemp2.x,
		  swpTemp2.y - CYDLGFRAME - staticheight,
		  swpTemp2.cx,
		  staticheight,
		  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(pWindowDesc->hwndChannelsNBP, ST_CHLENGTHINTIME), &swpTemp2);
  WinSetWindowPos(WinWindowFromID(pWindowDesc->hwndChannelsNBP, ST_CHLENGTHINSAMPLES),
		  HWND_TOP,
		  swpTemp2.x,
		  swpTemp2.y - CYDLGFRAME - staticheight,
		  swpTemp2.cx,
		  staticheight,
		  SWP_MOVE | SWP_SIZE);

  // That's all!
  if (iDisabledWindowUpdate)
    WinEnableWindowUpdate(pWindowDesc->hwndChannelsNBP, TRUE);

}

static void UpdateChannelsNotebookChPropsControlValues(HWND hwnd, PropertiesWindowList_p pWindowDesc)
{
  char *pchBuffer;
  int iBufferSize;
  long lID;
  WaWEChannel_p pChannel, pTempChannel;
  WaWEChannelSet_p pChannelSet;
  __int64 i64HH, i64MM, i64SS, i64msec;
  int iPos;

  iBufferSize = 2048;
  pchBuffer = (char *) malloc(iBufferSize);
  if (pchBuffer)
  {
    // Get the currently selected item in listbox
    lID = (long) WinSendDlgItemMsg(hwnd, CX_LISTOFCHANNELS, LM_QUERYSELECTION,
				   (MPARAM) LIT_CURSOR, (MPARAM) 0);
    if (lID==LIT_NONE)
    {
      // No item selected in listbox!
      pChannel = NULL;
    } else
    {
      pChannel = (WaWEChannel_p) WinSendDlgItemMsg(hwnd, CX_LISTOFCHANNELS, LM_QUERYITEMHANDLE,
						   (MPARAM) lID, (MPARAM) 0);
    }

    if (pChannel)
    {
      pChannelSet = pChannel->pParentChannelSet;
    }

    if ((pChannel) && (pChannelSet))
    {
      // Fill stuffs with data from this channel!

      // Only update the entry fields if they differ! This resolves recursive updating.
      // (updating it means changing it, meaning calling back to wawe, meaning its updating, etc...)
      WinQueryWindowText(WinWindowFromID(hwnd, EF_CHANNELNAME),
			 iBufferSize,
			 pchBuffer);
      if (strcmp(pchBuffer, pChannel->achName))
      {
	WinSetWindowText(WinWindowFromID(hwnd, EF_CHANNELNAME), pChannel->achName);
        WinSendDlgItemMsg(hwnd, EF_CHANNELNAME, EM_QUERYCHANGED, (MPARAM) 0, (MPARAM) 0); // Clear "Changed" flag!
      }

      // Samples
      snprintf(pchBuffer, iBufferSize, "%I64d sample(s)",
	       pChannel->Length);
      WinSetWindowText(WinWindowFromID(hwnd, ST_CHLENGTHINSAMPLES),
		       pchBuffer);

      // Time
      i64msec = pChannel->Length*1000/pChannelSet->OriginalFormat.iFrequency;

      i64SS = i64msec / 1000;
      i64msec = i64msec % 1000;

      i64MM = i64SS / 60;
      i64SS = i64SS % 60;

      i64HH = i64MM / 60;
      i64MM = i64MM % 60;

      snprintf(pchBuffer, iBufferSize, "%I64d:%02I64d:%02I64d.%03I64d",
	       i64HH, i64MM, i64SS, i64msec);
      WinSetWindowText(WinWindowFromID(hwnd, ST_CHLENGTHINTIME),
		       pchBuffer);

      // msec
      snprintf(pchBuffer, iBufferSize, "%I64d msec",
	       (pChannel->Length*1000/pChannelSet->OriginalFormat.iFrequency));
      WinSetWindowText(WinWindowFromID(hwnd, ST_CHLENGTHINMSEC),
		       pchBuffer);

      // Channel position!
      iPos = 0;
      if (DosRequestMutexSem(pChannelSet->hmtxUseChannelList, SEM_INDEFINITE_WAIT)==NO_ERROR)
      {
        iPos = 1;
	pTempChannel = pChannelSet->pChannelListHead;
	while ((pTempChannel) && (pTempChannel!=pChannel))
	{
	  iPos++;
          pTempChannel = pTempChannel->pNext;
	}
        if (!pTempChannel) iPos = 0;
	DosReleaseMutexSem(pChannelSet->hmtxUseChannelList);
      }
      // iPos==0 means that couldn't find.
      if (iPos==0)
	snprintf(pchBuffer, iBufferSize, "Unknown");
      else
	snprintf(pchBuffer, iBufferSize, "%d.",
		 iPos);

      WinSetWindowText(WinWindowFromID(hwnd, ST_CHANNELPOSITION),
		       pchBuffer);
    } else
    {
      // No current channel selected, so put something in there.
      sprintf(pchBuffer, "");
      WinSetWindowText(WinWindowFromID(hwnd, EF_CHANNELNAME),
		       pchBuffer);
      WinSendDlgItemMsg(hwnd, EF_CHANNELNAME, EM_QUERYCHANGED, (MPARAM) 0, (MPARAM) 0); // Clear "Changed" flag!
      WinSetWindowText(WinWindowFromID(hwnd, ST_CHLENGTHINSAMPLES),
		       pchBuffer);
      WinSetWindowText(WinWindowFromID(hwnd, ST_CHLENGTHINTIME),
		       pchBuffer);
      WinSetWindowText(WinWindowFromID(hwnd, ST_CHLENGTHINMSEC),
		       pchBuffer);
      WinSetWindowText(WinWindowFromID(hwnd, ST_CHANNELPOSITION),
		       pchBuffer);
    }
    // done
    free(pchBuffer);
  }
}

static void UpdateChannelsNotebookControlValues(HWND hwnd, PropertiesWindowList_p pWindowDesc)
{
  long lID, lOldSelectedID;
  WaWEChannel_p pChannel, pOldChannel;

  // Get current selection
  lOldSelectedID = (long) WinSendDlgItemMsg(hwnd, CX_LISTOFCHANNELS, LM_QUERYSELECTION,
                                            (MPARAM) LIT_CURSOR, (MPARAM) 0);
  if (lOldSelectedID!=LIT_NONE)
    pOldChannel = (WaWEChannel_p) WinSendDlgItemMsg(hwnd, CX_LISTOFCHANNELS, LM_QUERYITEMHANDLE,
                                                    (MPARAM) lOldSelectedID, (MPARAM) 0);
  else
    pOldChannel = NULL;

  WinSendDlgItemMsg(hwnd, CX_LISTOFCHANNELS, LM_DELETEALL, (MPARAM) 0, (MPARAM) 0);

  // Fill the Channels listbox with channel names,
  if (DosRequestMutexSem(pWindowDesc->pChannelSet->hmtxUseChannelList, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    pChannel = pWindowDesc->pChannelSet->pChannelListHead;
    while (pChannel)
    {
      lID = (long) WinSendDlgItemMsg(hwnd, CX_LISTOFCHANNELS, LM_INSERTITEM,
				     (MPARAM) LIT_END, (MPARAM) pChannel->achName);
      WinSendDlgItemMsg(hwnd, CX_LISTOFCHANNELS, LM_SETITEMHANDLE,
			(MPARAM) lID, (MPARAM) pChannel);

      // If there was no old selection, we'll select the first one.
      if ((lOldSelectedID==LIT_NONE) || (pChannel==NULL))
        lOldSelectedID = lID;
      else
      {
        if (pChannel == pOldChannel)
          lOldSelectedID = lID;
      }

      pChannel = pChannel->pNext;
    }
    DosReleaseMutexSem(pWindowDesc->pChannelSet->hmtxUseChannelList);
  }
#ifdef DEBUG_BUILD
  else
  {
    // Yikes, could not get semaphore!
    printf("[UpdateChannelsNotebookControlValues] : Warning! Could not get semaphore!\n"); fflush(stdout);
  }
#endif

  // Select old, or first!
  WinSendDlgItemMsg(hwnd, CX_LISTOFCHANNELS, LM_SELECTITEM,
		    (MPARAM) lOldSelectedID, (MPARAM) TRUE);

  // then fill the channel properties groupbox controls with values,
  // based on the selected channel!
  UpdateChannelsNotebookChPropsControlValues(hwnd, pWindowDesc);
}

static void MoveChannelUp(HWND hwnd, PropertiesWindowList_p pWindowDesc)
{
  WaWEChannel_p pMoveAfterThis, pPrevChannel, pChannel;
  int lID;

  // Get the currently selected item in listbox
  lID = (long) WinSendDlgItemMsg(hwnd, CX_LISTOFCHANNELS, LM_QUERYSELECTION,
				 (MPARAM) LIT_CURSOR, (MPARAM) 0);
  if (lID==LIT_NONE)
  {
    // No item selected in listbox!
    pChannel = NULL;
  } else
  {
    pChannel = (WaWEChannel_p) WinSendDlgItemMsg(hwnd, CX_LISTOFCHANNELS, LM_QUERYITEMHANDLE,
						 (MPARAM) lID, (MPARAM) 0);
  }

  if (pChannel)
  {
    // We can move channel UP only if there is a previous one.
    if (pChannel->pPrev)
    {
      pPrevChannel = pChannel->pPrev;
      if (pPrevChannel->pPrev)
	pMoveAfterThis = pPrevChannel->pPrev;
      else
	pMoveAfterThis = WAWECHANNEL_TOP;

      PluginHelpers.fn_WaWEHelper_ChannelSet_MoveChannel(pChannel, pMoveAfterThis);
      PluginHelpers.fn_WaWEHelper_Global_RedrawChanges();
    }
  }

  // As we've moved the stuff, it will generate a callback, and
  // that will update this window anyway, so no need to call
  // the UpdateChannelsNotebookChPropsControlValues() function here.
}

static void MoveChannelDown(HWND hwnd, PropertiesWindowList_p pWindowDesc)
{
  WaWEChannel_p pMoveAfterThis, pChannel;
  int lID;

  // Get the currently selected item in listbox
  lID = (long) WinSendDlgItemMsg(hwnd, CX_LISTOFCHANNELS, LM_QUERYSELECTION,
				 (MPARAM) LIT_CURSOR, (MPARAM) 0);
  if (lID==LIT_NONE)
  {
    // No item selected in listbox!
    pChannel = NULL;
  } else
  {
    pChannel = (WaWEChannel_p) WinSendDlgItemMsg(hwnd, CX_LISTOFCHANNELS, LM_QUERYITEMHANDLE,
						 (MPARAM) lID, (MPARAM) 0);
  }

  if (pChannel)
  {
    // We can move channel DOWN only if there is a next one.
    if (pChannel->pNext)
    {
      pMoveAfterThis = pChannel->pNext;
      PluginHelpers.fn_WaWEHelper_ChannelSet_MoveChannel(pChannel, pMoveAfterThis);
      PluginHelpers.fn_WaWEHelper_Global_RedrawChanges();
    }
  }

  // As we've moved the stuff, it will generate a callback, and
  // that will update this window anyway, so no need to call
  // the UpdateChannelsNotebookChPropsControlValues() function here.
}

static void DoChannelChanged(HWND hwnd, PropertiesWindowList_p pWindowDesc)
{
  WaWEChannel_p pChannel;
  int lID;

  // Get the currently selected item in listbox
  lID = (long) WinSendDlgItemMsg(hwnd, CX_LISTOFCHANNELS, LM_QUERYSELECTION,
				 (MPARAM) LIT_CURSOR, (MPARAM) 0);
  if (lID==LIT_NONE)
  {
    // No item selected in listbox!
    pChannel = NULL;
  } else
  {
    pChannel = (WaWEChannel_p) WinSendDlgItemMsg(hwnd, CX_LISTOFCHANNELS, LM_QUERYITEMHANDLE,
						 (MPARAM) lID, (MPARAM) 0);
  }

  if (pChannel)
  {
    // Read the new channel-name into this channel's descriptor!
    WinQueryDlgItemText(hwnd, EF_CHANNELNAME, WAWE_CHANNEL_NAME_LEN, pChannel->achName);
    PluginHelpers.fn_WaWEHelper_Channel_Changed(pChannel);
    PluginHelpers.fn_WaWEHelper_Global_RedrawChanges();
  }
}

static void DoSetBitsPerSample(HWND hwnd, PropertiesWindowList_p pWindowDesc, int iNewBits)
{
  WaWEChannel_p pChannel;

  // Set new bits value in channel-set and in channels!
  if (DosRequestMutexSem(pWindowDesc->pChannelSet->hmtxUseChannelList, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    pChannel = pWindowDesc->pChannelSet->pChannelListHead;
    while (pChannel)
    {
      if (pChannel->VirtualFormat.iBits != iNewBits)
      {
	pChannel->VirtualFormat.iBits = iNewBits;
	PluginHelpers.fn_WaWEHelper_Channel_Changed(pChannel);
      }

      pChannel = pChannel->pNext;
    }
    DosReleaseMutexSem(pWindowDesc->pChannelSet->hmtxUseChannelList);

    // Also update value in channel-set!
    if (pWindowDesc->pChannelSet->OriginalFormat.iBits != iNewBits)
    {
      pWindowDesc->pChannelSet->OriginalFormat.iBits = iNewBits;
      PluginHelpers.fn_WaWEHelper_ChannelSet_Changed(pWindowDesc->pChannelSet);
    }

    // Redraw changes!
    PluginHelpers.fn_WaWEHelper_Global_RedrawChanges();
  }
#ifdef DEBUG_BUILD
  else
  {
    // Yikes, could not get semaphore!
    printf("[DoSetBitsPerSample] : Warning! Could not get semaphore!\n"); fflush(stdout);
  }
#endif
}

static void DoSetSignedUnsigned(HWND hwnd, PropertiesWindowList_p pWindowDesc, int iNewIsSigned)
{
  WaWEChannel_p pChannel;

  // Set new IsSigned value in channel-set and in channels!
  if (DosRequestMutexSem(pWindowDesc->pChannelSet->hmtxUseChannelList, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    pChannel = pWindowDesc->pChannelSet->pChannelListHead;
    while (pChannel)
    {

      if (pChannel->VirtualFormat.iIsSigned!=iNewIsSigned)
      {
	pChannel->VirtualFormat.iIsSigned = iNewIsSigned;
	PluginHelpers.fn_WaWEHelper_Channel_Changed(pChannel);
      }

      pChannel = pChannel->pNext;
    }
    DosReleaseMutexSem(pWindowDesc->pChannelSet->hmtxUseChannelList);

    // Also update value in channel-set!
    if (pWindowDesc->pChannelSet->OriginalFormat.iIsSigned != iNewIsSigned)
    {
      pWindowDesc->pChannelSet->OriginalFormat.iIsSigned = iNewIsSigned;
      PluginHelpers.fn_WaWEHelper_ChannelSet_Changed(pWindowDesc->pChannelSet);
    }

    // Redraw changes!
    PluginHelpers.fn_WaWEHelper_Global_RedrawChanges();
  }
#ifdef DEBUG_BUILD
  else
  {
    // Yikes, could not get semaphore!
    printf("[DoSetSignedUnsigned] : Warning! Could not get semaphore!\n"); fflush(stdout);
  }
#endif
}

static void DoChangeSamplerate(HWND hwnd, PropertiesWindowList_p pWindowDesc, int iNewSamplerate)
{
  WaWEChannel_p pChannel;

  // Set new iFrequency value in channel-set and in channels!
  if (DosRequestMutexSem(pWindowDesc->pChannelSet->hmtxUseChannelList, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    pChannel = pWindowDesc->pChannelSet->pChannelListHead;
    while (pChannel)
    {

      if (pChannel->VirtualFormat.iFrequency!=iNewSamplerate)
      {
	pChannel->VirtualFormat.iFrequency = iNewSamplerate;
	PluginHelpers.fn_WaWEHelper_Channel_Changed(pChannel);
      }

      pChannel = pChannel->pNext;
    }
    DosReleaseMutexSem(pWindowDesc->pChannelSet->hmtxUseChannelList);

    // Also update value in channel-set!
    if (pWindowDesc->pChannelSet->OriginalFormat.iFrequency != iNewSamplerate)
    {
      pWindowDesc->pChannelSet->OriginalFormat.iFrequency = iNewSamplerate;
      PluginHelpers.fn_WaWEHelper_ChannelSet_Changed(pWindowDesc->pChannelSet);
    }

    // Redraw changes!
    PluginHelpers.fn_WaWEHelper_Global_RedrawChanges();
  }
#ifdef DEBUG_BUILD
  else
  {
    // Yikes, could not get semaphore!
    printf("[DoChangeSamplerate] : Warning! Could not get semaphore!\n"); fflush(stdout);
  }
#endif
}

MRESULT EXPENTRY AddNewFormatStringDlgProc(HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2)
{
  switch (msg) {
    case WM_INITDLG:
      AdjustNewFormatStringDlgControls(hwnd);
      break;
    case WM_FORMATFRAME:
      AdjustNewFormatStringDlgControls(hwnd);
      break;
    default:
      break;
  } // end of switch of msg

  // Call the default procedure if it has not been processed yet!
  return WinDefDlgProc(hwnd, msg, mp1, mp2);  
}

MRESULT EXPENTRY ProgressIndicatorDlgProc(HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2)
{
  switch (msg) {
    case WM_INITDLG:
#ifdef DEBUG_BUILD
//      printf("[ProgressIndicatorDlgProc] : WM_INITDLG\n"); fflush(stdout);
#endif
      break;

    default:
#ifdef DEBUG_BUILD
//      printf("[ProgressIndicatorDlgProc] : Msg: 0x%x\n", msg); fflush(stdout);
#endif

      break;
  } // end of switch of msg

  // Call the default procedure if it has not been processed yet!
  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}


#define MAX_WRITEBUFFER_SIZE   32768

void SamplerateConverterThreadFunc(void *pParm)
{
  PropertiesWindowList_p pWindowDesc = pParm;
  WaWEChannel_p pChannel;
  int iOldSamplerate, iNewSamplerate;
  int iNumOfChannels, iCurrChannelNum, iPercentagePerChannel;
  int iOldPercentage, iNewPercentage;
  WaWESample_p  pWriteBuffer;
  int iWriteBufferSamples;
  int iCachedToDelete;
  WaWEPosition_t WriteBufferStartPos;

#ifdef DEBUG_BUILD
  printf("[SamplerateConverterThreadFunc] : Enter!\n"); fflush(stdout);
#endif

  pWriteBuffer = (WaWESample_p) malloc(MAX_WRITEBUFFER_SIZE * sizeof(WaWESample_t));
  if (!pWriteBuffer)
  {
#ifdef DEBUG_BUILD
    printf("[SamplerateConverterThreadFunc] : Not enough memory for write buffer,\n");
    printf("[SamplerateConverterThreadFunc] : conversion will be unbuffered!\n");
#endif
  }
  iWriteBufferSamples = 0;
  WriteBufferStartPos = 0;

  // Set new iFrequency value in channel-set and in channels!
  if (DosRequestMutexSem(pWindowDesc->pChannelSet->hmtxUseChannelList, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {

    // First of all, set new samplerate in every channel and channelset, then do
    // the real change!

    iNewSamplerate = pWindowDesc->iSamplerateToConvertTo;
    iOldSamplerate = pWindowDesc->pChannelSet->OriginalFormat.iFrequency;
    if (iOldSamplerate==0) iOldSamplerate = 1; // To prevent division by zero!

    pChannel = pWindowDesc->pChannelSet->pChannelListHead;
    while (pChannel)
    {

      if (pChannel->VirtualFormat.iFrequency!=iNewSamplerate)
      {
	pChannel->VirtualFormat.iFrequency = iNewSamplerate;
	PluginHelpers.fn_WaWEHelper_Channel_Changed(pChannel);
      }

      pChannel = pChannel->pNext;
    }
    DosReleaseMutexSem(pWindowDesc->pChannelSet->hmtxUseChannelList);

    // Also update value in channel-set!
    if (pWindowDesc->pChannelSet->OriginalFormat.iFrequency != iNewSamplerate)
    {
      pWindowDesc->pChannelSet->OriginalFormat.iFrequency = iNewSamplerate;
      PluginHelpers.fn_WaWEHelper_ChannelSet_Changed(pWindowDesc->pChannelSet);
    }

    // Ok, go through the channels again!
    //DosRequestMutexSem(pWindowDesc->pChannelSet->hmtxUseChannelList, SEM_INDEFINITE_WAIT);

    // Setting this will result in *not* updating our progress window (length and so)
    pWindowDesc->bConvertInProgress = 1;

    // Let the pChannel point to the first channel in list!
    pChannel = pWindowDesc->pChannelSet->pChannelListHead;

    iNumOfChannels = pWindowDesc->pChannelSet->iNumOfChannels;
    iCurrChannelNum = 0;
    iPercentagePerChannel = 100/iNumOfChannels;
    iOldPercentage = 0;
    // Now start the conversion.
    while ((pChannel) && (!(pWindowDesc->bSamplerateConverterCancelRequest)))
    {
      // Convert a channel
      WaWESize_t Counter;
      WaWESize_t InsertedSamples;
      WaWEPosition_t SamplePos, OrgChnSamplePos;
      WaWESize_t CouldWrite, CouldRead, CouldDelete;
      WaWESample_t NewSample;
      WaWESample_t Samples[2];

      SamplePos = 0;
      OrgChnSamplePos = 0;

      if (iNewSamplerate < iOldSamplerate)
      {
	// Changing samplerate to a lower value, so we'll have to delete samples!
	// What we do is that we write the new samples one by one, then
	// delete the remaining samples.
#ifdef DEBUG_BUILD
	printf("[SamplerateConverterThreadFunc] : Convert DOWN %I64d samples from %I64d samples\n",
	       (pChannel->Length * iNewSamplerate)/iOldSamplerate,
	       pChannel->Length);
	fflush(stdout);
#endif
	Counter = 0;
	while ((OrgChnSamplePos<pChannel->Length) &&
	       (!(pWindowDesc->bSamplerateConverterCancelRequest)))
	{
	  if (Counter==0)
	  {
	    // Coooool, the new sample has to be made from exactly one sample!
	    NewSample = 0;
#ifdef DEBUG_BUILD
//	    printf("@R1\n"); fflush(stdout);
#endif

	    PluginHelpers.fn_WaWEHelper_Channel_ReadSample(pChannel,
							   OrgChnSamplePos,
							   1,
							   &NewSample,
                                                           &CouldRead);
	  } else
	  {
	    // The new sample has to be made from two weighted samples.
#ifdef DEBUG_BUILD
//	    printf("@R2\n"); fflush(stdout);
#endif

            Samples[0] = Samples[1] = 0;
	    PluginHelpers.fn_WaWEHelper_Channel_ReadSample(pChannel,
							   OrgChnSamplePos,
							   2,
							   &(Samples[0]),
							   &CouldRead);
	    if (CouldRead==1)
	      Samples[1] = Samples[0];

            // Make a weighted new sample!
	    NewSample = Samples[0] +
	      (Samples[1] - Samples[0]) * Counter / iNewSamplerate;
	  }
#ifdef DEBUG_BUILD
//	    printf("@W\n"); fflush(stdout);
#endif
          if (pWriteBuffer)
          {
            // Use the write cache!
            pWriteBuffer[iWriteBufferSamples] = NewSample;
            iWriteBufferSamples++;
            if (iWriteBufferSamples>=MAX_WRITEBUFFER_SIZE)
            {
#ifdef DEBUG_BUILD
//              printf("Cached writesample!\n"); fflush(stdout);
#endif

              // Writebuffer is full, so write out the contents!
              PluginHelpers.fn_WaWEHelper_Channel_WriteSample(pChannel,
                                                              WriteBufferStartPos,
                                                              iWriteBufferSamples,
                                                              pWriteBuffer,
                                                              &CouldWrite);
              WriteBufferStartPos += iWriteBufferSamples;
              iWriteBufferSamples = 0;
            }
          } else
          {
            // No cache... sigh.
            PluginHelpers.fn_WaWEHelper_Channel_WriteSample(pChannel,
                                                            SamplePos,
                                                            1,
                                                            &NewSample,
                                                            &CouldWrite);
          }
	  SamplePos++;

	  Counter+=iOldSamplerate;
	  while (Counter>=iNewSamplerate)
	  {
	    Counter-=iNewSamplerate;
            OrgChnSamplePos++;
	  }

	  // Calculate progress from iCurrChannelNum and SamplePos!
	  if (iNumOfChannels == 0)
	    iNewPercentage = 0;
	  else
	    iNewPercentage = 100 * iCurrChannelNum / iNumOfChannels;
	  if (pChannel->Length!=0)
	    iNewPercentage += iPercentagePerChannel * OrgChnSamplePos / pChannel->Length;
	  if (iNewPercentage!=iOldPercentage)
	  {
#ifdef DEBUG_BUILD
	    if (iNewPercentage%10==0)
	    {
	      printf("[SamplerateConverterThreadFunc] : Progress: %d%%\n", iNewPercentage); fflush(stdout);
	    }
#endif
	    iOldPercentage = iNewPercentage;
	    WinPostMsg(WinWindowFromID(pWindowDesc->hwndConvertSamplerateProgressWindow, DID_PROGRESSINDICATOR),
		       WM_SETPERCENTAGE,
		       (MPARAM) iNewPercentage, (MPARAM) NULL);
	  }
        }

        // Conversion is done, now we have to flush the cache if we have one,
        // and delete the end of channel-set, which is the remaining junk.

        if ((pWriteBuffer) && (iWriteBufferSamples>0))
        {
#ifdef DEBUG_BUILD
//          printf("Flushing writesample cache!\n"); fflush(stdout);
#endif

          // Flush out the writebuffer, and set it to standard position again
          // so it will be prepared for the next channel!
          if (iWriteBufferSamples)
            PluginHelpers.fn_WaWEHelper_Channel_WriteSample(pChannel,
                                                            WriteBufferStartPos,
                                                            iWriteBufferSamples,
                                                            pWriteBuffer,
                                                            &CouldWrite);
          WriteBufferStartPos = 0;
          iWriteBufferSamples = 0;
        }

#ifdef DEBUG_BUILD
	printf("[SamplerateConverterThreadFunc] : Wrote %I64d samples\n",
	       SamplePos);
	fflush(stdout);
#endif

	if (!(pWindowDesc->bSamplerateConverterCancelRequest))
	{
	  // We've finished the conversion, so we can delete the remaining samples from channel!
	  if (pChannel->Length-SamplePos>0)
	  {
#ifdef DEBUG_BUILD
	    printf("[SamplerateConverterThreadFunc] : Deleting %I64d samples from end of chn.\n", pChannel->Length-SamplePos);
	    fflush(stdout);
#endif

	    PluginHelpers.fn_WaWEHelper_Channel_DeleteSample(pChannel,
							     SamplePos,
							     pChannel->Length-SamplePos,
							     &CouldDelete);
	  }
	}
      } else
      if (iNewSamplerate > iOldSamplerate)
      {
        WaWESize_t NewLength;
        // Changing samplerate to a higher value, so we'll have to insert samples!
        NewLength = (pChannel->Length * iNewSamplerate)/iOldSamplerate;
#ifdef DEBUG_BUILD
	printf("[SamplerateConverterThreadFunc] : Convert UP %I64d samples from %I64d samples\n",
	       NewLength,
	       pChannel->Length);
#endif
	Counter = 0;
        InsertedSamples = 0;
        iCachedToDelete = 0;
	// We'll have to have two samples in here, all the time. Fill this buffer then!
	Samples[0] = Samples[1] = 0;
        PluginHelpers.fn_WaWEHelper_Channel_ReadSample(pChannel,
						       OrgChnSamplePos,
						       2,
						       &(Samples[0]),
						       &CouldRead);
	if (CouldRead==1)
	  Samples[1] = Samples[0];

        PluginHelpers.fn_WaWEHelper_Channel_DeleteSample(pChannel,
							 OrgChnSamplePos,
							 1,
							 &CouldRead);
        InsertedSamples--;

#ifdef DEBUG_BUILD
//	    printf("Samples=[%d, %d]\n", Samples[0], Samples[1]);
//	    fflush(stdout);
#endif


        // Now go with the main loop!
        while ((OrgChnSamplePos+InsertedSamples<pChannel->Length) &&
	       (!(pWindowDesc->bSamplerateConverterCancelRequest)))
	{
	  // We have to interpolate a new sample from the two we have!
	  NewSample = Samples[0] +
	    (Samples[1] - Samples[0]) * Counter / iNewSamplerate;


#ifdef DEBUG_BUILD
//	  printf("@I%I64d ", SamplePos);
//	  fflush(stdout);
#endif

          if (pWriteBuffer)
          {
            // Use the write cache for buffering the samples to insert!
            pWriteBuffer[iWriteBufferSamples] = NewSample;
            iWriteBufferSamples++;
            if (iWriteBufferSamples>=MAX_WRITEBUFFER_SIZE)
            {
#ifdef DEBUG_BUILD
//              printf("Cached insertsample!\n"); fflush(stdout);
#endif

              // Writebuffer is full, so write out the contents!
              PluginHelpers.fn_WaWEHelper_Channel_InsertSample(pChannel,
                                                               WriteBufferStartPos,
                                                               iWriteBufferSamples,
                                                               pWriteBuffer,
                                                               &CouldWrite);
              WriteBufferStartPos += iWriteBufferSamples;
              InsertedSamples += iWriteBufferSamples;
              iWriteBufferSamples = 0;
            }
            SamplePos++;
          } else
          {
            // No write cache, so do the conversion uncached!
            PluginHelpers.fn_WaWEHelper_Channel_InsertSample(pChannel,
                                                             SamplePos,
                                                             1,
                                                             &NewSample,
                                                             &CouldWrite);

            SamplePos+=CouldWrite;
            InsertedSamples+=CouldWrite; // Original position has to be changed too.
          }

	  Counter+=iOldSamplerate;
	  while (Counter>=iNewSamplerate)
	  {
	    Counter-=iNewSamplerate;


#ifdef DEBUG_BUILD
//	    printf("@D%I64d ", OrgChnSamplePos + InsertedSamples + 1); fflush(stdout);
#endif

            if (pWriteBuffer)
            {
              // There is write buffer, so use delete buffer too!
              iCachedToDelete++;
              if (iCachedToDelete>=MAX_WRITEBUFFER_SIZE)
              {
#ifdef DEBUG_BUILD
//                printf("Mass delete\n"); fflush(stdout);
#endif
                PluginHelpers.fn_WaWEHelper_Channel_DeleteSample(pChannel,
                                                                 OrgChnSamplePos - iCachedToDelete + InsertedSamples + 1,
                                                                 iCachedToDelete,
                                                                 &CouldRead);

                InsertedSamples-=CouldRead;
                iCachedToDelete = 0;
              }
            } else
            {
              // There is no write buffer, so do plain deleting!
              PluginHelpers.fn_WaWEHelper_Channel_DeleteSample(pChannel,
                                                               OrgChnSamplePos + InsertedSamples + 1,
                                                               1,
                                                               &CouldRead);

              InsertedSamples-=CouldRead;
            }
	    OrgChnSamplePos++;
	    Samples[0] = Samples[1]; Samples[1] = 0;
#ifdef DEBUG_BUILD
//	    printf("@R%I64d ", OrgChnSamplePos+InsertedSamples+1); fflush(stdout);
#endif

	    PluginHelpers.fn_WaWEHelper_Channel_ReadSample(pChannel,
							   OrgChnSamplePos + InsertedSamples + 1,
							   1,
							   &(Samples[1]),
							   &CouldRead);
#ifdef DEBUG_BUILD
//	    printf(": Samples=[%d, %d] (ReadPos = %I64d, CouldRead = %I64d)\n",
//		   Samples[0], Samples[1],
//		   OrgChnSamplePos + InsertedSamples + 1,
//                   CouldRead);
//	    fflush(stdout);
#endif

	  }

	  // Calculate progress from iCurrChannelNum and SamplePos!
	  if (iNumOfChannels == 0)
	    iNewPercentage = 0;
	  else
	    iNewPercentage = 100 * iCurrChannelNum / iNumOfChannels;
	  if (pChannel->Length!=0)
	    iNewPercentage += iPercentagePerChannel * (SamplePos) / NewLength;
	  if (iNewPercentage!=iOldPercentage)
	  {
#ifdef DEBUG_BUILD
	    if (iNewPercentage%10==0)
	    {
	      printf("[SamplerateConverterThreadFunc] : Progress: %d%%\n", iNewPercentage); fflush(stdout);
	    }
#endif
	    iOldPercentage = iNewPercentage;
	    WinPostMsg(WinWindowFromID(pWindowDesc->hwndConvertSamplerateProgressWindow, DID_PROGRESSINDICATOR),
		       WM_SETPERCENTAGE,
		       (MPARAM) iNewPercentage, (MPARAM) NULL);
	  }
        }

        // End of conversion, flush the cache!
        if (pWriteBuffer)
        {
          // Flush the cache
#ifdef DEBUG_BUILD
//          printf("Flushing cached insertsample!\n"); fflush(stdout);
#endif

          // The insert buffer
          if (iWriteBufferSamples)
            PluginHelpers.fn_WaWEHelper_Channel_InsertSample(pChannel,
                                                             WriteBufferStartPos,
                                                             iWriteBufferSamples,
                                                             pWriteBuffer,
                                                             &CouldWrite);
          WriteBufferStartPos = 0;
          InsertedSamples += iWriteBufferSamples;
          iWriteBufferSamples = 0;

          // The delete buffer
          if (iCachedToDelete>0)
          {
            PluginHelpers.fn_WaWEHelper_Channel_DeleteSample(pChannel,
                                                             OrgChnSamplePos - iCachedToDelete + InsertedSamples + 1,
                                                             iCachedToDelete,
                                                             &CouldRead);

            InsertedSamples-=CouldRead;
            iCachedToDelete = 0;
          }

        }

	if (!(pWindowDesc->bSamplerateConverterCancelRequest))
	{
          // Delete last original sample!
	  PluginHelpers.fn_WaWEHelper_Channel_DeleteSample(pChannel,
							   OrgChnSamplePos + InsertedSamples + 1,
							   1,
							   &CouldRead);
	}
#ifdef DEBUG_BUILD
	printf("[SamplerateConverterThreadFunc] : Wrote %I64d samples\n",
	       SamplePos);
	fflush(stdout);
#endif
      }

      // Go for next channel!
      pChannel = pChannel->pNext;
      iCurrChannelNum++;
    }
    //DosReleaseMutexSem(pWindowDesc->pChannelSet->hmtxUseChannelList);

    if (pWindowDesc->bSamplerateConverterCancelRequest)
    {
#ifdef DEBUG_BUILD
      printf("[SamplerateConverterThreadFunc] : Cancel request got, conversion cancelled.\n"); fflush(stdout);
#endif
      pWindowDesc->bSamplerateConversionCancelled=1;
    }
    // End of conversion
    pWindowDesc->bConvertInProgress = 0;
  }
#ifdef DEBUG_BUILD
  else
  {
    // Yikes, could not get semaphore!
    printf("[SamplerateConverterThreadFunc] : Warning! Could not get semaphore!\n"); fflush(stdout);
  }
#endif

  if (pWriteBuffer)
  {
    // Free the writebuffer! (cache)
    free(pWriteBuffer); pWriteBuffer = NULL;
  }

#ifdef DEBUG_BUILD
  printf("[SamplerateConverterThreadFunc] : Done!\n"); fflush(stdout);
#endif

  // Make sure the other thread will get out of WinProcessDlg()!
  WinPostMsg(pWindowDesc->hwndConvertSamplerateProgressWindow, WM_CLOSE, (MPARAM) 0, (MPARAM) 0);

  _endthread();
}


static void DoConvertSamplerate(HWND hwnd, PropertiesWindowList_p pWindowDesc, int iNewSamplerate)
{
  HWND hwndProgressDlg;
  HAB hab;

#ifdef DEBUG_BUILD
  printf("[DoConvertSamplerate] : Converting samplerate from %d Hz to %d Hz\n",
	 pWindowDesc->pChannelSet->OriginalFormat.iFrequency,
	 iNewSamplerate);
  fflush(stdout);
#endif

  hab = WinQueryAnchorBlock(hwnd);

  // Register private class
  WinRegisterClass(hab, pchProgressIndicatorClassName, // Register a private class for
		   ProgressIndicatorClassProc,         // progress indicator
		   0, 16); // 16 bytes of private storage should be enough...

  // Note that we're using this channel-set!
  pWindowDesc->pChannelSet->lUsageCounter++;

  // Now create the progress window, and start the conversion.
  hwndProgressDlg = WinLoadDlg(HWND_DESKTOP,
			       hwnd, // Owner
			       (PFNWP)ProgressIndicatorDlgProc,
			       pWindowDesc->hmodOurDLLHandle,
			       DLG_CONVERTINGSAMPLERATE,
			       NULL);

  if (!hwndProgressDlg)
  {
    ErrorMsg("Could not create progress window, convertation aborted!");
  } else
  {
    SWP swpParent, swpDlg, swpTemp;
    POINTL ptlTemp;
    ULONG CXDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
    ULONG CYDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);

    // Try to restore window position and size from ini file!
    // We'll use only the size from it!
    WinRestoreWindowPos(WinStateSave_AppName, WinStateSave_ConvertSamplerateProgressWindowKey, hwndProgressDlg);

    // Create custom control in window!
    WinQueryWindowPos(WinWindowFromID(hwndProgressDlg, PB_CANCELSAMPLERATECONVERSION), &swpTemp);
    WinQueryWindowPos(hwndProgressDlg, &swpDlg);

    WinCreateWindow(hwndProgressDlg,                    // hwndParent
		    pchProgressIndicatorClassName,   // pszClass
		    "Progress Indicator", // pszName
		    WS_VISIBLE,                 // flStyle
		    CXDLGFRAME*4,                              // x
		    swpTemp.y + swpTemp.cy + CYDLGFRAME*2, // y
		    swpDlg.cx - CXDLGFRAME*8, // cx
                    swpTemp.cy*2, // cy
		    hwndProgressDlg,                    // hwndOwner
		    HWND_TOP,                   // hwndInsertBehind
		    DID_PROGRESSINDICATOR,       // id
		    NULL,                       // pCtlData
		    NULL);                      // pPresParams

    // Center dialog in parent
    WinQueryWindowPos(hwndProgressDlg, &swpDlg);
    WinQueryWindowPos(hwnd, &swpParent);
    ptlTemp.x = swpParent.x;
    ptlTemp.y = swpParent.y;
    WinMapWindowPoints(hwnd, HWND_DESKTOP, &ptlTemp, 1);
    {
      // Center dialog box within the parent window
      int ix, iy;
      ix = ptlTemp.x + (swpParent.cx - swpDlg.cx)/2;
      iy = ptlTemp.y + (swpParent.cy - swpDlg.cy)/2;
      WinSetWindowPos(hwndProgressDlg, HWND_TOP, ix, iy, 0, 0,
		      SWP_MOVE);
    }

    WinSetWindowPos(hwndProgressDlg, HWND_TOP, 0, 0, 0, 0,
		    SWP_SHOW | SWP_ACTIVATE | SWP_ZORDER);
    WinSetFocus(HWND_DESKTOP, hwndProgressDlg);

    pWindowDesc->hwndConvertSamplerateProgressWindow = hwndProgressDlg;
    pWindowDesc->iSamplerateToConvertTo = iNewSamplerate;
    pWindowDesc->bSamplerateConverterCancelRequest = 0;
    pWindowDesc->bSamplerateConversionCancelled = 0;
    pWindowDesc->tidSamplerateConvertTID =
      _beginthread(SamplerateConverterThreadFunc, 0,
		   1024*1024, pWindowDesc);

    if (pWindowDesc->tidSamplerateConvertTID<=0)
    {
      ErrorMsg("Could not create samplerate converter thread!\n"
	       "Operation aborted.");
    } else
    {
      DosSetPriority(PRTYS_THREAD, PRTYC_REGULAR, 0, pWindowDesc->tidSamplerateConvertTID);
      // Process the dialog!
#ifdef DEBUG_BUILD
      printf("Calling WinProcessDlg() for hwndProgressDlg...\n"); fflush(stdout);
#endif
      WinProcessDlg(hwndProgressDlg);
#ifdef DEBUG_BUILD
      printf("Returned from WinProcessDlg() for hwndProgressDlg...\n"); fflush(stdout);
#endif

      // If the converter thread would still run, make sure it wil stop!
      pWindowDesc->bSamplerateConverterCancelRequest = 1;
      DosWaitThread(&(pWindowDesc->tidSamplerateConvertTID), DCWW_WAIT);
    }

    // Save window position and size (we'll only use the size) into INI file!
    WinStoreWindowPos(WinStateSave_AppName, WinStateSave_ConvertSamplerateProgressWindowKey, hwndProgressDlg);

    // Destroy the dialog window and its resources
    WinDestroyWindow(hwndProgressDlg);

    if (pWindowDesc->bSamplerateConversionCancelled)
      WarningMsg("The samplerate conversion has been cancelled. The channel-set is not fully converted!");
  }

  // Note that we've finished using this channel-set!
  pWindowDesc->pChannelSet->lUsageCounter--;

  // Redraw everything that we've changed
  PluginHelpers.fn_WaWEHelper_Global_RedrawChanges();
}


MRESULT EXPENTRY ChangeSamplerateDlgProc(HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2)
{
  char achTemp[32];

  switch (msg) {
    case WM_INITDLG:
      AdjustChangeSamplerateDlgControls(hwnd);
      break;

    case WM_FORMATFRAME:
      AdjustChangeSamplerateDlgControls(hwnd);
      break;

    case WM_CONTROL:
      switch (SHORT1FROMMP(mp1))
      {
	case CX_NEWSAMPLERATE:
          if (SHORT2FROMMP(mp1)==EN_KILLFOCUS)
          {
	    int iValue;

	    // Read the new value!
	    iValue = 0;
            achTemp[0] = 0;
	    WinQueryDlgItemText(hwnd, CX_NEWSAMPLERATE, sizeof(achTemp), achTemp);
            iValue = atoi(achTemp);
	    if ((iValue<=0) || (iValue>96000)) // Max 96KHz. Can be lifted, if required.
	    {
	      // Invalid value, set it to a default one!
	      WarningMsg("Invalid samplerate!\n"
			 "The samplerate has been set to the default one.");
	      WinSetDlgItemText(hwnd, CX_NEWSAMPLERATE, "44100");
	    }
	  }
	  break;
	default:
          break;
      }
      break;

    default:
      break;
  } // end of switch of msg

  // Call the default procedure if it has not been processed yet!
  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}

MRESULT EXPENTRY PropertiesDlgProc( HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2 )
{
  InitDlgAppData_p pInitDlgAppData;
  PropertiesWindowList_p pWindowDesc;

  switch (msg) {
    case WM_INITDLG:
      pInitDlgAppData = (InitDlgAppData_p) mp2;
      // Save pointer to pWindowDesc into user storage of dialog window!
      if (pInitDlgAppData)
	WinSetWindowULong(hwnd, QWL_USER, (ULONG) (pInitDlgAppData->pRealData));
      else
        WinSetWindowULong(hwnd, QWL_USER, (ULONG) NULL);
      break;
    case WM_FORMATFRAME:
#ifdef DEBUG_BUILD
//      printf("[PropertiesDlgProc] : WM_FORMATFRAME\n"); fflush(stdout);
#endif

      pWindowDesc = (PropertiesWindowList_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (pWindowDesc)
	AdjustPropertiesDlgControls(pWindowDesc);

      break;
    case WM_COMMAND:
      switch SHORT1FROMMP(mp2) {
	case CMDSRC_PUSHBUTTON:           // ---- A WM_COMMAND from a pushbutton ------
	  switch (SHORT1FROMMP(mp1)) {
            case PB_HELP:                             // Help button pressed
#ifdef DEBUG_BUILD
              printf("[SETUP] Help button pressed! (ToDo!)\n");
#endif
              // TODO!
	      return (MRESULT) TRUE;
	    default:
	      break;
	  }
	  break; // End of CMDSRC_PUSHBUTTON processing
      } // end of switch inside WM_COMMAND
      return (MRESULT) 0;
    default:
      break;
  } // end of switch of msg

  // Call the default procedure if it has not been processed yet!
  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}

MRESULT EXPENTRY fnGeneralNotebookPage( HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2 )
{
  InitDlgAppData_p pInitDlgAppData;
  PropertiesWindowList_p pWindowDesc;

  switch (msg) {
    case WM_INITDLG:
      pInitDlgAppData = (InitDlgAppData_p) mp2;
      // Save pointer to pWindowDesc into user storage of dialog window!
      if (pInitDlgAppData)
      {
	WinSetWindowULong(hwnd, QWL_USER, (ULONG) (pInitDlgAppData->pRealData));
	pWindowDesc = (PropertiesWindowList_p) (pInitDlgAppData->pRealData);
	// Now initialize the controls!
        // First set the limits
	WinSendDlgItemMsg(hwnd, EF_CHSETNAME, EM_SETTEXTLIMIT, (MPARAM) WAWE_CHANNELSET_NAME_LEN, (MPARAM) 0);
	WinSendDlgItemMsg(hwnd, EF_CHSETFILENAME, EM_SETTEXTLIMIT, (MPARAM) WAWE_CHANNELSET_FILENAME_LEN, (MPARAM) 0);
	// Then set the initial control values!
        UpdateGeneralNotebookControlValues(hwnd, pWindowDesc);
      }
      else
	WinSetWindowULong(hwnd, QWL_USER, (ULONG) NULL);
      break;

    case WM_FORMATFRAME:
#ifdef DEBUG_BUILD
//      printf("[fnGeneralNotebookPage] : WM_FORMATFRAME\n"); fflush(stdout);
#endif
      pWindowDesc = (PropertiesWindowList_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (pWindowDesc)
	AdjustGeneralNotebookControls(pWindowDesc);

      break;

    case WM_UPDATECONTROLVALUES:
      pWindowDesc = (PropertiesWindowList_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (pWindowDesc)
	UpdateGeneralNotebookControlValues(hwnd, pWindowDesc);
      break;

    case WM_COMMAND:
      return (MRESULT) 0;

    case WM_CONTROL:
      switch (SHORT1FROMMP(mp1))
      {
        case EF_CHSETNAME:
          if (SHORT2FROMMP(mp1)==EN_KILLFOCUS)
          {
            // The content of the entry field might has been changed!
            pWindowDesc = (PropertiesWindowList_p) WinQueryWindowULong(hwnd, QWL_USER);
            if ((pWindowDesc) && (pWindowDesc->iWindowCreated==1))
            {
              // If changed, then propagate the changes!
	      if (WinSendDlgItemMsg(hwnd, (SHORT1FROMMP(mp1)), EM_QUERYCHANGED, (MPARAM) 0, (MPARAM) 0))
              {
                // Read the new channelset-name into this channel-set's descriptor!
                WinQueryDlgItemText(hwnd, EF_CHSETNAME, WAWE_CHANNELSET_NAME_LEN, pWindowDesc->pChannelSet->achName);
                PluginHelpers.fn_WaWEHelper_ChannelSet_Changed(pWindowDesc->pChannelSet);
                PluginHelpers.fn_WaWEHelper_Global_RedrawChanges();
              }
            }
          }
          break;
        case EF_CHSETFILENAME:
          if (SHORT2FROMMP(mp1)==EN_KILLFOCUS)
          {
            // The content of the entry field might has been changed!
            pWindowDesc = (PropertiesWindowList_p) WinQueryWindowULong(hwnd, QWL_USER);
            if ((pWindowDesc) && (pWindowDesc->iWindowCreated==1))
            {
              // If changed, then propagate the changes!
              if (WinSendDlgItemMsg(hwnd, (SHORT1FROMMP(mp1)), EM_QUERYCHANGED, (MPARAM) 0, (MPARAM) 0))
              {
                // Read the new channelset filename into this channel-set's descriptor!
                WinQueryDlgItemText(hwnd, EF_CHSETFILENAME, WAWE_CHANNELSET_FILENAME_LEN, pWindowDesc->pChannelSet->achFileName);
                PluginHelpers.fn_WaWEHelper_ChannelSet_Changed(pWindowDesc->pChannelSet);
                PluginHelpers.fn_WaWEHelper_Global_RedrawChanges();
              }
            }
          }
          break;
	default:
          break;
      }
      return (MRESULT) 0;

    default:
      break;
  }
  // Call the default procedure if it has not been processed yet!
  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}

MRESULT EXPENTRY fnFormatCommonNotebookPage( HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2 )
{
  InitDlgAppData_p pInitDlgAppData;
  PropertiesWindowList_p pWindowDesc;
  long lID;

  switch (msg) {
    case WM_INITDLG:
      pInitDlgAppData = (InitDlgAppData_p) mp2;
      // Save pointer to pWindowDesc into user storage of dialog window!
      if (pInitDlgAppData)
      {
	WinSetWindowULong(hwnd, QWL_USER, (ULONG) (pInitDlgAppData->pRealData));
	pWindowDesc = (PropertiesWindowList_p) (pInitDlgAppData->pRealData);
	// Now initialize the controls!
        // First set the limits
	WinSendDlgItemMsg(hwnd, EF_BITSPERSAMPLE, EM_SETTEXTLIMIT, (MPARAM) 4, (MPARAM) 0);
	// Then fill the Signed samples combobox with Yes (Signed) and No (Unsigned) values!
	WinSendDlgItemMsg(hwnd, CX_SIGNEDUNSIGNED, LM_DELETEALL, (MPARAM) 0, (MPARAM) 0);
	lID = (LONG) WinSendDlgItemMsg(hwnd, CX_SIGNEDUNSIGNED, LM_INSERTITEM,
				       (MPARAM) LIT_END,
				       (MPARAM) "Yes (Signed)");
	WinSendDlgItemMsg(hwnd, CX_SIGNEDUNSIGNED, LM_SETITEMHANDLE,
			  (MPARAM) lID,
			  (MPARAM) 1);
        lID = (LONG) WinSendDlgItemMsg(hwnd, CX_SIGNEDUNSIGNED, LM_INSERTITEM,
				       (MPARAM) LIT_END,
				       (MPARAM) "No (Unsigned)");
	WinSendDlgItemMsg(hwnd, CX_SIGNEDUNSIGNED, LM_SETITEMHANDLE,
			  (MPARAM) lID,
			  (MPARAM) 0);

	// Then set the initial control values!
	UpdateFormatCommonNotebookControlValues(hwnd, pWindowDesc);
      }
      else
	WinSetWindowULong(hwnd, QWL_USER, (ULONG) NULL);
      break;
    case WM_FORMATFRAME:
#ifdef DEBUG_BUILD
//      printf("[fnFormatCommonNotebookPage] : WM_FORMATFRAME\n"); fflush(stdout);
#endif
      pWindowDesc = (PropertiesWindowList_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (pWindowDesc)
	AdjustFormatCommonNotebookControls(pWindowDesc);

      break;

    case WM_CONTROL:
      switch (SHORT1FROMMP(mp1))
      {
	case CX_SIGNEDUNSIGNED:
	  if (SHORT2FROMMP(mp1)==LN_SELECT)
	  {
	    // The listbox selection has been changed.
	    pWindowDesc = (PropertiesWindowList_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pWindowDesc)
	    {
	      int iIsSigned;

	      // Get the currently selected item in listbox
	      lID = (long) WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), LM_QUERYSELECTION,
					     (MPARAM) LIT_CURSOR, (MPARAM) 0);
	      if (lID!=LIT_NONE)
	      {
		iIsSigned = (int) WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), LM_QUERYITEMHANDLE,
						    (MPARAM) lID, (MPARAM) 0);
                DoSetSignedUnsigned(hwnd, pWindowDesc, iIsSigned);
	      }
	    }
	  }
          break;

	case EF_BITSPERSAMPLE:
	  if (SHORT2FROMMP(mp1)==EN_SETFOCUS)
	  {
	    // Save original bits per sample value, so it can be restored later,
            // if the user enters something stupid.
            pWindowDesc = (PropertiesWindowList_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pWindowDesc)
	      WinQueryDlgItemText(hwnd, EF_BITSPERSAMPLE, 32, pWindowDesc->achOldBitsPerSample);
	  }
          if (SHORT2FROMMP(mp1)==EN_KILLFOCUS)
          {
            // The content of the entry field might has been changed!
            pWindowDesc = (PropertiesWindowList_p) WinQueryWindowULong(hwnd, QWL_USER);
            if ((pWindowDesc) && (pWindowDesc->iWindowCreated==1))
	    {
              // If changed, then propagate the changes!
	      if (WinSendDlgItemMsg(hwnd, (SHORT1FROMMP(mp1)), EM_QUERYCHANGED, (MPARAM) 0, (MPARAM) 0))
	      {
		SHORT sValue;

		// Read the new channelset-name into this channel-set's descriptor!
                sValue = 0;
		WinQueryDlgItemShort(hwnd, EF_BITSPERSAMPLE, &sValue, FALSE); // FALSE = UNSIGNED
		if ((sValue>=8) && (sValue<=32))
		{
		  // Ok, valid value!
		  DoSetBitsPerSample(hwnd, pWindowDesc, sValue);
		} else
		{
		  // Invalid value, restore old one!
		  WinSetDlgItemText(hwnd, EF_BITSPERSAMPLE, pWindowDesc->achOldBitsPerSample);
                  WinSendDlgItemMsg(hwnd, EF_BITSPERSAMPLE, EM_QUERYCHANGED, (MPARAM) 0, (MPARAM) 0); // Clear changed flag!
		}
              }
	    }
	  }
	  break;
	default:
          break;
      }
      return (MRESULT) 0;

    case WM_COMMAND:
      if (SHORT1FROMMP(mp2)==CMDSRC_PUSHBUTTON)
      {
	switch ((USHORT)mp1)
	{
	  case PB_SAMPLERATECHANGE:
            pWindowDesc = (PropertiesWindowList_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pWindowDesc)
	    {
              HWND hwndDlg;

	      hwndDlg = WinLoadDlg(HWND_DESKTOP,
				   hwnd, // Owner
				   (PFNWP)ChangeSamplerateDlgProc,
				   pWindowDesc->hmodOurDLLHandle,
				   DLG_CHANGESAMPLERATE,
				   NULL);

	      if (!hwndDlg)
	      {
		ErrorMsg("Could not create dialog window!");
	      } else
	      {
		SWP swpParent, swpDlg;
                POINTL ptlTemp;
		char achTemp[64];
		int iResult;
                int iNewSamplerate;

		// Set initial values of controls!
                iNewSamplerate = pWindowDesc->pChannelSet->OriginalFormat.iFrequency;
                sprintf(achTemp, "%d Hz", pWindowDesc->pChannelSet->OriginalFormat.iFrequency);
                WinSetDlgItemText(hwndDlg, ST_SHOWNORIGINALSAMPLERATE, achTemp);
                WinSendDlgItemMsg(hwndDlg, CX_NEWSAMPLERATE, LM_DELETEALL,
                                  (MPARAM) 0, (MPARAM) 0);
                WinSendDlgItemMsg(hwndDlg, CX_NEWSAMPLERATE, LM_INSERTITEM,
                                  (MPARAM) LIT_END, (MPARAM) "48000");
                WinSendDlgItemMsg(hwndDlg, CX_NEWSAMPLERATE, LM_INSERTITEM,
                                  (MPARAM) LIT_END, (MPARAM) "44100");
                WinSendDlgItemMsg(hwndDlg, CX_NEWSAMPLERATE, LM_INSERTITEM,
                                  (MPARAM) LIT_END, (MPARAM) "22050");
                WinSendDlgItemMsg(hwndDlg, CX_NEWSAMPLERATE, LM_INSERTITEM,
                                  (MPARAM) LIT_END, (MPARAM) "11025");
                sprintf(achTemp, "%d", pWindowDesc->pChannelSet->OriginalFormat.iFrequency);
                WinSetDlgItemText(hwndDlg, CX_NEWSAMPLERATE, achTemp);

		// Try to restore window position and size from ini file!
		// We'll use only the size from it!
		WinRestoreWindowPos(WinStateSave_AppName, WinStateSave_ChangeSamplerateWindowKey, hwndDlg);

		// Center dialog in parent
                WinQueryWindowPos(hwndDlg, &swpDlg);
		WinQueryWindowPos(hwnd, &swpParent);
		ptlTemp.x = swpParent.x;
		ptlTemp.y = swpParent.y;
		WinMapWindowPoints(hwnd, HWND_DESKTOP, &ptlTemp, 1);
		{
		  // Center dialog box within the parent window
		  int ix, iy;
		  ix = ptlTemp.x + (swpParent.cx - swpDlg.cx)/2;
		  iy = ptlTemp.y + (swpParent.cy - swpDlg.cy)/2;
		  WinSetWindowPos(hwndDlg, HWND_TOP, ix, iy, 0, 0,
				  SWP_MOVE);
		}

		WinSetWindowPos(hwndDlg, HWND_TOP, 0, 0, 0, 0,
				SWP_SHOW | SWP_ACTIVATE | SWP_ZORDER);
		WinSetFocus(HWND_DESKTOP, hwndDlg);

		// Process the dialog!
		iResult = WinProcessDlg(hwndDlg);

		// Get new samplerate value!
                achTemp[0] = 0;
		WinQueryDlgItemText(hwndDlg, CX_NEWSAMPLERATE, sizeof(achTemp), &achTemp);
		iNewSamplerate = atoi(achTemp);

                // Save window position and size (we'll only use the size) into INI file!
		WinStoreWindowPos(WinStateSave_AppName, WinStateSave_ChangeSamplerateWindowKey, hwndDlg);

                // Destroy the dialog window and its resources
                WinDestroyWindow(hwndDlg);

		if (iResult == PB_CHANGESAMPLERATE)
		{
#ifdef DEBUG_BUILD
		  printf("[fnFormatCommonNotebookPage] : Changing samplerate to %d\n", iNewSamplerate); fflush(stdout);
#endif
                  DoChangeSamplerate(hwnd, pWindowDesc, iNewSamplerate);
		} else
		if (iResult == PB_CONVERTSAMPLERATETO)
		{
#ifdef DEBUG_BUILD
		  printf("[fnFormatCommonNotebookPage] : Converting samplerate to %d\n", iNewSamplerate); fflush(stdout);
#endif
                  DoConvertSamplerate(hwnd, pWindowDesc, iNewSamplerate);
		}
	      }
	    }
	    return (MRESULT) 1; // Processed!
	  default:
            break;
	}
      }
      return (MRESULT) 0;

    case WM_UPDATECONTROLVALUES:
      pWindowDesc = (PropertiesWindowList_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (pWindowDesc)
	UpdateFormatCommonNotebookControlValues(hwnd, pWindowDesc);
      break;
    default:
      break;
  }
  // Call the default procedure if it has not been processed yet!
      return WinDefDlgProc(hwnd, msg, mp1, mp2);
}

MRESULT EXPENTRY fnFormatSpecificNotebookPage( HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2 )
{
  InitDlgAppData_p pInitDlgAppData;
  PropertiesWindowList_p pWindowDesc;
  long lID;

  switch (msg) {
    case WM_INITDLG:
      pInitDlgAppData = (InitDlgAppData_p) mp2;
      // Save pointer to pWindowDesc into user storage of dialog window!
      if (pInitDlgAppData)
      {
	WinSetWindowULong(hwnd, QWL_USER, (ULONG) (pInitDlgAppData->pRealData));
	pWindowDesc = (PropertiesWindowList_p) (pInitDlgAppData->pRealData);
	// Now initialize the controls!
	// Hmm, no controls need initialization in this window...:)

	// Then set the initial control values!
	UpdateFormatSpecificNotebookControlValues(hwnd, pWindowDesc);
      }
      else
	WinSetWindowULong(hwnd, QWL_USER, (ULONG) NULL);
      break;
    case WM_FORMATFRAME:
      pWindowDesc = (PropertiesWindowList_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (pWindowDesc)
	AdjustFormatSpecificNotebookControls(pWindowDesc);

      break;

    case WM_COMMAND:
      if (SHORT1FROMMP(mp2)==CMDSRC_PUSHBUTTON)
      {
	switch ((USHORT)mp1)
	{
	  case PB_MOVEUP:
            pWindowDesc = (PropertiesWindowList_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pWindowDesc)
            {
              lID = (long) WinSendDlgItemMsg(hwnd, LB_FORMATSPECSTRINGS, LM_QUERYSELECTION,
                                             (MPARAM) LIT_CURSOR, (MPARAM) 0);

              if (lID==LIT_NONE)
              {
                WinMessageBox(HWND_DESKTOP, hwnd,
                              "Please select a format string to be moved!",
                              "Properties plugin warning",
                              0, MB_OK | MB_MOVEABLE | MB_ICONEXCLAMATION);
              } else
              {
                int iChanged;
                lID = (long) WinSendDlgItemMsg(hwnd, LB_FORMATSPECSTRINGS, LM_QUERYITEMHANDLE,
                                               (MPARAM) lID, (MPARAM) 0);
                PluginHelpers.fn_WaWEHelper_ChannelSet_StartChSetFormatUsage(pWindowDesc->pChannelSet);
                iChanged = (WAWEHELPER_NOERROR == PluginHelpers.fn_WaWEHelper_ChannelSetFormat_MoveKey(&(pWindowDesc->pChannelSet->ChannelSetFormat),
                                                                                                       lID, lID-1));
                PluginHelpers.fn_WaWEHelper_ChannelSet_StopChSetFormatUsage(pWindowDesc->pChannelSet, iChanged);

                if (iChanged)
                {
                  WinSendDlgItemMsg(hwnd, LB_FORMATSPECSTRINGS, LM_SELECTITEM,
                                    (MPARAM) (lID-1), (MPARAM) TRUE);
                  // Every Async stuff has to initiate a redraw when it thinks that the
                  // changes should be shown now!
                  PluginHelpers.fn_WaWEHelper_Global_RedrawChanges();
                }
              }
            }
	    return (MRESULT) 1; // Processed!
          case PB_MOVEDOWN:
            pWindowDesc = (PropertiesWindowList_p) WinQueryWindowULong(hwnd, QWL_USER);
            if (pWindowDesc)
            {
              lID = (long) WinSendDlgItemMsg(hwnd, LB_FORMATSPECSTRINGS, LM_QUERYSELECTION,
                                             (MPARAM) LIT_CURSOR, (MPARAM) 0);

              if (lID==LIT_NONE)
              {
                WinMessageBox(HWND_DESKTOP, hwnd,
                              "Please select a format string to be moved!",
                              "Properties plugin warning",
                              0, MB_OK | MB_MOVEABLE | MB_ICONEXCLAMATION);
              } else
              {
                int iChanged;
                lID = (long) WinSendDlgItemMsg(hwnd, LB_FORMATSPECSTRINGS, LM_QUERYITEMHANDLE,
                                               (MPARAM) lID, (MPARAM) 0);
                PluginHelpers.fn_WaWEHelper_ChannelSet_StartChSetFormatUsage(pWindowDesc->pChannelSet);
                iChanged = (WAWEHELPER_NOERROR == PluginHelpers.fn_WaWEHelper_ChannelSetFormat_MoveKey(&(pWindowDesc->pChannelSet->ChannelSetFormat),
                                                                                                       lID, lID+1));
                PluginHelpers.fn_WaWEHelper_ChannelSet_StopChSetFormatUsage(pWindowDesc->pChannelSet, iChanged);

                if (iChanged)
                {
                  WinSendDlgItemMsg(hwnd, LB_FORMATSPECSTRINGS, LM_SELECTITEM,
                                    (MPARAM) (lID+1), (MPARAM) TRUE);
                  // Every Async stuff has to initiate a redraw when it thinks that the
                  // changes should be shown now!
                  PluginHelpers.fn_WaWEHelper_Global_RedrawChanges();
                }
              }
            }
            return (MRESULT) 1; // Processed!

          case PB_DELETESELECTED:
            pWindowDesc = (PropertiesWindowList_p) WinQueryWindowULong(hwnd, QWL_USER);
            if (pWindowDesc)
            {
              lID = (long) WinSendDlgItemMsg(hwnd, LB_FORMATSPECSTRINGS, LM_QUERYSELECTION,
                                             (MPARAM) LIT_CURSOR, (MPARAM) 0);

              if (lID==LIT_NONE)
              {
                WinMessageBox(HWND_DESKTOP, hwnd,
                              "Please select a format string to be deleted!",
                              "Properties plugin warning",
                              0, MB_OK | MB_MOVEABLE | MB_ICONEXCLAMATION);
	      } else
              if (WinMessageBox(HWND_DESKTOP, hwnd,
				"Are you sure you want to delete the selected format string?",
				"Deleting format string",
				0, MB_YESNO | MB_MOVEABLE | MB_ICONQUESTION)==MBID_YES)
	      {
                int iChanged;
                lID = (long) WinSendDlgItemMsg(hwnd, LB_FORMATSPECSTRINGS, LM_QUERYITEMHANDLE,
                                               (MPARAM) lID, (MPARAM) 0);
                PluginHelpers.fn_WaWEHelper_ChannelSet_StartChSetFormatUsage(pWindowDesc->pChannelSet);
                iChanged = (WAWEHELPER_NOERROR == PluginHelpers.fn_WaWEHelper_ChannelSetFormat_DeleteKeyByPosition(&(pWindowDesc->pChannelSet->ChannelSetFormat), lID));
                PluginHelpers.fn_WaWEHelper_ChannelSet_StopChSetFormatUsage(pWindowDesc->pChannelSet, iChanged);

                if (iChanged)
                {
                  WinSendDlgItemMsg(hwnd, LB_FORMATSPECSTRINGS, LM_SELECTITEM,
                                    (MPARAM) (lID), (MPARAM) TRUE);
                  // Every Async stuff has to initiate a redraw when it thinks that the
                  // changes should be shown now!
                  PluginHelpers.fn_WaWEHelper_Global_RedrawChanges();
                }
              }
            }
	    return (MRESULT) 1; // Processed!

          case PB_ADDNEW:
            pWindowDesc = (PropertiesWindowList_p) WinQueryWindowULong(hwnd, QWL_USER);
            if (pWindowDesc)
	    {
	      int iResult;
	      HWND hwndDlg;
	      hwndDlg = WinLoadDlg(HWND_DESKTOP,
				   hwnd, // Owner
				   (PFNWP)AddNewFormatStringDlgProc,
				   pWindowDesc->hmodOurDLLHandle,
				   DLG_NEWFORMATSPECIFICSTRING,
				   NULL);

	      if (!hwndDlg)
	      {
		ErrorMsg("Could not create dialog window!");
	      } else
	      {
		SWP swpParent, swpDlg;
		POINTL ptlTemp;
		char *pchName, *pchValue;
		int iNameLen, iValueLen;

		// Set initial values of controls!
		WinSendDlgItemMsg(hwndDlg, EF_KEYNAME, EM_SETTEXTLIMIT, (MPARAM) 1024, (MPARAM) 0);
		WinSendDlgItemMsg(hwndDlg, EF_KEYVALUE, EM_SETTEXTLIMIT, (MPARAM) 1024, (MPARAM) 0);
		WinSetWindowText(hwndDlg, "Add new format string");
		WinSetDlgItemText(hwndDlg, PB_CREATE, "~Create");

		WinSetDlgItemText(hwndDlg, EF_KEYNAME, "New Key");
		WinSetDlgItemText(hwndDlg, EF_KEYVALUE, "New Value");

		// Try to restore window position and size from ini file!
		// We'll use only the size from it!
		WinRestoreWindowPos(WinStateSave_AppName, WinStateSave_AddNewFormatStringWindowKey, hwndDlg);
  
		// Center dialog in parent
		WinQueryWindowPos(hwndDlg, &swpDlg);
		WinQueryWindowPos(hwnd, &swpParent);
		ptlTemp.x = swpParent.x;
		ptlTemp.y = swpParent.y;
		WinMapWindowPoints(hwnd, HWND_DESKTOP, &ptlTemp, 1);
		{
		  // Center dialog box within the parent window
		  int ix, iy;
		  ix = ptlTemp.x + (swpParent.cx - swpDlg.cx)/2;
		  iy = ptlTemp.y + (swpParent.cy - swpDlg.cy)/2;
		  WinSetWindowPos(hwndDlg, HWND_TOP, ix, iy, 0, 0,
				  SWP_MOVE);
		}
  
		WinSetWindowPos(hwndDlg, HWND_TOP, 0, 0, 0, 0,
				SWP_SHOW | SWP_ACTIVATE | SWP_ZORDER);
		WinSetFocus(HWND_DESKTOP, hwndDlg);

		// Process the dialog!
		iResult = WinProcessDlg(hwndDlg);
  
		// Save window position and size (we'll only use the size) into INI file!
		WinStoreWindowPos(WinStateSave_AppName, WinStateSave_AddNewFormatStringWindowKey, hwndDlg);

		if (iResult == PB_CREATE)
		{
#ifdef DEBUG_BUILD
		  printf("[fnFormatSpecificNotebookPage] : Create new string!\n"); fflush(stdout);
#endif
		  iNameLen = WinQueryDlgItemTextLength(hwndDlg, EF_KEYNAME);
		  iValueLen = WinQueryDlgItemTextLength(hwndDlg, EF_KEYVALUE);

		  if ((iNameLen==0) || (iValueLen==0))
		  {
		    ErrorMsg("Invalid format string name or value!");
		  } else
		  {
		    iNameLen++;
		    iValueLen++;
		    pchName = (char *) malloc(iNameLen);
		    pchValue = (char *) malloc(iValueLen);
		    if ((pchName) && (pchValue))
		    {
		      WinQueryDlgItemText(hwndDlg, EF_KEYNAME, iNameLen, pchName);
		      WinQueryDlgItemText(hwndDlg, EF_KEYVALUE, iValueLen, pchValue);
		      PluginHelpers.fn_WaWEHelper_ChannelSet_StartChSetFormatUsage(pWindowDesc->pChannelSet);
		      PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pWindowDesc->pChannelSet->ChannelSetFormat),
									    pchName,
									    pchValue);
		      PluginHelpers.fn_WaWEHelper_ChannelSet_StopChSetFormatUsage(pWindowDesc->pChannelSet, TRUE);
		      PluginHelpers.fn_WaWEHelper_Global_RedrawChanges();
		      free(pchName); free(pchValue); pchName = pchValue = NULL;
		    } else
		    {
		      ErrorMsg("Out of memory!");
		      if (pchName) free(pchName);
		      if (pchValue) free(pchValue);
		      pchName = pchValue = NULL;
		    }
		  }
		}

		// Destroy the dialog window and its resources
		WinDestroyWindow(hwndDlg);
	      }
            }
	    return (MRESULT) 1; // Processed!

          case PB_MODIFYSELECTED:
            pWindowDesc = (PropertiesWindowList_p) WinQueryWindowULong(hwnd, QWL_USER);
            if (pWindowDesc)
            {
              lID = (long) WinSendDlgItemMsg(hwnd, LB_FORMATSPECSTRINGS, LM_QUERYSELECTION,
                                             (MPARAM) LIT_CURSOR, (MPARAM) 0);

              if (lID==LIT_NONE)
              {
                WinMessageBox(HWND_DESKTOP, hwnd,
                              "Please select a format string to be modified!",
                              "Properties plugin warning",
                              0, MB_OK | MB_MOVEABLE | MB_ICONEXCLAMATION);
              } else
              {
                int iResult;
                HWND hwndDlg;
                hwndDlg = WinLoadDlg(HWND_DESKTOP,
                                     hwnd, // Owner
                                     (PFNWP)AddNewFormatStringDlgProc,
                                     pWindowDesc->hmodOurDLLHandle,
                                     DLG_NEWFORMATSPECIFICSTRING,
                                     NULL);
  
                if (!hwndDlg)
                {
                  ErrorMsg("Could not create dialog window!");
                } else
                {
                  SWP swpParent, swpDlg;
                  POINTL ptlTemp;
                  char *pchName, *pchValue;
                  int iNameLen, iValueLen;
  
                  // Set initial values of controls!
                  WinSendDlgItemMsg(hwndDlg, EF_KEYNAME, EM_SETTEXTLIMIT, (MPARAM) 1024, (MPARAM) 0);
                  WinSendDlgItemMsg(hwndDlg, EF_KEYVALUE, EM_SETTEXTLIMIT, (MPARAM) 1024, (MPARAM) 0);
                  WinSetWindowText(hwndDlg, "Modify format string");
                  WinSetDlgItemText(hwndDlg, PB_CREATE, "~Modify");
                  PluginHelpers.fn_WaWEHelper_ChannelSet_StartChSetFormatUsage(pWindowDesc->pChannelSet);
                  PluginHelpers.fn_WaWEHelper_ChannelSetFormat_GetKeySizeByPosition(&(pWindowDesc->pChannelSet->ChannelSetFormat),
                                                                                    lID,
                                                                                    &iNameLen,
                                                                                    &iValueLen);
                  pchName = (char *) malloc(iNameLen);
                  pchValue = (char *) malloc(iValueLen);
                  if ((pchName) && (pchValue))
                    PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKeyByPosition(&(pWindowDesc->pChannelSet->ChannelSetFormat),
                                                                                   lID,
                                                                                   pchName,
                                                                                   iNameLen,
                                                                                   pchValue,
                                                                                   iValueLen);
                  PluginHelpers.fn_WaWEHelper_ChannelSet_StopChSetFormatUsage(pWindowDesc->pChannelSet, FALSE);
  
                  if ((pchName) && (pchValue))
                  {
                    WinSetDlgItemText(hwndDlg, EF_KEYNAME, pchName);
                    WinSetDlgItemText(hwndDlg, EF_KEYVALUE, pchValue);
  
                    if (pchName) free(pchName);
                    if (pchValue) free(pchValue);
                    pchName = NULL;
                    pchValue = NULL;
  
                    // Try to restore window position and size from ini file!
                    // We'll use only the size from it!
                    WinRestoreWindowPos(WinStateSave_AppName, WinStateSave_ModifyFormatStringWindowKey, hwndDlg);
    
                    // Center dialog in parent
                    WinQueryWindowPos(hwndDlg, &swpDlg);
                    WinQueryWindowPos(hwnd, &swpParent);
                    ptlTemp.x = swpParent.x;
                    ptlTemp.y = swpParent.y;
                    WinMapWindowPoints(hwnd, HWND_DESKTOP, &ptlTemp, 1);
                    {
                      // Center dialog box within the parent window
                      int ix, iy;
                      ix = ptlTemp.x + (swpParent.cx - swpDlg.cx)/2;
                      iy = ptlTemp.y + (swpParent.cy - swpDlg.cy)/2;
                      WinSetWindowPos(hwndDlg, HWND_TOP, ix, iy, 0, 0,
                                      SWP_MOVE);
                    }
    
                    WinSetWindowPos(hwndDlg, HWND_TOP, 0, 0, 0, 0,
                                    SWP_SHOW | SWP_ACTIVATE | SWP_ZORDER);
                    WinSetFocus(HWND_DESKTOP, hwndDlg);
    
                    // Process the dialog!
                    iResult = WinProcessDlg(hwndDlg);
    
                    // Save window position and size (we'll only use the size) into INI file!
                    WinStoreWindowPos(WinStateSave_AppName, WinStateSave_ModifyFormatStringWindowKey, hwndDlg);
  
                    if (iResult == PB_CREATE)
                    {
#ifdef DEBUG_BUILD
                      printf("[fnFormatSpecificNotebookPage] : Create new string!\n"); fflush(stdout);
#endif
                      iNameLen = WinQueryDlgItemTextLength(hwndDlg, EF_KEYNAME);
                      iValueLen = WinQueryDlgItemTextLength(hwndDlg, EF_KEYVALUE);
  
                      if ((iNameLen==0) || (iValueLen==0))
                      {
                        ErrorMsg("Invalid format string name or value!");
                      } else
                      {
                        iNameLen++;
                        iValueLen++;
                        pchName = (char *) malloc(iNameLen);
                        pchValue = (char *) malloc(iValueLen);
                        if ((pchName) && (pchValue))
                        {
			  WinQueryDlgItemText(hwndDlg, EF_KEYNAME, iNameLen, pchName);
                          WinQueryDlgItemText(hwndDlg, EF_KEYVALUE, iValueLen, pchValue);
                          PluginHelpers.fn_WaWEHelper_ChannelSet_StartChSetFormatUsage(pWindowDesc->pChannelSet);
                          PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ModifyKeyByPosition(&(pWindowDesc->pChannelSet->ChannelSetFormat),
                                                                                           lID,
                                                                                           pchName,
                                                                                           pchValue);
                          PluginHelpers.fn_WaWEHelper_ChannelSet_StopChSetFormatUsage(pWindowDesc->pChannelSet, TRUE);
                          PluginHelpers.fn_WaWEHelper_Global_RedrawChanges();
                          free(pchName); free(pchValue); pchName = pchValue = NULL;
                        } else
                        {
                          ErrorMsg("Out of memory!");
                          if (pchName) free(pchName);
                          if (pchValue) free(pchValue);
                          pchName = pchValue = NULL;
                        }
                      }
                    }
  
                  } else
                  {
                    if (pchName) free(pchName);
                    if (pchValue) free(pchValue);
                    pchName = NULL;
                    pchValue = NULL;
                    ErrorMsg("Out of memory!");
                  }
  
                  // Destroy the dialog window and its resources
                  WinDestroyWindow(hwndDlg);
                }
              }
            }
	    return (MRESULT) 1; // Processed!

          default:
            break;
	}
      }
      return (MRESULT) 0;

    case WM_UPDATECONTROLVALUES:
      pWindowDesc = (PropertiesWindowList_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (pWindowDesc)
	UpdateFormatSpecificNotebookControlValues(hwnd, pWindowDesc);
      break;
    default:
      break;
  }
  // Call the default procedure if it has not been processed yet!
  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}

MRESULT EXPENTRY fnChannelsNotebookPage( HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2 )
{
  InitDlgAppData_p pInitDlgAppData;
  PropertiesWindowList_p pWindowDesc;

  switch (msg) {
    case WM_INITDLG:
      pInitDlgAppData = (InitDlgAppData_p) mp2;
      // Save pointer to pWindowDesc into user storage of dialog window!
      if (pInitDlgAppData)
      {
	WinSetWindowULong(hwnd, QWL_USER, (ULONG) (pInitDlgAppData->pRealData));
	pWindowDesc = (PropertiesWindowList_p) (pInitDlgAppData->pRealData);
	// Now initialize the controls!

        // First set the limits
	WinSendDlgItemMsg(hwnd, EF_CHANNELNAME, EM_SETTEXTLIMIT, (MPARAM) WAWE_CHANNEL_NAME_LEN, (MPARAM) 0);

	// Then set the initial control values!
	UpdateChannelsNotebookControlValues(hwnd, pWindowDesc);
      }
      else
	WinSetWindowULong(hwnd, QWL_USER, (ULONG) NULL);
      break;
    case WM_FORMATFRAME:
      pWindowDesc = (PropertiesWindowList_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (pWindowDesc)
	AdjustChannelsNotebookControls(pWindowDesc);

      break;

    case WM_UPDATECONTROLVALUES:
      pWindowDesc = (PropertiesWindowList_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (pWindowDesc)
	UpdateChannelsNotebookControlValues(hwnd, pWindowDesc);
      break;

    case WM_CONTROL:
      switch (SHORT1FROMMP(mp1))
      {
	case CX_LISTOFCHANNELS:
	  if (SHORT2FROMMP(mp1)==LN_SELECT)
	  {
	    // The listbox selection has been changed.
	    pWindowDesc = (PropertiesWindowList_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pWindowDesc)
	      UpdateChannelsNotebookChPropsControlValues(hwnd, pWindowDesc);
	  }
          break;
        case EF_CHANNELNAME:
          if (SHORT2FROMMP(mp1)==EN_KILLFOCUS)
          {
            // The content of the entry field might has been changed!
            pWindowDesc = (PropertiesWindowList_p) WinQueryWindowULong(hwnd, QWL_USER);
            if ((pWindowDesc) && (pWindowDesc->iWindowCreated==1))
            {
              // If changed, then propagate the changes!
              if (WinSendDlgItemMsg(hwnd, EF_CHANNELNAME, EM_QUERYCHANGED, (MPARAM) 0, (MPARAM) 0))
                DoChannelChanged(hwnd, pWindowDesc);
            }
          }
          break;
	default:
          break;
      }
      return (MRESULT) 0;

    case WM_COMMAND:
      if (SHORT1FROMMP(mp2)==CMDSRC_PUSHBUTTON)
      {
	switch ((USHORT)mp1)
	{
	  case PB_CHMOVEUP:
            pWindowDesc = (PropertiesWindowList_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pWindowDesc)
              MoveChannelUp(hwnd, pWindowDesc);
	    return (MRESULT) 1; // Processed!

	  case PB_CHMOVEDOWN:
            pWindowDesc = (PropertiesWindowList_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pWindowDesc)
	      MoveChannelDown(hwnd, pWindowDesc);

            return (MRESULT) 1; // Processed!
	  default:
            break;
	}
      }
      return (MRESULT) 0;

    default:
      break;
  }
  // Call the default procedure if it has not been processed yet!
  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}


void WindowMsgProcessorThreadFunc(void *pParm)
{
  InitDlgAppData_t InitDlgAppData;
  PropertiesWindowList_p pNewWindowDesc = pParm;
  PropertiesWindowList_p pTemp, pPrev;
  SWP swpTemp, swpParent;
  unsigned short usTabTextLength=0, usTabTextHeight=0;
  HAB hab;
  HMQ hmq;

  // We'll pass the pNewWindowDesc to the dialog windows at WM_INITDLG time.
  // For this, we have to create this small structure, because PM wants an unsigned
  // short at the beginning, telling the size of the structure. Eeeek.
  InitDlgAppData.usAreaSize = sizeof(InitDlgAppData_t);
  InitDlgAppData.pRealData = pNewWindowDesc;

  // Setup this thread for using PM (sending messages etc...)
  hab = WinInitialize(0);
  hmq = WinCreateMsgQueue(hab, 0);

  pNewWindowDesc->hwndPropertiesWindow =
    WinLoadDlg(HWND_DESKTOP, HWND_DESKTOP, (PFNWP)PropertiesDlgProc, pNewWindowDesc->hmodOurDLLHandle,
               DLG_CHANNELSETPROPERTIES, &InitDlgAppData);

  if (!(pNewWindowDesc->hwndPropertiesWindow))
  {
#ifdef DEBUG_BUILD
    printf("[CreateNewPropertiesWindow] : Could not load resource!\n");
#endif
    pNewWindowDesc->iWindowCreated = -1; // Meaning Error!
  } else
  {
    // Query the handle of the notebook control
    pNewWindowDesc->hwndPropertiesNB = WinWindowFromID(pNewWindowDesc->hwndPropertiesWindow, NBK_PROPERTIES);
  
    // Now load and add the pages, one by one...
    pNewWindowDesc->hwndGeneralNBP = WinLoadDlg(pNewWindowDesc->hwndPropertiesNB,
                                                pNewWindowDesc->hwndPropertiesNB,
                                                (PFNWP) fnGeneralNotebookPage,
                                                pNewWindowDesc->hmodOurDLLHandle,
                                                NBP_GENERAL, &InitDlgAppData);
  
    pNewWindowDesc->hwndFormatCommonNBP = WinLoadDlg(pNewWindowDesc->hwndPropertiesNB,
                                                     pNewWindowDesc->hwndPropertiesNB,
                                                     (PFNWP) fnFormatCommonNotebookPage,
                                                     pNewWindowDesc->hmodOurDLLHandle,
                                                     NBP_FORMATCOMMON, &InitDlgAppData);
  
    pNewWindowDesc->hwndFormatSpecificNBP = WinLoadDlg(pNewWindowDesc->hwndPropertiesNB,
                                                       pNewWindowDesc->hwndPropertiesNB,
                                                       (PFNWP) fnFormatSpecificNotebookPage,
                                                       pNewWindowDesc->hmodOurDLLHandle,
                                                       NBP_FORMATSPECIFIC, &InitDlgAppData);
  
    pNewWindowDesc->hwndChannelsNBP = WinLoadDlg(pNewWindowDesc->hwndPropertiesNB,
                                                 pNewWindowDesc->hwndPropertiesNB,
                                                 (PFNWP) fnChannelsNotebookPage,
                                                 pNewWindowDesc->hmodOurDLLHandle,
                                                 NBP_CHANNELS, &InitDlgAppData);
  
    // Save the size of one notebook page for later use.
    // (after adding, it will be resized, because of the AUTOSIZE flag of notebook!)
    WinQueryWindowPos(pNewWindowDesc->hwndGeneralNBP, &swpTemp);
  
    // Add pages to notebook:
    AddNotebookTab(pNewWindowDesc->hwndPropertiesNB,
                   pNewWindowDesc->hwndGeneralNBP,
                   BKA_MAJOR,
                   "General", "General properties",
                   &(pNewWindowDesc->ulGeneralNBPID),
                   &usTabTextLength, &usTabTextHeight);
  
    AddNotebookTab(pNewWindowDesc->hwndPropertiesNB,
                   pNewWindowDesc->hwndFormatCommonNBP,
                   BKA_MAJOR,
                   "Format", "Common channel-set format (Page 1 of 2)",
                   &(pNewWindowDesc->ulFormatCommonNBPID),
                   &usTabTextLength, &usTabTextHeight);
  
    AddNotebookTab(pNewWindowDesc->hwndPropertiesNB,
                   pNewWindowDesc->hwndFormatSpecificNBP,
                   BKA_MINOR,
		   "Format", "Format-specific values (Page 2 of 2)",
                   &(pNewWindowDesc->ulFormatSpecificNBPID),
		   &usTabTextLength, &usTabTextHeight);

    AddNotebookTab(pNewWindowDesc->hwndPropertiesNB,
                   pNewWindowDesc->hwndChannelsNBP,
                   BKA_MAJOR,
                   "Channels", "Channels of channel-set",
                   &(pNewWindowDesc->ulChannelsNBPID),
                   &usTabTextLength, &usTabTextHeight);
  
    // The followings are discarded on Warp4, but makes things look better on Warp3:
  
    // Set notebook info field color (default is white... Yipe...)
    WinSendMsg(pNewWindowDesc->hwndPropertiesNB,
               BKM_SETNOTEBOOKCOLORS,
               MPFROMLONG(SYSCLR_FIELDBACKGROUND), MPFROMSHORT(BKA_BACKGROUNDPAGECOLORINDEX));
    // Adjust tab size:
    WinSendMsg(pNewWindowDesc->hwndPropertiesNB,
               BKM_SETDIMENSIONS,
               MPFROM2SHORT(usTabTextLength + 10, (SHORT)((float)usTabTextHeight * 0.8)),
               MPFROMSHORT(BKA_MAJORTAB));
    WinSendMsg(pNewWindowDesc->hwndPropertiesNB,
               BKM_SETDIMENSIONS,
               MPFROM2SHORT(0, 0),
               MPFROMSHORT(BKA_MINORTAB));
  
  
    // Try to restore window position and size from ini file!
    // We'll use only the size from it!
    WinRestoreWindowPos(WinStateSave_AppName, WinStateSave_PropertiesWindowKey, pNewWindowDesc->hwndPropertiesWindow);

    // Position the window in the middle of its parent!
    WinQueryWindowPos(pNewWindowDesc->hwndOwner, &swpParent);
    WinQueryWindowPos(pNewWindowDesc->hwndPropertiesWindow, &swpTemp);
    swpTemp.x = (swpParent.cx-swpTemp.cx)/2 + swpParent.x;
    swpTemp.y = (swpParent.cy-swpTemp.cy)/2 + swpParent.y;
    WinSetWindowPos(pNewWindowDesc->hwndPropertiesWindow, HWND_TOP,
                    swpTemp.x, swpTemp.y,
                    0, 0,
                    SWP_MOVE);
  
    // Show window
    WinShowWindow(pNewWindowDesc->hwndPropertiesWindow, TRUE);
    WinSetFocus(HWND_DESKTOP, pNewWindowDesc->hwndPropertiesNB);
  
#ifdef DEBUG_BUILD
    printf("[WindowMsgProcessorThreadFunc] : Window opened!\n"); fflush(stdout);
#endif

    pNewWindowDesc->iWindowCreated = 1; // Meaning: Window is created well!
    // Process messages
    WinProcessDlg(pNewWindowDesc->hwndPropertiesWindow);

#ifdef DEBUG_BUILD
    printf("[WindowMsgProcessorThreadFunc] : Window closed!\n"); fflush(stdout);
#endif

    // Save window position and size (we'll only use the size) into INI file!
    WinStoreWindowPos(WinStateSave_AppName, WinStateSave_PropertiesWindowKey, pNewWindowDesc->hwndPropertiesWindow);
    // Now cleanup
    WinDestroyWindow(pNewWindowDesc->hwndPropertiesWindow);
  
    // Remove this window from the list of properties windows!
    DosRequestMutexSem(hmtxUseWindowList, SEM_INDEFINITE_WAIT);
    pPrev = NULL;
    pTemp = pWindowListHead;
    while ((pTemp) && (pTemp!=pNewWindowDesc))
    {
      pPrev = pTemp;
      pTemp = pTemp->pNext;
    }
  
    if (pTemp)
    {
      // Found!
      // Unlink it
      if (pPrev)
        pPrev->pNext = pTemp->pNext;
      else
        pWindowListHead = pTemp->pNext;
      // Free it
      free(pTemp);
    }
#ifdef DEBUG_BUILD
    else
    {
      printf("[WindowMsgProcessorThreadFunc] : Internal error: WindowDesc not found in list!\n"); fflush(stdout);
    }
#endif
    DosReleaseMutexSem(hmtxUseWindowList);
  }
  // Uninitialize PM
  WinDestroyMsgQueue(hmq);
  WinTerminate(hab);

  _endthread();
}


int CreateNewPropertiesWindow(PropertiesWindowList_p pNewWindowDesc,
                              HMODULE hmodOurDLLHandle,
                              WaWEChannelSet_p pChannelSet,
                              HWND hwndOwner)
{
  if ((!pNewWindowDesc) || (!pChannelSet)) return 0;

  // Fill window desc structure!
  memset(pNewWindowDesc, 0, sizeof(PropertiesWindowList_t));
  pNewWindowDesc->pChannelSet = pChannelSet;
  pNewWindowDesc->hmodOurDLLHandle = hmodOurDLLHandle;
  pNewWindowDesc->hwndOwner = hwndOwner;
  pNewWindowDesc->iWindowCreated = 0;

  // Start a thread to do the job!
  pNewWindowDesc->tidWindowThread =
    _beginthread(WindowMsgProcessorThreadFunc, 0, 1024*1024, (void *) pNewWindowDesc);
  // Set priority of thread to high, so it will process PM updates quickly...
  DosSetPriority(PRTYS_THREAD, PRTYC_REGULAR, +5, pNewWindowDesc->tidWindowThread);

  // While until the thread does its job!
  while (pNewWindowDesc->iWindowCreated==0) DosSleep(64);

  if (pNewWindowDesc->iWindowCreated!=1)
  {
    // Ooops, an error has occured!
    // Wait for the thread to die, and return with failure message!
    DosWaitThread(&pNewWindowDesc->tidWindowThread, DCWW_WAIT);
    return 0;
  }

  // All done, return success!
  return 1;
}

void DestroyPropertiesWindow(PropertiesWindowList_p pWindowDesc)
{
  HWND hwndWindow;
  TID tidWindowThread;

  if (pWindowDesc)
  {
    DosRequestMutexSem(hmtxUseWindowList, SEM_INDEFINITE_WAIT);

    hwndWindow = pWindowDesc->hwndPropertiesWindow;
    tidWindowThread = pWindowDesc->tidWindowThread;
    DosReleaseMutexSem(hmtxUseWindowList);

    // Send a WM_QUIT to the properties window!
    WinPostMsg(hwndWindow, WM_QUIT, (MPARAM) NULL, (MPARAM) NULL);

    // Wait for the window to be destroyed and the server thread to die!
    DosWaitThread(&tidWindowThread, DCWW_WAIT);
  }
}

