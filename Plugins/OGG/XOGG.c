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
 * OGG Vorbis export plugin                                                  *
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <process.h>
#include <string.h>
#include <time.h>

#define INCL_TYPES
#define INCL_WIN
#define INCL_DOSRESOURCES
#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>

#include "vorbis\vorbisenc.h"
#include "OGG-Resources.h"

#include "WaWECommon.h"    // Common defines and structures


// HACK HACK!
// For some (still unknown) reason, the ogg encoder crashes if
// this hack is not enabled... Huh.
#define USE_OGGENCSETUP_HACK

// Global variables
// (Per process, because the DLL is initinstance/terminstance!)
HMODULE hmodOurDLLHandle;
char achUndoBuf[512]; // Undo buffer for entry fields
WaWEPluginHelpers_t PluginHelpers;

char *WinStateSave_Name = "WaWE";
char *WinStateSave_Key_ExportDialog  = "ExportPlugin_OGG_ExportPP"; // Presentation parameters for window
char *WinStateSave_Key_CommentDialog  = "ExportPlugin_OGG_CommentPP"; // Presentation parameters for window

char *pchPluginName = "OGG Vorbis Export plugin";
char *pchAboutTitle = "About OGG Vorbis Export plugin";
char *pchAboutMessage =
  "OGG Vorbis Export plugin v1.0 for WaWE\n"
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
                Msg, "OGG Vorbis Export Plugin Error", 0, MB_OK | MB_MOVEABLE | MB_ICONEXCLAMATION);
}

typedef struct _OGGEncHandle
{
  ogg_stream_state os;
  vorbis_info vi;
  vorbis_comment vc;
  vorbis_dsp_state vd;
  vorbis_block vb;
  ogg_page og;
  ogg_packet op;

  FILE *hFile;
  WaWEExP_Create_Desc_t FormatDesc;

} OGGEncHandle_t, *OGGEncHandle_p;

#ifdef USE_OGGENCSETUP_HACK
void OGGEnc_Setup_Hack()
{
  ogg_stream_state os; /* take physical pages, weld into a logical
			  stream of packets */
 
  vorbis_info      vi; /* struct that stores all the static vorbis bitstream
			  settings */
  vorbis_comment   vc; /* struct that stores all the user comments */

  vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
  vorbis_block     vb; /* local working space for packet->PCM decode */

  int ret;

  /********** Encode setup ************/
  vorbis_info_init(&vi);
  ret=vorbis_encode_init_vbr(&vi,2,44100,.5);

  if(ret) return;

  /* add a comment */
  vorbis_comment_init(&vc);

  /* set up the analysis state and auxiliary encoding storage */
  vorbis_analysis_init(&vd,&vi);
  vorbis_block_init(&vd,&vb);
  
  /* set up our packet->stream encoder */
  /* pick a random serial number; that way we can more likely build
     chained streams just by concatenation */
  srand(time(NULL));
  ogg_stream_init(&os,rand());

  /* clean up and exit.  vorbis_info_clear() must be called last */
  ogg_stream_clear(&os);
  vorbis_block_clear(&vb);
  vorbis_dsp_clear(&vd);
  vorbis_comment_clear(&vc);
  vorbis_info_clear(&vi);
}
#endif

void ResizeCommentDialogControls(HWND hwnd)
{
  SWP Swp1, Swp2, Swp3;
  ULONG CXDLGFRAME, CYDLGFRAME, CYTITLEBAR;
  int iGBHeight, iStaticTextHeight, iEntryFieldHeight;
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
  WinSetWindowPos(WinWindowFromID(hwnd, PB_OK), HWND_TOP,
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

  // The Comment groupbox
  WinQueryWindowPos(WinWindowFromID(hwnd, PB_OK), &Swp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_COMMENTNAME), &Swp3);
  iStaticTextHeight = Swp3.cy;
  iEntryFieldHeight = iStaticTextHeight + CYDLGFRAME;
  iGBHeight = Swp1.cy - CYTITLEBAR - CYDLGFRAME - CYDLGFRAME - CYDLGFRAME -
    Swp2.cy - Swp2.y - CYDLGFRAME;
  WinSetWindowPos(WinWindowFromID(hwnd, GB_COMMENT), HWND_TOP,
                  2*CXDLGFRAME,
                  Swp1.cy - 2*CYDLGFRAME - CYTITLEBAR - iGBHeight,
                  Swp1.cx - 4*CXDLGFRAME,
                  iGBHeight,
                  SWP_MOVE | SWP_SIZE);

  // The static texts fields inside the groupbox
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_COMMENT), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_COMMENTNAME), HWND_TOP,
                  Swp2.x + CXDLGFRAME,
                  Swp2.y + Swp2.cy - iStaticTextHeight - CYDLGFRAME - iEntryFieldHeight,
                  0, 0,
                  SWP_MOVE);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_COMMENTNAME), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_COMMENTVALUE), HWND_TOP,
                  Swp2.x,
                  Swp2.y - CYDLGFRAME - iEntryFieldHeight,
                  0, 0,
                  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_COMMENTNAME), &Swp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_COMMENT), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, EF_COMMENTNAME), HWND_TOP,
                  Swp2.x + Swp2.cx + 2*CXDLGFRAME,
                  Swp2.y,
                  Swp3.cx - (Swp2.x + Swp2.cx + 2*CXDLGFRAME) - CXDLGFRAME,
                  iEntryFieldHeight,
                  SWP_MOVE | SWP_SIZE);

  WinSetWindowPos(WinWindowFromID(hwnd, MLE_COMMENTVALUE), HWND_TOP,
                  Swp2.x + Swp2.cx + 2*CXDLGFRAME,
                  Swp3.y + 2*CYDLGFRAME,
                  Swp3.cx - (Swp2.x + Swp2.cx + 2*CXDLGFRAME) - CXDLGFRAME,
                  Swp3.cy - 2*CYDLGFRAME - Swp3.y - CYDLGFRAME,
                  SWP_MOVE | SWP_SIZE);


  if (iDisabledWindowUpdate)
    WinEnableWindowUpdate(hwnd, TRUE);
}

MRESULT EXPENTRY OGGCommentDialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  switch (msg)
  {
    case WM_FORMATFRAME:
      {
	ResizeCommentDialogControls(hwnd);
        break;
      }
      break;
    default:
      break;
  } // End of switch msg

  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}


