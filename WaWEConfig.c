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
 * WaWE configuration module                                                 *
 *                                                                           *
 * This module contains the current configuration of WaWE, and the functions *
 * to modify it.                                                             *
 *****************************************************************************/

#include "WaWE.h"

#define MIN_ACCEPTABLE_CACHEPAGESIZE   128
#define MAX_ACCEPTABLE_CACHEPAGESIZE   (1024*512)

#define MIN_ACCEPTABLE_NUMOFCACHEPAGES 4
#define MAX_ACCEPTABLE_NUMOFCACHEPAGES (1024*1024)

#define WM_UPDATEVALUES   (WM_USER+100)

WaWEConfiguration_t WaWEConfiguration;

static WaWEConfiguration_t WaWEUndoConfiguration; // Undo config

static HWND hwndConfigDlg; // Main window handle to config window
static HWND hwndConfigNB;  // Window handle to the notebook inside the config window
static HWND hwndCacheNBP; // Window handle to the Cache settings notebook page
static unsigned long ulCacheNBPID; // Notebook Page ID
static HWND hwndInterfaceNBP; // Window handle to the Interface settings notebook page
static unsigned long ulInterfaceNBPID; // Notebook Page ID

static char achTextUndoBuf[1024]; // Undo buffer for entry fields in config window


static void SetDefaultWaWEConfig()
{
  WaWEConfiguration.bUseCache = TRUE;
  WaWEConfiguration.uiCachePageSize = 4096;
  WaWEConfiguration.uiNumOfCachePages = 2048; // Maximum of 8 megs per channel-set, alltogether

  WaWEConfiguration.iTimeFormat = WAWE_TIMEFORMAT_MSEC;

  WaWEConfiguration.achLastOpenFolder[0] = 0; // Empty string
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

static void AdjustControls(int NBPwidth, int NBPheight)
{
  SWP swpTemp;
  int xsize, ysize;
  RECTL rectl;
  POINTL pointl;
  int buttonheight;
  ULONG CXDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
  ULONG CYDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);

  // Query button height in pixels
  if (WinQueryWindowPos(WinWindowFromID(hwndConfigDlg, PB_DEFAULT), &swpTemp))
    buttonheight = swpTemp.cy;
  else
    buttonheight = 0;

  // Set the size and position of Notebook

  // The size is interesting. We should know how much additional space this notebook requires.
  // For this, we calculate how big is a notebook for a known client size (e.g., 0x0)

  rectl.xLeft = 0; rectl.xRight = 0;
  rectl.yBottom = 0; rectl.yTop = 0;

  WinSendMsg(hwndConfigNB, BKM_CALCPAGERECT, MPFROMP(&rectl), MPFROMSHORT(FALSE));
  // Ok, this must be added to client size in order to be able to display the given client dialog:
  NBPwidth += rectl.xRight;
  NBPheight += rectl.yTop;

  // Now check how much shift is inside the window:
  // (client/notebook shift)
  pointl.x = 0;
  pointl.y = 0;
  WinMapWindowPoints(hwndInterfaceNBP, hwndConfigNB, &pointl, 1);
  // Add this too:
  NBPwidth += pointl.x;
  NBPheight += pointl.y;

  // Notebook position is above the buttons, which is some pixels above the bottom of window:
  swpTemp.x = CXDLGFRAME+2;
  swpTemp.y = CYDLGFRAME + buttonheight + 5;
  // Notebook size is just calculated:
  swpTemp.cx = NBPwidth;
  swpTemp.cy = NBPheight;
  WinSetWindowPos(hwndConfigNB, HWND_TOP,
		  swpTemp.x, swpTemp.y,
		  swpTemp.cx, swpTemp.cy,
		  SWP_MOVE | SWP_SIZE);

  // Ok, the notebook is resized. Now resize the dialog window to be just
  // as big as it's needed for the notebook:
  xsize = swpTemp.cx + CXDLGFRAME*2 + 2*2;
  ysize = swpTemp.cy + CYDLGFRAME*2 + buttonheight + 5 + WinQuerySysValue(HWND_DESKTOP, SV_CYTITLEBAR) + 5;
  WinSetWindowPos(hwndConfigDlg, HWND_TOP,
		  0, 0,
		  xsize,
                  ysize,
		  SWP_SIZE);

  // Set the position of buttons
  //  Undo Button
  WinQueryWindowPos(WinWindowFromID(hwndConfigDlg, PB_UNDO), &swpTemp);
  swpTemp.y = CYDLGFRAME+2;
  swpTemp.x = (xsize/6)*1-swpTemp.cx/2;
  WinSetWindowPos(WinWindowFromID(hwndConfigDlg, PB_UNDO), HWND_TOP,
		  swpTemp.x, swpTemp.y,
		  swpTemp.cx, swpTemp.cy,
		  SWP_MOVE);

  //  Default Button
  WinQueryWindowPos(WinWindowFromID(hwndConfigDlg, PB_DEFAULT), &swpTemp);
  swpTemp.y = CYDLGFRAME+2;
  swpTemp.x = (xsize/6)*3-swpTemp.cx/2;
  WinSetWindowPos(WinWindowFromID(hwndConfigDlg, PB_DEFAULT), HWND_TOP,
		  swpTemp.x, swpTemp.y,
		  swpTemp.cx, swpTemp.cy,
		  SWP_MOVE);
  //  Help Button
  WinQueryWindowPos(WinWindowFromID(hwndConfigDlg, PB_HELP), &swpTemp);
  swpTemp.y = CYDLGFRAME+2;
  swpTemp.x = (xsize/6)*5-swpTemp.cx/2;
  WinSetWindowPos(WinWindowFromID(hwndConfigDlg, PB_HELP), HWND_TOP,
		  swpTemp.x, swpTemp.y,
		  swpTemp.cx, swpTemp.cy,
		  SWP_MOVE);

  // That's all!
}

