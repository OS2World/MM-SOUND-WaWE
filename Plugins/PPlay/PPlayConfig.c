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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "PPlayConfig.h"
#include "PPlayDART.h"
#include "PPlay-Resources.h"

#define MIN_ACCEPTABLE_FREQ 8000
#define MAX_ACCEPTABLE_FREQ 48000

PluginConfiguration_t PluginConfiguration;

static HWND hwndSetupDlg; // Main window handle to config window
static HWND hwndSetupNB;  // Window handle to the notebook inside the config window
static HWND hwndGeneralNBP; // Window handle to the General settings notebook page
static unsigned long ulGeneralNBPID; // Notebook Page ID
static HWND hwndDARTNBP;    // Window handle to the DART settings notebook page
static unsigned long ulDARTNBPID; // Notebook Page ID
//static HWND hwndUNIAUDNBP;    // Window handle to the UNIAUD settings notebook page
//static unsigned long ulUNIAUDNBPID; // Notebook Page ID

static char achTextUndoBuf[1024]; // Undo buffer for entry fields in General page

int GetConfigFileEntry(FILE *hFile, char **ppchKey, char **ppchValue)
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

static void SetDefaultPluginConfig()
{
  PluginConfiguration.iDeviceToUse = DEVICE_DART;
  PluginConfiguration.bAlwaysConvertToDefaultConfig = 1;
  PluginConfiguration.bSlowMachineMode = 0;;
  PluginConfiguration.iDefaultFreq = 44100;
  PluginConfiguration.iDefaultBits = 16;
  PluginConfiguration.iDefaultChannels = 2;
}

void LoadPluginConfig()
{
  char *pchKey, *pchValue;
  FILE *hFile;

  // First of all, set the defaults!
  // -------------------------------

  // Set the defaults for all sub-modules!
  DART_SetDefaultConfig();
  //UNIAUD_SetDefaultConfig();

  // Then set our own defaults
  SetDefaultPluginConfig();

  // Then try to load the values from the file!
  // ------------------------------------------

  // We'll open PPlay.cfg, because when this function is called,
  // the current directory is "Plugins"!

  hFile = fopen("PPlay.cfg", "rt");
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
    if (!stricmp(pchKey, "General_iDeviceToUse"))
      PluginConfiguration.iDeviceToUse = atoi(pchValue);
    else
    if (!stricmp(pchKey, "General_bAlwaysConvertToDefaultConfig"))
      PluginConfiguration.bAlwaysConvertToDefaultConfig = atoi(pchValue);
    else
    if (!stricmp(pchKey, "General_bSlowMachineMode"))
      PluginConfiguration.bSlowMachineMode = atoi(pchValue);
    else
    if (!stricmp(pchKey, "General_iDefaultFreq"))
      PluginConfiguration.iDefaultFreq = atoi(pchValue);
    else
    if (!stricmp(pchKey, "General_iDefaultBits"))
      PluginConfiguration.iDefaultBits = atoi(pchValue);
    else
    if (!stricmp(pchKey, "General_iDefaultChannels"))
      PluginConfiguration.iDefaultChannels = atoi(pchValue);

    free(pchKey); pchKey = NULL;
    free(pchValue); pchValue = NULL;
  }

  // Ok, now that the general stuffs are here, load the submodule specific stuffs!
  DART_LoadConfig(hFile);
  // UNIAUD_LoadConfig(hFile);

  fclose(hFile);
}