void ResizeExportDialogControls(HWND hwnd)
{
  SWP Swp1, Swp2, Swp3;
  ULONG CXDLGFRAME, CYDLGFRAME, CYTITLEBAR;
  int iGBHeight, iStaticTextHeight;
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
  WinSetWindowPos(WinWindowFromID(hwnd, PB_OK), HWND_TOP,
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
  // The OGG format settings groupbox
  WinQueryWindowPos(WinWindowFromID(hwnd, EF_BITRATEUPPER), &Swp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_BITRATEUPPER), &Swp3);
  iStaticTextHeight = Swp3.cy;
  iGBHeight = 3*(Swp2.cy+CYDLGFRAME) + Swp3.cy + CYDLGFRAME;
  WinSetWindowPos(WinWindowFromID(hwnd, GB_GENERALOGGFORMATSETTINGS), HWND_TOP,
                  2*CXDLGFRAME,
                  Swp1.cy - 2*CYDLGFRAME - CYTITLEBAR - iGBHeight,
                  Swp1.cx - 4*CXDLGFRAME,
                  iGBHeight,
                  SWP_MOVE | SWP_SIZE);
  // Position the entry fields inside the groupbox
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_GENERALOGGFORMATSETTINGS), &Swp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, EF_BITRATEUPPER), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, EF_BITRATEUPPER), HWND_TOP,
                  Swp2.x + Swp2.cx - CXDLGFRAME - Swp3.cx,
                  Swp2.y + 2*Swp3.cy + 4*CYDLGFRAME,
                  0, 0,
                  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, EF_BITRATENOMINAL), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, EF_BITRATENOMINAL), HWND_TOP,
                  Swp2.x + Swp2.cx - CXDLGFRAME - Swp3.cx,
                  Swp2.y + 1*Swp3.cy + 3*CYDLGFRAME,
                  0, 0,
                  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, EF_BITRATELOWER), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, EF_BITRATELOWER), HWND_TOP,
                  Swp2.x + Swp2.cx - CXDLGFRAME - Swp3.cx,
                  Swp2.y + 2*CYDLGFRAME,
                  0, 0,
                  SWP_MOVE);

  // Position the static text in line with the entry fields
  WinQueryWindowPos(WinWindowFromID(hwnd, EF_BITRATEUPPER), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_BITRATEUPPER), HWND_TOP,
                  Swp2.x + CXDLGFRAME,
                  Swp3.y,
                  0, 0,
                  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, EF_BITRATENOMINAL), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_BITRATENOMINAL), HWND_TOP,
                  Swp2.x + CXDLGFRAME,
                  Swp3.y,
                  0, 0,
                  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, EF_BITRATELOWER), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_BITRATELOWER), HWND_TOP,
                  Swp2.x + CXDLGFRAME,
                  Swp3.y,
                  0, 0,
                  SWP_MOVE);


  // The OGG Comments groupbox
  WinQueryWindowPos(WinWindowFromID(hwnd, PB_CANCEL), &Swp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_GENERALOGGFORMATSETTINGS), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, GB_OGGCOMMENTS), HWND_TOP,
                  2*CXDLGFRAME,
                  Swp2.y + Swp2.cy + 2*CYDLGFRAME,
                  Swp1.cx - 4*CXDLGFRAME,
                  Swp3.y - CYDLGFRAME - (Swp2.y + Swp2.cy + 2*CYDLGFRAME),
                  SWP_MOVE | SWP_SIZE);

  // The stuffs inside the OGG Comments groupbox:
  // First the New Modify Delete buttons at the bottom
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_OGGCOMMENTS), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, PB_NEW), HWND_TOP,
                  Swp2.x + CXDLGFRAME,
                  Swp2.y + CYDLGFRAME,
                  0, 0,
                  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, PB_MODIFY), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, PB_MODIFY), HWND_TOP,
                  Swp2.x + CXDLGFRAME + (Swp2.cx - 2*CXDLGFRAME -Swp3.cx)/2,
                  Swp2.y + CYDLGFRAME,
                  0, 0,
                  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, PB_DELETE), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, PB_DELETE), HWND_TOP,
                  Swp2.x + Swp2.cx - CXDLGFRAME - Swp3.cx,
                  Swp2.y + CYDLGFRAME,
                  0, 0,
                  SWP_MOVE);
  // Then the Up and Down buttons
  WinQueryWindowPos(WinWindowFromID(hwnd, PB_MOVEUP), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, PB_MOVEUP), HWND_TOP,
                  Swp2.x + Swp2.cx - CXDLGFRAME - Swp3.cx,
                  Swp2.y + Swp2.cy - CYDLGFRAME - iStaticTextHeight - CYDLGFRAME - Swp3.cy,
                  0, 0,
                  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, PB_DELETE), &Swp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, PB_MOVEUP), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, PB_MOVEDOWN), HWND_TOP,
                  Swp3.x,
                  Swp2.y + Swp2.cy + 2*CYDLGFRAME,
                  0, 0,
                  SWP_MOVE);

  // And at last, the listbox (LB_COMMENTS)
  WinQueryWindowPos(WinWindowFromID(hwnd, PB_NEW), &Swp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, PB_MOVEUP), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, LB_COMMENTS), HWND_TOP,
                  Swp2.x,
                  Swp2.y + Swp2.cy + 2*CYDLGFRAME,
                  Swp3.x - CXDLGFRAME - Swp2.x,
                  Swp3.y + Swp3.cy - (Swp2.y + Swp2.cy + 2*CYDLGFRAME),
                  SWP_MOVE | SWP_SIZE);


  if (iDisabledWindowUpdate)
    WinEnableWindowUpdate(hwnd, TRUE);
}