MRESULT EXPENTRY ConfigDlgProc( HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2 )
{
  switch (msg) {
    case WM_INITDLG:
      break;
    case WM_CLOSE:
      // Save the configuration info!
      SaveWaWEConfig();
      break;
    case WM_COMMAND:
      switch SHORT1FROMMP(mp2) {
	case CMDSRC_PUSHBUTTON:           // ---- A WM_COMMAND from a pushbutton ------
	  switch (SHORT1FROMMP(mp1)) {
            case PB_DEFAULT:                             // Default button pressed
#ifdef DEBUG_BUILD
              printf("[CONFIG_DLG] : Default button pressed!\n");
#endif
              SetDefaultWaWEConfig();
              WinPostMsg(hwndInterfaceNBP, WM_UPDATEVALUES, NULL, NULL);
              WinPostMsg(hwndCacheNBP, WM_UPDATEVALUES, NULL, NULL);
	      return (MRESULT) TRUE;
            case PB_HELP:                             // Help button pressed
#ifdef DEBUG_BUILD
              printf("[CONFIG_DLG] : Help button pressed!\n");
#endif
              // TODO!
	      return (MRESULT) TRUE;
            case PB_UNDO:                             // Undo button pressed
#ifdef DEBUG_BUILD
              printf("[CONFIG_DLG] : Undo button pressed!\n");
#endif
              memcpy(&WaWEConfiguration, &WaWEUndoConfiguration, sizeof(WaWEConfiguration));
              WinPostMsg(hwndInterfaceNBP, WM_UPDATEVALUES, NULL, NULL);
              WinPostMsg(hwndCacheNBP, WM_UPDATEVALUES, NULL, NULL);
	      return (MRESULT) TRUE;
	    default:
	      break;
	  }
	  break; // End os CMDSRC_PUSHBUTTON processing
      } // end of switch inside WM_COMMAND
      break;
    default:
      break;
  } // end of switch of msg

  // Call the default procedure if it has not been processed yet!
  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}