void SavePluginConfig()
{
  FILE *hFile;

  // We'll open "Plugins\PPlay.cfg", because when this function is called,
  // the current directory is not "Plugins"!
  hFile = fopen("Plugins\\PPlay.cfg", "wt");
  if (!hFile)
    return; // Could not open file!

  // Save our values to the first line!
  fprintf(hFile, "; ----------------------------------------------------------\n");
  fprintf(hFile, "; This is an automatically generated configuration file for\n");
  fprintf(hFile, "; Playback Plugin for WaWE.\n");
  fprintf(hFile, "; Do not modify it by hand!\n");
  fprintf(hFile, "\n");
  fprintf(hFile, "; ----------------------------------------------------------\n");
  fprintf(hFile, "; General configuration\n");
  fprintf(hFile, "; ----------------------------------------------------------\n");
  fprintf(hFile, "\n");
  fprintf(hFile, "General_iDeviceToUse = %d\n",
          PluginConfiguration.iDeviceToUse);
  fprintf(hFile, "General_bAlwaysConvertToDefaultConfig = %d\n",
          PluginConfiguration.bAlwaysConvertToDefaultConfig);
  fprintf(hFile, "General_bSlowMachineMode = %d\n",
          PluginConfiguration.bSlowMachineMode);
  fprintf(hFile, "General_iDefaultFreq = %d\n",
          PluginConfiguration.iDefaultFreq);
  fprintf(hFile, "General_iDefaultBits = %d\n",
          PluginConfiguration.iDefaultBits);
  fprintf(hFile, "General_iDefaultChannels = %d\n",
          PluginConfiguration.iDefaultChannels);

  // Then save the values of the submodules!
  DART_SaveConfig(hFile);
  // UNIAUD_SaveConfig(hFile);

  fprintf(hFile, "\n");
  fprintf(hFile, "; ----------------------------------------------------------\n");
  fprintf(hFile, "; End of configuration file\n");
  fprintf(hFile, "; ----------------------------------------------------------\n");
  fclose(hFile);
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
  if (WinQueryWindowPos(WinWindowFromID(hwndSetupDlg, PB_DEFAULT), &swpTemp))
    buttonheight = swpTemp.cy;
  else
    buttonheight = 0;

  // Set the size and position of Notebook

  // The size is interesting. We should know how much additional space this notebook requires.
  // For this, we calculate how big is a notebook for a known client size (e.g., 0x0)

  rectl.xLeft = 0; rectl.xRight = 0;
  rectl.yBottom = 0; rectl.yTop = 0;

  WinSendMsg(hwndSetupNB, BKM_CALCPAGERECT, MPFROMP(&rectl), MPFROMSHORT(FALSE));
  // Ok, this must be added to client size in order to be able to display the given client dialog:
  NBPwidth += rectl.xRight;
  NBPheight += rectl.yTop;

  // Now check how much shift is inside the window:
  // (client/notebook shift)
  pointl.x = 0;
  pointl.y = 0;
  WinMapWindowPoints(hwndGeneralNBP, hwndSetupNB, &pointl, 1);
  // Add this too:
  NBPwidth += pointl.x;
  NBPheight += pointl.y;

  // Notebook position is above the buttons, which is some pixels above the bottom of window:
  swpTemp.x = CXDLGFRAME+2;
  swpTemp.y = CYDLGFRAME + buttonheight + 5;
  // Notebook size is just calculated:
  swpTemp.cx = NBPwidth;
  swpTemp.cy = NBPheight;
  WinSetWindowPos(hwndSetupNB, HWND_TOP,
		  swpTemp.x, swpTemp.y,
		  swpTemp.cx, swpTemp.cy,
		  SWP_MOVE | SWP_SIZE);

  // Ok, the notebook is resized. Now resize the dialog window to be just
  // as big as it's needed for the notebook:
  xsize = swpTemp.cx + CXDLGFRAME*2 + 2*2;
  ysize = swpTemp.cy + CYDLGFRAME*2 + buttonheight + 5 + WinQuerySysValue(HWND_DESKTOP, SV_CYTITLEBAR) + 5;
  WinSetWindowPos(hwndSetupDlg, HWND_TOP,
		  0, 0,
		  xsize,
                  ysize,
		  SWP_SIZE);

  // Set the position of buttons
  //  Default Button
  WinQueryWindowPos(WinWindowFromID(hwndSetupDlg, PB_DEFAULT), &swpTemp);
  swpTemp.y = CYDLGFRAME+2;
  swpTemp.x = (xsize/4)-swpTemp.cx/2;
  WinSetWindowPos(WinWindowFromID(hwndSetupDlg, PB_DEFAULT), HWND_TOP,
		  swpTemp.x, swpTemp.y,
		  swpTemp.cx, swpTemp.cy,
		  SWP_MOVE);
  //  Close Button
  WinQueryWindowPos(WinWindowFromID(hwndSetupDlg, PB_HELP), &swpTemp);
  swpTemp.y = CYDLGFRAME+2;
  swpTemp.x = (xsize/4)*3-swpTemp.cx/2;
  WinSetWindowPos(WinWindowFromID(hwndSetupDlg, PB_HELP), HWND_TOP,
		  swpTemp.x, swpTemp.y,
		  swpTemp.cx, swpTemp.cy,
		  SWP_MOVE);

  // That's all!
}