MRESULT EXPENTRY ExportDialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  char achTemp[512];
  int iCurrSelection, iMaxSelection;
  HWND hwndDlg;
  SWP swpDlg;
  SWP swpParent;

  switch (msg)
  {
    case WM_INITDLG:
      {
	// Clear entry fields
	WinSendDlgItemMsg(hwnd, EF_BITRATEUPPER,
			  EM_CLEAR,
			  (MPARAM) 0,
                          (MPARAM) 0);
	WinSendDlgItemMsg(hwnd, EF_BITRATENOMINAL,
			  EM_CLEAR,
			  (MPARAM) 0,
			  (MPARAM) 0);
	WinSendDlgItemMsg(hwnd, EF_BITRATELOWER,
			  EM_CLEAR,
			  (MPARAM) 0,
			  (MPARAM) 0);
	// Clear listbox
	WinSendDlgItemMsg(hwnd, LB_COMMENTS,
			  LM_DELETEALL,
			  (MPARAM) 0,
                          (MPARAM) 0);
        // Ok, stuffs initialized.
	return (MRESULT) 0;
      }
    case WM_FORMATFRAME:
      {
	ResizeExportDialogControls(hwnd);
        break;
      }
    case WM_CONTROL:
      // Check who sent the WM_CONTROL message!
      switch (SHORT1FROMMP(mp1))
      {
	case EF_BITRATEUPPER:
	case EF_BITRATENOMINAL:
	case EF_BITRATELOWER:
	  // It's one of the entry fields, which can take
	  // only numbers.
	  if (SHORT2FROMMP(mp1) == EN_SETFOCUS)
	  {
            WinQueryDlgItemText(hwnd, SHORT1FROMMP(mp1), sizeof(achUndoBuf), achUndoBuf);
	  } else
	  if (SHORT2FROMMP(mp1) == EN_KILLFOCUS)
	  {
            int iValue;
	    WinQueryDlgItemText(hwnd, SHORT1FROMMP(mp1), sizeof(achTemp), achTemp);
	    iValue = atoi(achTemp);
	    if ((iValue<0) || (iValue>1024000))
              WinSetDlgItemText(hwnd, SHORT1FROMMP(mp1), achUndoBuf);
	  } 
	  break;
	default:
          break;
      }
      break;
    case WM_COMMAND:
      // Check who sent the WM_COMMAND message!
      switch (SHORT1FROMMP(mp2)) // usSource parameter of WM_COMMAND
      {
        case CMDSRC_PUSHBUTTON:
          // Hmm, it's a pushbutton! Let's see which one!
          switch ((ULONG) mp1)
          {
            case PB_MOVEUP:
              // Move the currently selected text up!
              // Get the current selection first!
              iCurrSelection = (int) WinSendDlgItemMsg(hwnd, LB_COMMENTS,
                                                       LM_QUERYSELECTION, (MPARAM) LIT_CURSOR, NULL);
              if (iCurrSelection == LIT_NONE)
              {
                ErrorMsg("Please select a comment to move up!");
              } else
              if (iCurrSelection>0) // We cannot move the topmost item upper...
              {
                char *pchText;
                int iLength;
                iLength = (int) WinSendDlgItemMsg(hwnd, LB_COMMENTS,
                                                  LM_QUERYITEMTEXTLENGTH, (MPARAM) iCurrSelection, NULL);
                pchText = (char *) malloc(iLength+1);
                if (!pchText)
                {
                  ErrorMsg("Out of memory!\nOperation cancelled.");
                } else
                {
                  // Get item text
                  WinSendDlgItemMsg(hwnd, LB_COMMENTS,
                                    LM_QUERYITEMTEXT, MPFROM2SHORT(iCurrSelection, iLength+1), (MPARAM) pchText);
                  // Delete item
                  WinSendDlgItemMsg(hwnd, LB_COMMENTS,
                                    LM_DELETEITEM, (MPARAM) iCurrSelection, NULL);
                  // Insert one line upper!
                  iCurrSelection = (int) WinSendDlgItemMsg(hwnd, LB_COMMENTS,
                                                           LM_INSERTITEM, (MPARAM) (iCurrSelection-1), (MPARAM) pchText);
                  // And make that the current selection!
                  WinSendDlgItemMsg(hwnd, LB_COMMENTS,
                                    LM_SELECTITEM, (MPARAM) iCurrSelection, (MPARAM) TRUE);
                  
                  free(pchText);
                }
              }
              return (MPARAM) 1;
            case PB_MOVEDOWN:
              // Move the currently selected text down!
              // Get the maximum selection index number!
              iMaxSelection = (int) WinSendDlgItemMsg(hwnd, LB_COMMENTS,
                                                      LM_QUERYITEMCOUNT, NULL, NULL);
              // Get the current selection!
              iCurrSelection = (int) WinSendDlgItemMsg(hwnd, LB_COMMENTS,
                                                       LM_QUERYSELECTION, (MPARAM) LIT_CURSOR, NULL);
              if (iCurrSelection == LIT_NONE)
              {
                ErrorMsg("Please select a comment to move down!");
              } else
              if (iCurrSelection<iMaxSelection-1)
              {
                char *pchText;
                int iLength;
                iLength = (int) WinSendDlgItemMsg(hwnd, LB_COMMENTS,
                                                  LM_QUERYITEMTEXTLENGTH, (MPARAM) iCurrSelection, NULL);
                pchText = (char *) malloc(iLength+1);
                if (!pchText)
                {
                  ErrorMsg("Out of memory!\nOperation cancelled.");
                } else
                {
                  // Get item text
                  WinSendDlgItemMsg(hwnd, LB_COMMENTS,
                                    LM_QUERYITEMTEXT, MPFROM2SHORT(iCurrSelection, iLength+1), (MPARAM) pchText);
                  // Delete item
                  WinSendDlgItemMsg(hwnd, LB_COMMENTS,
                                    LM_DELETEITEM, (MPARAM) iCurrSelection, NULL);
                  // Insert one line upper!
                  iCurrSelection = (int) WinSendDlgItemMsg(hwnd, LB_COMMENTS,
                                                           LM_INSERTITEM, (MPARAM) (iCurrSelection+1), (MPARAM) pchText);
                  // And make that the current selection!
                  WinSendDlgItemMsg(hwnd, LB_COMMENTS,
                                    LM_SELECTITEM, (MPARAM) iCurrSelection, (MPARAM) TRUE);

                  free(pchText);
                }
              }
              return (MPARAM) 1;
            case PB_NEW:
              // Have to create a new dialog window
              hwndDlg = WinLoadDlg(HWND_DESKTOP,
                                   hwnd, // Owner
                                   OGGCommentDialogProc, // Dialog proc
                                   hmodOurDLLHandle, // Source module handle
                                   DLG_OGGCOMMENT,
                                   NULL);
              if (!hwndDlg)
              {
                ErrorMsg("Could not create dialog window!");
              } else
              {
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
                WinRestoreWindowPos(WinStateSave_Name, WinStateSave_Key_CommentDialog, hwndDlg);
                // Now that we have the size and position of the dialog,
                // we can reposition and resize the controls inside it, so it
                // will look nice in every resolution.
                ResizeCommentDialogControls(hwndDlg);

                WinSetWindowPos(hwndDlg, HWND_TOP, 0, 0, 0, 0,
                                SWP_SHOW | SWP_ACTIVATE | SWP_ZORDER);
                WinSetFocus(HWND_DESKTOP, hwndDlg);

                // Set initial values in dialog window:
                WinSetDlgItemText(hwndDlg, EF_COMMENTNAME, "NEWCOMMENT");
                WinSetDlgItemText(hwndDlg, MLE_COMMENTVALUE, "WaWE Rulez!");

                // Process the dialog!
                if (WinProcessDlg(hwndDlg)==PB_OK)
                {
                  // The user pressed OK!
                  // Get settings from the window
                  int iNameLen, iValueLen;
                  char *pchText;

                  iNameLen = WinQueryDlgItemTextLength(hwndDlg, EF_COMMENTNAME);
                  iValueLen = WinQueryDlgItemTextLength(hwndDlg, MLE_COMMENTVALUE);

                  pchText = (char *) malloc(iNameLen+iValueLen+1+1);
                  if (!pchText)
                    ErrorMsg("Out of memory!\nAction cancelled.\n");
                  else
                  {
                    // Get values from dialog box
                    WinQueryDlgItemText(hwndDlg, EF_COMMENTNAME, iNameLen+1, pchText);
                    pchText[iNameLen] = '=';
                    WinQueryDlgItemText(hwndDlg, MLE_COMMENTVALUE, iValueLen+1, pchText+iNameLen+1);
                    // Ok, we have the string. Now insert it into out listbox!
                    iCurrSelection = (int) WinSendDlgItemMsg(hwnd, LB_COMMENTS,
                                                             LM_INSERTITEM, (MPARAM) LIT_END, (MPARAM) pchText);
                    // And make that the current selection!
                    WinSendDlgItemMsg(hwnd, LB_COMMENTS,
                                      LM_SELECTITEM, (MPARAM) iCurrSelection, (MPARAM) TRUE);

                    free(pchText);
                  }
                }
                // Now save size and position info into INI file
                WinStoreWindowPos(WinStateSave_Name, WinStateSave_Key_CommentDialog, hwndDlg);
                WinDestroyWindow(hwndDlg);
              }
              return (MRESULT) 1;
            case PB_MODIFY:
              // Get the current selection!
              iCurrSelection = (int) WinSendDlgItemMsg(hwnd, LB_COMMENTS,
                                                       LM_QUERYSELECTION, (MPARAM) LIT_CURSOR, NULL);
              if (iCurrSelection == LIT_NONE)
              {
                ErrorMsg("Please select a comment to modify!");
              } else
              {
                char *pchComment, *pchValue;
                int iLength;
                // Get the text of selected comment!
                iLength = (int) WinSendDlgItemMsg(hwnd, LB_COMMENTS,
                                                  LM_QUERYITEMTEXTLENGTH, (MPARAM) iCurrSelection, NULL);
                pchComment = (char *) malloc(iLength+1);
                if (!pchComment)
                {
                  ErrorMsg("Out of memory!\nOperation cancelled.");
                } else
                {
                  // Get item text
                  WinSendDlgItemMsg(hwnd, LB_COMMENTS,
                                    LM_QUERYITEMTEXT, MPFROM2SHORT(iCurrSelection, iLength+1), (MPARAM) pchComment);

                  // Find the '=' sign!
                  pchValue = pchComment;
                  while ((*pchValue) && (*pchValue!='=')) pchValue++;
                  if (*pchValue=='=')
                  {
                    *pchValue=0;
                    pchValue++;
                  }

                  // Okay, we have the comment string and the value string.

                  // Have to create a new dialog window
                  hwndDlg = WinLoadDlg(HWND_DESKTOP,
                                       hwnd, // Owner
                                       OGGCommentDialogProc, // Dialog proc
                                       hmodOurDLLHandle, // Source module handle
                                       DLG_OGGCOMMENT,
                                       NULL);
                  if (!hwndDlg)
                  {
                    ErrorMsg("Could not create dialog window!");
                  } else
                  {
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
                    WinRestoreWindowPos(WinStateSave_Name, WinStateSave_Key_CommentDialog, hwndDlg);
                    // Now that we have the size and position of the dialog,
                    // we can reposition and resize the controls inside it, so it
                    // will look nice in every resolution.
                    ResizeCommentDialogControls(hwndDlg);

                    WinSetWindowPos(hwndDlg, HWND_TOP, 0, 0, 0, 0,
                                    SWP_SHOW | SWP_ACTIVATE | SWP_ZORDER);
                    WinSetFocus(HWND_DESKTOP, hwndDlg);

                    // Set initial values in dialog window:
                    WinSetDlgItemText(hwndDlg, EF_COMMENTNAME, pchComment);
                    WinSetDlgItemText(hwndDlg, MLE_COMMENTVALUE, pchValue);

                    // Process the dialog!
                    if (WinProcessDlg(hwndDlg)==PB_OK)
                    {
                      // The user pressed OK!
                      // Get settings from the window
                      int iNameLen, iValueLen;
                      char *pchText;

                      iNameLen = WinQueryDlgItemTextLength(hwndDlg, EF_COMMENTNAME);
                      iValueLen = WinQueryDlgItemTextLength(hwndDlg, MLE_COMMENTVALUE);

                      pchText = (char *) malloc(iNameLen+iValueLen+1+1);
                      if (!pchText)
                        ErrorMsg("Out of memory!\nAction cancelled.\n");
                      else
                      {
                        // Get values from dialog box
                        WinQueryDlgItemText(hwndDlg, EF_COMMENTNAME, iNameLen+1, pchText);
                        pchText[iNameLen] = '=';
                        WinQueryDlgItemText(hwndDlg, MLE_COMMENTVALUE, iValueLen+1, pchText+iNameLen+1);
                        // Ok, we have the string. Now change the old one to this one!
                        WinSendDlgItemMsg(hwnd, LB_COMMENTS,
                                          LM_SETITEMTEXT, (MPARAM) iCurrSelection, (MPARAM) pchText);
                        free(pchText);
                      }
                    }
                    // Now save size and position info into INI file
                    WinStoreWindowPos(WinStateSave_Name, WinStateSave_Key_CommentDialog, hwndDlg);
                    WinDestroyWindow(hwndDlg);
                  }
                  free(pchComment);
                }
              }
              return (MRESULT) 1;
            case PB_DELETE:
              // The currently selected text should be deleted!
              // Get the current selection!
              iCurrSelection = (int) WinSendDlgItemMsg(hwnd, LB_COMMENTS,
                                                       LM_QUERYSELECTION, (MPARAM) LIT_CURSOR, NULL);
              if (iCurrSelection == LIT_NONE)
              {
                ErrorMsg("Please select a comment to delete!");
              } else
              {
                if (WinMessageBox(HWND_DESKTOP, hwnd,
                                  "Are you sure you want to delete the selected comment?", "Delete comment", 0, MB_YESNO | MB_MOVEABLE | MB_ICONQUESTION)==MBID_YES)
                {
                  // Delete item
                  WinSendDlgItemMsg(hwnd, LB_COMMENTS,
                                    LM_DELETEITEM, (MPARAM) iCurrSelection, NULL);

                }
              }
              return (MPARAM) 1;
            default:
              break;
          }
          break;
        default:
          // It's not a pushbutton...
          break;
      } // End of switch for usSource parameter of WM_COMMAND
      break;
    default:
      break;
  } // End of switch msg
  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}