MRESULT EXPENTRY fnCacheNotebookPage( HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2 )
{
  char sTemp[256];
  int iTemp;
  int iCacheSize;

  switch (msg)
  {
    case WM_UPDATEVALUES:
    case WM_INITDLG:
      // Initialization of this page

      // Calculate full cache size
      iCacheSize = WaWEConfiguration.uiCachePageSize * WaWEConfiguration.uiNumOfCachePages;

      // Fill the entry fields

      // Set Cache page size
      WinSetDlgItemText(hwnd, EF_SIZEOFCACHEPAGE, (MPARAM) itoa(WaWEConfiguration.uiCachePageSize, sTemp, 10));
      // Set num of cache pages
      WinSetDlgItemText(hwnd, EF_MAXNUMOFCACHEPAGES, (MPARAM) itoa(WaWEConfiguration.uiNumOfCachePages, sTemp, 10));
      // Set full cache size
      WinSetDlgItemText(hwnd, EF_MAXIMUMCACHESIZEFORCHSET, (MPARAM) itoa(iCacheSize, sTemp, 10));

      // Checkbox of use cache
      WinSendMsg(WinWindowFromID(hwnd, CB_USEREADCACHEFORCHANNELSETS), BM_SETCHECK,
                 (MPARAM) (WaWEConfiguration.bUseCache),
                 (MPARAM) (0));

      break;
    case WM_CONTROL:  // ------------------- WM_CONTROL messages ---------------------------
      switch SHORT1FROMMP(mp1) {
	case EF_SIZEOFCACHEPAGE:           // - - - - - - - Message from SizeOfCachePage entry field - - - - - - -
	  switch (SHORT2FROMMP(mp1)) {
	    case EN_SETFOCUS:                 // Entered into entry field
	      // Save old text from here:
	      WinQueryDlgItemText(hwnd, SHORT1FROMMP(mp1),
				  1024, achTextUndoBuf);
	      break;
	    case EN_KILLFOCUS:                // Leaving entry field
	      {
		// Check the text there, and restore old one, if it's invalid!
		WinQueryDlgItemText(hwnd, SHORT1FROMMP(mp1),
				    256, sTemp);
		iTemp = atoi(sTemp);
		if ((iTemp>=MIN_ACCEPTABLE_CACHEPAGESIZE) && (iTemp<=MAX_ACCEPTABLE_CACHEPAGESIZE))
		{
		  WaWEConfiguration.uiCachePageSize = iTemp;
                  WinSetDlgItemText(hwnd, SHORT1FROMMP(mp1), (MPARAM) itoa(iTemp, sTemp, 10));
                  iCacheSize = WaWEConfiguration.uiCachePageSize * WaWEConfiguration.uiNumOfCachePages;
                  WinSetDlgItemText(hwnd, EF_MAXIMUMCACHESIZEFORCHSET, (MPARAM) itoa(iCacheSize, sTemp, 10));
		}
                else
		  WinSetDlgItemText(hwnd, SHORT1FROMMP(mp1), achTextUndoBuf);

		break;
	      }
	    default:
	      break;
          }
          return (MPARAM) TRUE;
	  break; // End of EF_* processing
	case EF_MAXNUMOFCACHEPAGES:           // - - - - - - - Message from NumOfCachePages entry field - - - - - - -
	  switch (SHORT2FROMMP(mp1)) {
	    case EN_SETFOCUS:                 // Entered into entry field
	      // Save old text from here:
	      WinQueryDlgItemText(hwnd, SHORT1FROMMP(mp1),
				  1024, achTextUndoBuf);
	      break;
	    case EN_KILLFOCUS:                // Leaving entry field
	      {
		// Check the text there, and restore old one, if it's invalid!
		WinQueryDlgItemText(hwnd, SHORT1FROMMP(mp1),
				    256, sTemp);
		iTemp = atoi(sTemp);
		if ((iTemp>=MIN_ACCEPTABLE_NUMOFCACHEPAGES) && (iTemp<=MAX_ACCEPTABLE_NUMOFCACHEPAGES))
		{
		  WaWEConfiguration.uiNumOfCachePages = iTemp;
                  WinSetDlgItemText(hwnd, SHORT1FROMMP(mp1), (MPARAM) itoa(iTemp, sTemp, 10));
                  iCacheSize = WaWEConfiguration.uiCachePageSize * WaWEConfiguration.uiNumOfCachePages;
                  WinSetDlgItemText(hwnd, EF_MAXIMUMCACHESIZEFORCHSET, (MPARAM) itoa(iCacheSize, sTemp, 10));
		}
                else
		  WinSetDlgItemText(hwnd, SHORT1FROMMP(mp1), achTextUndoBuf);

		break;
	      }
	    default:
	      break;
          }
          return (MPARAM) TRUE;
	  break; // End of EF_* processing
          // -------------------------------- Checkboxes --------------------
	case CB_USEREADCACHEFORCHANNELSETS:
	  if (SHORT2FROMMP(mp1) == BN_CLICKED)
	  {
            WaWEConfiguration.bUseCache =
              WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1));
          }
          break;
        default:
	  break;
      } // end of switch inside WM_CONTROL
      break;
    default:
      break;
  } // end of switch of msg

  // Call the default procedure if it has not been processed yet!
  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}