MRESULT EXPENTRY SetupDlgProc( HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2 )
{
  switch (msg) {
    case WM_INITDLG:
      break;
    case WM_CLOSE:
      // Save the configuration info!
      SavePluginConfig();
      break;
    case WM_COMMAND:
      switch SHORT1FROMMP(mp2) {
	case CMDSRC_PUSHBUTTON:           // ---- A WM_COMMAND from a pushbutton ------
	  switch (SHORT1FROMMP(mp1)) {
            case PB_DEFAULT:                             // Default button pressed
#ifdef DEBUG_BUILD
              printf("[SETUP] Default button pressed!\n");
#endif

              WinPostMsg(hwndGeneralNBP, WM_SETDEFAULTVALUES, NULL, NULL);
              WinPostMsg(hwndDARTNBP, WM_SETDEFAULTVALUES, NULL, NULL);
              // WinPostMsg(hwndUNIAUDNBP_WM_SETDEFAULTVALUES, NULL, NULL);
              return (MRESULT) TRUE;
            case PB_HELP:                             // Help button pressed
#ifdef DEBUG_BUILD
              printf("[SETUP] Help button pressed!\n");
#endif
              // TODO!
	      return (MRESULT) TRUE;
	    default:
	      break;
	  }
	  break; // End of CMDSRC_PUSHBUTTON processing
      } // end of switch inside WM_COMMAND
      break;
    default:
      break;
  } // end of switch of msg

  // Call the default procedure if it has not been processed yet!
  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}