// Functions called from outside:

#define CheckAndGetKeyword(pchKeyword, pchOGGKeyword) \
  i = 0; \
  while (WAWEHELPER_NOERROR==PluginHelpers.fn_WaWEHelper_ChannelSetFormat_GetKeySize(&(pFormatDesc->ChannelSetFormat), \
                                                                                     pchKeyword, i, \
                                                                                     &iStrSize)) \
  { \
    char *pchComment; \
 \
    pchComment = (char *) malloc(strlen(pchOGGKeyword)+1+iStrSize); \
    if (pchComment) \
    { \
      /* Add this keyword as a comment! */ \
      sprintf(pchComment, "%s=", pchOGGKeyword); \
      /* Read the value for this key */ \
      rc = PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKey(&(pFormatDesc->ChannelSetFormat), \
                                                                pchKeyword, i, \
                                                                pchComment+strlen(pchOGGKeyword)+1, \
                                                                iStrSize); \
      if (rc!=WAWEHELPER_NOERROR) \
      { \
        /* Error at readkey! */ \
        free(pchComment); \
      } else \
      { \
        /* Okay, all is done! */ \
        /* Add this pchComment string to comments! */ \
 \
        iNumOfComments++; \
        if (!ppchComments) \
        { \
          ppchComments = (char **) malloc(sizeof(char *) * iNumOfComments); \
          ppchComments[iNumOfComments-1] = pchComment; \
        } else \
        { \
          char **ppchNewComments; \
          ppchNewComments = (char **) realloc(ppchComments, sizeof(char *) * iNumOfComments); \
          if (ppchNewComments)\
          { \
            ppchComments = ppchNewComments; \
            ppchComments[iNumOfComments-1] = pchComment; \
          } \
        } \
      } \
    } \
    i++; \
  }