MRESULT EXPENTRY fnInterfaceNotebookPage( HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2 )
{
  switch (msg)
  {
    case WM_UPDATEVALUES:
    case WM_INITDLG:
      // Initialization of this page

      // Checkbox of time format
      if (WaWEConfiguration.iTimeFormat == WAWE_TIMEFORMAT_MSEC)
      {
#ifdef DEBUG_BUILD
//        printf("Sending BM_SETCHECK to RB_MSEC\n");
#endif
        WinSendMsg(WinWindowFromID(hwnd, RB_MSEC), BM_SETCHECK,
                   (MPARAM) (1),
                   (MPARAM) (0));
      }
      if (WaWEConfiguration.iTimeFormat == WAWE_TIMEFORMAT_HHMMSSMSEC)
      {
#ifdef DEBUG_BUILD
//        printf("Sending BM_SETCHECK to RB_HRMMSECMS (0x%p)\n", WinWindowFromID(hwnd, RB_HRMMSECMS));
#endif
        WinSendMsg(WinWindowFromID(hwnd, RB_HRMMSECMS), BM_SETCHECK,
                   (MPARAM) (1),
                   (MPARAM) (0));
      }
      if (WaWEConfiguration.iTimeFormat == WAWE_TIMEFORMAT_SAMPLES)
      {
#ifdef DEBUG_BUILD
//        printf("Sending BM_SETCHECK to RB_SAMPLES\n");
#endif

        WinSendMsg(WinWindowFromID(hwnd, RB_SAMPLES), BM_SETCHECK,
                   (MPARAM) (1),
                   (MPARAM) (0));
      }

      // Make sure the changes will be visible
      if (msg == WM_UPDATEVALUES)
        AddNewWorker1Msg(WORKER1_MSG_UPDATEPOSITIONANDSIZESTRINGS, 0, NULL);

      break;
    case WM_CONTROL:  // ------------------- WM_CONTROL messages ---------------------------
      switch SHORT1FROMMP(mp1) {
          // -------------------------------- Radioboxes --------------------
	case RB_MSEC:
	  if (SHORT2FROMMP(mp1) == BN_CLICKED)
	  {
            WaWEConfiguration.iTimeFormat = WAWE_TIMEFORMAT_MSEC;
            AddNewWorker1Msg(WORKER1_MSG_UPDATEPOSITIONANDSIZESTRINGS, 0, NULL);
          }
          break;
	case RB_HRMMSECMS:
	  if (SHORT2FROMMP(mp1) == BN_CLICKED)
	  {
            WaWEConfiguration.iTimeFormat = WAWE_TIMEFORMAT_HHMMSSMSEC;
            AddNewWorker1Msg(WORKER1_MSG_UPDATEPOSITIONANDSIZESTRINGS, 0, NULL);
          }
          break;
	case RB_SAMPLES:
	  if (SHORT2FROMMP(mp1) == BN_CLICKED)
	  {
            WaWEConfiguration.iTimeFormat = WAWE_TIMEFORMAT_SAMPLES;
            AddNewWorker1Msg(WORKER1_MSG_UPDATEPOSITIONANDSIZESTRINGS, 0, NULL);
          }
          break;
        default:
	  break;
      } // end of switch inside WM_CONTROL
      break;
    default:
      break;
  } // end of switch of msg

  // Call the default procedure if it has not been processed yet!
  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}