MRESULT EXPENTRY fnGeneralNotebookPage( HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2 )
{
  char sTemp[256];
  int iTemp, iToSelect, iItemCount;

  switch (msg)
  {
    case WM_SETDEFAULTVALUES:
      SetDefaultPluginConfig();

      // Now set stuffs
      // ---
      iItemCount = (int) WinSendMsg(WinWindowFromID(hwnd, CX_SOUNDDRIVER), LM_QUERYITEMCOUNT, NULL, NULL);
      iToSelect = 0;
      for (iTemp = 0; iTemp<iItemCount; iTemp++)
      {
        if (PluginConfiguration.iDeviceToUse == (int) WinSendMsg(WinWindowFromID(hwnd, CX_SOUNDDRIVER), LM_QUERYITEMHANDLE, MPFROMSHORT(iTemp), (MPARAM) NULL))
          iToSelect = iTemp;
      }
      WinSendMsg(WinWindowFromID(hwnd, CX_SOUNDDRIVER), LM_SELECTITEM,
                 (MPARAM) (iToSelect), (MPARAM) (TRUE));

      // ---
      iItemCount = (int) WinSendMsg(WinWindowFromID(hwnd, CX_BITS), LM_QUERYITEMCOUNT, NULL, NULL);
      iToSelect = 0;
      for (iTemp = 0; iTemp<iItemCount; iTemp++)
      {
        if (PluginConfiguration.iDefaultBits == (int) WinSendMsg(WinWindowFromID(hwnd, CX_BITS), LM_QUERYITEMHANDLE, MPFROMSHORT(iTemp), (MPARAM) NULL))
          iToSelect = iTemp;
      }
      WinSendMsg(WinWindowFromID(hwnd, CX_BITS), LM_SELECTITEM,
                 (MPARAM) (iToSelect), (MPARAM) (TRUE));

      // ---
      iItemCount = (int) WinSendMsg(WinWindowFromID(hwnd, CX_CHANNELS), LM_QUERYITEMCOUNT, NULL, NULL);
      iToSelect = 0;
      for (iTemp = 0; iTemp<iItemCount; iTemp++)
      {
        if (PluginConfiguration.iDefaultChannels == (int) WinSendMsg(WinWindowFromID(hwnd, CX_CHANNELS), LM_QUERYITEMHANDLE, MPFROMSHORT(iTemp), (MPARAM) NULL))
          iToSelect = iTemp;
      }
      WinSendMsg(WinWindowFromID(hwnd, CX_CHANNELS), LM_SELECTITEM,
                 (MPARAM) (iToSelect), (MPARAM) (TRUE));

      // ---
      WinSetDlgItemText(hwnd, EF_FREQUENCY, (MPARAM) itoa(PluginConfiguration.iDefaultFreq, sTemp, 10));

      // ---
      // Checkbox of always use default
      WinSendMsg(WinWindowFromID(hwnd, CB_ALWAYSUSETHEDEFAULTOUTPUTFORMAT), BM_SETCHECK,
                 (MPARAM) (PluginConfiguration.bAlwaysConvertToDefaultConfig),
                 (MPARAM) (0));

      // Checkbox of Slow machine mode
      WinSendMsg(WinWindowFromID(hwnd, CB_SLOWMACHINEMODE), BM_SETCHECK,
                 (MPARAM) (PluginConfiguration.bSlowMachineMode),
                 (MPARAM) (0));

      return (MRESULT) TRUE;
    case WM_INITDLG:
      // Initialization of this page

      // Fill the sound driver combo with list of available sound drivers

      iToSelect = 0;

      // DART driver
      iTemp = (int) WinSendMsg(WinWindowFromID(hwnd, CX_SOUNDDRIVER), LM_INSERTITEM,
                               (MPARAM) LIT_END, (MPARAM) ("DART driver"));
      WinSendMsg(WinWindowFromID(hwnd, CX_SOUNDDRIVER), LM_SETITEMHANDLE,
                 (MPARAM) iTemp, (MPARAM) DEVICE_DART);
      if (PluginConfiguration.iDeviceToUse == DEVICE_DART) iToSelect = iTemp;

//      // UNIAUD driver
//      iTemp = (int) WinSendMsg(WinWindowFromID(hwnd, CX_SOUNDDRIVER), LM_INSERTITEM,
//                               (MPARAM) LIT_END, (MPARAM) ("UniAUD driver"));
//      WinSendMsg(WinWindowFromID(hwnd, CX_SOUNDDRIVER), LM_SETITEMHANDLE,
//                 (MPARAM) iTemp, (MPARAM) DEVICE_UNIAUD);
//      if (PluginConfiguration.iDeviceToUse == DEVICE_UNIAUD) iToSelect = iTemp;

      // No Sound driver
      iTemp = (int) WinSendMsg(WinWindowFromID(hwnd, CX_SOUNDDRIVER), LM_INSERTITEM,
                               (MPARAM) LIT_END, (MPARAM) ("No Sound"));
      WinSendMsg(WinWindowFromID(hwnd, CX_SOUNDDRIVER), LM_SETITEMHANDLE,
                 (MPARAM) iTemp, (MPARAM) DEVICE_NOSOUND);
      if (PluginConfiguration.iDeviceToUse == DEVICE_NOSOUND) iToSelect = iTemp;

      // Then select the current one!
      WinSendMsg(WinWindowFromID(hwnd, CX_SOUNDDRIVER), LM_SELECTITEM,
                 (MPARAM) (iToSelect), (MPARAM) (TRUE));

      // Set frequency:
      WinSetDlgItemText(hwnd, EF_FREQUENCY, (MPARAM) itoa(PluginConfiguration.iDefaultFreq, sTemp, 10));


      // Fill the number of bits combo

      iToSelect = 0;

      iTemp = (int) WinSendMsg(WinWindowFromID(hwnd, CX_BITS), LM_INSERTITEM,
                               (MPARAM) LIT_END, (MPARAM) ("8 bits"));
      WinSendMsg(WinWindowFromID(hwnd, CX_BITS), LM_SETITEMHANDLE,
                 (MPARAM) iTemp, (MPARAM) 8);
      if (PluginConfiguration.iDefaultBits == 8) iToSelect = iTemp;

      iTemp = (int) WinSendMsg(WinWindowFromID(hwnd, CX_BITS), LM_INSERTITEM,
                               (MPARAM) LIT_END, (MPARAM) ("16 bits"));
      WinSendMsg(WinWindowFromID(hwnd, CX_BITS), LM_SETITEMHANDLE,
                 (MPARAM) iTemp, (MPARAM) 16);
      if (PluginConfiguration.iDefaultBits == 16) iToSelect = iTemp;

      iTemp = (int) WinSendMsg(WinWindowFromID(hwnd, CX_BITS), LM_INSERTITEM,
                               (MPARAM) LIT_END, (MPARAM) ("24 bits"));
      WinSendMsg(WinWindowFromID(hwnd, CX_BITS), LM_SETITEMHANDLE,
                 (MPARAM) iTemp, (MPARAM) 24);
      if (PluginConfiguration.iDefaultBits == 24) iToSelect = iTemp;

      iTemp = (int) WinSendMsg(WinWindowFromID(hwnd, CX_BITS), LM_INSERTITEM,
                               (MPARAM) LIT_END, (MPARAM) ("32 bits"));
      WinSendMsg(WinWindowFromID(hwnd, CX_BITS), LM_SETITEMHANDLE,
                 (MPARAM) iTemp, (MPARAM) 32);
      if (PluginConfiguration.iDefaultBits == 32) iToSelect = iTemp;

      // Then select the current one!
      WinSendMsg(WinWindowFromID(hwnd, CX_BITS), LM_SELECTITEM,
                 (MPARAM) (iToSelect), (MPARAM) (TRUE));

      // Fill the number of channels combo

      iToSelect = 0;

      iTemp = (int) WinSendMsg(WinWindowFromID(hwnd, CX_CHANNELS), LM_INSERTITEM,
                               (MPARAM) LIT_END, (MPARAM) ("1 channel (Mono)"));
      WinSendMsg(WinWindowFromID(hwnd, CX_CHANNELS), LM_SETITEMHANDLE,
                 (MPARAM) iTemp, (MPARAM) 1);
      if (PluginConfiguration.iDefaultChannels == 1) iToSelect = iTemp;

      iTemp = (int) WinSendMsg(WinWindowFromID(hwnd, CX_CHANNELS), LM_INSERTITEM,
                               (MPARAM) LIT_END, (MPARAM) ("2 channels (Stereo)"));
      WinSendMsg(WinWindowFromID(hwnd, CX_CHANNELS), LM_SETITEMHANDLE,
                 (MPARAM) iTemp, (MPARAM) 2);
      if (PluginConfiguration.iDefaultChannels == 2) iToSelect = iTemp;

      iTemp = (int) WinSendMsg(WinWindowFromID(hwnd, CX_CHANNELS), LM_INSERTITEM,
                               (MPARAM) LIT_END, (MPARAM) ("3 channels"));
      WinSendMsg(WinWindowFromID(hwnd, CX_CHANNELS), LM_SETITEMHANDLE,
                 (MPARAM) iTemp, (MPARAM) 3);
      if (PluginConfiguration.iDefaultChannels == 3) iToSelect = iTemp;

      iTemp = (int) WinSendMsg(WinWindowFromID(hwnd, CX_CHANNELS), LM_INSERTITEM,
                               (MPARAM) LIT_END, (MPARAM) ("4 channels"));
      WinSendMsg(WinWindowFromID(hwnd, CX_CHANNELS), LM_SETITEMHANDLE,
                 (MPARAM) iTemp, (MPARAM) 4);
      if (PluginConfiguration.iDefaultChannels == 4) iToSelect = iTemp;

      iTemp = (int) WinSendMsg(WinWindowFromID(hwnd, CX_CHANNELS), LM_INSERTITEM,
                               (MPARAM) LIT_END, (MPARAM) ("5 channels"));
      WinSendMsg(WinWindowFromID(hwnd, CX_CHANNELS), LM_SETITEMHANDLE,
                 (MPARAM) iTemp, (MPARAM) 5);
      if (PluginConfiguration.iDefaultChannels == 5) iToSelect = iTemp;

      iTemp = (int) WinSendMsg(WinWindowFromID(hwnd, CX_CHANNELS), LM_INSERTITEM,
                               (MPARAM) LIT_END, (MPARAM) ("6 channels"));
      WinSendMsg(WinWindowFromID(hwnd, CX_CHANNELS), LM_SETITEMHANDLE,
                 (MPARAM) iTemp, (MPARAM) 6);
      if (PluginConfiguration.iDefaultChannels == 6) iToSelect = iTemp;

      iTemp = (int) WinSendMsg(WinWindowFromID(hwnd, CX_CHANNELS), LM_INSERTITEM,
                               (MPARAM) LIT_END, (MPARAM) ("7 channels"));
      WinSendMsg(WinWindowFromID(hwnd, CX_CHANNELS), LM_SETITEMHANDLE,
                 (MPARAM) iTemp, (MPARAM) 7);
      if (PluginConfiguration.iDefaultChannels == 7) iToSelect = iTemp;

      iTemp = (int) WinSendMsg(WinWindowFromID(hwnd, CX_CHANNELS), LM_INSERTITEM,
                               (MPARAM) LIT_END, (MPARAM) ("8 channels"));
      WinSendMsg(WinWindowFromID(hwnd, CX_CHANNELS), LM_SETITEMHANDLE,
                 (MPARAM) iTemp, (MPARAM) 8);
      if (PluginConfiguration.iDefaultChannels == 8) iToSelect = iTemp;

      // Then select the current one!
      WinSendMsg(WinWindowFromID(hwnd, CX_CHANNELS), LM_SELECTITEM,
                 (MPARAM) (iToSelect), (MPARAM) (TRUE));

      // Checkbox of always use default
      WinSendMsg(WinWindowFromID(hwnd, CB_ALWAYSUSETHEDEFAULTOUTPUTFORMAT), BM_SETCHECK,
                 (MPARAM) (PluginConfiguration.bAlwaysConvertToDefaultConfig),
                 (MPARAM) (0));

      // Checkbox of Slow machine mode
      WinSendMsg(WinWindowFromID(hwnd, CB_SLOWMACHINEMODE), BM_SETCHECK,
                 (MPARAM) (PluginConfiguration.bSlowMachineMode),
                 (MPARAM) (0));
      break;
    case WM_CONTROL:  // ------------------- WM_CONTROL messages ---------------------------
      switch SHORT1FROMMP(mp1) {
	case EF_FREQUENCY:           // - - - - - - - Message from Frequency entry field - - - - - - -
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
		if ((iTemp>=MIN_ACCEPTABLE_FREQ) && (iTemp<=MAX_ACCEPTABLE_FREQ))
		{
		  PluginConfiguration.iDefaultFreq = iTemp;
		  WinSetDlgItemText(hwnd, SHORT1FROMMP(mp1), (MPARAM) itoa(iTemp, sTemp, 10));
		}
                else
		  WinSetDlgItemText(hwnd, SHORT1FROMMP(mp1), achTextUndoBuf);

		break;
	      }
	    default:
	      break;
          }
          return (MPARAM) TRUE;
	  break; // End of EF_FREQUENCY processing
          // -------------------------------- Combo boxes --------------------
	case CX_SOUNDDEVICE:
	  if (SHORT2FROMMP(mp1) == LN_SELECT)
          {
            int iItem;
	    iItem = (int) (WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1),
                                             LM_QUERYSELECTION, (MPARAM) LIT_CURSOR, 0));
            PluginConfiguration.iDeviceToUse =
              (int) (WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1),
                                             LM_QUERYITEMHANDLE, (MPARAM) iItem, 0));
	  }
	  break;
	case CX_BITS:
	  if (SHORT2FROMMP(mp1) == LN_SELECT)
          {
            int iItem;
	    iItem = (int) (WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1),
                                             LM_QUERYSELECTION, (MPARAM) LIT_CURSOR, 0));
            PluginConfiguration.iDefaultBits =
              (int) (WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1),
                                             LM_QUERYITEMHANDLE, (MPARAM) iItem, 0));
	  }
	  break;
	case CX_CHANNELS:
	  if (SHORT2FROMMP(mp1) == LN_SELECT)
          {
            int iItem;
	    iItem = (int) (WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1),
                                             LM_QUERYSELECTION, (MPARAM) LIT_CURSOR, 0));
            PluginConfiguration.iDefaultChannels =
              (int) (WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1),
                                             LM_QUERYITEMHANDLE, (MPARAM) iItem, 0));
	  }
	  break;
          // -------------------------------- Checkboxes --------------------
	case CB_ALWAYSUSETHEDEFAULTOUTPUTFORMAT:
	  if (SHORT2FROMMP(mp1) == BN_CLICKED)
	  {
            PluginConfiguration.bAlwaysConvertToDefaultConfig =
              WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1));
          }
          break;
        case CB_SLOWMACHINEMODE:
	  if (SHORT2FROMMP(mp1) == BN_CLICKED)
	  {
            PluginConfiguration.bSlowMachineMode =
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

void DoConfigureWindow(HWND hwndOwner, HMODULE hmodOurDLLHandle)
{
  SWP swpTemp, swpParent;
  POINTL ptlTemp;
  unsigned short usTabTextLength=0, usTabTextHeight=0;

  // The user wants to configure this plugin.

  // For this, we'll create a notebook window.
  // The first page of the notebook will contain the general
  // settings, and there will be a new page for every submodule,
  // like DART or UNIAUD.

#ifdef DEBUG_BUILD
  printf("[DoConfigureWindow] : Loading dialog from DLL... (handle = %d)\n", hmodOurDLLHandle);
#endif

  hwndSetupDlg=WinLoadDlg(HWND_DESKTOP, hwndOwner, (PFNWP)SetupDlgProc, hmodOurDLLHandle,
                          DLG_SETUPWINDOW, NULL);

  if (!hwndSetupDlg)
  {
#ifdef DEBUG_BUILD
    printf("[DoConfigureWindow] : Could not load resource!\n");
#endif
    return;
  }

#ifdef DEBUG_BUILD
  printf("[DoConfigureWindow] : Dialog loaded, others come...\n");
#endif

  // Query the handle of the notebook control
  hwndSetupNB = WinWindowFromID(hwndSetupDlg, NBK_SETUPNOTEBOOK);

  // Now load and add the pages, one by one...
  hwndGeneralNBP = WinLoadDlg(hwndSetupNB, hwndSetupNB, (PFNWP) fnGeneralNotebookPage,
                              hmodOurDLLHandle, DLG_GENERALCONFIGDIALOG, NULL);

  hwndDARTNBP    = WinLoadDlg(hwndSetupNB, hwndSetupNB, (PFNWP) fnDARTNotebookPage,
                              hmodOurDLLHandle, DLG_DARTCONFIGDIALOG, NULL);

//  hwndUNIAUDNBP  = WinLoadDlg(hwndSetupNB, hwndSetupNB, (PFNWP) fnUNIAUDNotebookPage,
//                              hmodOurDLLHandle, DLG_UNIAUDCONFIGDIALOG, NULL);

  // Save the size of one notebook page for later use.
  // (after adding, it will be resized, because of the AUDIOSIZE flag of notebook!)
  WinQueryWindowPos(hwndGeneralNBP, &swpTemp);

  // Add pages to notebook:
  AddNotebookTab(hwndSetupNB, hwndGeneralNBP, BKA_MAJOR, "General", "General settings", &(ulGeneralNBPID), &usTabTextLength, &usTabTextHeight);
  AddNotebookTab(hwndSetupNB, hwndDARTNBP, BKA_MAJOR, "DART", "Configuration of DART", &(ulDARTNBPID), &usTabTextLength, &usTabTextHeight);
//  AddNotebookTab(hwndSetupNB, hwndUNIAUDNBP, BKA_MAJOR, "UniAUD", "Configuration of UniAUD", &(ulUNIAUDNBPID), &usTabTextLength, &usTabTextHeight);

  // The followings are discarder on Warp4, but makes things look better on Warp3:

  // Set notebook info field color (default is white... Yipe...)
  WinSendMsg(hwndSetupNB,
             BKM_SETNOTEBOOKCOLORS,
	     MPFROMLONG(SYSCLR_FIELDBACKGROUND), MPFROMSHORT(BKA_BACKGROUNDPAGECOLORINDEX));
  // Adjust tab size:
  WinSendMsg(hwndSetupNB,
             BKM_SETDIMENSIONS,
             MPFROM2SHORT(usTabTextLength + 10, (SHORT)((float)usTabTextHeight * 0.8)),
	     MPFROMSHORT(BKA_MAJORTAB));
  WinSendMsg(hwndSetupNB,
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
  WinQueryWindowPos(hwndSetupDlg, &swpTemp);
  swpTemp.x = (swpParent.cx-swpTemp.cx)/2 + swpParent.x;
  swpTemp.y = (swpParent.cy-swpTemp.cy)/2 + swpParent.y;
  WinSetWindowPos(hwndSetupDlg, HWND_TOP,
                  swpTemp.x, swpTemp.y,
                  0, 0,
                  SWP_MOVE);

  // Show window
  WinShowWindow(hwndSetupDlg, TRUE);
  WinSetFocus(HWND_DESKTOP, hwndSetupNB);

#ifdef DEBUG_BUILD
  printf("[DoConfigureWindow] : Processing config window...\n");
#endif
  // Process messages
  WinProcessDlg(hwndSetupDlg);

#ifdef DEBUG_BUILD
  printf("[DoConfigureWindow] : End of configuration!\n");
#endif

  // Now cleanup
  WinDestroyWindow(hwndSetupDlg);
}