#define CheckAndGetCommentKeyword(pchKeyword, pchOGGKeyword) \
  i = 0; \
  while (WAWEHELPER_NOERROR==PluginHelpers.fn_WaWEHelper_ChannelSetFormat_GetKeySize(&(pFormatDesc->ChannelSetFormat), \
                                                                                     pchKeyword, i, \
                                                                                     &iStrSize)) \
  { \
    char *pchComment; \
 \
    pchComment = (char *) malloc(strlen(pchOGGKeyword)+1+iStrSize); \
    if (pchComment) \
    { \
      /* Add this keyword as a comment! */ \
      sprintf(pchComment, "%s=", pchOGGKeyword); \
      /* Read the value for this key */ \
      rc = PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKey(&(pFormatDesc->ChannelSetFormat), \
                                                                pchKeyword, i, \
                                                                pchComment+strlen(pchOGGKeyword)+1, \
                                                                iStrSize); \
      if (rc!=WAWEHELPER_NOERROR) \
      { \
        /* Error at readkey! */ \
        free(pchComment); \
      } else \
      { \
        /* Okay, all is done! */ \
        /* Add this pchComment string to comments! */ \
 \
        iNumOfComments++; \
        if (!ppchComments) \
        { \
          ppchComments = (char **) malloc(sizeof(char *) * iNumOfComments); \
          ppchComments[iNumOfComments-1] = pchComment; \
        } else \
        { \
          char **ppchNewComments; \
          ppchNewComments = (char **) realloc(ppchComments, sizeof(char *) * iNumOfComments); \
          if (ppchNewComments)\
          { \
            ppchComments = ppchNewComments; \
            ppchComments[iNumOfComments-1] = pchComment; \
          } \
        } \
      } \
    } \
    i++; \
  }