void DoWaWEConfigureWindow(HWND hwndOwner)
{
  SWP swpTemp, swpParent;
  POINTL ptlTemp;
  unsigned short usTabTextLength=0, usTabTextHeight=0;

  // The user wants to configure WaWE!

  // Save old config for Undo!
  memcpy(&WaWEUndoConfiguration, &WaWEConfiguration, sizeof(WaWEConfiguration));

  // For this, we'll create a notebook window.
  // The first page of the notebook will contain the cache settings.
  // Later, it can have more notebook tabs, if required.

#ifdef DEBUG_BUILD
  printf("[DoWaWEConfigureWindow] : Loading dialog from EXE...\n");
#endif

  hwndConfigDlg=WinLoadDlg(HWND_DESKTOP, hwndOwner, (PFNWP)ConfigDlgProc, NULL,
                          DLG_CONFIGURATION, NULL);

  if (!hwndConfigDlg)
  {
#ifdef DEBUG_BUILD
    printf("[DoWaWEConfigureWindow] : Could not load resource!\n");
#endif
    return;
  }

#ifdef DEBUG_BUILD
  printf("[DoWaWEConfigureWindow] : Dialog loaded, others come...\n");
#endif

  // Query the handle of the notebook control
  hwndConfigNB = WinWindowFromID(hwndConfigDlg, NB_CONFIGURATION);

  // Now load and add the pages, one by one...
  hwndCacheNBP = WinLoadDlg(hwndConfigNB, hwndConfigNB, (PFNWP) fnCacheNotebookPage,
                            NULL, DLG_CONFIGURATION_CACHE, NULL);
  hwndInterfaceNBP = WinLoadDlg(hwndConfigNB, hwndConfigNB, (PFNWP) fnInterfaceNotebookPage,
                                NULL, DLG_CONFIGURATION_INTERFACE, NULL);

  // Save the size of one notebook page for later use.
  // (after adding, it will be resized, because of the AUDIOSIZE flag of notebook!)
  WinQueryWindowPos(hwndCacheNBP, &swpTemp);

  // Add pages to notebook:
  AddNotebookTab(hwndConfigNB, hwndInterfaceNBP, BKA_MAJOR, "Interface", "Interface settings", &(ulInterfaceNBPID), &usTabTextLength, &usTabTextHeight);
  AddNotebookTab(hwndConfigNB, hwndCacheNBP, BKA_MAJOR, "Cache", "Read-cache settings", &(ulCacheNBPID), &usTabTextLength, &usTabTextHeight);

  // The followings are discarder on Warp4, but makes things look better on Warp3:

  // Set notebook info field color (default is white... Yipe...)
  WinSendMsg(hwndConfigNB,
             BKM_SETNOTEBOOKCOLORS,
	     MPFROMLONG(SYSCLR_FIELDBACKGROUND), MPFROMSHORT(BKA_BACKGROUNDPAGECOLORINDEX));
  // Adjust tab size:
  WinSendMsg(hwndConfigNB,
             BKM_SETDIMENSIONS,
             MPFROM2SHORT(usTabTextLength + 10, (SHORT)((float)usTabTextHeight * 0.8)),
	     MPFROMSHORT(BKA_MAJORTAB));
  WinSendMsg(hwndConfigNB,
	     BKM_SETDIMENSIONS,
             MPFROM2SHORT(0, 0),
	     MPFROMSHORT(BKA_MINORTAB));


  // Adjust controls and window size
  AdjustControls(swpTemp.cx, swpTemp.cy);

  // Position the window in the middle of its parent!
  WinQueryWindowPos(hwndOwner, &swpParent);
  ptlTemp.x = swpParent.x;
  ptlTemp.y = swpParent.y;
  WinMapWindowPoints(hwndOwner, HWND_DESKTOP, &ptlTemp, 1);
  swpParent.x = ptlTemp.x;
  swpParent.y = ptlTemp.y;
  WinQueryWindowPos(hwndConfigDlg, &swpTemp);
  swpTemp.x = (swpParent.cx-swpTemp.cx)/2 + swpParent.x;
  swpTemp.y = (swpParent.cy-swpTemp.cy)/2 + swpParent.y;
  WinSetWindowPos(hwndConfigDlg, HWND_TOP,
                  swpTemp.x, swpTemp.y,
                  0, 0,
                  SWP_MOVE);

  // Show window
  WinShowWindow(hwndConfigDlg, TRUE);
  WinSetFocus(HWND_DESKTOP, hwndConfigNB);

#ifdef DEBUG_BUILD
  printf("[DoWaWEConfigureWindow] : Processing config window...\n");
#endif
  // Process messages
  WinProcessDlg(hwndConfigDlg);

#ifdef DEBUG_BUILD
  printf("[DoWaWEConfigureWindow] : End of configuration!\n");
#endif

  // Now cleanup
  WinDestroyWindow(hwndConfigDlg);
}