void *                WAWECALL plugin_Create(char *pchFileName,
                                             WaWEExP_Create_Desc_p pFormatDesc,
                                             HWND hwndOwner)
{
  OGGEncHandle_p pHandle;
  char achBuffer[1024];

  int iBitrate_Upper = 0;
  int iBitrate_Nominal = 128000;
  int iBitrate_Lower = 0;
  int iNumOfComments = 0;
  char **ppchComments = NULL;
  int rc;
  int i;
  int iStrSize;
  ogg_packet header;
  ogg_packet header_comm;
  ogg_packet header_code;
  HWND dlg;
  SWP DlgSwap, ParentSwap;


  // Extract extended format info from FormatDesc

  // First, get bitrate info
  rc = PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKey(&(pFormatDesc->ChannelSetFormat),
                                                            "bitrate", 0,
                                                            achBuffer, sizeof(achBuffer));
  if (rc==WAWEHELPER_NOERROR)
  {
#ifdef DEBUG_BUILD
    printf("[plugin_Create] : Found bitrate\n");
#endif
    iBitrate_Nominal = atoi(achBuffer);
  }

  rc = PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKey(&(pFormatDesc->ChannelSetFormat),
                                                            "bitrate/nominal bitrate", 0,
                                                            achBuffer, sizeof(achBuffer));
  if (rc==WAWEHELPER_NOERROR)
  {
#ifdef DEBUG_BUILD
    printf("[plugin_Create] : Found nominal bitrate\n");
#endif
    iBitrate_Nominal = atoi(achBuffer);
  }

  rc = PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKey(&(pFormatDesc->ChannelSetFormat),
                                                            "bitrate/upper bitrate", 0,
                                                            achBuffer, sizeof(achBuffer));
  if (rc==WAWEHELPER_NOERROR)
  {
#ifdef DEBUG_BUILD
    printf("[plugin_Create] : Found upper bitrate\n");
#endif
    iBitrate_Upper = atoi(achBuffer);
  }

  rc = PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKey(&(pFormatDesc->ChannelSetFormat),
                                                            "bitrate/lower bitrate", 0,
                                                            achBuffer, sizeof(achBuffer));
  if (rc==WAWEHELPER_NOERROR)
  {
#ifdef DEBUG_BUILD
    printf("[plugin_Create] : Found lower bitrate\n");
#endif
    iBitrate_Lower = atoi(achBuffer);
  }

  // Then get other known keywords
  CheckAndGetKeyword("title", "TITLE");
  CheckAndGetKeyword("artist", "ARTIST");
  CheckAndGetKeyword("album", "ALBUM");
  CheckAndGetKeyword("track number", "TRACKNUMBER");
  CheckAndGetKeyword("performer", "PERFORMER");
  CheckAndGetKeyword("copyright", "COPYRIGHT");
  CheckAndGetKeyword("license", "LICENSE");
  CheckAndGetKeyword("description", "DESCRIPTION");
  CheckAndGetKeyword("genre", "GENRE");
  CheckAndGetKeyword("date", "DATE");
  CheckAndGetKeyword("location", "LOCATION");
  CheckAndGetKeyword("contact", "CONTACT");
  CheckAndGetKeyword("ISRC", "ISRC");
  CheckAndGetCommentKeyword("comment", "COMMENT");

  // Now we should show a dialog window to user to ask if these
  // settings are Ok for him!

#ifdef DEBUG_BUILD
  printf("[plugin_Create] : I should use the following settings to save OGG file:\n");
  printf("    iBitrate_Upper   = %d\n", iBitrate_Upper);
  printf("    iBitrate_Nominal = %d\n", iBitrate_Nominal);
  printf("    iBitrate_Lower   = %d\n", iBitrate_Lower);
  printf("    iNumOfComments   = %d\n", iNumOfComments);
  if (iNumOfComments>0)
  {
    printf("    Comments:\n");
    for (i=0; i<iNumOfComments; i++)
    {
      printf("      [%s]\n", ppchComments[i]);
    }
  }
  printf("[plugin_Create] : Asking user if these will be ok...\n");
#endif


  // Dialog window

  dlg = WinLoadDlg(HWND_DESKTOP,
                   hwndOwner, // Owner will be the WaWE app itself
                   ExportDialogProc, // Dialog proc
                   hmodOurDLLHandle, // Source module handle
                   DLG_OGGFORMATSETTINGS,
                   NULL);
  if (!dlg)
  {
    ErrorMsg("Could not create dialog window!");

    // Clean up
    if (ppchComments)
    {
      for (i=0; i<iNumOfComments; i++)
        free(ppchComments[i]);
    
      free(ppchComments); ppchComments = NULL;
    }
    return NULL;
  }

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
  WinRestoreWindowPos(WinStateSave_Name, WinStateSave_Key_ExportDialog, dlg);
  // Now that we have the size and position of the dialog,
  // we can reposition and resize the controls inside it, so it
  // will look nice in every resolution.
  ResizeExportDialogControls(dlg);

  WinSetWindowPos(dlg, HWND_TOP, 0, 0, 0, 0,
                  SWP_SHOW | SWP_ACTIVATE | SWP_ZORDER);
  WinSetFocus(HWND_DESKTOP, dlg);

  // Set initial values in dialog window:
  // - Entry fields
  WinSetDlgItemText(dlg, EF_BITRATEUPPER, itoa(iBitrate_Upper, achBuffer, 10));
  WinSetDlgItemText(dlg, EF_BITRATENOMINAL, itoa(iBitrate_Nominal, achBuffer, 10));
  WinSetDlgItemText(dlg, EF_BITRATELOWER, itoa(iBitrate_Lower, achBuffer, 10));
  // - Listbox
  WinSendDlgItemMsg(dlg, LB_COMMENTS, LM_DELETEALL, NULL, NULL);
  for (i=0; i<iNumOfComments; i++)
    WinSendDlgItemMsg(dlg, LB_COMMENTS, LM_INSERTITEM, (MPARAM) LIT_END, (MPARAM) ppchComments[i]);

  // Process the dialog!
  if (WinProcessDlg(dlg)!=PB_OK)
  {
    // The user pressed cancel!
#ifdef DEBUG_BUILD
    printf("[plugin_Create] : The user has pressed cancel.\n");
#endif

    // Clean up
    if (ppchComments)
    {
      for (i=0; i<iNumOfComments; i++)
        free(ppchComments[i]);

      free(ppchComments); ppchComments = NULL;
    }
    // Now save size and position info into INI file
    WinStoreWindowPos(WinStateSave_Name, WinStateSave_Key_ExportDialog, dlg);
    WinDestroyWindow(dlg);
    return NULL;
  }

  // Get settings from the window
  WinQueryDlgItemText(dlg, EF_BITRATEUPPER, sizeof(achBuffer), achBuffer);
  iBitrate_Upper = atoi(achBuffer);
  WinQueryDlgItemText(dlg, EF_BITRATENOMINAL, sizeof(achBuffer), achBuffer);
  iBitrate_Nominal = atoi(achBuffer);
  WinQueryDlgItemText(dlg, EF_BITRATELOWER, sizeof(achBuffer), achBuffer);
  iBitrate_Lower = atoi(achBuffer);

  // Rebuild ppchComments from listbox!
  if (ppchComments)
  {
    for (i=0; i<iNumOfComments; i++)
      free(ppchComments[i]);

    free(ppchComments); ppchComments = NULL;
  }
  iNumOfComments = 0;

  iNumOfComments = (int) WinSendDlgItemMsg(dlg, LB_COMMENTS, LM_QUERYITEMCOUNT, NULL, NULL);
  if (iNumOfComments==0)
  {
    ppchComments = NULL;
  } else
  {
    ppchComments = (char **) malloc(sizeof(char *) * iNumOfComments);
    if (!ppchComments)
    {
      // Yikes, not enough memory to allocate list of comments!
      ErrorMsg("Not enough memory to allocate list of comments!\nComments will not be saved.");
      iNumOfComments = 0;
    } else
    {
      for (i=0; i<iNumOfComments; i++)
      {
        int iLen;
        iLen = (int) WinSendDlgItemMsg(dlg, LB_COMMENTS, LM_QUERYITEMTEXTLENGTH, (MPARAM) i, NULL);
        if (iLen>0)
        {
          ppchComments[i] = (char *) malloc(iLen+1);
          if (ppchComments[i])
            WinSendDlgItemMsg(dlg, LB_COMMENTS, LM_QUERYITEMTEXT, MPFROM2SHORT(i, sizeof(achBuffer)), (MPARAM) ppchComments[i]);
        }
      }
    }
  }

  // Now save size and position info into INI file
  WinStoreWindowPos(WinStateSave_Name, WinStateSave_Key_ExportDialog, dlg);
  WinDestroyWindow(dlg);

#ifdef DEBUG_BUILD
  printf("[plugin_Create] : Will use the following settings to save OGG file:\n");
  printf("    iBitrate_Upper   = %d\n", iBitrate_Upper);
  printf("    iBitrate_Nominal = %d\n", iBitrate_Nominal);
  printf("    iBitrate_Lower   = %d\n", iBitrate_Lower);
  printf("    iNumOfComments   = %d\n", iNumOfComments);
  if (iNumOfComments>0)
  {
    printf("    Comments:\n");
    for (i=0; i<iNumOfComments; i++)
    {
      printf("      [%s]\n", ppchComments[i]);
    }
  }
#endif



  // Ok, settings are done then. Now, try to open file,
  // and set up vorbis encoder!

#ifdef USE_OGGENCSETUP_HACK
  // This has to be called from this thread... by experience.
  OGGEnc_Setup_Hack();
#endif

  pHandle = (OGGEncHandle_p) malloc(sizeof(OGGEncHandle_t));
  if (pHandle)
  {

    // First try to open the file
    pHandle->hFile = fopen(pchFileName, "wb+");
    if (!(pHandle->hFile))
    {
      ErrorMsg("Could not create file!\n");
      free(pHandle);
      return NULL;
    }

    // Ok, file opened for writing!
    // Now prepare vorbis!

#ifdef DEBUG_BUILD
//    printf("  vorbis_info_init()\n"); fflush(stdout);
#endif
    vorbis_info_init(&(pHandle->vi));

#ifdef DEBUG_BUILD
//    printf("  vorbis_encode_init()\n"); fflush(stdout);
#endif

    rc = vorbis_encode_init(&(pHandle->vi),
                            pFormatDesc->iChannels,
                            pFormatDesc->iFrequency,
                            iBitrate_Upper,
                            iBitrate_Nominal,
                            iBitrate_Lower);
    if (rc)
    {
      ErrorMsg("Error while initializing encoder!");
      free(pHandle);
      fclose(pHandle->hFile);
      return NULL;
    }

    // Setup comments
#ifdef DEBUG_BUILD
//    printf("  Setting up comments\n"); fflush(stdout);
#endif
    vorbis_comment_init(&(pHandle->vc));
    for (i=0; i<iNumOfComments; i++)
      vorbis_comment_add(&(pHandle->vc), ppchComments[i]);

    // Setup the analysis state and auxiliary encodeing storage
#ifdef DEBUG_BUILD
//    printf("  Setting up analysis\n"); fflush(stdout);
#endif
    vorbis_analysis_init(&(pHandle->vd), &(pHandle->vi));
#ifdef DEBUG_BUILD
//    printf("  Setting up block\n"); fflush(stdout);
#endif
    vorbis_block_init(&(pHandle->vd), &(pHandle->vb));

    // Setup packet->stream encoder
#ifdef DEBUG_BUILD
//    printf("  Initializing ogg stream\n"); fflush(stdout);
#endif
    srand(time(NULL));
    ogg_stream_init(&(pHandle->os), rand());

    // Create the three standard headers
#ifdef DEBUG_BUILD
//    printf("  Creating standard headers\n"); fflush(stdout);
#endif
    vorbis_analysis_headerout(&(pHandle->vd),
                              &(pHandle->vc),
                              &header,
                              &header_comm,
                              &header_code);
    ogg_stream_packetin(&(pHandle->os), &header);
    ogg_stream_packetin(&(pHandle->os), &header_comm);
    ogg_stream_packetin(&(pHandle->os), &header_code);

    // Write out headers
#ifdef DEBUG_BUILD
//    printf("  Writing out header\n"); fflush(stdout);
#endif
    rc = ogg_stream_flush(&(pHandle->os), &(pHandle->og));
    while (rc)
    {
      fwrite((pHandle->og).header, 1, (pHandle->og).header_len, pHandle->hFile);
      fwrite((pHandle->og).body, 1, (pHandle->og).body_len, pHandle->hFile);
      rc = ogg_stream_flush(&(pHandle->os), &(pHandle->og));
    }

    memcpy(&(pHandle->FormatDesc), pFormatDesc, sizeof(WaWEExP_Create_Desc_t));

    // Ok, all we should now do is to write out the packets, which should be
    // done from plugin_Write()!

  } else
  {
    ErrorMsg("Out of memory!");
  }

  // Clean up
  if (ppchComments)
  {
    for (i=0; i<iNumOfComments; i++)
      free(ppchComments[i]);

    free(ppchComments); ppchComments = NULL;
  }

  return (void *) pHandle;
}