static int GetConfigFileEntry(FILE *hFile, char **ppchKey, char **ppchValue)
{
  int iLineCount, rc;

  *ppchKey = NULL;
  *ppchValue = NULL;

  // Allocate memory for variables we'll return
  *ppchKey = malloc(4096);
  if (!*ppchKey) return 0;

  *ppchValue = malloc(4096);
  if (!*ppchValue)
  {
    free(*ppchKey);
    *ppchKey = NULL;
    return 0;
  }

  // Process every lines of the config file, but
  // max. 1000, so if something goes wrong, wo won't end up an infinite
  // loop here.
  iLineCount = 0;
  while ((!feof(hFile)) && (iLineCount<1000))
  {
    iLineCount++;

    // First find the beginning of the key
    fscanf(hFile, "%*[\n\t= ]");
    // Now get the key
    rc=fscanf(hFile, "%[^\n\t= ]",*ppchKey);
    if ((rc==1) && ((*ppchKey)[0]!=';'))
    { // We have the key! (and not a remark!)
      // Skip the whitespace after the key, if there are any
      fscanf(hFile, "%*[ \t]");
      // There must be a '=' here
      rc=fscanf(hFile, "%[=]", *ppchValue);
      if (rc==1)
      {
        // Ok, the '=' is there
        // Skip the whitespace, if there are any
        fscanf(hFile, "%*[ \t]");
        // and read anything until the end of line
        rc=fscanf(hFile, "%[^\n]\n", *ppchValue);
        if (rc==1)
        {
          // Found a key/value pair!!
          return 1;
        } else fscanf(hFile, "%*[^\n]\n"); // find EOL
      } else fscanf(hFile, "%*[^\n]\n"); // find EOL
    } else fscanf(hFile, "%*[^\n]\n"); // find EOL
  }
  // Ok, skin file processed, no more pairs!

  free(*ppchKey); *ppchKey = NULL;
  free(*ppchValue); *ppchValue = NULL;
  return 0;
}