WaWESize_t            WAWECALL plugin_Write(void *pHandle,
                                            char *pchBuffer,
                                            WaWESize_t cBytes)
{
  OGGEncHandle_p pOGGHandle;
  float **buffer;
  int iSamples, i, c;
  int iNumOfChannels, iIsSigned;
  WaWESize_t Written;

  if (!pHandle) return 0;

  pOGGHandle = (OGGEncHandle_p) pHandle;

  Written = 0;
  iSamples = cBytes / ((pOGGHandle->FormatDesc.iBits+7)/8) / pOGGHandle->FormatDesc.iChannels;

#ifdef DEBUG_BUILD
//  printf(" W: vorbis_analysis_buffer\n"); fflush(stdout);
#endif

  buffer = vorbis_analysis_buffer(&(pOGGHandle->vd), iSamples);
  if (buffer)
  {
    iNumOfChannels = pOGGHandle->FormatDesc.iChannels;
    iIsSigned = pOGGHandle->FormatDesc.iIsSigned;

#ifdef DEBUG_BUILD
//  printf(" W: Converting\n"); fflush(stdout);
#endif

    switch (pOGGHandle->FormatDesc.iBits)
    {
      case 8:
	{
	  signed char chOneSample;

	  for (i=0; i<iSamples; i++)
	    for (c=0; c<iNumOfChannels; c++)
	    {
	      chOneSample = (signed char) (pchBuffer[i * iNumOfChannels + c]);
	      if (!iIsSigned)
		chOneSample += 128;

	      buffer[c][i] = chOneSample / 128.f;
	    }
	  break;
	}
      default:
      case 16:
	{
	  signed short int sOneSample;
	  signed short int *psiBuffer = (signed short int *)pchBuffer;

	  for (i=0; i<iSamples; i++)
	    for (c=0; c<iNumOfChannels; c++)
	    {
	      sOneSample = (signed short int) (psiBuffer[i * iNumOfChannels + c]);
	      if (!iIsSigned)
		sOneSample += 32768;

	      buffer[c][i] = sOneSample / 32768.f;
	    }
	  break;
	}
      case 24:
	{
	  signed long lOneSample;
	  float middle = 1<<23;

	  for (i=0; i<iSamples; i++)
	    for (c=0; c<iNumOfChannels; c++)
	    {
	      lOneSample = 0xff & ((signed long) (pchBuffer[i * iNumOfChannels * 3 + c*3]));
	      lOneSample |= 0xff00 & (((signed long) (pchBuffer[i * iNumOfChannels * 3 + c*3+1]))<<8);
	      lOneSample |= 0xff0000 & (((signed long) (pchBuffer[i * iNumOfChannels * 3 + c*3+2]))<<16);

	      if (!iIsSigned)
		lOneSample += (1<<23);

	      buffer[c][i] = lOneSample / middle;
	    }
	  break;
	}
      case 32:
	{
	  signed long lOneSample;
	  signed long *plBuffer = (signed long *) pchBuffer;
	  float middle = 1<<31;

	  for (i=0; i<iSamples; i++)
	    for (c=0; c<iNumOfChannels; c++)
	    {
	      lOneSample = plBuffer[i * iNumOfChannels + c];

	      if (!iIsSigned)
		lOneSample += (1<<31);

	      buffer[c][i] = lOneSample / middle;
	    }
	  break;
	}
    }
#ifdef DEBUG_BUILD
//  printf(" W: vorbis_analysis_wrote, iSamples = %d\n", (int) iSamples); fflush(stdout);
#endif

    vorbis_analysis_wrote(&(pOGGHandle->vd), iSamples);
    while (vorbis_analysis_blockout(&(pOGGHandle->vd), &(pOGGHandle->vb))==1)
    {
#ifdef DEBUG_BUILD
//  printf(" W: vorbis_analysis and addblock\n"); fflush(stdout);
#endif
      vorbis_analysis(&(pOGGHandle->vb), NULL);
      vorbis_bitrate_addblock(&(pOGGHandle->vb));

      while (vorbis_bitrate_flushpacket(&(pOGGHandle->vd), &(pOGGHandle->op)))
      {
#ifdef DEBUG_BUILD
//        printf(" W: bytes = %d\n", pOGGHandle->op.bytes); fflush(stdout);
#endif
        ogg_stream_packetin(&(pOGGHandle->os), &(pOGGHandle->op));

        while (!ogg_page_eos(&(pOGGHandle->og)))
        {
          if (ogg_stream_pageout(&(pOGGHandle->os), &(pOGGHandle->og))==0) break;

          Written += fwrite(pOGGHandle->og.header, 1, pOGGHandle->og.header_len, pOGGHandle->hFile);
          Written += fwrite(pOGGHandle->og.body, 1, pOGGHandle->og.body_len, pOGGHandle->hFile);
        }
#ifdef DEBUG_BUILD
//        printf(" W: written %d bytes\n", (int) Written); fflush(stdout);
#endif

      }
    }
  }
  return cBytes;
}