void LoadWaWEConfig()
{
  char *pchKey, *pchValue;
  FILE *hFile;

  // First of all, set the defaults!
  // -------------------------------

  SetDefaultWaWEConfig();

  // Then try to load the values from the file!
  // ------------------------------------------

  hFile = fopen("WaWE.cfg", "rt");
  if (!hFile)
    return; // Could not open file!

  // Go to the beginning of the file
  fseek(hFile, 0, SEEK_SET);
  // Process every entry of the config file
  pchKey = NULL;
  pchValue = NULL;

  while (GetConfigFileEntry(hFile, &pchKey, &pchValue))
  {
    // Found a key/value pair!
    if (!stricmp(pchKey, "Cache_bUseCache"))
      WaWEConfiguration.bUseCache = atoi(pchValue);
    else
    if (!stricmp(pchKey, "Cache_uiCachePageSize"))
      WaWEConfiguration.uiCachePageSize = atoi(pchValue);
    else
    if (!stricmp(pchKey, "Cache_uiNumOfCachePages"))
      WaWEConfiguration.uiNumOfCachePages = atoi(pchValue);
    else
    if (!stricmp(pchKey, "Interface_iTimeFormat"))
      WaWEConfiguration.iTimeFormat = atoi(pchValue);
    else
    if (!stricmp(pchKey, "Interface_achLastOpenFolder"))
    {
      if (pchValue)
        strncpy(WaWEConfiguration.achLastOpenFolder, pchValue, sizeof(WaWEConfiguration.achLastOpenFolder));
      else
        strcpy(WaWEConfiguration.achLastOpenFolder, "");
    }

    free(pchKey); pchKey = NULL;
    if (pchValue)
    {
      free(pchValue); pchValue = NULL;
    }
  }

  fclose(hFile);
}

void SaveWaWEConfig()
{
  FILE *hFile;

  hFile = fopen("WaWE.cfg", "wt");
  if (!hFile)
    return; // Could not open file!

  // Save our values to the first line!
  fprintf(hFile, "; ----------------------------------------------------------\n");
  fprintf(hFile, "; This is an automatically generated configuration file for\n");
  fprintf(hFile, "; The Warped Wave Editor.\n");
  fprintf(hFile, "; Do not modify it by hand!\n");
  fprintf(hFile, "\n");
  fprintf(hFile, "; ----------------------------------------------------------\n");
  fprintf(hFile, "; Settings for read-cache\n");
  fprintf(hFile, "; ----------------------------------------------------------\n");
  fprintf(hFile, "\n");
  fprintf(hFile, "Cache_bUseCache = %d\n",
          WaWEConfiguration.bUseCache);
  fprintf(hFile, "Cache_uiCachePageSize = %d\n",
          WaWEConfiguration.uiCachePageSize);
  fprintf(hFile, "Cache_uiNumOfCachePages = %d\n",
          WaWEConfiguration.uiNumOfCachePages);
  fprintf(hFile, "\n");
  fprintf(hFile, "; ----------------------------------------------------------\n");
  fprintf(hFile, "; Settings for interface\n");
  fprintf(hFile, "; ----------------------------------------------------------\n");
  fprintf(hFile, "\n");
  fprintf(hFile, "Interface_iTimeFormat = %d\n",
          WaWEConfiguration.iTimeFormat);
  fprintf(hFile, "Interface_achLastOpenFolder = %s\n",
          WaWEConfiguration.achLastOpenFolder);
  fprintf(hFile, "\n");
  fprintf(hFile, "; ----------------------------------------------------------\n");
  fprintf(hFile, "; End of configuration file\n");
  fprintf(hFile, "; ----------------------------------------------------------\n");
  fclose(hFile);
}