long                  WAWECALL plugin_Close(void *pHandle, WaWEChannelSet_p pSavedChannelSet)
{
  OGGEncHandle_p pOGGHandle;

  if (!pHandle) return 0;

  pOGGHandle = (OGGEncHandle_p) pHandle;

  vorbis_analysis_wrote(&(pOGGHandle->vd), 0);
  ogg_stream_clear(&(pOGGHandle->os));
  vorbis_block_clear(&(pOGGHandle->vb));
  vorbis_dsp_clear(&(pOGGHandle->vd));
  vorbis_comment_clear(&(pOGGHandle->vc));
  vorbis_info_clear(&(pOGGHandle->vi));

  fclose(pOGGHandle->hFile);

  free(pOGGHandle);

  if (pSavedChannelSet)
  {
    // Modify channel-set format, set FourCC to "OGG"!
    PluginHelpers.fn_WaWEHelper_ChannelSetFormat_DeleteKeysAndSubkeys(&(pSavedChannelSet->ChannelSetFormat),
                                                                      "FourCC");
    PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pSavedChannelSet->ChannelSetFormat),
                                                          "FourCC",
                                                          "OGG");
  }

  return 1;
}

long                  WAWECALL plugin_IsFormatSupported(WaWEExP_Create_Desc_p pFormatDesc)
{
  char achTemp[64];
  int rc;

  rc = PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKey(&(pFormatDesc->ChannelSetFormat),
                                                            "FourCC", 0,
                                                            achTemp, sizeof(achTemp));
#ifdef DEBUG_BUILD
  printf("[plugin_IsFormatSupported] : rc = %d, achTemp = [%s]\n", rc, achTemp);
#endif

  if ((rc==WAWEHELPER_NOERROR) &&
      ((!strcmp(achTemp, "OGG")) ||
       (!strcmp(achTemp, "OGG "))))
  {
#ifdef DEBUG_BUILD
    printf("[plugin_IsFormatSupported] : Yes!\n");
#endif
    return 1;
  }
  return 0;
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

  // Use this plugin to save OGG files
  pPluginDesc->iPluginImportance = WAWE_PLUGIN_IMPORTANCE_MAX-5;

  pPluginDesc->iPluginType = WAWE_PLUGIN_TYPE_EXPORT;

  // Plugin functions:
  pPluginDesc->fnAboutPlugin = plugin_About;
  pPluginDesc->fnConfigurePlugin = NULL;
  pPluginDesc->fnInitializePlugin = plugin_Initialize;
  pPluginDesc->fnPrepareForShutdown = NULL;
  pPluginDesc->fnUninitializePlugin = plugin_Uninitialize;

  pPluginDesc->TypeSpecificInfo.ExportPluginInfo.fnIsFormatSupported = plugin_IsFormatSupported;
  pPluginDesc->TypeSpecificInfo.ExportPluginInfo.fnCreate            = plugin_Create;
  pPluginDesc->TypeSpecificInfo.ExportPluginInfo.fnWrite             = plugin_Write;
  pPluginDesc->TypeSpecificInfo.ExportPluginInfo.fnClose             = plugin_Close;

  // Save module handle for later use
  // (We will need it when we want to load some resources from DLL)
  hmodOurDLLHandle = pPluginDesc->hmodDLL;

  // Return success!
  return 1;
}
