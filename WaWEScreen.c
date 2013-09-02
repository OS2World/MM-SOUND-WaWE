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
 * Screen redrawer stuff                                                     *
 *****************************************************************************/

#include "WaWE.h"

#define DEFAULT_FONT_SIZE       8
#define DEFAULT_FONT_NAME       "Helv"
#define DEFAULT_FONT_SIZENAME   "8.Helv"

#define CHANNELSET_WINDOW_CLASS   "WaWEChannelSet"
#define SIZER_WINDOW_CLASS        "WaWESizer"
#define HEADERBG_WINDOW_CLASS     "WaWEHeaderBackground"
#define CHANNELDRAW_WINDOW_CLASS  "WaWEChannelDraw"
#define VISIBLEPART_WINDOW_CLASS  "WaWEVisiblePartSelector"


// Colors of Channel-set header in active and inactive state:
#define CHANNELSET_HEADER_FIRSTLINE_INACTIVE_BG_COLOR (RGB_YELLOW + 0xA0)
#define CHANNELSET_HEADER_FIRSTLINE_ACTIVE_BG_COLOR (0x004040af)
#define CHANNELSET_HEADER_FIRSTLINE_INACTIVE_FG_COLOR (RGB_BLACK)
#define CHANNELSET_HEADER_FIRSTLINE_ACTIVE_FG_COLOR (RGB_YELLOW+0xC0)

// How much space to leave from sides
#define HEADERBG_FRAME_SPACE          2
// How much space to leave between lines
#define HEADERBG_SPACE_BETWEEN_LINES  2
// How much space to leave between rows
#define HEADERBG_SPACE_BETWEEN_ROWS   2

// HEADERBG Class defines: ----------------------
#define HEADERBG_HEADERTYPE_CHANNELSET 0
#define HEADERBG_HEADERTYPE_CHANNEL    1
// The positions of private window variables in private storage:
#define HEADERBG_STORAGE_HEADERTYPE      0
#define HEADERBG_STORAGE_PCHANNELSET     4
#define HEADERBG_STORAGE_PCHANNEL        8

#define HEADERBG_STORAGE_NEEDED_BYTES   (3*sizeof(ULONG))

// SIZER Class defines: -------------------------
// The positions of private window variables in private storage:
#define SIZER_STORAGE_SIZINGFLAG     0
#define SIZER_STORAGE_SIZESTARTPOSY  4

#define SIZER_STORAGE_NEEDED_BYTES   (2*sizeof(ULONG))

// CHANNELDRAW Class defines: -------------------------
// The positions of private window variables in private storage:
#define CHANNELDRAW_STORAGE_PCHANNEL 0

#define CHANNELDRAW_STORAGE_NEEDED_BYTES   (1*sizeof(ULONG))

// VISIBLEPART Class defines: -------------------------
// The positions of private window variables in private storage:
#define VISIBLEPART_STORAGE_ULSTARTFROMLO      0
#define VISIBLEPART_STORAGE_ULSTARTFROMHI      4
#define VISIBLEPART_STORAGE_ULZOOMCOUNTLO      8
#define VISIBLEPART_STORAGE_ULZOOMCOUNTHI     12
#define VISIBLEPART_STORAGE_ULZOOMDENOMLO     16
#define VISIBLEPART_STORAGE_ULZOOMDENOMHI     20
#define VISIBLEPART_STORAGE_ULVISIBLEPIXELS   24
#define VISIBLEPART_STORAGE_ULMAXSAMPLELO     28
#define VISIBLEPART_STORAGE_ULMAXSAMPLEHI     32
#define VISIBLEPART_STORAGE_ULMOUSECAPTURED   36

#define VISIBLEPART_STORAGE_NEEDED_BYTES   (10*sizeof(ULONG))

#define WM_VISIBLEPART_SETSTARTFROM     (WM_USER+1)
#define WM_VISIBLEPART_SETZOOMCOUNT     (WM_USER+2)
#define WM_VISIBLEPART_SETZOOMDENOM     (WM_USER+3)
#define WM_VISIBLEPART_SETVISIBLEPIXELS (WM_USER+4)
#define WM_VISIBLEPART_SETMAXSAMPLE     (WM_USER+5)

#define VPN_VALUESCHANGED                     0

typedef struct _VPNValuesChanged_struct
{
  ULONG ulStartFromLo;
  ULONG ulStartFromHi;
  ULONG ulZoomCountLo;
  ULONG ulZoomCountHi;
  ULONG ulZoomDenomLo;
  ULONG ulZoomDenomHi;
} VPNValuesChanged_t, *VPNValuesChanged_p;

// ID of Channel-Set autoscroll timer:
#define TID_AUTOSCROLLTIMER 1

// Variables:
int iChannelSetHeaderHeight;   // Height of a channel set header
int iChannelHeaderHeight;      // Height of a channel header
int iChannelEditAreaHeight;    // Default height for one channel edit area
int iDefaultFontHeight;        // Height of default font
int iDefaultTextWindowHeight;  // Height for text window with default font inside

// Horizontal Scrollbar specific stuff
int iFirstScreenYPos;
int iScrollBarShowsHeight_Client;
int iScrollBarShowsHeight_Sets;

// Vertical Scrollbar specific stuff
int iScrollBarHeight;

// Default sizer height:
int iSizerHeight;

// Undo buffer for ZoomSet dialog
char achZoomSetDlgUndoBuf[1024];

// Some state variable
WaWEChannelSet_p pActiveChannelSet = NULL;

void DrawFramedRect(HPS hps, PRECTL prclRect, int bRaised)
{
  RECTL rclInnerRect;
  POINTL point;

  memcpy(&rclInnerRect, prclRect, sizeof(rclInnerRect));

  // Draw the inner part
  if (rclInnerRect.xRight>0) rclInnerRect.xRight--;
  if (rclInnerRect.yTop>0) rclInnerRect.yTop--;
  rclInnerRect.xLeft++;
  rclInnerRect.yBottom++;
  WinFillRect(hps, &rclInnerRect, SYSCLR_BUTTONMIDDLE);
  // Then the frame!
  point.x = prclRect->xLeft;
  point.y = prclRect->yBottom;
  GpiMove(hps, &point);

  if (bRaised)
    GpiSetColor(hps, SYSCLR_BUTTONLIGHT);
  else
    GpiSetColor(hps, SYSCLR_BUTTONDARK);
  point.x = prclRect->xLeft;
  point.y = prclRect->yTop-1;
  GpiLine(hps, &point);
  point.x = prclRect->xRight-1;
  point.y = prclRect->yTop-1;
  GpiLine(hps, &point);
  if (bRaised)
    GpiSetColor(hps, SYSCLR_BUTTONDARK);
  else
    GpiSetColor(hps, SYSCLR_BUTTONLIGHT);
  point.x = prclRect->xRight-1;
  point.y = prclRect->yBottom;
  GpiLine(hps, &point);
  point.x = prclRect->xLeft;
  point.y = prclRect->yBottom;
  GpiLine(hps, &point);

}

// Filter proc.
// This is used to subclass public window classes, so
// even if they process WM_BUTTONxDOWN messages, we'll still
// get it too, which means that we can set active window, even if the
// user clicks on them!
MRESULT EXPENTRY PublicClassFilterProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  PFNWP pOldProc;
  if ((msg==WM_BUTTON1DOWN) ||
      (msg==WM_BUTTON2DOWN) ||
      (msg==WM_BUTTON3DOWN)
      )
  {
    WinPostMsg(WinQueryWindow(hwnd, QW_PARENT), msg, mp1, mp2);
  }

  pOldProc = (PFNWP) WinQueryWindowPtr(hwnd, 0);

  if (pOldProc)
    return (*pOldProc)(hwnd, msg, mp1, mp2);
  else
    return WinDefWindowProc(hwnd, msg, mp1, mp2);
}

void SubclassPublicClass(HWND hwnd)
{
  PFNWP pOldProc;
  pOldProc = WinSubclassWindow(hwnd, PublicClassFilterProc);
  WinSetWindowPtr(hwnd, 0, pOldProc);
}

// Filter proc.
// This is used to subclass static stuffs, so their parent will
// get the mouse button click messages!
MRESULT EXPENTRY StaticClassFilterProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  PFNWP pOldProc;
  if ((msg==WM_BUTTON1CLICK) ||
      (msg==WM_BUTTON2CLICK) ||
      (msg==WM_BUTTON3CLICK)
     )
  {
    return WinSendMsg(WinQueryWindow(hwnd, QW_PARENT), msg, mp1, mp2);
  }

  pOldProc = (PFNWP) WinQueryWindowPtr(hwnd, 0);

  if (pOldProc)
    return (*pOldProc)(hwnd, msg, mp1, mp2);
  else
    return WinDefWindowProc(hwnd, msg, mp1, mp2);
}

void SubclassStaticClass(HWND hwnd)
{
  PFNWP pOldProc;
  pOldProc = WinSubclassWindow(hwnd, StaticClassFilterProc);
  WinSetWindowPtr(hwnd, 0, pOldProc);
}

void UpdateChannelSetHScrollBar(WaWEChannelSet_p pChannelSet)
{
  HWND hwndScrollBar;
  WaWESize_t NumOfVisibleSamples;
  int iPositionOfSlider, iVisiblePerc;

//  printf("UpdateChannelSetHScrollBar : Enter\n");
  if (!pChannelSet) return;

//  printf("UpdateChannelSetHScrollBar : WinWindowFromID\n");
  hwndScrollBar = WinWindowFromID(pChannelSet->hwndChannelSet, DID_CHANNELSET_HORZSCROLL);
  if (!hwndScrollBar) return;

//  printf("Calculation...\n");

  // One small problem we have to take care is that the slider's position and
  // range parameters are signed shorts, so we can not give it a bigger value than
  // 32767. This means that we cannot go the easy way of setting the range to
  // 0..MaxNeededPixelsToDrawAllWave, because at a zoom of 1/1, where one pixel means
  // one sample, taking a wave of 1 sec at 44100Hz would make the range to be
  // 0..44100, which is already bigger than the limit.
  // To work around this, we'll keep track of position in percentage, but in a slightly
  // bigger precision. So, Slider range will always be 0..10000, where 10000 means 100%.

  // Calculate number of visible samples
  NumOfVisibleSamples = pChannelSet->iWidth * pChannelSet->ZoomDenom / pChannelSet->ZoomCount;

//  printf("NumOfVisibleSamples = %d\n", (long) NumOfVisibleSamples);
  // Now the top range of slider!
  // If the visible samples is more or equal to the number of samples in the whole
  // channelset, so the whole channelset is visible, then disable the slider!
  if (NumOfVisibleSamples>=pChannelSet->Length)
  {
//    printf("All visible, disabling scrollbar!\n");
    WinSendMsg(hwndScrollBar,
               SBM_SETSCROLLBAR,
               MPFROMSHORT(0),
               MPFROM2SHORT(0, 0));
    WinSendMsg(hwndScrollBar,
               SBM_SETTHUMBSIZE,
               MPFROM2SHORT(1, 0),
               MPFROMSHORT(0));
  }
  else
  {
    // Now the position of slider, in "percentage"
    iPositionOfSlider =
      10000*                                     // We want value from 0 to 10000
      pChannelSet->FirstVisiblePosition/         // Current position
      (pChannelSet->Length-NumOfVisibleSamples); // Max positions
    // Size of visible part, in percentage:
    iVisiblePerc =
      10000*                                     // We want value from 0 to 10000
      NumOfVisibleSamples /                      // Visible length
      pChannelSet->Length;                       // All length

    // Set slider position, and scrollbar range!
    WinSendMsg(hwndScrollBar,
               SBM_SETSCROLLBAR,
               MPFROMSHORT(iPositionOfSlider),
               MPFROM2SHORT(0, 10000));
    // Set thumb size
    WinSendMsg(hwndScrollBar,
               SBM_SETTHUMBSIZE,
               MPFROM2SHORT(iVisiblePerc,10000),
               MPFROMSHORT(0));
//    printf("Slider pos: %d,  visible: %d\n", iPositionOfSlider, iVisiblePerc);
  }
}

int UpdateChannelSetWindowSizes()
{
  WaWEChannelSet_p pChannelSet;
  int iFullHeight;
  int iScrollBarPos;
  int iScrollBarChanged = 0;
  int iCHSetChanged = 0;
  SWP swp;

  // It is called when the client window is resized.
  // If it's resized vertically, all we have to do is to modify the
  // scrollbar to show the real visible part values.
  // If it's resized horizontally (too), we have to resize the
  // channel-set windows too!

  // Returns true if it has done something.
  if (DosRequestMutexSem(ChannelSetListProtector_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    WinQueryWindowPos(hwndClient, &swp);

    // Set scrollbar position and size according to the visible stuff
    iFullHeight = 0;
    pChannelSet = ChannelSetListHead;
    while (pChannelSet)
    {
      iFullHeight += pChannelSet->iHeight;
      pChannelSet = pChannelSet->pNext;
    }



    // If the window client size
    // becomes bigger than the visible area of channelsets, then
    // move the channelsets!
    if (swp.cy>iFullHeight-iFirstScreenYPos)
    {
      iFirstScreenYPos=iFullHeight - swp.cy;
      if (iFirstScreenYPos<0) iFirstScreenYPos = 0;
    }

    iScrollBarPos = (int) WinSendDlgItemMsg(hwndFrame, FID_VERTSCROLL,
					    SBM_QUERYPOS, (MPARAM) 0, (MPARAM) 0);

    // Update scrollbar stuff only if it doesn't show the current
    // status.
    if ((iScrollBarPos!=iFirstScreenYPos) ||
	(iScrollBarShowsHeight_Client != swp.cy) ||
	(iScrollBarShowsHeight_Sets != iFullHeight-swp.cy))
    {

      WinSendDlgItemMsg(hwndFrame, FID_VERTSCROLL,
			SBM_SETTHUMBSIZE,
			MPFROM2SHORT(swp.cy, iFullHeight),
			(MPARAM) 0);
      WinSendDlgItemMsg(hwndFrame, FID_VERTSCROLL,
			SBM_SETSCROLLBAR,
			(MPARAM) iFirstScreenYPos,
			MPFROM2SHORT(0, iFullHeight-swp.cy));

      iScrollBarChanged = 1;

      iScrollBarShowsHeight_Client = swp.cy;
      iScrollBarShowsHeight_Sets = iFullHeight;
    }

    // Resize the channel-set windows, if the client window width
    // is changed!

    pChannelSet = ChannelSetListHead;
    while (pChannelSet)
    {
      SWP swpCSWin;

      WinQueryWindowPos(pChannelSet->hwndChannelSet,
                        &swpCSWin);
      if (swpCSWin.cx != swp.cx)
      {
        // Resize this channel-set!
        swpCSWin.cx = swp.cx;
        WinSetWindowPos(pChannelSet->hwndChannelSet,
                        HWND_TOP,
                        0, 0,
                        swpCSWin.cx,
                        swpCSWin.cy,
                        SWP_SIZE);
        // Now that the channel-set window is resized, the scrollbar
        // values should be updated too!

        UpdateChannelSetHScrollBar(pChannelSet);
        iCHSetChanged = 1;
      }
      pChannelSet = pChannelSet->pNext;
    }

    DosReleaseMutexSem(ChannelSetListProtector_Sem);
  }

  if (iScrollBarChanged)
  {
    UpdateChannelSetsByMainScrollbar();
  }

  return ((iScrollBarChanged) || (iCHSetChanged));
}

void UpdateChannelSetsByMainScrollbar()
{
  WaWEChannelSet_p pChannelSet;
  SWP swp, swp2;
  // It is called when the scrollbar has been moved. All we have to do is
  // to update the position of ChannelSet windows.

  if (DosRequestMutexSem(ChannelSetListProtector_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    // Get client window parameters
    WinQueryWindowPos(hwndClient, &swp);
    // Get scrollbar position
    iFirstScreenYPos = (int) WinSendDlgItemMsg(hwndFrame, FID_VERTSCROLL,
                                               SBM_QUERYPOS, (MPARAM) 0, (MPARAM) 0);

#ifdef DEBUG_BUILD
//    printf("iFirstScreenYPos: %d\n", iFirstScreenYPos); fflush(stdout);
#endif

    pChannelSet = ChannelSetListHead;
    while (pChannelSet)
    {
      SWP swpCSWin;

      WinQueryWindowPos(pChannelSet->hwndChannelSet,
                        &swpCSWin);

      if (pChannelSet->pPrev)
      {
        // If it's not the first channelset,
        // then check if it's aligned at prev channel set!
        if (swpCSWin.y!=(swp2.y-pChannelSet->iHeight))
        {
//          printf("Repositioning intermediate channelset!\n");
          swpCSWin.y = (swp2.y-pChannelSet->iHeight);
          WinSetWindowPos(pChannelSet->hwndChannelSet,
                          HWND_TOP,
                          swpCSWin.x,
                          swpCSWin.y,
                          0, 0,
                          SWP_MOVE);
        }
      } else
      {
        // If it's the first channelset,
        // then check if it's at the position it has to be!
        if (swpCSWin.y!=(swp.cy-pChannelSet->iHeight+iFirstScreenYPos))
        {
//          printf("Repositioning first channelset to top!\n");
          swpCSWin.y = (swp.cy-pChannelSet->iHeight+iFirstScreenYPos);
          WinSetWindowPos(pChannelSet->hwndChannelSet,
                          HWND_TOP,
                          swpCSWin.x,
                          swpCSWin.y,
                          0, 0,
                          SWP_MOVE);
        }
      }
      memcpy(&swp2, &swpCSWin, sizeof(swp));
      pChannelSet = pChannelSet->pNext;
    }

    DosReleaseMutexSem(ChannelSetListProtector_Sem);
  }
}

void RedrawChannelsInChannelSet(WaWEChannelSet_p pChannelSet)
{
  WaWEChannel_p pChannel;
  WaWEPosition_t FirstVisiblePosition; // First sample to show at the left side
  WaWEPosition_t LastVisiblePosition;  // Last sample to shot at the right side
  WaWEWorker1Msg_ChannelDraw_p pWorkerMsg;
  SWP swp;

  // It is called when the horizontal scrollbar has been moved or the
  // zoom factor has been changed. All we have to do is to send a redraw
  // request to every channel inside!

  if (DosRequestMutexSem(ChannelSetListProtector_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    FirstVisiblePosition = pChannelSet->FirstVisiblePosition;
    LastVisiblePosition =
      pChannelSet->FirstVisiblePosition +
      pChannelSet->iWidth * pChannelSet->ZoomDenom / pChannelSet->ZoomCount
      -1;

#ifdef DEBUG_BUILD
//    printf("Redrawing channels in channelset: from %d to %d (Width in pixels: %d)\n",
//	   (long) FirstVisiblePosition,
//	   (long) LastVisiblePosition,
//	   pChannelSet->iWidth);
#endif
    
    pChannel = pChannelSet->pChannelListHead;
    while (pChannel)
    {
      WinQueryWindowPos(pChannel->hwndEditAreaWindow, &swp);
      if (DosRequestMutexSem(pChannel->hmtxUseRequiredFields, 1000)==NO_ERROR)
      {
	pChannel->RequiredFirstSamplePos = FirstVisiblePosition;
	pChannel->RequiredLastSamplePos = LastVisiblePosition;
	pChannel->RequiredWidth = swp.cx;
	pChannel->RequiredHeight = swp.cy;
        DosReleaseMutexSem(pChannel->hmtxUseRequiredFields);
	pWorkerMsg = (WaWEWorker1Msg_ChannelDraw_p) dbg_malloc(sizeof(WaWEWorker1Msg_ChannelDraw_t));
	if (pWorkerMsg)
	{
          pWorkerMsg->pChannel = pChannel;
          pWorkerMsg->bDontOptimize = 0;
	  if (!AddNewWorker1Msg(WORKER1_MSG_CHANNELDRAW, sizeof(WaWEWorker1Msg_ChannelDraw_t), pWorkerMsg))
          {
            // Error adding the worker message to the queue, the queue might be full?
            dbg_free(pWorkerMsg);
          }
        }
        // Make sure it will be invalidated, so even if the calculation is
        // not finished, something will be drawn.
        WinInvalidateRect(pChannel->hwndEditAreaWindow, NULL, TRUE);
      }
      pChannel = pChannel->pNext;
    }
    DosReleaseMutexSem(ChannelSetListProtector_Sem);
  }
}


void ResizeAndRepositionChannelSetWindowChildren(HWND hwnd,
                                                 WaWEChannelSet_p pChannelSet,
                                                 int iWidth, int iHeight)
{
  SWP swp;
  WaWEChannel_p pChannel;
  int iNumOfChannels;
  int iChannelNum;
  int iHeightForChannels;
  int iChannelHeaderBase;
  int iScrollBarHeight;
  int iDisabledWindowUpdate=0;

  if (WinIsWindowVisible(hwnd))
  {
    WinEnableWindowUpdate(hwnd, FALSE);
    iDisabledWindowUpdate = 1;
  }


  // Ok, resize from bottom to top.
  // The sizer and the horizontal scrollbar stays where tey are, but their
  // width might change according to the new width
  WinQueryWindowPos(WinWindowFromID(hwnd, DID_CHANNELSET_SIZER), &swp);
  WinSetWindowPos(WinWindowFromID(hwnd, DID_CHANNELSET_SIZER),
                  HWND_TOP,
                  0, 0,
                  iWidth,
                  swp.cy,
                  SWP_SIZE);
  WinQueryWindowPos(WinWindowFromID(hwnd, DID_CHANNELSET_HORZSCROLL), &swp);
  WinSetWindowPos(WinWindowFromID(hwnd, DID_CHANNELSET_HORZSCROLL),
                  HWND_TOP,
                  0, 0,
                  iWidth,
                  swp.cy,
                  SWP_SIZE);

  // Then the channelset header background!
  // It should be resized, and repositioned too, to stay on top of window.
  WinQueryWindowPos(WinWindowFromID(hwnd, DID_CHANNELSET_HEADERBG), &swp);
  WinSetWindowPos(WinWindowFromID(hwnd, DID_CHANNELSET_HEADERBG),
                  HWND_TOP,
                  0,
                  iHeight-swp.cy,
                  iWidth,
                  swp.cy,
                  SWP_SIZE | SWP_MOVE);

  // Now the channel headers!
  WinQueryWindowPos(WinWindowFromID(hwnd, DID_CHANNELSET_HORZSCROLL), &swp);
  iScrollBarHeight = swp.cy;
  iNumOfChannels = GetNumOfChannelsFromChannelSet(pChannelSet);
  iChannelNum = iNumOfChannels;
  pChannel = pChannelSet->pChannelListHead;
  iHeightForChannels =
    pChannelSet->iHeight -                           // Full height minus
    iChannelSetHeaderHeight -                        //  ChannelSetHeader
    iScrollBarHeight -                               //  Scrollbar
    iSizerHeight;                                    //  and sizer
  iChannelHeaderBase =
    iScrollBarHeight +                               //  Scrollbar
    iSizerHeight;                                    //  and sizer
  while (pChannel)
  {
    int iNextChannelHeaderPosY;
    int iThisChannelHeaderPosY;

    iThisChannelHeaderPosY =
      iHeightForChannels*iChannelNum/iNumOfChannels -
      iChannelHeaderHeight +
      iChannelHeaderBase;
    iNextChannelHeaderPosY =
      iHeightForChannels*(iChannelNum-1)/iNumOfChannels -
      iChannelHeaderHeight +
      iChannelHeaderBase;
//    printf("Resizing %x\n", WinWindowFromID(hwnd, DID_CHANNELSET_CH_EDITAREA_BASE + pChannel->iChannelID));
    WinSetWindowPos(WinWindowFromID(hwnd, DID_CHANNELSET_CH_HEADERBG_BASE + pChannel->iChannelID),
                    HWND_TOP,
                    0,                          // x
                    iThisChannelHeaderPosY,     // y
                    pChannelSet->iWidth,        // cx
                    iChannelHeaderHeight,       // cy
                    SWP_MOVE | SWP_SIZE);

    WinSetWindowPos(WinWindowFromID(hwnd, DID_CHANNELSET_CH_EDITAREA_BASE + pChannel->iChannelID),
		    HWND_TOP,
                    0,                          // x
                    iNextChannelHeaderPosY+iChannelHeaderHeight, // y
                    pChannelSet->iWidth,        // cx
                    iThisChannelHeaderPosY-
                    iNextChannelHeaderPosY-
                    iChannelHeaderHeight,       // cy
                    SWP_MOVE | SWP_SIZE);

    iChannelNum--;
    pChannel = pChannel->pNext;
  }
  // Ok, all resized and repositioned!

  if (iDisabledWindowUpdate)
    WinEnableWindowUpdate(hwnd, TRUE);
}

void SetActiveChannelSet(WaWEChannelSet_p pChannelSet)
{
  if (pActiveChannelSet==pChannelSet) return;

  if (pActiveChannelSet)
  {
    // Send deactivate message to old
#ifdef DEBUG_BUILD
    printf("[SetActiveChannelSet] : Sending deactivate message to old\n"); fflush(stdout);
#endif
    WinPostMsg(pActiveChannelSet->hwndChannelSet,
               WM_ACTIVATE,
               (MPARAM) FALSE,
               (MPARAM) (pActiveChannelSet->hwndChannelSet));
  }
  pActiveChannelSet = pChannelSet;
  if (pActiveChannelSet)
  {
    // Send activate message to new
#ifdef DEBUG_BUILD
    printf("[SetActiveChannelSet] : Sending activate message to new\n"); fflush(stdout);
#endif
    WinPostMsg(pActiveChannelSet->hwndChannelSet,
               WM_ACTIVATE,
               (MPARAM) TRUE,
               (MPARAM) (pActiveChannelSet->hwndChannelSet));

  }
}

void ResizeZoomSetDialogControls(HWND hwnd)
{
  SWP Swp1, Swp2, Swp3, Swp4;
  ULONG CXDLGFRAME, CYDLGFRAME, CYTITLEBAR;
  int iGBHeight, iStaticTextHeight, iEntryFieldHeight, iTempSize, iTempPos;
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

  WinQueryWindowPos(WinWindowFromID(hwnd, PB_HELP), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, PB_HELP), HWND_TOP,
                  (Swp1.cx - Swp2.cx)/2,
                  2*CYDLGFRAME,
		  0, 0,
                  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, PB_CANCEL), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, PB_CANCEL), HWND_TOP,
                  Swp1.cx - 3*CXDLGFRAME - Swp2.cx,
                  2*CYDLGFRAME,
		  0, 0,
                  SWP_MOVE);

  // The top groupbox
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_SAMPLES), &Swp3);
  iStaticTextHeight = Swp3.cy;
  iEntryFieldHeight = iStaticTextHeight + CYDLGFRAME;
  iGBHeight = iStaticTextHeight + CYDLGFRAME +
    iEntryFieldHeight + CYDLGFRAME +
    iEntryFieldHeight + CYDLGFRAME +
    iEntryFieldHeight + CYDLGFRAME +
    2*CYDLGFRAME;
  WinSetWindowPos(WinWindowFromID(hwnd, GB_SETTINGVALUESBYHAND), HWND_TOP,
                  2*CXDLGFRAME,
                  Swp1.cy - 2*CYDLGFRAME - CYTITLEBAR - iGBHeight,
                  Swp1.cx - 4*CXDLGFRAME,
                  iGBHeight,
                  SWP_MOVE | SWP_SIZE);

  // The static text fields and entry fields inside the groupbox
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_SETTINGVALUESBYHAND), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_LENGTHOFCHANNELSET), HWND_TOP,
                  Swp2.x + CXDLGFRAME,
                  Swp2.y + Swp2.cy - iStaticTextHeight - CYDLGFRAME - iEntryFieldHeight,
                  0, 0,
                  SWP_MOVE);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_LENGTHOFCHANNELSET), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_VISIBLEPARTSTARTSAT), HWND_TOP,
                  Swp2.x,
                  Swp2.y - CYDLGFRAME - iEntryFieldHeight,
                  0, 0,
                  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_VISIBLEPARTSTARTSAT), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_ZOOMIS), HWND_TOP,
                  Swp2.x,
                  Swp2.y - CYDLGFRAME - iEntryFieldHeight,
                  0, 0,
                  SWP_MOVE);

  //  The two 'Samples' text at the right side
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_LENGTHOFCHANNELSET), &Swp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_SETTINGVALUESBYHAND), &Swp3);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_SAMPLES), &Swp4);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_SAMPLES), HWND_TOP,
                  Swp3.x + Swp3.cx - 2*CXDLGFRAME - Swp4.cx,
                  Swp2.y,
                  0, 0,
                  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_ZOOMIS), &Swp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_SAMPLES2), &Swp4);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_SAMPLES2), HWND_TOP,
                  Swp3.x + Swp3.cx - 2*CXDLGFRAME - Swp4.cx,
                  Swp2.y,
                  0, 0,
                  SWP_MOVE);

  // The entry fields

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_LENGTHOFCHANNELSET), &Swp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_SAMPLES), &Swp3);
  iTempSize = Swp3.x - 2*CXDLGFRAME - Swp2.x - Swp2.cx - 2*CXDLGFRAME;
  iTempPos = Swp2.x + Swp2.cx + 2*CXDLGFRAME;
  WinSetWindowPos(WinWindowFromID(hwnd, EF_NUMOFSAMPLESINCHANNELSET), HWND_TOP,
                  iTempPos,
                  Swp2.y,
                  iTempSize,
                  iStaticTextHeight,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_VISIBLEPARTSTARTSAT), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, EF_VISIBLEPARTSTARTSAT), HWND_TOP,
                  iTempPos,
                  Swp2.y,
                  iTempSize,
                  iStaticTextHeight,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_ZOOMIS), &Swp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_SAMPLES2), &Swp3);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_PIXELSPER), &Swp4);
  iTempSize = (Swp3.x - Swp2.x - Swp2.cx - Swp4.cx - 8*CXDLGFRAME)/2; // Width of EF
  WinSetWindowPos(WinWindowFromID(hwnd, EF_NUMOFPIXELS), HWND_TOP,
                  Swp2.x + Swp2.cx + 2*CXDLGFRAME,
                  Swp2.y,
                  iTempSize,
                  iStaticTextHeight,
                  SWP_MOVE | SWP_SIZE);

  WinSetWindowPos(WinWindowFromID(hwnd, ST_PIXELSPER), HWND_TOP,
                  Swp2.x + Swp2.cx + 2*CXDLGFRAME + iTempSize + 2*CXDLGFRAME,
                  Swp2.y,
                  0, 0,
                  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_PIXELSPER), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, EF_NUMOFSAMPLES), HWND_TOP,
                  Swp2.x + Swp2.cx + 2*CXDLGFRAME,
                  Swp2.y,
                  Swp3.x - 2*CXDLGFRAME - (Swp2.x + Swp2.cx + 2*CXDLGFRAME),
                  iStaticTextHeight,
                  SWP_MOVE | SWP_SIZE);

  // GB_SETTINGVALUESBYMOUSE
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_SETTINGVALUESBYHAND), &Swp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, PB_OK), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, GB_SETTINGVALUESBYMOUSE), HWND_TOP,
                  2*CXDLGFRAME,
                  Swp3.y + Swp3.cy + 2*CYDLGFRAME,
                  Swp1.cx - 4*CXDLGFRAME,
                  Swp2.y - CYDLGFRAME - (Swp3.y + Swp3.cy + 2*CYDLGFRAME),
                  SWP_MOVE | SWP_SIZE);

  // The usage text to the groupbox!
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_SETTINGVALUESBYMOUSE), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_USETHE), HWND_TOP,
                  Swp2.x + 2*CXDLGFRAME,
                  Swp2.y + 2*CYDLGFRAME,
                  Swp2.cx - 4*CXDLGFRAME,
                  Swp2.cy-iStaticTextHeight-4*CYDLGFRAME,
                  SWP_MOVE | SWP_SIZE);

  // The user control in the middle of last groupbox
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_SETTINGVALUESBYMOUSE), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, USERCTRL_VISIBLEPART), HWND_TOP,
                  Swp2.x + 5*CXDLGFRAME,
                  Swp2.y + 4*CYDLGFRAME,
                  Swp2.cx - 10*CXDLGFRAME,
                  Swp2.cy-iStaticTextHeight - 6*CYDLGFRAME,
                  SWP_MOVE | SWP_SIZE | SWP_ZORDER);

  if (iDisabledWindowUpdate)
    WinEnableWindowUpdate(hwnd, TRUE);
}

MRESULT EXPENTRY ZoomSetDialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  char achTemp[128];

  switch(msg)
  {
    case WM_FORMATFRAME:
      ResizeZoomSetDialogControls(hwnd);
      break;
    case WM_CONTROL:
      // Check who sent the WM_CONTROL message!
      switch (SHORT1FROMMP(mp1))
      {
        case USERCTRL_VISIBLEPART:
          // Hm, it's our custom control reporting the new values set by
          // mouse clicking!
          if (SHORT2FROMMP(mp1) == VPN_VALUESCHANGED)
          {
            VPNValuesChanged_p pChangeStruct = (VPNValuesChanged_p) mp2;
            if (pChangeStruct)
            {
              WaWEPosition_t StartPos;
              WaWESize_t ZoomCount, ZoomDenom;
              StartPos = pChangeStruct->ulStartFromHi;
              StartPos = (StartPos<<32) | (pChangeStruct->ulStartFromLo);
              ZoomCount = pChangeStruct->ulZoomCountHi;
              ZoomCount = (ZoomCount<<32) | (pChangeStruct->ulZoomCountLo);
              ZoomDenom = pChangeStruct->ulZoomDenomHi;
              ZoomDenom = (ZoomDenom<<32) | (pChangeStruct->ulZoomDenomLo);
              sprintf(achTemp, "%I64d", StartPos);
              WinSetDlgItemText(hwnd, EF_VISIBLEPARTSTARTSAT, achTemp);
              sprintf(achTemp, "%I64d", ZoomCount);
              WinSetDlgItemText(hwnd, EF_NUMOFPIXELS, achTemp);
              sprintf(achTemp, "%I64d", ZoomDenom);
              WinSetDlgItemText(hwnd, EF_NUMOFSAMPLES, achTemp);
            }
            return (MRESULT) 1;
          }
          break;
        case EF_VISIBLEPARTSTARTSAT:
          // The VisiblePartStartsAt entry field can only take positive numbers, and
          // has int64 numbers!
	  if (SHORT2FROMMP(mp1) == EN_SETFOCUS)
	  {
            WinQueryDlgItemText(hwnd, SHORT1FROMMP(mp1), sizeof(achZoomSetDlgUndoBuf), achZoomSetDlgUndoBuf);
	  } else
	  if (SHORT2FROMMP(mp1) == EN_KILLFOCUS)
	  {
            WaWEPosition_t i64Value;
            int iTemp;
	    WinQueryDlgItemText(hwnd, SHORT1FROMMP(mp1), sizeof(achTemp), achTemp);
            iTemp = sscanf(achTemp, "%I64d", &i64Value);
            if ((iTemp!=1) || (i64Value<0))
              WinSetDlgItemText(hwnd, SHORT1FROMMP(mp1), achZoomSetDlgUndoBuf);
            else
              // Tell the visiblepart control the new start value!
              WinSendDlgItemMsg(hwnd, USERCTRL_VISIBLEPART, WM_VISIBLEPART_SETSTARTFROM,
                                (MPARAM) ((LONG) i64Value), (MPARAM) ((LONG) (i64Value>>32)));
	  }
	  break;
        case EF_NUMOFPIXELS:
        case EF_NUMOFSAMPLES:
          // The NumOfPixels and NumOfSamples entry fields can only take positive, non-null numbers, and
          // has normal int numbers!
	  if (SHORT2FROMMP(mp1) == EN_SETFOCUS)
	  {
            WinQueryDlgItemText(hwnd, SHORT1FROMMP(mp1), sizeof(achZoomSetDlgUndoBuf), achZoomSetDlgUndoBuf);
	  } else
	  if (SHORT2FROMMP(mp1) == EN_KILLFOCUS)
	  {
            WaWESize_t i64Value;
            int iTemp;
	    WinQueryDlgItemText(hwnd, SHORT1FROMMP(mp1), sizeof(achTemp), achTemp);
            iTemp = sscanf(achTemp, "%I64d", &i64Value);
            if ((iTemp!=1) || (i64Value<=0))
              WinSetDlgItemText(hwnd, SHORT1FROMMP(mp1), achZoomSetDlgUndoBuf);
            else
            {
              // Tell the visiblepart control the new zoom values!
              if (SHORT1FROMMP(mp1) == EF_NUMOFPIXELS)
                WinSendDlgItemMsg(hwnd, USERCTRL_VISIBLEPART, WM_VISIBLEPART_SETZOOMCOUNT,
                                  (MPARAM) ((LONG) i64Value), (MPARAM) ((LONG) (i64Value>>32)));
              else
                WinSendDlgItemMsg(hwnd, USERCTRL_VISIBLEPART, WM_VISIBLEPART_SETZOOMDENOM,
                                  (MPARAM) ((LONG) i64Value), (MPARAM) ((LONG) (i64Value>>32)));
            }
	  } 
	  break;
	default:
          break;
      }
      break;

    case WM_COMMAND:
      {
        // Now let's see who sent this WM_COMMAND!
	switch SHORT1FROMMP(mp2)
	{
	  case CMDSRC_PUSHBUTTON:
	    {
	      // Okay, a Pushbutton has sent us a WM_COMMAND!
	      // Let's see who was it!
	      switch ((USHORT)mp1)
              {
                case PB_HELP:
                  // Help button!
                  WinMessageBox(HWND_DESKTOP, hwnd,
                                "This is the place where you can set the visible area of the channel-set in a comfortable way.\n"
                                "\n"
                                "You can either set the values by hand, or use the mouse to set the visible area. If you plan to "
                                "use the mouse, use the Left mouse button to set the beginning of the visible area, and use the "
                                "Right mouse button to set the end of the visible area.\n",
                                "WaWE Help", 0, MB_OK | MB_MOVEABLE | MB_INFORMATION);

                  return (MRESULT) 1;
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
    default:
      break;
  }

  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}

MRESULT EXPENTRY ChannelSetWindowProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  WaWEChannelSet_p pChannelSet;
  char achBuffer[64];

  switch (msg)
  {
    case WM_DESTROY:
      {
//        printf("[ChannelSetWindowProc] : WM_DESTROY (hwnd is 0x%x)\n", hwnd);
        // Set in channel set descriptor that there is no more window...
        pChannelSet = WinQueryWindowPtr(hwnd, 0);
        if (pChannelSet)
          pChannelSet->hwndChannelSet = NULLHANDLE;
        break;
      }
    case WM_CREATE:
      {
        PCREATESTRUCT pCreateSt;
//        printf("[ChannelSetWindowProc] : WM_CREATE\n");
        // We save the pointer to the WaWEChannelSet in our
        // window ptr.
        pCreateSt = (PCREATESTRUCT) mp2;
        WinSetWindowPtr(hwnd, 0, pCreateSt->pPresParams);
        break;
      }
    case WM_BUTTON1DOWN:
    case WM_BUTTON2DOWN:
    case WM_BUTTON3DOWN:
      {
        pChannelSet = WinQueryWindowPtr(hwnd, 0);
        if ((pChannelSet) && (pActiveChannelSet!=pChannelSet))
          SetActiveChannelSet(pChannelSet);
        return (MRESULT) 1;
      }
    case WM_BUTTON2CLICK:
      {
        // Note: If all goes well, it will be never called, because
        // the Channel-Draw window and the header background windows
        // will handle it. But, to make sure, I left it here.
        pChannelSet = WinQueryWindowPtr(hwnd, 0);
        if (pChannelSet)
        {
#ifdef DEBUG_BUILD
          printf("[ChannelSetWindowProc] : WM_BUTTON2CLICK for chn-set %d!\n", pChannelSet->iChannelSetID);
          fflush(stdout);
#endif
          CreatePopupMenu(NULL, pChannelSet);
        }
        return (MRESULT) 1;
      }

    case WM_ACTIVATE:
      {
#ifdef DEBUG_BUILD
        printf("[ChannelSetWindowProc] : WM_ACTIVATE\n"); fflush(stdout);
#endif
        pChannelSet = WinQueryWindowPtr(hwnd, 0);
        if (pChannelSet)
        {
          long lColor;
          HWND hwnd1, hwnd2;

          if ((USHORT)mp1)
          {
#ifdef DEBUG_BUILD
            printf("   Activated!\n");
#endif
            hwnd1 = WinWindowFromID(WinWindowFromID(pChannelSet->hwndChannelSet, DID_CHANNELSET_HEADERBG),
                                    DID_CHANNELSET_NAME);
            hwnd2 = WinWindowFromID(WinWindowFromID(pChannelSet->hwndChannelSet, DID_CHANNELSET_HEADERBG),
                                    DID_CHANNELSET_FORMAT);
            lColor = CHANNELSET_HEADER_FIRSTLINE_ACTIVE_BG_COLOR;
            WinSetPresParam(hwnd1, PP_BACKGROUNDCOLOR, sizeof(lColor), &lColor);
            WinSetPresParam(hwnd2, PP_BACKGROUNDCOLOR, sizeof(lColor), &lColor);
            lColor = CHANNELSET_HEADER_FIRSTLINE_ACTIVE_FG_COLOR;
            WinSetPresParam(hwnd1, PP_FOREGROUNDCOLOR, sizeof(lColor), &lColor);
            WinSetPresParam(hwnd2, PP_FOREGROUNDCOLOR, sizeof(lColor), &lColor);
          }
          else
          {
#ifdef DEBUG_BUILD
            printf("   Deactivated!\n");
#endif
            hwnd1 = WinWindowFromID(WinWindowFromID(pChannelSet->hwndChannelSet, DID_CHANNELSET_HEADERBG),
                                    DID_CHANNELSET_NAME);
            hwnd2 = WinWindowFromID(WinWindowFromID(pChannelSet->hwndChannelSet, DID_CHANNELSET_HEADERBG),
                                    DID_CHANNELSET_FORMAT);
            lColor = CHANNELSET_HEADER_FIRSTLINE_INACTIVE_BG_COLOR;
            WinSetPresParam(hwnd1, PP_BACKGROUNDCOLOR, sizeof(lColor), &lColor);
            WinSetPresParam(hwnd2, PP_BACKGROUNDCOLOR, sizeof(lColor), &lColor);
            lColor = CHANNELSET_HEADER_FIRSTLINE_INACTIVE_FG_COLOR;
            WinSetPresParam(hwnd1, PP_FOREGROUNDCOLOR, sizeof(lColor), &lColor);
            WinSetPresParam(hwnd2, PP_FOREGROUNDCOLOR, sizeof(lColor), &lColor);
          }
        }
        break;
      }
    case WM_TIMER:
      {
        pChannelSet = WinQueryWindowPtr(hwnd, 0);
        if (pChannelSet)
        {
          switch (SHORT1FROMMP(mp1)) // Check the timer ID
          {
            case TID_AUTOSCROLLTIMER:
              {
                POINTL ptlMousePos;
                POINTL ptlBottomLeft, ptlTopRight;
                RECTL rectl;
                WinQueryPointerPos(HWND_DESKTOP, &ptlMousePos);
                WinQueryWindowRect(hwnd, &rectl);
                ptlBottomLeft.x = rectl.xLeft;
                ptlBottomLeft.y = rectl.yBottom;
                ptlTopRight.x = rectl.xRight;
                ptlTopRight.y = rectl.yTop;
                WinMapWindowPoints(hwnd, HWND_DESKTOP, &ptlBottomLeft, 1);
                WinMapWindowPoints(hwnd, HWND_DESKTOP, &ptlTopRight, 1);
                // Let's see if the mouse is out of the window in horizontal direction!
                if (ptlMousePos.x>ptlTopRight.x)
                {
                  WaWEPosition_t LastVisiblePosition;
                  // change position by some samples
//                  printf("Mouse is to the right by %d pixels\n",
//                         ptlMousePos.x - ptlTopRight.x);
                  pChannelSet->FirstVisiblePosition+=(ptlMousePos.x - ptlTopRight.x) * pChannelSet->ZoomDenom / pChannelSet->ZoomCount;
		  LastVisiblePosition =
		    pChannelSet->FirstVisiblePosition +
		    pChannelSet->iWidth * pChannelSet->ZoomDenom / pChannelSet->ZoomCount
		    -1;

		  if (LastVisiblePosition>=pChannelSet->Length)
		  {
		    pChannelSet->FirstVisiblePosition-=(LastVisiblePosition-pChannelSet->Length)+1;
		    if (pChannelSet->FirstVisiblePosition<0) pChannelSet->FirstVisiblePosition = 0;
                  }
		  // Now calculate position of slider, in "percentage"!
		  UpdateChannelSetHScrollBar(pChannelSet);
                  RedrawChannelsInChannelSet(pChannelSet);
                  // As the stuffs have moved, we should notify the channel draw window
                  // that it should change its selected region.
                  // The easiest way is to generate a WM_MOUSEMOVE in there!
                  WinSetPointerPos(HWND_DESKTOP, ptlMousePos.x, ptlMousePos.y);
                } else
                if (ptlMousePos.x<ptlBottomLeft.x)
                {
//                  printf("Mouse is to the left by %d pixels (x=%d)\n",
//                         ptlBottomLeft.x - ptlMousePos.x, ptlMousePos.x);
                  pChannelSet->FirstVisiblePosition-= (ptlBottomLeft.x - ptlMousePos.x ) * pChannelSet->ZoomDenom / pChannelSet->ZoomCount;
		  if (pChannelSet->FirstVisiblePosition<0) pChannelSet->FirstVisiblePosition = 0;
		  // Now calculate position of slider, in "percentage"!
		  UpdateChannelSetHScrollBar(pChannelSet);
                  RedrawChannelsInChannelSet(pChannelSet);
                  // As the stuffs have moved, we should notify the channel draw window
                  // that it should change its selected region.
                  // The easiest way is to generate a WM_MOUSEMOVE in there!
                  WinSetPointerPos(HWND_DESKTOP, ptlMousePos.x, ptlMousePos.y);
                }
                return (MRESULT) 1;
              }
            default:
              break;
          }
        }
        break;
      }
    case WM_PAINT:
      {
	HPS hpsBeginPaint;
        RECTL rclRect;

        hpsBeginPaint = WinBeginPaint(hwnd, NULL, &rclRect);

        if (!WinIsRectEmpty(WinQueryAnchorBlock(hwnd), &rclRect))
        {
          HPS hps;
          // If there is something to draw:
          hps = WinGetPS(hwnd);
          WinQueryWindowRect(hwnd, &rclRect);
  
          WinFillRect(hpsBeginPaint, &rclRect, SYSCLR_BUTTONDARK);
  
          WinReleasePS(hps);
        }

	WinEndPaint(hpsBeginPaint);
	return (MRESULT) FALSE;
      }
    case WM_SIZE: // The main window has been resized!
      {
	WaWEPosition_t FirstVisiblePosition, LastVisiblePosition;
	
//        printf("[ChannelSetWindowProc] : WM_SIZE\n");
        pChannelSet = WinQueryWindowPtr(hwnd, 0);
        if (pChannelSet)
        {
          pChannelSet->iWidth = SHORT1FROMMP(mp2);
	  pChannelSet->iHeight = SHORT2FROMMP(mp2);
	  
	  FirstVisiblePosition = pChannelSet->FirstVisiblePosition;
	  LastVisiblePosition =
	    pChannelSet->FirstVisiblePosition +
	    pChannelSet->iWidth * pChannelSet->ZoomDenom / pChannelSet->ZoomCount
	    -1;
	  
	  if (LastVisiblePosition>=pChannelSet->Length)
	  {
	    FirstVisiblePosition-=(LastVisiblePosition-pChannelSet->Length)+1;
	    if (FirstVisiblePosition<0) FirstVisiblePosition = 0;
	  }
	  pChannelSet->FirstVisiblePosition = FirstVisiblePosition;
	  
	  // Resize and/or reposition child windows according to the new size we have!
	  ResizeAndRepositionChannelSetWindowChildren(hwnd, pChannelSet, SHORT1FROMMP(mp2), SHORT2FROMMP(mp2));
          UpdateChannelSetHScrollBar(pChannelSet);
	}
        return (MRESULT) FALSE;
      }
    case WM_MOVE:
      {
        RECTL rect;
        POINTL point;
//        printf("[ChannelSetWindowProc] : WM_MOVE\n");
        pChannelSet = WinQueryWindowPtr(hwnd, 0);
        if (pChannelSet)
        {
          WinQueryWindowRect(hwnd, &rect);
          point.x = rect.xLeft;
          point.y = rect.yBottom;
          WinMapWindowPoints(hwnd, hwndClient, &point, 1);
          pChannelSet->iYPos = point.y;
        }
        break;
      }
    case WM_HSCROLL:
      {
	if ((USHORT)mp1 == DID_CHANNELSET_HORZSCROLL)
	{
          // This is a message from our vertical scrollbar!
          pChannelSet = WinQueryWindowPtr(hwnd, 0);
          if (pChannelSet)
          {
            // Let's see what command it is!
            switch (SHORT2FROMMP(mp2))
            {
              case SB_LINELEFT:
		{
		  WaWEPosition_t ToChange = pChannelSet->ZoomDenom / pChannelSet->ZoomCount;
                  if (ToChange<1) ToChange = 1;
//                  printf("SB_LINELEFT of scrollbar at %d\n",
//                         SHORT1FROMMP(mp2));
                  // change position by 1 pixel or 1 sample

		  pChannelSet->FirstVisiblePosition-=ToChange;
		  if (pChannelSet->FirstVisiblePosition<0) pChannelSet->FirstVisiblePosition = 0;
		  // Now calculate position of slider, in "percentage"!
		  UpdateChannelSetHScrollBar(pChannelSet);
                  RedrawChannelsInChannelSet(pChannelSet);
                  break;
                }
              case SB_LINERIGHT:
                {
		  WaWEPosition_t LastVisiblePosition;
		  WaWEPosition_t ToChange = pChannelSet->ZoomDenom / pChannelSet->ZoomCount;
                  if (ToChange<1) ToChange = 1;

//                  printf("SB_LINERIGHT of scrollbar at %d\n",
//                         SHORT1FROMMP(mp2));
		  // change position by 1 pixel or 1 sample
		  pChannelSet->FirstVisiblePosition+=ToChange;
		  LastVisiblePosition =
		    pChannelSet->FirstVisiblePosition +
		    pChannelSet->iWidth * pChannelSet->ZoomDenom / pChannelSet->ZoomCount
		    -1;

		  if (LastVisiblePosition>=pChannelSet->Length)
		  {
		    pChannelSet->FirstVisiblePosition-=(LastVisiblePosition-pChannelSet->Length)+1;
		    if (pChannelSet->FirstVisiblePosition<0) pChannelSet->FirstVisiblePosition = 0;
		  }
		  // Now calculate position of slider, in "percentage"!
		  UpdateChannelSetHScrollBar(pChannelSet);
                  RedrawChannelsInChannelSet(pChannelSet);
                  break;
                }
              case SB_PAGELEFT:
                {
//                  printf("SB_PAGELEFT of scrollbar at %d\n",
//                         SHORT1FROMMP(mp2));
		  // change position by some samples
		  pChannelSet->FirstVisiblePosition-= (pChannelSet->iWidth)/2 * pChannelSet->ZoomDenom / pChannelSet->ZoomCount;
		  if (pChannelSet->FirstVisiblePosition<0) pChannelSet->FirstVisiblePosition = 0;
		  // Now calculate position of slider, in "percentage"!
		  UpdateChannelSetHScrollBar(pChannelSet);
                  RedrawChannelsInChannelSet(pChannelSet);
                  break;
                }
              case SB_PAGERIGHT:
                {
//                  printf("SB_PAGERIGHT of scrollbar at %d\n",
//                         SHORT1FROMMP(mp2));
                  WaWEPosition_t LastVisiblePosition;
		  // change position by some samples    	  
		  pChannelSet->FirstVisiblePosition+=(pChannelSet->iWidth)/2 * pChannelSet->ZoomDenom / pChannelSet->ZoomCount;
		  LastVisiblePosition =
		    pChannelSet->FirstVisiblePosition +
		    pChannelSet->iWidth * pChannelSet->ZoomDenom / pChannelSet->ZoomCount
		    -1;

		  if (LastVisiblePosition>=pChannelSet->Length)
		  {
		    pChannelSet->FirstVisiblePosition-=(LastVisiblePosition-pChannelSet->Length)+1;
		    if (pChannelSet->FirstVisiblePosition<0) pChannelSet->FirstVisiblePosition = 0;
		  }
		  // Now calculate position of slider, in "percentage"!
		  UpdateChannelSetHScrollBar(pChannelSet);
                  RedrawChannelsInChannelSet(pChannelSet);
                  break;
		}
		
              case SB_SLIDERPOSITION:
                {
		  int iPos;
		  
		  iPos = SHORT1FROMMP(mp2);
		  if (!iPos)
		    iPos = (int) WinSendDlgItemMsg(hwnd, (USHORT) mp1,
						   SBM_QUERYPOS, (MPARAM) 0, (MPARAM) 0);
		  
//		  printf("SB_SLIDERPOSITION of scrollbar at %d\n",
//			 iPos);
		  
		  // Now update window content according to the new scrollbar position!
                  pChannelSet->FirstVisiblePosition =
                    (pChannelSet->Length  - // This is in Samples
                     (pChannelSet->iWidth * pChannelSet->ZoomDenom / pChannelSet->ZoomCount))  // This is in pixels, so its Denom/Count!
		    * iPos / 10000;

                  RedrawChannelsInChannelSet(pChannelSet);
                  break;
                }
              case SB_SLIDERTRACK:
		{
                  int iPos;
//		  printf("SB_SLIDERTRACK of scrollbar at %d\n",
//			 SHORT1FROMMP(mp2));

		  iPos = SHORT1FROMMP(mp2);
		  //(int) WinSendDlgItemMsg(hwnd, (USHORT) mp1,
		  //  			    SBM_QUERYPOS, (MPARAM) 0, (MPARAM) 0);

                  // The slidertrack does not change scrollbar position, so
                  // we do it, so the UpdateChannelSetsByMainScrollbar() will
                  // get the new position!
                  WinSendDlgItemMsg(hwnd, (USHORT) mp1,
                                    SBM_SETPOS,
                                    (MPARAM) SHORT1FROMMP(mp2),
                                    (MPARAM) 0);

                  // Now update window content according to the new scrollbar position!
                  pChannelSet->FirstVisiblePosition =
                    (pChannelSet->Length  - // This is in Samples
                     (pChannelSet->iWidth * pChannelSet->ZoomDenom / pChannelSet->ZoomCount))  // This is in pixels, so its Denom/Count!
		    * iPos / 10000;
//		  printf("New first vis pos: %d\n", (long) pChannelSet->FirstVisiblePosition);
                  RedrawChannelsInChannelSet(pChannelSet);
                  break;
                }
              default:
  //              printf("  WM_SCROLL message of %d\n", SHORT2FROMMP(mp2));
                break;
  
            }
            return (MRESULT) TRUE;
          } // end of 'if (pChannelSet)'
	}
        break; // Let the default one process, if it's not our scrollbar.
      }
    case WM_CONTROL:
      {
	switch SHORT1FROMMP(mp1)
	{
	  case DID_CHANNELSET_WORKWITHCHSET:
	    {
	      // Cool the checkbox has been pressed!
              // Save new checkbox state in channelset!
	      pChannelSet = WinQueryWindowPtr(hwnd, 0);
	      if (pChannelSet)
	      {
		pChannelSet->bWorkWithChannelSet =
		  (int) WinQueryButtonCheckstate(WinWindowFromID(pChannelSet->hwndChannelSet,
								 DID_CHANNELSET_HEADERBG),
						 DID_CHANNELSET_WORKWITHCHSET);
#ifdef DEBUG_BUILD
//		printf("[ChannelSetWindowProc] : ChSet %d : WorkWithChSet = %d\n",
//		       pChannelSet->iChannelSetID, pChannelSet->bWorkWithChannelSet);
#endif
	      }
	      break;
	    }
	  default:
            break;
	}
        break;
      }
    case WM_COMMAND:
      {
        // Now let's see who sent this WM_COMMAND!
	switch SHORT1FROMMP(mp2)
	{
	  case CMDSRC_PUSHBUTTON:
	    {
	      // Okay, a Pushbutton has sent us a WM_COMMAND!
	      // Let's see who was it!
	      switch ((USHORT)mp1)
              {
                case DID_CHANNELSET_ZOOMSET:
                  pChannelSet = WinQueryWindowPtr(hwnd, 0);
                  if (pChannelSet)
                  {
                    HWND hwndDlg;
#ifdef DEBUG_BUILD
//                    printf("Zoom-Set pressed!\n");
#endif
                    // Create the Zoom-Set dialog window invisible,
                    // then create the custom control inside it,
                    // reposition and resize its controls,
                    // then make it visible, and process it!

                    // Create the dialog window first!
                    hwndDlg = WinLoadDlg(HWND_DESKTOP, hwnd,
                                         ZoomSetDialogProc, NULLHANDLE,
                                         DLG_SETVISIBLEPART, NULL);
                    if (hwndDlg)
                    {
                      SWP swap;
                      HWND hwndCustom;

                      // Window is created, fine. Now create our custom control inside it!

                      hwndCustom = WinCreateWindow(hwndDlg,                    // hwndParent
                                                   VISIBLEPART_WINDOW_CLASS,   // pszClass
                                                   "Visible part of channel-set", // pszName
                                                   WS_VISIBLE,                 // flStyle
                                                   0,                          // x
                                                   0,                          // y
                                                   100,                        // cx
                                                   50,                         // cy
                                                   hwndDlg,                    // hwndOwner
                                                   HWND_TOP,                   // hwndInsertBehind
                                                   USERCTRL_VISIBLEPART,       // id
                                                   NULL,                       // pCtlData
                                                   NULL);                      // pPresParams

                      // Now try to reload size and position information from INI file
                      if (!WinRestoreWindowPos(WinStateSave_AppName, WinStateSave_ZoomSetWindowKey, hwndDlg))
                      {
                        SWP DlgSwap, ParentSwap;

                        // If could not, then center the window to its parent!
                        if (WinQueryWindowPos(hwndDlg, &DlgSwap))
                          if (WinQueryWindowPos(hwndFrame, &ParentSwap))
                          {
                            // Center dialog box within the parent window
                            int ix, iy;
                            ix = ParentSwap.x + (ParentSwap.cx - DlgSwap.cx)/2;
                            iy = ParentSwap.y + (ParentSwap.cy - DlgSwap.cy)/2;
                            WinSetWindowPos(hwndDlg, HWND_TOP, ix, iy, 0, 0,
                                            SWP_MOVE);
                          }
                      }

                      // Move the window more down if it would be out of the screen
                      WinQueryWindowPos(hwndDlg, &swap);
                      if (swap.y+swap.cy > WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN))
                      {
                        WinSetWindowPos(hwndDlg, HWND_TOP, swap.x,
                                        WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN) - swap.cy, 0, 0, SWP_MOVE);
                        WinQueryWindowPos(hwndFrame, &swap);
                      }
                      if (swap.x+swap.cx > WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN))
                      {
                        if (WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN) - swap.cx >= 0)
                          WinSetWindowPos(hwndDlg, HWND_TOP,
                                          WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN) - swap.cx, swap.y,  0, 0, SWP_MOVE);
                      }

                      // Now that we have the size and position of the dialog,
                      // we can reposition and resize the controls inside it, so it
                      // will look nice in every resolution.
                      ResizeZoomSetDialogControls(hwndDlg);

                      // Set the initial values of controls

                      snprintf(achBuffer, sizeof(achBuffer), "%I64d", pChannelSet->Length);
                      WinSetDlgItemText(hwndDlg, EF_NUMOFSAMPLESINCHANNELSET, achBuffer);

                      snprintf(achBuffer, sizeof(achBuffer), "%I64d", pChannelSet->FirstVisiblePosition);
                      WinSetDlgItemText(hwndDlg, EF_VISIBLEPARTSTARTSAT, achBuffer);

                      snprintf(achBuffer, sizeof(achBuffer), "%I64d", pChannelSet->ZoomCount);
                      WinSetDlgItemText(hwndDlg, EF_NUMOFPIXELS, achBuffer);

                      snprintf(achBuffer, sizeof(achBuffer), "%I64d", pChannelSet->ZoomDenom);
                      WinSetDlgItemText(hwndDlg, EF_NUMOFSAMPLES, achBuffer);

                      WinSendDlgItemMsg(hwndDlg, USERCTRL_VISIBLEPART,
                                        WM_VISIBLEPART_SETSTARTFROM,
                                        (MPARAM) ((ULONG) pChannelSet->FirstVisiblePosition),
                                        (MPARAM) ((ULONG) (pChannelSet->FirstVisiblePosition >> 32)));

                      WinSendDlgItemMsg(hwndDlg, USERCTRL_VISIBLEPART,
                                        WM_VISIBLEPART_SETZOOMCOUNT,
                                        (MPARAM) ((ULONG) pChannelSet->ZoomCount),
                                        (MPARAM) ((ULONG) pChannelSet->ZoomCount >> 32));

                      WinSendDlgItemMsg(hwndDlg, USERCTRL_VISIBLEPART,
                                        WM_VISIBLEPART_SETZOOMDENOM,
                                        (MPARAM) ((ULONG) pChannelSet->ZoomDenom),
                                        (MPARAM) ((ULONG) pChannelSet->ZoomDenom >> 32));

                      WinQueryWindowPos(pChannelSet->hwndChannelSet, &swap);
                      WinSendDlgItemMsg(hwndDlg, USERCTRL_VISIBLEPART,
                                        WM_VISIBLEPART_SETVISIBLEPIXELS,
                                        (MPARAM) swap.cx, (MPARAM) NULL);

                      WinSendDlgItemMsg(hwndDlg, USERCTRL_VISIBLEPART,
                                        WM_VISIBLEPART_SETMAXSAMPLE,
                                        (MPARAM) ((ULONG) pChannelSet->Length),
                                        (MPARAM) ((ULONG) (pChannelSet->Length >> 32)));

                      // Activate and make it visible
                      WinSetWindowPos(hwndDlg, HWND_TOP, 0, 0, 0, 0,
                                      SWP_SHOW | SWP_ACTIVATE | SWP_ZORDER);
                      WinSetFocus(HWND_DESKTOP, hwndDlg);


                      // Now process the window
                      if (WinProcessDlg(hwndDlg)==PB_OK)
                      {
                        WaWESize_t ZoomCount;
                        WaWESize_t ZoomDenom;
                        WaWEPosition_t StartFrom;
                        int iTemp;

#ifdef DEBUG_BUILD
                        printf("[ChannelSetWindowProc] : Modifying visible part of channel-set.\n");
#endif
                        // Fiiine, now read the settings, and modify the
                        // pChannelSet according to the values!

#ifdef DEBUG_BUILD
                        printf("[ChannelSetWindowProc] : Querying settings.\n");
#endif
                        WinQueryDlgItemText(hwndDlg, EF_VISIBLEPARTSTARTSAT, sizeof(achBuffer), achBuffer);
                        iTemp = sscanf(achBuffer, "%I64d", &StartFrom);
                        if ((iTemp!=1) || (StartFrom<0))
                          StartFrom = 0;
                        WinQueryDlgItemText(hwndDlg, EF_NUMOFPIXELS, sizeof(achBuffer), achBuffer);
                        iTemp = sscanf(achBuffer, "%I64d", &ZoomCount);
                        if ((iTemp!=1) || (ZoomCount<=0))
                          ZoomCount = 1;
                        WinQueryDlgItemText(hwndDlg, EF_NUMOFSAMPLES, sizeof(achBuffer), achBuffer);
                        iTemp = sscanf(achBuffer, "%I64d", &ZoomDenom);
                        if ((iTemp!=1) || (ZoomDenom<=0))
                          ZoomDenom = 1;


#ifdef DEBUG_BUILD
                        printf("[ChannelSetWindowProc] : Setting pChannelSet fields (%I64d %I64d %I64d)\n",
                              StartFrom, ZoomCount, ZoomDenom);
#endif

                        pChannelSet->FirstVisiblePosition = StartFrom;
                        pChannelSet->ZoomCount = ZoomCount;
                        pChannelSet->ZoomDenom = ZoomDenom;
                        

#ifdef DEBUG_BUILD
                        printf("[ChannelSetWindowProc] : Updating GUI...\n");
#endif

                        // Reposition scrollbar
                        UpdateChannelSetHScrollBar(pChannelSet);
                        // Update string
                        UpdateChannelSetZoomString(pChannelSet);
                        // Redraw window insider
                        RedrawChannelsInChannelSet(pChannelSet);
#ifdef DEBUG_BUILD
                        printf("[ChannelSetWindowProc] : Fine!\n");
#endif
                      }

                      // Now save size and position info into INI file
                      WinStoreWindowPos(WinStateSave_AppName, WinStateSave_ZoomSetWindowKey, hwndDlg);

                      WinDestroyWindow(hwndDlg);
                    }
                    // That's all for the processing of ZOOMSET button.
                  }
                  break;
		case DID_CHANNELSET_ZOOMIN:
		  {
		    // Cool the ZoomIN button has been pressed!
		    pChannelSet = WinQueryWindowPtr(hwnd, 0);
		    if (pChannelSet)
		    {
		      WaWEPosition_t FirstVisiblePosition, LastVisiblePosition;

		      if (pChannelSet->ZoomDenom>1) pChannelSet->ZoomDenom--;
		      else pChannelSet->ZoomCount++;

		      FirstVisiblePosition = pChannelSet->FirstVisiblePosition;
		      LastVisiblePosition =
			pChannelSet->FirstVisiblePosition +
			pChannelSet->iWidth * pChannelSet->ZoomDenom / pChannelSet->ZoomCount
			-1;
		      
		      if (LastVisiblePosition>=pChannelSet->Length)
		      {
			FirstVisiblePosition-=(LastVisiblePosition-pChannelSet->Length)+1;
			if (FirstVisiblePosition<0) FirstVisiblePosition = 0;
		      }
		      pChannelSet->FirstVisiblePosition = FirstVisiblePosition;
		      
		      // Reposition scrollbar
		      UpdateChannelSetHScrollBar(pChannelSet);
                      // Update string
                      UpdateChannelSetZoomString(pChannelSet);
		      // Redraw window insider
		      RedrawChannelsInChannelSet(pChannelSet);
		    }
                    break;
		  }
		case DID_CHANNELSET_ZOOMOUT:
		  {
		    // The ZoomOut button has been pressed
		    pChannelSet = WinQueryWindowPtr(hwnd, 0);
		    if (pChannelSet)
		    {
		      WaWEPosition_t FirstVisiblePosition, LastVisiblePosition;

		      if (pChannelSet->ZoomCount>1) pChannelSet->ZoomCount--;
		      else pChannelSet->ZoomDenom++;

		      FirstVisiblePosition = pChannelSet->FirstVisiblePosition;
		      LastVisiblePosition =
			pChannelSet->FirstVisiblePosition +
			pChannelSet->iWidth * pChannelSet->ZoomDenom / pChannelSet->ZoomCount
			-1;
		      
		      if (LastVisiblePosition>=pChannelSet->Length)
		      {
			FirstVisiblePosition-=(LastVisiblePosition-pChannelSet->Length)+1;
			if (FirstVisiblePosition<0) FirstVisiblePosition = 0;
		      }
		      pChannelSet->FirstVisiblePosition = FirstVisiblePosition;
		      
		      // Reposition scrollbar
		      UpdateChannelSetHScrollBar(pChannelSet);
                      // Update string
                      UpdateChannelSetZoomString(pChannelSet);
		      // Redraw window insider
		      RedrawChannelsInChannelSet(pChannelSet);
		    }
                    break;
		  }
		default:
		  break;
	      }
              break;
	    }
	  default:
            // We don't know who sent this WM_COMMAND
            break;
	}
        break;
      }
    default:
//      printf("Unhandled message in channelset window: %d\n", msg); fflush(stdout);
      break;
  }
//  printf("WinDef for %d\n", msg);
  return WinDefWindowProc(hwnd, msg, mp1, mp2);
}

void GetChannelSetCombinedTitle(WaWEChannelSet_p pChannelSet, char *pchBuffer, int iBufferSize)
{
  char *pchFileName, *pchTemp;

  if ((!pChannelSet) || (!pchBuffer)) return;

  // Search for filename part in full path!
  pchTemp = pchFileName = pChannelSet->achFileName;
  while (*pchTemp)
  {
    if (*pchTemp=='\\') pchFileName = pchTemp+1;
    pchTemp++;
  }

  if (pchFileName[0])
  {
    // There is some filename
    snprintf(pchBuffer, iBufferSize, "%s%s (%s)",
             pChannelSet->bModified?"*":"",
             pChannelSet->achName,
             pchFileName);
  } else
  {
    // There is no filename
    snprintf(pchBuffer, iBufferSize, "%s%s (no filename)",
             pChannelSet->bModified?"*":"",
             pChannelSet->achName);
  }
}

void GetChannelSetCombinedFormat(WaWEChannelSet_p pChannelSet, char *pchBuffer, int iBufferSize)
{
  if ((!pChannelSet) || (!pchBuffer)) return;

  snprintf(pchBuffer, iBufferSize, "%d Ch., %d Hz, %d bits, %s",
           pChannelSet->iNumOfChannels,
           pChannelSet->OriginalFormat.iFrequency,
           pChannelSet->OriginalFormat.iBits,
           pChannelSet->OriginalFormat.iIsSigned?"Signed":"Unsigned");
}

void GetChannelSetLengthString(WaWEChannelSet_p pChannelSet, char *pchBuffer, int iBufferSize)
{
  if ((!pChannelSet) || (!pchBuffer)) return;

  if (WaWEConfiguration.iTimeFormat == WAWE_TIMEFORMAT_SAMPLES)
  {
    snprintf(pchBuffer, iBufferSize, "Length: %I64d sample(s)",
             pChannelSet->Length);
  } else
  if (WaWEConfiguration.iTimeFormat == WAWE_TIMEFORMAT_HHMMSSMSEC)
  {
    __int64 i64HH, i64MM, i64SS, i64msec;

    i64msec = pChannelSet->Length*1000/pChannelSet->OriginalFormat.iFrequency;

    i64SS = i64msec / 1000;
    i64msec = i64msec % 1000;

    i64MM = i64SS / 60;
    i64SS = i64SS % 60;

    i64HH = i64MM / 60;
    i64MM = i64MM % 60;

    snprintf(pchBuffer, iBufferSize, "Length: %I64d:%02I64d:%02I64d.%03I64d",
             i64HH, i64MM, i64SS, i64msec);
  } else
  {
    snprintf(pchBuffer, iBufferSize, "Length: %I64d msec",
             (pChannelSet->Length*1000/pChannelSet->OriginalFormat.iFrequency));
  }
}

void GetChannelSetZoom(WaWEChannelSet_p pChannelSet, char *pchBuffer, int iBufferSize)
{
  if ((!pChannelSet) || (!pchBuffer)) return;

  snprintf(pchBuffer, iBufferSize, "Zoom: %I64d/%I64d",
           pChannelSet->ZoomCount, pChannelSet->ZoomDenom);
}

void GetChannelSetPointerPosition(WaWEChannelSet_p pChannelSet, char *pchBuffer, int iBufferSize)
{
  if ((!pChannelSet) || (!pchBuffer)) return;

  if (WaWEConfiguration.iTimeFormat == WAWE_TIMEFORMAT_SAMPLES)
  {
    snprintf(pchBuffer, iBufferSize, "Pointer is at %I64d. sample",
             pChannelSet->PointerPosition);

  } else
  if (WaWEConfiguration.iTimeFormat == WAWE_TIMEFORMAT_HHMMSSMSEC)
  {
    __int64 i64HH, i64MM, i64SS, i64msec;

    i64msec = pChannelSet->PointerPosition*1000/pChannelSet->OriginalFormat.iFrequency;

    i64SS = i64msec / 1000;
    i64msec = i64msec % 1000;

    i64MM = i64SS / 60;
    i64SS = i64SS % 60;

    i64HH = i64MM / 60;
    i64MM = i64MM % 60;

    snprintf(pchBuffer, iBufferSize, "Pointer is at %I64d:%02I64d:%02I64d.%03I64d",
             i64HH, i64MM, i64SS, i64msec);
  } else
  {
    snprintf(pchBuffer, iBufferSize, "Pointer is at %I64d msec",
             pChannelSet->PointerPosition*1000/pChannelSet->OriginalFormat.iFrequency);
  }
}

void UpdateChannelSetPointerPositionString(WaWEChannelSet_p pChannelSet)
{
  char *pchTemp;

  if (!pChannelSet) return;

  pchTemp = (char *) dbg_malloc(128);
  if (pchTemp)
  {
    GetChannelSetPointerPosition(pChannelSet, pchTemp, 128);
    WinSetDlgItemText(WinWindowFromID(pChannelSet->hwndChannelSet, DID_CHANNELSET_HEADERBG), DID_CHANNELSET_POINTERPOSITION, pchTemp);
    dbg_free(pchTemp);
  }
}

void UpdateChannelSetNameString(WaWEChannelSet_p pChannelSet)
{
  char *pchTemp;

  if (!pChannelSet) return;

  pchTemp = dbg_malloc(WAWE_CHANNELSET_NAME_LEN + WAWE_CHANNELSET_FILENAME_LEN + 10);
  if (pchTemp)
  {
    GetChannelSetCombinedTitle(pChannelSet, pchTemp, WAWE_CHANNELSET_NAME_LEN + WAWE_CHANNELSET_FILENAME_LEN + 10);
    WinSetDlgItemText(WinWindowFromID(pChannelSet->hwndChannelSet,
                                      DID_CHANNELSET_HEADERBG),
                      DID_CHANNELSET_NAME,
                      pchTemp);
    dbg_free(pchTemp);
  }
}

void UpdateChannelSetFormatString(WaWEChannelSet_p pChannelSet)
{
  char *pchTemp;

  if (!pChannelSet) return;

  pchTemp = dbg_malloc(WAWE_CHANNELSET_NAME_LEN+10);
  if (pchTemp)
  {
    GetChannelSetCombinedFormat(pChannelSet, pchTemp, WAWE_CHANNELSET_NAME_LEN+10);
    WinSetDlgItemText(WinWindowFromID(pChannelSet->hwndChannelSet,
                                      DID_CHANNELSET_HEADERBG),
                      DID_CHANNELSET_FORMAT,
                      pchTemp);
    dbg_free(pchTemp);
  }
}

void UpdateChannelSetLengthString(WaWEChannelSet_p pChannelSet)
{
  char *pchTemp;

  if (!pChannelSet) return;

  pchTemp = dbg_malloc(128);
  if (pchTemp)
  {
    GetChannelSetLengthString(pChannelSet, pchTemp, 128);
    WinSetDlgItemText(WinWindowFromID(pChannelSet->hwndChannelSet,
                                      DID_CHANNELSET_HEADERBG),
                      DID_CHANNELSET_LENGTH,
                      pchTemp);
    dbg_free(pchTemp);
  }
}

void UpdateChannelSetZoomString(WaWEChannelSet_p pChannelSet)
{
  char *pchTemp;

  if (!pChannelSet) return;

  pchTemp = dbg_malloc(128);
  if (pchTemp)
  {
    GetChannelSetZoom(pChannelSet, pchTemp, 128);
    WinSetDlgItemText(WinWindowFromID(pChannelSet->hwndChannelSet,
				      DID_CHANNELSET_HEADERBG),
		      DID_CHANNELSET_ZOOM,
		      pchTemp);
    dbg_free(pchTemp);
  }
}


void ResizeAndRepositionChannelSetHeaderBgWindowChildren(HWND hwnd, WaWEChannelSet_p pChannelSet)
{
  SWP swp;
  int iDisabledWindowUpdate=0;

  if (WinIsWindowVisible(hwnd))
  {
    WinEnableWindowUpdate(hwnd, FALSE);
    iDisabledWindowUpdate = 1;
  }

  // Channel-set Name and filename combo into top-left corner
  WinSetWindowPos(WinWindowFromID(hwnd, DID_CHANNELSET_NAME),
                  HWND_TOP,
		  HEADERBG_FRAME_SPACE,                                                       // PosX
		  2*iDefaultTextWindowHeight+HEADERBG_FRAME_SPACE+2*HEADERBG_SPACE_BETWEEN_LINES,   // PosY
		  (pChannelSet->iWidth-2*HEADERBG_FRAME_SPACE)/2,                             // Width
		  iDefaultTextWindowHeight,                                                         // Height
                  SWP_MOVE | SWP_SIZE);

  // Channel-set parameters into top-right corner
  WinSetWindowPos(WinWindowFromID(hwnd, DID_CHANNELSET_FORMAT),
                  HWND_TOP,
		  pChannelSet->iWidth/2,                                                      // PosX
		  2*iDefaultTextWindowHeight+HEADERBG_FRAME_SPACE+2*HEADERBG_SPACE_BETWEEN_LINES,   // PosY
		  pChannelSet->iWidth/2+pChannelSet->iWidth%2-HEADERBG_FRAME_SPACE,           // Width
		  iDefaultTextWindowHeight,                                                         // Height
                  SWP_MOVE | SWP_SIZE);

  // Channel-set length
  WinSetWindowPos(WinWindowFromID(hwnd, DID_CHANNELSET_LENGTH),
                  HWND_TOP,
		  HEADERBG_FRAME_SPACE,                                                       // PosX
		  1*iDefaultTextWindowHeight+HEADERBG_FRAME_SPACE+1*HEADERBG_SPACE_BETWEEN_LINES,   // PosY
		  (pChannelSet->iWidth-HEADERBG_SPACE_BETWEEN_ROWS)/2-HEADERBG_FRAME_SPACE,   // Width
		  iDefaultTextWindowHeight,                                                         // Height
                  SWP_MOVE | SWP_SIZE);

  // Channel Zoom-Set
  WinSetWindowPos(WinWindowFromID(hwnd, DID_CHANNELSET_ZOOMSET),
                  HWND_TOP,
		  pChannelSet->iWidth-HEADERBG_FRAME_SPACE-iDefaultTextWindowHeight-HEADERBG_SPACE_BETWEEN_ROWS,                // PosX
		  1*iDefaultTextWindowHeight+HEADERBG_FRAME_SPACE+HEADERBG_SPACE_BETWEEN_LINES/2,   // PosY
                  iDefaultTextWindowHeight+HEADERBG_SPACE_BETWEEN_ROWS, // Width
                  iDefaultTextWindowHeight+HEADERBG_SPACE_BETWEEN_LINES, // Height
                  SWP_MOVE | SWP_SIZE);

  // Channel Zoom-Out
  WinSetWindowPos(WinWindowFromID(hwnd, DID_CHANNELSET_ZOOMOUT),
                  HWND_TOP,
		  pChannelSet->iWidth-HEADERBG_FRAME_SPACE-HEADERBG_SPACE_BETWEEN_ROWS-2*(iDefaultTextWindowHeight+HEADERBG_SPACE_BETWEEN_ROWS), // PosX
		  1*iDefaultTextWindowHeight+HEADERBG_FRAME_SPACE+HEADERBG_SPACE_BETWEEN_LINES/2,   // PosY
                  iDefaultTextWindowHeight+HEADERBG_SPACE_BETWEEN_ROWS, // Width
                  iDefaultTextWindowHeight+HEADERBG_SPACE_BETWEEN_LINES, // Height
                  SWP_MOVE | SWP_SIZE);
  // Channel Zoom-In
  WinSetWindowPos(WinWindowFromID(hwnd, DID_CHANNELSET_ZOOMIN),
                  HWND_TOP,
		  pChannelSet->iWidth-HEADERBG_FRAME_SPACE-2*HEADERBG_SPACE_BETWEEN_ROWS-3*(iDefaultTextWindowHeight+HEADERBG_SPACE_BETWEEN_ROWS), // PosX
		  1*iDefaultTextWindowHeight+HEADERBG_FRAME_SPACE+HEADERBG_SPACE_BETWEEN_LINES/2,   // PosY
                  iDefaultTextWindowHeight+HEADERBG_SPACE_BETWEEN_ROWS, // Width
                  iDefaultTextWindowHeight+HEADERBG_SPACE_BETWEEN_LINES, // Height
                  SWP_MOVE | SWP_SIZE);

  // Channel-set Zoom
  WinSetWindowPos(WinWindowFromID(hwnd, DID_CHANNELSET_ZOOM),
                  HWND_TOP,
		  pChannelSet->iWidth/2+HEADERBG_SPACE_BETWEEN_ROWS,                          // PosX
		  1*iDefaultTextWindowHeight+HEADERBG_FRAME_SPACE+1*HEADERBG_SPACE_BETWEEN_LINES,   // PosY
		  pChannelSet->iWidth/2+pChannelSet->iWidth%2-HEADERBG_FRAME_SPACE-HEADERBG_SPACE_BETWEEN_ROWS*4-(iDefaultTextWindowHeight+HEADERBG_SPACE_BETWEEN_ROWS)*3, // Width
		  iDefaultTextWindowHeight,                                                         // Height
                  SWP_MOVE | SWP_SIZE);

  // Channel-set Pointer position
  WinSetWindowPos(WinWindowFromID(hwnd, DID_CHANNELSET_POINTERPOSITION),
                  HWND_TOP,
		  HEADERBG_FRAME_SPACE,                        // PosX
		  HEADERBG_FRAME_SPACE,                        // PosY
		  pChannelSet->iWidth/2-HEADERBG_FRAME_SPACE,  // Width
		  iDefaultTextWindowHeight,                          // Height
		  SWP_MOVE | SWP_SIZE);

  // Work with channel-set checkbox
  WinQueryWindowPos(WinWindowFromID(hwnd, DID_CHANNELSET_WORKWITHCHSET), &swp);
  WinSetWindowPos(WinWindowFromID(hwnd, DID_CHANNELSET_WORKWITHCHSET), HWND_TOP,
		  pChannelSet->iWidth-HEADERBG_FRAME_SPACE-swp.cx,
		  HEADERBG_FRAME_SPACE,
		  swp.cx,
		  iDefaultTextWindowHeight,
                  SWP_MOVE | SWP_SIZE);

  if (iDisabledWindowUpdate)
    WinEnableWindowUpdate(hwnd, TRUE);
}

void CreateChannelSetBgChildren(HWND hwndBg, HWND hwndOwner, WaWEChannelSet_p pChannelSet)
{
  HWND hwnd;
  long lColor;
  char *pchBuffer;
  SWP swp;

  pchBuffer = (char *) dbg_malloc(WAWE_CHANNELSET_NAME_LEN + WAWE_CHANNELSET_FILENAME_LEN + 10);
  if (pchBuffer)
  {
    GetChannelSetCombinedTitle(pChannelSet, pchBuffer, WAWE_CHANNELSET_NAME_LEN + WAWE_CHANNELSET_FILENAME_LEN + 10);
    // Channel-set Name and filename combo into top-left corner
    hwnd = WinCreateWindow(hwndBg,
                           WC_STATIC,
                           pchBuffer,
                           WS_VISIBLE | SS_TEXT | DT_LEFT | DT_VCENTER,
                           HEADERBG_FRAME_SPACE,                                                       // PosX
                           2*iDefaultTextWindowHeight+HEADERBG_FRAME_SPACE+2*HEADERBG_SPACE_BETWEEN_LINES,   // PosY
                           pChannelSet->iWidth/2-HEADERBG_FRAME_SPACE,                                 // Width
                           iDefaultTextWindowHeight,                                                         // Height
                           hwndOwner,
                           HWND_TOP,
                           DID_CHANNELSET_NAME,
                           NULL,
                           NULL);
    SubclassStaticClass(hwnd);

//    lColor = WinQuerySysColor(HWND_DESKTOP, SYSCLR_BUTTONMIDDLE, 0);
    lColor = CHANNELSET_HEADER_FIRSTLINE_INACTIVE_BG_COLOR;
    WinSetPresParam(hwnd, PP_BACKGROUNDCOLOR, sizeof(lColor), &lColor);
    lColor = CHANNELSET_HEADER_FIRSTLINE_INACTIVE_FG_COLOR;
    WinSetPresParam(hwnd, PP_FOREGROUNDCOLOR, sizeof(lColor), &lColor);
    // make it bold!
    strcpy(pchBuffer, DEFAULT_FONT_SIZENAME);
    WinQueryPresParam(hwnd, PP_FONTNAMESIZE, 0,
                      NULL,
                      WAWE_CHANNELSET_NAME_LEN + WAWE_CHANNELSET_FILENAME_LEN + 10,
                      pchBuffer,
                      0);

    strcat(pchBuffer,".Bold");
    WinSetPresParam(hwnd, PP_FONTNAMESIZE, strlen(pchBuffer)+1, pchBuffer);

    // Channel-set parameters into top-right corner
    GetChannelSetCombinedFormat(pChannelSet, pchBuffer, WAWE_CHANNELSET_NAME_LEN + WAWE_CHANNELSET_FILENAME_LEN + 10);
    hwnd = WinCreateWindow(hwndBg,
                           WC_STATIC,
                           pchBuffer,
                           WS_VISIBLE | SS_TEXT | DT_RIGHT | DT_VCENTER,
                           pChannelSet->iWidth/2,                                                      // PosX
                           2*iDefaultTextWindowHeight+HEADERBG_FRAME_SPACE+2*HEADERBG_SPACE_BETWEEN_LINES,   // PosY
                           pChannelSet->iWidth/2+pChannelSet->iWidth%2-HEADERBG_FRAME_SPACE,           // Width
                           iDefaultTextWindowHeight,                                                         // Height
                           hwndOwner,
                           HWND_TOP,
                           DID_CHANNELSET_FORMAT,
                           NULL,
                           NULL);
    SubclassStaticClass(hwnd);
  
//    lColor = WinQuerySysColor(HWND_DESKTOP, SYSCLR_BUTTONMIDDLE, 0);
    lColor = CHANNELSET_HEADER_FIRSTLINE_INACTIVE_BG_COLOR;
    WinSetPresParam(hwnd, PP_BACKGROUNDCOLOR, sizeof(lColor), &lColor);
    lColor = CHANNELSET_HEADER_FIRSTLINE_INACTIVE_FG_COLOR;
    WinSetPresParam(hwnd, PP_FOREGROUNDCOLOR, sizeof(lColor), &lColor);
    // make it bold!
    strcpy(pchBuffer, DEFAULT_FONT_SIZENAME);
    WinQueryPresParam(hwnd, PP_FONTNAMESIZE, 0,
                      NULL,
                      WAWE_CHANNELSET_NAME_LEN + WAWE_CHANNELSET_FILENAME_LEN + 10,
                      pchBuffer,
                      0);
    strcat(pchBuffer,".Bold");
    WinSetPresParam(hwnd, PP_FONTNAMESIZE, strlen(pchBuffer)+1, pchBuffer);

    // Channel-set length
    GetChannelSetLengthString(pChannelSet, pchBuffer, WAWE_CHANNELSET_NAME_LEN + WAWE_CHANNELSET_FILENAME_LEN + 10);
    hwnd = WinCreateWindow(hwndBg,
                           WC_STATIC,
                           pchBuffer,
                           WS_VISIBLE | SS_TEXT | DT_LEFT | DT_VCENTER,
                           HEADERBG_FRAME_SPACE,                                                       // PosX
                           1*iDefaultTextWindowHeight+HEADERBG_FRAME_SPACE+1*HEADERBG_SPACE_BETWEEN_LINES,   // PosY
                           (pChannelSet->iWidth-2*HEADERBG_FRAME_SPACE-HEADERBG_SPACE_BETWEEN_ROWS)/2, // Width
                           iDefaultTextWindowHeight,                                                         // Height
                           hwndOwner,
                           HWND_TOP,
                           DID_CHANNELSET_LENGTH,
                           NULL,
                           NULL);
    SubclassStaticClass(hwnd);

    lColor = WinQuerySysColor(HWND_DESKTOP, SYSCLR_BUTTONMIDDLE, 0);
//    lColor = RGB_YELLOW;
    WinSetPresParam(hwnd, PP_BACKGROUNDCOLOR, sizeof(lColor), &lColor);
    lColor = RGB_BLACK;
    WinSetPresParam(hwnd, PP_FOREGROUNDCOLOR, sizeof(lColor), &lColor);

    // Channel Zoom-Set
    hwnd = WinCreateWindow(hwndBg,
                           WC_BUTTON,
                           "?",
                           WS_VISIBLE | BS_PUSHBUTTON | BS_NOPOINTERFOCUS,
                           pChannelSet->iWidth-HEADERBG_FRAME_SPACE-iDefaultTextWindowHeight-HEADERBG_SPACE_BETWEEN_ROWS,           // PosX
                           1*iDefaultTextWindowHeight+HEADERBG_FRAME_SPACE+HEADERBG_SPACE_BETWEEN_LINES/2,   // PosY
                           iDefaultTextWindowHeight+HEADERBG_SPACE_BETWEEN_ROWS, // Width
                           iDefaultTextWindowHeight+HEADERBG_SPACE_BETWEEN_LINES, // Height
                           hwndOwner,
                           HWND_TOP,
                           DID_CHANNELSET_ZOOMSET,
                           NULL,
                           NULL);
    SubclassPublicClass(hwnd);

    // Channel Zoom-Out
    hwnd = WinCreateWindow(hwndBg,
                           WC_BUTTON,
                           "-",
                           WS_VISIBLE | BS_PUSHBUTTON | BS_NOPOINTERFOCUS,
                           pChannelSet->iWidth-HEADERBG_FRAME_SPACE-HEADERBG_SPACE_BETWEEN_ROWS-2*(iDefaultTextWindowHeight+HEADERBG_SPACE_BETWEEN_ROWS),            // PosX
                           1*iDefaultTextWindowHeight+HEADERBG_FRAME_SPACE+HEADERBG_SPACE_BETWEEN_LINES/2,   // PosY
                           iDefaultTextWindowHeight+HEADERBG_SPACE_BETWEEN_ROWS, // Width
                           iDefaultTextWindowHeight+HEADERBG_SPACE_BETWEEN_LINES, // Height
                           hwndOwner,
                           HWND_TOP,
                           DID_CHANNELSET_ZOOMOUT,
                           NULL,
                           NULL);
    SubclassPublicClass(hwnd);

    // Channel Zoom-In
    hwnd = WinCreateWindow(hwndBg,
                           WC_BUTTON,
                           "+",
                           WS_VISIBLE | BS_PUSHBUTTON | BS_NOPOINTERFOCUS,
                           pChannelSet->iWidth-HEADERBG_FRAME_SPACE-2*HEADERBG_SPACE_BETWEEN_ROWS-3*(iDefaultTextWindowHeight+HEADERBG_SPACE_BETWEEN_ROWS),            // PosX
                           1*iDefaultTextWindowHeight+HEADERBG_FRAME_SPACE+HEADERBG_SPACE_BETWEEN_LINES/2,   // PosY
                           iDefaultTextWindowHeight+HEADERBG_SPACE_BETWEEN_ROWS, // Width
                           iDefaultTextWindowHeight+HEADERBG_SPACE_BETWEEN_LINES, // Height
                           hwndOwner,
                           HWND_TOP,
                           DID_CHANNELSET_ZOOMIN,
                           NULL,
                           NULL);
    SubclassPublicClass(hwnd);

    // Channel-set Zoom
    GetChannelSetZoom(pChannelSet, pchBuffer, WAWE_CHANNELSET_NAME_LEN + WAWE_CHANNELSET_FILENAME_LEN + 10);

    hwnd = WinCreateWindow(hwndBg,
                           WC_STATIC,
                           pchBuffer,
                           WS_VISIBLE | SS_TEXT | DT_RIGHT | DT_VCENTER,
                           (pChannelSet->iWidth+HEADERBG_SPACE_BETWEEN_ROWS)/2,                        // PosX
                           1*iDefaultTextWindowHeight+HEADERBG_FRAME_SPACE+1*HEADERBG_SPACE_BETWEEN_LINES,   // PosY
			   pChannelSet->iWidth/2+pChannelSet->iWidth%2-
			   HEADERBG_FRAME_SPACE-HEADERBG_SPACE_BETWEEN_ROWS*4-(iDefaultTextWindowHeight+HEADERBG_SPACE_BETWEEN_ROWS)*3,    // Width
                           iDefaultTextWindowHeight,                                                         // Height
                           hwndOwner,
                           HWND_TOP,
                           DID_CHANNELSET_ZOOM,
                           NULL,
                           NULL);
    SubclassStaticClass(hwnd);

    lColor = WinQuerySysColor(HWND_DESKTOP, SYSCLR_BUTTONMIDDLE, 0);
//    lColor = RGB_YELLOW;
    WinSetPresParam(hwnd, PP_BACKGROUNDCOLOR, sizeof(lColor), &lColor);
    lColor = RGB_BLACK;
    WinSetPresParam(hwnd, PP_FOREGROUNDCOLOR, sizeof(lColor), &lColor);

    // Channel-set Pointer position
    GetChannelSetPointerPosition(pChannelSet, pchBuffer, WAWE_CHANNELSET_NAME_LEN + WAWE_CHANNELSET_FILENAME_LEN + 10);
    hwnd = WinCreateWindow(hwndBg,
                           WC_STATIC,
                           pchBuffer,
                           WS_VISIBLE | SS_TEXT | DT_LEFT | DT_VCENTER,
                           HEADERBG_FRAME_SPACE,                        // PosX
                           HEADERBG_FRAME_SPACE,                        // PosY
                           pChannelSet->iWidth/2-HEADERBG_FRAME_SPACE,  // Width
                           iDefaultTextWindowHeight,                          // Height
                           hwndOwner,
                           HWND_TOP,
                           DID_CHANNELSET_POINTERPOSITION,
                           NULL,
                           NULL);
    SubclassStaticClass(hwnd);

    lColor = WinQuerySysColor(HWND_DESKTOP, SYSCLR_BUTTONMIDDLE, 0);
//    lColor = RGB_YELLOW;
    WinSetPresParam(hwnd, PP_BACKGROUNDCOLOR, sizeof(lColor), &lColor);
    lColor = RGB_BLACK;
    WinSetPresParam(hwnd, PP_FOREGROUNDCOLOR, sizeof(lColor), &lColor);

    // Work With Channelset checkbox
    // Note that we make it autosize, to get the width it needs, then reposition it!

    hwnd = WinCreateWindow(hwndBg,
                           WC_BUTTON,
                           "Work with channel-set",
			   WS_VISIBLE | BS_AUTOCHECKBOX | BS_NOPOINTERFOCUS | BS_AUTOSIZE,
                           0, 0, -1, -1,
                           hwndOwner,
                           HWND_TOP,
                           DID_CHANNELSET_WORKWITHCHSET,
                           NULL,
                           NULL);
    SubclassPublicClass(hwnd);

    // Then set its new size/position
    WinQueryWindowPos(hwnd, &swp);
    WinSetWindowPos(hwnd, HWND_TOP,
		    pChannelSet->iWidth-HEADERBG_FRAME_SPACE-swp.cx,
		    HEADERBG_FRAME_SPACE,
		    swp.cx,
		    iDefaultTextWindowHeight,
		    SWP_MOVE | SWP_SIZE);

    // Set its initial state
    WinSendMsg(hwnd, (ULONG) BM_SETCHECK, (MPARAM) pChannelSet->bWorkWithChannelSet, (MPARAM) NULL);

    lColor = WinQuerySysColor(HWND_DESKTOP, SYSCLR_BUTTONMIDDLE, 0);
//    lColor = RGB_YELLOW;
    WinSetPresParam(hwnd, PP_BACKGROUNDCOLOR, sizeof(lColor), &lColor);
    lColor = RGB_BLACK;
    WinSetPresParam(hwnd, PP_FOREGROUNDCOLOR, sizeof(lColor), &lColor);

    dbg_free(pchBuffer);
  }
}

void GetChannelSelectionString(WaWEChannel_p pChannel, char *pchBuffer, int iBufferSize)
{
  if ((!pChannel) || (!pchBuffer)) return;

  if (WaWEConfiguration.iTimeFormat == WAWE_TIMEFORMAT_SAMPLES)
  {
    snprintf(pchBuffer, iBufferSize, "Selected %I64d sample(s) in %d piece(s)",
             pChannel->ShownSelectedLength,
             pChannel->iShownSelectedPieces);
  } else
  if (WaWEConfiguration.iTimeFormat == WAWE_TIMEFORMAT_HHMMSSMSEC)
  {
    __int64 i64HH, i64MM, i64SS, i64msec;

    i64msec = pChannel->ShownSelectedLength*1000/pChannel->VirtualFormat.iFrequency;

    i64SS = i64msec / 1000;
    i64msec = i64msec % 1000;

    i64MM = i64SS / 60;
    i64SS = i64SS % 60;

    i64HH = i64MM / 60;
    i64MM = i64MM % 60;

    snprintf(pchBuffer, iBufferSize, "Selected %I64d:%02I64d:%02I64d.%03I64d in %d piece(s)",
             i64HH, i64MM, i64SS, i64msec,
             pChannel->iShownSelectedPieces);
  } else
  {
    snprintf(pchBuffer, iBufferSize, "Selected %I64d msec in %d piece(s)",
             pChannel->ShownSelectedLength*1000/pChannel->VirtualFormat.iFrequency,
             pChannel->iShownSelectedPieces);
  }
}

void GetChannelPositionString(WaWEChannel_p pChannel, char *pchBuffer, int iBufferSize)
{
  if ((!pChannel) || (!pchBuffer)) return;

  if (WaWEConfiguration.iTimeFormat == WAWE_TIMEFORMAT_SAMPLES)
  {
    snprintf(pchBuffer, iBufferSize, "Position: %I64d. sample",
             pChannel->CurrentPosition);
  } else
  if (WaWEConfiguration.iTimeFormat == WAWE_TIMEFORMAT_HHMMSSMSEC)
  {
    __int64 i64HH, i64MM, i64SS, i64msec;

    i64msec = pChannel->CurrentPosition*1000/pChannel->VirtualFormat.iFrequency;

    i64SS = i64msec / 1000;
    i64msec = i64msec % 1000;

    i64MM = i64SS / 60;
    i64SS = i64SS % 60;

    i64HH = i64MM / 60;
    i64MM = i64MM % 60;

    snprintf(pchBuffer, iBufferSize, "Position: %I64d:%02I64d:%02I64d.%03I64d",
             i64HH, i64MM, i64SS, i64msec);
             
  } else
  {
    snprintf(pchBuffer, iBufferSize, "Position: %I64d msec",
             pChannel->CurrentPosition*1000/pChannel->VirtualFormat.iFrequency);
  }
}

void UpdateChannelNameString(WaWEChannel_p pChannel)
{
  WaWEChannelSet_p pChannelSet;

  if (!pChannel) return;
  pChannelSet = pChannel->pParentChannelSet;

  WinSetDlgItemText(WinWindowFromID(pChannelSet->hwndChannelSet,
                                    DID_CHANNELSET_CH_HEADERBG_BASE + pChannel->iChannelID),
                    DID_CHANNELSET_CH_NAME_BASE + pChannel->iChannelID,
                    pChannel->achName);
}

void UpdateChannelSelectionString(WaWEChannel_p pChannel)
{
  char *pchTemp;
  WaWEChannelSet_p pChannelSet;

  if (!pChannel) return;
  pChannelSet = pChannel->pParentChannelSet;

  pchTemp = dbg_malloc(128);
  if (pchTemp)
  {
    GetChannelSelectionString(pChannel, pchTemp, 128);
    WinSetDlgItemText(WinWindowFromID(pChannelSet->hwndChannelSet,
				      DID_CHANNELSET_CH_HEADERBG_BASE + pChannel->iChannelID),
		      DID_CHANNELSET_CH_SELECTED_BASE + pChannel->iChannelID,
		      pchTemp);
    dbg_free(pchTemp);
  }
}

void UpdateChannelPositionString(WaWEChannel_p pChannel)
{
  char *pchTemp;
  WaWEChannelSet_p pChannelSet;

  if (!pChannel) return;
  pChannelSet = pChannel->pParentChannelSet;

  pchTemp = dbg_malloc(128);
  if (pchTemp)
  {
    GetChannelPositionString(pChannel, pchTemp, 128);
    WinSetDlgItemText(WinWindowFromID(pChannelSet->hwndChannelSet,
				      DID_CHANNELSET_CH_HEADERBG_BASE + pChannel->iChannelID),
		      DID_CHANNELSET_CH_POSITION_BASE + pChannel->iChannelID,
		      pchTemp);
    dbg_free(pchTemp);
  }
}

void ResizeAndRepositionChannelHeaderBgWindowChildren(HWND hwnd, WaWEChannel_p pChannel, WaWEChannelSet_p pChannelSet)
{
  int iDisabledWindowUpdate=0;

  if (WinIsWindowVisible(hwnd))
  {
    WinEnableWindowUpdate(hwnd, FALSE);
    iDisabledWindowUpdate = 1;
  }

  // Channel Name into top-left corner
  WinSetWindowPos(WinWindowFromID(hwnd, DID_CHANNELSET_CH_NAME_BASE+pChannel->iChannelID),
		  HWND_TOP,
		  HEADERBG_FRAME_SPACE,                                                   // PosX
		  iDefaultTextWindowHeight+HEADERBG_FRAME_SPACE+HEADERBG_SPACE_BETWEEN_LINES,   // PosY
		  pChannelSet->iWidth-2*HEADERBG_FRAME_SPACE,                             // Width
		  iDefaultTextWindowHeight,                                                     // Height
		  SWP_MOVE | SWP_SIZE);

  // Current position
  WinSetWindowPos(WinWindowFromID(hwnd, DID_CHANNELSET_CH_POSITION_BASE + pChannel->iChannelID),
		  HWND_TOP,
		  HEADERBG_FRAME_SPACE,                        // PosX
		  HEADERBG_FRAME_SPACE,                        // PosY
		  (pChannelSet->iWidth-2*HEADERBG_FRAME_SPACE-HEADERBG_SPACE_BETWEEN_ROWS)/2, // Width
		  iDefaultTextWindowHeight,                                                         // Height
		  SWP_MOVE | SWP_SIZE);

  // Channel selection
  WinSetWindowPos(WinWindowFromID(hwnd, DID_CHANNELSET_CH_SELECTED_BASE + pChannel->iChannelID),
                  HWND_TOP,
		  (pChannelSet->iWidth-HEADERBG_SPACE_BETWEEN_ROWS)/2+HEADERBG_SPACE_BETWEEN_ROWS,// PosX
		  HEADERBG_FRAME_SPACE,                                                       // PosY
		  (pChannelSet->iWidth-HEADERBG_SPACE_BETWEEN_ROWS)/2+
		  (pChannelSet->iWidth-HEADERBG_SPACE_BETWEEN_ROWS)%2-HEADERBG_FRAME_SPACE,   // Width
		  iDefaultTextWindowHeight,                                                         // Height
                  SWP_MOVE | SWP_SIZE);

  if (iDisabledWindowUpdate)
    WinEnableWindowUpdate(hwnd, TRUE);
}

void CreateChannelBgChildren(HWND hwndBg, WaWEChannel_p pChannel, WaWEChannelSet_p pChannelSet)
{
  HWND hwnd;
  long lColor;
  char *pchBuffer;

  pchBuffer = (char *) dbg_malloc(512);
  if (pchBuffer)
  {
    // Channel Name into top-left corner
    hwnd = WinCreateWindow(hwndBg,
                           WC_STATIC,
                           pChannel->achName,
                           WS_VISIBLE | SS_TEXT | DT_LEFT | DT_VCENTER,
                           HEADERBG_FRAME_SPACE,                                                   // PosX
                           iDefaultTextWindowHeight+HEADERBG_FRAME_SPACE+HEADERBG_SPACE_BETWEEN_LINES,   // PosY
                           pChannelSet->iWidth-2*HEADERBG_FRAME_SPACE,                             // Width
                           iDefaultTextWindowHeight,                                                     // Height
                           hwndBg,
                           HWND_TOP,
                           DID_CHANNELSET_CH_NAME_BASE+pChannel->iChannelID,
                           NULL,
                           NULL);
    SubclassStaticClass(hwnd);

    lColor = WinQuerySysColor(HWND_DESKTOP, SYSCLR_BUTTONMIDDLE, 0);
    lColor = RGB_YELLOW+0xC0; // Very pale yellow
    WinSetPresParam(hwnd, PP_BACKGROUNDCOLOR, sizeof(lColor), &lColor);
    lColor = RGB_BLACK;
    WinSetPresParam(hwnd, PP_FOREGROUNDCOLOR, sizeof(lColor), &lColor);
  
    // Channel Current Position
    GetChannelPositionString(pChannel, pchBuffer, 512);
    hwnd = WinCreateWindow(hwndBg,
                           WC_STATIC,
                           pchBuffer,
                           WS_VISIBLE | SS_TEXT | DT_LEFT | DT_VCENTER,
                           HEADERBG_FRAME_SPACE,                        // PosX
                           HEADERBG_FRAME_SPACE,                        // PosY
                           (pChannelSet->iWidth-2*HEADERBG_FRAME_SPACE-HEADERBG_SPACE_BETWEEN_ROWS)/2, // Width
                           iDefaultTextWindowHeight,                                                         // Height
                           hwndBg,
                           HWND_TOP,
                           DID_CHANNELSET_CH_POSITION_BASE + pChannel->iChannelID,
                           NULL,
                           NULL);
    SubclassStaticClass(hwnd);
  
    lColor = WinQuerySysColor(HWND_DESKTOP, SYSCLR_BUTTONMIDDLE, 0);
//    lColor = RGB_YELLOW;
    WinSetPresParam(hwnd, PP_BACKGROUNDCOLOR, sizeof(lColor), &lColor);
    lColor = RGB_BLACK;
    WinSetPresParam(hwnd, PP_FOREGROUNDCOLOR, sizeof(lColor), &lColor);

    // Channel selection
    GetChannelSelectionString(pChannel, pchBuffer, 512);
    hwnd = WinCreateWindow(hwndBg,
                           WC_STATIC,
                           pchBuffer,
                           WS_VISIBLE | SS_TEXT | DT_RIGHT | DT_VCENTER,
                           (pChannelSet->iWidth-HEADERBG_SPACE_BETWEEN_ROWS)/2+HEADERBG_SPACE_BETWEEN_ROWS,// PosX
                           HEADERBG_FRAME_SPACE,                                                       // PosY
			   (pChannelSet->iWidth-HEADERBG_SPACE_BETWEEN_ROWS)/2+
			   (pChannelSet->iWidth-HEADERBG_SPACE_BETWEEN_ROWS)%2-HEADERBG_FRAME_SPACE,   // Width
                           iDefaultTextWindowHeight,                                                         // Height
                           hwndBg,
                           HWND_TOP,
                           DID_CHANNELSET_CH_SELECTED_BASE + pChannel->iChannelID,
                           NULL,
                           NULL);
    SubclassStaticClass(hwnd);

    lColor = WinQuerySysColor(HWND_DESKTOP, SYSCLR_BUTTONMIDDLE, 0);
//    lColor = RGB_YELLOW;
    WinSetPresParam(hwnd, PP_BACKGROUNDCOLOR, sizeof(lColor), &lColor);
    lColor = RGB_BLACK;
    WinSetPresParam(hwnd, PP_FOREGROUNDCOLOR, sizeof(lColor), &lColor);

    dbg_free(pchBuffer);
  }
}

int RedrawChangedChannelsAndChannelSets()
{
  // To avoid deadlocks, we do this from PM thread all the time.
  WinPostMsg(hwndClient, WM_REDRAWCHANGEDCHANNELSANDCHANNELSETS, (MPARAM) 0, (MPARAM) 0);
  return 1;
}

int RedrawChangedChannelsAndChannelSets_PMThreadWorker()
{
  WaWEChannelSet_p pChannelSet;
  WaWEChannel_p pChannel;
  WaWESize_t NewChSetSize;
  WaWEPosition_t LastVisiblePosition, FirstVisiblePosition;
  WaWEWorker1Msg_ChannelDraw_p pWorkerMsg;
  SWP swp;

#ifdef DEBUG_BUILD
//  printf("[RedrawChangedChannelsAndChannelSets] : Enter\n"); fflush(stdout);
#endif

  if (DosRequestMutexSem(ChannelSetListProtector_Sem, SEM_INDEFINITE_WAIT)!=NO_ERROR)
  {
#ifdef DEBUG_BUILD
    printf("[RedrawChangedChannelsAndChannelSets] : Could not get mutex semaphore!\n"); fflush(stdout);
#endif
    return 0;
  }

  pChannelSet = ChannelSetListHead;
  while (pChannelSet)
  {
    // First of all, go through the channels in the channel-set, and
    // update the channel-set length, if needed!
    NewChSetSize = 0;
    pChannel = pChannelSet->pChannelListHead;
    while (pChannel)
    {
      if (pChannel->Length>NewChSetSize)
        NewChSetSize = pChannel->Length;
      pChannel = pChannel->pNext;
    }

    if (NewChSetSize!=pChannelSet->Length)
    {
#ifdef DEBUG_BUILD
      printf("[RedrawChangedChannelsAndChannelSets] : Channel-set length changed!\n"); fflush(stdout);
#endif
      // Channel-set length changed!
      pChannelSet->Length = NewChSetSize;
      pChannelSet->bRedrawNeeded = 1;
    }

    // It can happen that a lot of stuffs have been deleted
    // from the channel-set, so if the horizontal scrollbar
    // stays as it is, there will be no visible part of the
    // channel-set. Check this!
    FirstVisiblePosition = pChannelSet->FirstVisiblePosition;
    LastVisiblePosition =
      FirstVisiblePosition +
      pChannelSet->iWidth * pChannelSet->ZoomDenom / pChannelSet->ZoomCount
      -1;

#ifdef DEBUG_BUILD
//    printf("[RedrawChangedChannelsAndChannelSets] : Visible: %I64d -> %I64d, Len: %I64d\n",
//	   FirstVisiblePosition, LastVisiblePosition, pChannelSet->Length);
//    fflush(stdout);
#endif
    if (LastVisiblePosition>pChannelSet->Length)
    {
#ifdef DEBUG_BUILD
//      printf("[RedrawChangedChannelsAndChannelSets] : Ups!\n");
//      fflush(stdout);
#endif

      // Calculate FirstVisiblePos if the lastvisiblepos would be pChannelSet->Length!
      FirstVisiblePosition =
	pChannelSet->Length -
	pChannelSet->iWidth * pChannelSet->ZoomDenom / pChannelSet->ZoomCount
	+1;

#ifdef DEBUG_BUILD
//      printf("[RedrawChangedChannelsAndChannelSets] : New firstvisiblepos is %I64d\n", FirstVisiblePosition);
//      fflush(stdout);
#endif

      if (FirstVisiblePosition>=0)
      {
	// Move the scrollbar, so the end of channel-set will be at the
	// right side of the screen!
	pChannelSet->FirstVisiblePosition = FirstVisiblePosition;

	pChannel = pChannelSet->pChannelListHead;
	while (pChannel)
	{
	  pChannel->bRedrawNeeded = 1;
	  pChannel = pChannel->pNext;
	}
      }
    }

    // Check the channels inside the channel-set, and update them, if
    // required!
    pChannel = pChannelSet->pChannelListHead;
    while (pChannel)
    {
      if (pChannel->bRedrawNeeded)
      {
#ifdef DEBUG_BUILD
        printf("  Redrawing channel [%s]\n", pChannel->achName);
#endif
        // Make sure it will be redrawn!
        WinQueryWindowPos(pChannel->hwndEditAreaWindow, &swp);
        if (DosRequestMutexSem(pChannel->hmtxUseRequiredFields, 1000)==NO_ERROR)
	{
	  FirstVisiblePosition = pChannelSet->FirstVisiblePosition;
	  LastVisiblePosition =
	    FirstVisiblePosition +
	    pChannelSet->iWidth * pChannelSet->ZoomDenom / pChannelSet->ZoomCount
	    -1;
          pChannel->RequiredFirstSamplePos = FirstVisiblePosition;
          pChannel->RequiredLastSamplePos = LastVisiblePosition;
          pChannel->RequiredWidth = swp.cx;
          pChannel->RequiredHeight = swp.cy;
          DosReleaseMutexSem(pChannel->hmtxUseRequiredFields);
          pWorkerMsg = (WaWEWorker1Msg_ChannelDraw_p) dbg_malloc(sizeof(WaWEWorker1Msg_ChannelDraw_t));
          if (pWorkerMsg)
          {
            pWorkerMsg->pChannel = pChannel;
            pWorkerMsg->bDontOptimize = 1; // Make sure it will be redrawn!
            if (!AddNewWorker1Msg(WORKER1_MSG_CHANNELDRAW, sizeof(WaWEWorker1Msg_ChannelDraw_t), pWorkerMsg))
            {
              // Error adding the worker message to the queue, the queue might be full?
              dbg_free(pWorkerMsg);
            }
          }
        }

        // Update channel name
        UpdateChannelNameString(pChannel);
        // Update channel position
        UpdateChannelPositionString(pChannel);
        // Update channel selected region string
        UpdateChannelSelectionString(pChannel);
        pChannel->bRedrawNeeded = 0;

        pChannelSet->bRedrawNeeded = 1;
      }
      pChannel = pChannel->pNext;
    }

    // Now update the channel-set!
    if (pChannelSet->bRedrawNeeded)
    {
#ifdef DEBUG_BUILD
      printf("Redrawing channel-set [%s]\n", pChannelSet->achName);
#endif
      // Update channel-set length string
      UpdateChannelSetLengthString(pChannelSet);
      // Update channel-set name string
      UpdateChannelSetNameString(pChannelSet);
      // Update channel-set format string
      UpdateChannelSetFormatString(pChannelSet);

      // The channel-set structure has been changed!
      // Reset channel-set window size
      WinSetWindowPos(pChannelSet->hwndChannelSet,
                      HWND_TOP,
                      0, 0,
                      pChannelSet->iWidth,
                      pChannelSet->iHeight,
                      SWP_SIZE);

      // Now reposition everything to be at their final position!
      ResizeAndRepositionChannelSetWindowChildren(pChannelSet->hwndChannelSet, pChannelSet,
                                                  pChannelSet->iWidth, pChannelSet->iHeight);
      // Update the scrollbars
      UpdateChannelSetWindowSizes();
      // The function above might not update this scrollbar,
      // if the ChannelSet window size is not equal to the parent size.
      // So we call it here.
      UpdateChannelSetHScrollBar(pChannelSet);
      pChannelSet->bRedrawNeeded = 0;
    }
    pChannelSet = pChannelSet->pNext;
  }
  if (bChannelSetListChanged)
  {
    // Update the scrollbars
    UpdateChannelSetWindowSizes();

    bChannelSetListChanged = 0;
  }

  DosReleaseMutexSem(ChannelSetListProtector_Sem);

#ifdef DEBUG_BUILD
//  printf("[RedrawChangedChannelsAndChannelSets] : Leave\n"); fflush(stdout);
#endif

  return 1;
}

void CreateNewChannelWindow(WaWEChannel_p pChannel, int iEditAreaHeight)
{
  WaWEChannelSet_p pChannelSet;
  HWND hwndBg;
  WaWEWorker1Msg_ChannelDraw_p pWorkerMsg;

#ifdef DEBUG_BUILD
  printf("[CreateNewChannelWindow] : Enter\n"); fflush(stdout);
#endif

  if (!pChannel) return;

  pChannelSet = pChannel->pParentChannelSet;

  hwndBg = WinCreateWindow(pChannelSet->hwndChannelSet,// hwndParent
                           HEADERBG_WINDOW_CLASS,      // pszClass
                           pChannel->achName,          // pszName
                           WS_VISIBLE,                 // flStyle
                           0,                          // x
                           0,                          // y
                           pChannelSet->iWidth,        // cx
                           iChannelHeaderHeight,       // cy
                           pChannelSet->hwndChannelSet,// hwndOwner
                           HWND_TOP,                   // hwndInsertBehind
                           DID_CHANNELSET_CH_HEADERBG_BASE + pChannel->iChannelID,    // id
                           NULL,                       // pCtlData
                           NULL);                      // pPresParams
  WinSetWindowULong(hwndBg, HEADERBG_STORAGE_HEADERTYPE, HEADERBG_HEADERTYPE_CHANNEL);
  WinSetWindowULong(hwndBg, HEADERBG_STORAGE_PCHANNELSET, (ULONG) pChannelSet);
  WinSetWindowULong(hwndBg, HEADERBG_STORAGE_PCHANNEL, (ULONG) pChannel);

  WinSetPresParam(hwndBg, PP_FONTNAMESIZE, strlen(DEFAULT_FONT_SIZENAME)+1, DEFAULT_FONT_SIZENAME);

  // Now stuffs inside Channel header
  CreateChannelBgChildren(hwndBg, pChannel, pChannelSet);

  // Create window itself
  hwndBg = WinCreateWindow(pChannelSet->hwndChannelSet,// hwndParent
                           CHANNELDRAW_WINDOW_CLASS,   // pszClass
                           pChannel->achName,          // pszName
                           WS_VISIBLE,                 // flStyle
                           0,                          // x
                           0+iChannelHeaderHeight,     // y
                           pChannelSet->iWidth,        // cx
                           iEditAreaHeight,            // cy
                           pChannelSet->hwndChannelSet,// hwndOwner
                           HWND_TOP,                   // hwndInsertBehind
                           DID_CHANNELSET_CH_EDITAREA_BASE + pChannel->iChannelID,    // id
                           NULL,                       // pCtlData
                           pChannel);                  // pPresParams
  // Save window handle
  pChannel->hwndEditAreaWindow = hwndBg;

  // Now that the channel window has been created, notify the worker thread to
  // start rendering the contents!
  DosRequestMutexSem(pChannel->hmtxUseRequiredFields, 1000);
  pChannel->RequiredFirstSamplePos = 0;
  pChannel->RequiredLastSamplePos = pChannelSet->iWidth-1;
  pChannel->RequiredWidth = pChannelSet->iWidth;
  pChannel->RequiredHeight = iEditAreaHeight;
  DosReleaseMutexSem(pChannel->hmtxUseRequiredFields);
  pWorkerMsg = (WaWEWorker1Msg_ChannelDraw_p) dbg_malloc(sizeof(WaWEWorker1Msg_ChannelDraw_t));
  if (pWorkerMsg)
  {
    pWorkerMsg->pChannel = pChannel;
    pWorkerMsg->bDontOptimize = 0;
//        printf("CREATE : Telling worker to redraw ch%d from %d to %d\n",
//               pChannel->iChannelID,
//               (int) pChannel->RequiredFirstSamplePos,
//               (int) pChannel->RequiredLastSamplePos);

    if (!AddNewWorker1Msg(WORKER1_MSG_CHANNELDRAW, sizeof(WaWEWorker1Msg_ChannelDraw_t), pWorkerMsg))
    {
      // Error adding the worker message to the queue, the queue might be full?
      dbg_free(pWorkerMsg);
    }
  }

  // Ok, now make sure everything will look nice:

  // Make the scroll-bar the top-most child window
  WinSetWindowPos(WinWindowFromID(pChannelSet->hwndChannelSet, DID_CHANNELSET_HORZSCROLL),
                  HWND_TOP,
                  0,0,
                  0,0,
                  SWP_ZORDER);

  // Then make the sizer the top-most child window, so
  // even if we resize the window to small thing, the sizer will be
  // always visible!
  WinSetWindowPos(WinWindowFromID(pChannelSet->hwndChannelSet, DID_CHANNELSET_SIZER),
                  HWND_TOP,
                  0,0,
                  0,0,
                  SWP_ZORDER);
  // Now reposition everything to be at their final position!
  ResizeAndRepositionChannelSetWindowChildren(pChannelSet->hwndChannelSet, pChannelSet,
                                              pChannelSet->iWidth, pChannelSet->iHeight);

  // As we have a new channel set, we have to update the
  // scrollbars:
  UpdateChannelSetWindowSizes();

  // The function above does not update this scrollbar, because it only calls it
  // if the ChannelSet window size is not equal to the parent size, but as they are
  // just created, they are equal. So we call it here.
  UpdateChannelSetHScrollBar(pChannelSet);

#ifdef DEBUG_BUILD
//  printf("[CreateNewChannelWindow] : Done\n"); fflush(stdout);
#endif

  pChannel->bWindowCreated = 1;
}

void CreateNewChannelSetWindow(WaWEChannelSet_p pChannelSet)
{
  HWND result = NULLHANDLE;
  HWND hwndBg;
  HWND hwndTemp;
  SWP swp;

#ifdef DEBUG_BUILD
  printf("[CreateNewChannelSetWindow] : Enter\n"); fflush(stdout);
#endif

  WinQueryWindowPos(hwndClient, &swp);
  // Set initial width and height!
  pChannelSet->iHeight = iSizerHeight + iScrollBarHeight + iChannelSetHeaderHeight;
  pChannelSet->iWidth = swp.cx; // Client height

  result = WinCreateWindow(hwndClient,                 // hwndParent
                           CHANNELSET_WINDOW_CLASS,    // pszClass
                           pChannelSet->achName,       // pszName
                           WS_VISIBLE,                 // flStyle
                           0,                          // x
                           pChannelSet->iYPos,         // y
                           pChannelSet->iWidth,        // cx
                           pChannelSet->iHeight,       // cy
                           hwndClient,                 // hwndOwner
                           HWND_TOP,                   // hwndInsertBehind
                           pChannelSet->iChannelSetID, // id
                           NULL,                       // pCtlData
                           pChannelSet);               // pPresParams

  // Save the result in the structure
  pChannelSet->hwndChannelSet = result;
  // If we could create the window, let's create the controls inside it!
  if (result)
  {

    WinSetPresParam(result, PP_FONTNAMESIZE, strlen(DEFAULT_FONT_SIZENAME)+1, DEFAULT_FONT_SIZENAME);
    // So, create controls
    // Start creating them from bottom to up!

    // First the sizer
    WinCreateWindow(result,                     // hwndParent
                    SIZER_WINDOW_CLASS,         // pszClass
                    pChannelSet->achName,       // pszName
                    WS_VISIBLE,                 // flStyle
                    0,                          // x
                    0,                          // y
                    pChannelSet->iWidth,        // cx
                    iSizerHeight,               // cy
                    result,                     // hwndOwner
                    HWND_TOP,                   // hwndInsertBehind
                    DID_CHANNELSET_SIZER,       // id
                    NULL,                       // pCtlData
                    NULL);                      // pPresParams

    // The horizontal scrollbar
    hwndTemp = WinCreateWindow(result,                     // hwndParent
                               WC_SCROLLBAR,               // pszClass
                               pChannelSet->achName,       // pszName
                               SBS_HORZ | WS_VISIBLE,      // flStyle
                               0,                          // x
                               iSizerHeight,               // y
                               pChannelSet->iWidth,        // cx
                               iScrollBarHeight,           // cy
                               result,                     // hwndOwner
                               HWND_TOP,                   // hwndInsertBehind
                               DID_CHANNELSET_HORZSCROLL,  // id
                               NULL,                       // pCtlData
                               NULL);                      // pPresParams
    SubclassPublicClass(hwndTemp);


    // Then the Channel-Set header background
    hwndBg = WinCreateWindow(result,                     // hwndParent
                             HEADERBG_WINDOW_CLASS,      // pszClass
                             pChannelSet->achName,       // pszName
                             WS_VISIBLE,                 // flStyle
                             0,                          // x
                             pChannelSet->iHeight-iChannelSetHeaderHeight, // y
                             pChannelSet->iWidth,        // cx
                             iChannelSetHeaderHeight,    // cy
                             result,                     // hwndOwner
                             HWND_TOP,                   // hwndInsertBehind
                             DID_CHANNELSET_HEADERBG,    // id
                             NULL,                       // pCtlData
			     NULL);                      // pPresParams
    WinSetWindowULong(hwndBg, HEADERBG_STORAGE_HEADERTYPE, HEADERBG_HEADERTYPE_CHANNELSET);
    WinSetWindowULong(hwndBg, HEADERBG_STORAGE_PCHANNELSET, (ULONG) pChannelSet);
    WinSetWindowULong(hwndBg, HEADERBG_STORAGE_PCHANNEL, (ULONG) NULL);

    WinSetPresParam(hwndBg, PP_FONTNAMESIZE, strlen(DEFAULT_FONT_SIZENAME)+1, DEFAULT_FONT_SIZENAME);

    // Now stuffs inside Channel-Set header
    CreateChannelSetBgChildren(hwndBg, result, pChannelSet);

    // Then make the scroll-bar the top-most child window
    WinSetWindowPos(WinWindowFromID(result, DID_CHANNELSET_HORZSCROLL),
                    HWND_TOP,
                    0,0,
                    0,0,
                    SWP_ZORDER);

    // Then make the sizer the top-most child window, so
    // even if we resize the window to small thing, the sizer will be
    // always visible!
    WinSetWindowPos(WinWindowFromID(result, DID_CHANNELSET_SIZER),
                    HWND_TOP,
                    0,0,
                    0,0,
                    SWP_ZORDER);

    // Now reposition everything to be at their final position!
    ResizeAndRepositionChannelSetWindowChildren(result, pChannelSet,
                                                pChannelSet->iWidth, pChannelSet->iHeight);

    // As we have a new channel set, we have to update the
    // scrollbars:
    UpdateChannelSetWindowSizes();

    // The function above does not update this scrollbar, because it only calls it
    // if the ChannelSet window size is not equal to the parent size, but as they are
    // just created, they are equal. So we call it here.
    UpdateChannelSetHScrollBar(pChannelSet);
  }
#ifdef DEBUG_BUILD
  printf("[CreateNewChannelSetWindow] : Done\n"); fflush(stdout);
#endif

  pChannelSet->bWindowCreated = 1;
}



// ----------------- Sizer Window Class Stuff ---------------------------------

MRESULT EXPENTRY SizerWindowProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  HWND hwndParent;
  int iSizingFlag;
  int iSizeStartPosY;
  POINTL ptlMousePos;
  SWP swp;

  switch (msg)
  {
    case WM_DESTROY:
      {
//        printf("[SizerWindowProc] : WM_DESTROY\n");
        break;
      }
    case WM_CREATE:
      {
//	printf("[SizerWindowProc] : WM_CREATE\n");
	WinSetWindowULong(hwnd, SIZER_STORAGE_SIZINGFLAG, 0);
        WinSetWindowULong(hwnd, SIZER_STORAGE_SIZESTARTPOSY, 0);
        break;
      }
    case WM_MOUSEMOVE:
      {
	WinSetPointer(HWND_DESKTOP, WinQuerySysPointer(HWND_DESKTOP, SPTR_SIZENS, FALSE));
	iSizingFlag = WinQueryWindowULong(hwnd, SIZER_STORAGE_SIZINGFLAG);

	if (iSizingFlag)
	{
          int iSizeDiff;
	  // If sizing, then the parent window should be resized!
          iSizeStartPosY = WinQueryWindowULong(hwnd, SIZER_STORAGE_SIZESTARTPOSY);
          // Get parent window handle
	  hwndParent = WinQueryWindow(hwnd, QW_PARENT);
          // Get new mouse position on screen
	  ptlMousePos.x = SIGNEDSHORT1FROMMP(mp1);
	  ptlMousePos.y = SIGNEDSHORT2FROMMP(mp1);
//          printf("  Mouse moved to window pos %d\n", ptlMousePos.y);
          // And make it screen coordinate
	  WinMapWindowPoints(hwnd, hwndParent, &ptlMousePos, 1);
//          printf("              to parent pos %d\n", ptlMousePos.y);
	  // Calculate difference from previous mouse position
//          printf("  Old = %d, New = %d, Diff = %d\n", iSizeStartPosY, ptlMousePos.y, iSizeStartPosY-ptlMousePos.y);
	  iSizeDiff =iSizeStartPosY - ptlMousePos.y;
	  // Change size of parent window by this much!
	  if (iSizeDiff!=0)
	  {
	    WinQueryWindowPos(hwndParent, &swp);
	    // Make sure the sizediff won't make the window smaller
            // than the sizer itself, so the sizer will always be accessible, to
            // resize the window. And also, if we would let the window to be
            // smaller than zero, the Y position of the window would change, which
            // is a thing we don't want.
	    if (iSizeDiff<0) // If making the window smaller
            {
              if (swp.cy+iSizeDiff<iSizerHeight) // and it would become smaller than a limit:
		iSizeDiff = iSizerHeight - swp.cy;
            }
            // Resize the parent window!
            if (iSizeDiff!=0)
            {
              int iFirstScreenYPosNow, iFirstScreenYPosThen;
              // Get scrollbar position
              iFirstScreenYPosNow = (int) WinSendDlgItemMsg(hwndFrame, FID_VERTSCROLL,
                                                            SBM_QUERYPOS, (MPARAM) 0, (MPARAM) 0);

	      swp.cy+=iSizeDiff;
	      swp.y-=iSizeDiff;
	      WinSetWindowPos(hwndParent, HWND_TOP,
			      swp.x, swp.y,
			      swp.cx, swp.cy,
                              SWP_SIZE | SWP_MOVE);
	      // And as the window size has been changed,
	      // it might be needed to resize/reposition all other
              // channelset windows!
              UpdateChannelSetWindowSizes();

              // Get scrollbar position again
              iFirstScreenYPosThen = (int) WinSendDlgItemMsg(hwndFrame, FID_VERTSCROLL,
                                                             SBM_QUERYPOS, (MPARAM) 0, (MPARAM) 0);
              if (iFirstScreenYPosNow!=iFirstScreenYPosThen)
              {
                // Now position mouse pointer to base sizing position.
                // This solves problem when the smaller window size causes
                // the windows to be moved, because this way they fit into client area.
                // That move would cause the sizing to be activated again, because
                // the window is at a different position now relateive to the mouse.
                // But this way, it's solved.
                ptlMousePos.y = iSizeStartPosY;
                WinMapWindowPoints(hwndParent, HWND_DESKTOP, &ptlMousePos, 1);
                WinSetPointerPos(HWND_DESKTOP, ptlMousePos.x, ptlMousePos.y);
              }
	    }
	  }
	}
        return (MRESULT) 1;
      }
    case WM_BUTTON1DOWN:
    case WM_BUTTON2DOWN:
      {
        // If not sizing yet, then start sizing!
	iSizingFlag = WinQueryWindowULong(hwnd, SIZER_STORAGE_SIZINGFLAG);
	if (!iSizingFlag)
	{
          if (msg==WM_BUTTON1DOWN)
	    iSizingFlag = 1; // Store the button that started the sizing!
          else
	    iSizingFlag = 2; // Store the button that started the sizing!
          // Get parent window handle
	  hwndParent = WinQueryWindow(hwnd, QW_PARENT);
	  // Get base position of mouse for sizing
          ptlMousePos.x = SIGNEDSHORT1FROMMP(mp1);
	  ptlMousePos.y = SIGNEDSHORT2FROMMP(mp1);
	  // And make it parent coordinate
//          printf("  Sizing started at window pos %d\n", ptlMousePos.y);
	  WinMapWindowPoints(hwnd, hwndParent, &ptlMousePos, 1);
          iSizeStartPosY = ptlMousePos.y;
          // Try to capture mouse
	  if (WinSetCapture(HWND_DESKTOP, hwnd))
	  {
	    // Store settings in window
	    WinSetWindowULong(hwnd, SIZER_STORAGE_SIZINGFLAG, iSizingFlag);
	    WinSetWindowULong(hwnd, SIZER_STORAGE_SIZESTARTPOSY, iSizeStartPosY);
//	    printf("                 at parent pos %d\n", iSizeStartPosY);
          }
          // Redraw ourselves
          WinInvalidateRect(hwnd, NULL, FALSE);
        }
        // Let parent get it too! (to be able to set active channel-set)
//        return (MRESULT) 1;
        break;
      }
    case WM_BUTTON1UP:
    case WM_BUTTON2UP:
      {

        // If was sizing with *this button*, then stop sizing!
	iSizingFlag = WinQueryWindowULong(hwnd, SIZER_STORAGE_SIZINGFLAG);
	if (((msg==WM_BUTTON1UP) && (iSizingFlag==1)) ||
            ((msg==WM_BUTTON2UP) && (iSizingFlag==2)))
	{
          iSizingFlag = 0;
          // Release mouse
	  WinSetCapture(HWND_DESKTOP, NULLHANDLE);
          // Store settings in window
          WinSetWindowULong(hwnd, SIZER_STORAGE_SIZINGFLAG, iSizingFlag);
          WinSetWindowULong(hwnd, SIZER_STORAGE_SIZESTARTPOSY, 0);
        //          printf("  Sizing stopped\n");
          // Redraw ourselves
          WinInvalidateRect(hwnd, NULL, FALSE);
          return (MRESULT) 1;
	}
        break;
      }
    case WM_PAINT:
      {
	HPS hpsBeginPaint;
	RECTL rclRect;
	POINTL point;
	int iDot;
	int iSpaceBetweenDots;
        int iNumOfDots;

        // If was sizing with *this button*, then stop sizing!
	iSizingFlag = WinQueryWindowULong(hwnd, SIZER_STORAGE_SIZINGFLAG);

        hpsBeginPaint = WinBeginPaint(hwnd, NULL, &rclRect);

        if (!WinIsRectEmpty(WinQueryAnchorBlock(hwnd), &rclRect))
        {
          // If there is something to repaint, the repaint:
          WinQueryWindowRect(hwnd, &rclRect);
  
          // Draw a raised frame
          if (!iSizingFlag)
            DrawFramedRect(hpsBeginPaint, &rclRect, TRUE);
          else
            DrawFramedRect(hpsBeginPaint, &rclRect, FALSE);
            
  
          // And add some dots into the middle!
          iNumOfDots = 7;
          iSpaceBetweenDots = WinQuerySysValue(HWND_DESKTOP, SV_CXVSCROLL)/2;
          if (!iSizingFlag)
            GpiSetColor(hpsBeginPaint, SYSCLR_BUTTONDARK);
          else
            GpiSetColor(hpsBeginPaint, SYSCLR_BUTTONLIGHT);
          for (iDot = 0; iDot<iNumOfDots; iDot++)
          {
            point.x = (rclRect.xRight+rclRect.xLeft)/2 - (iNumOfDots/2)*iSpaceBetweenDots + iDot*iSpaceBetweenDots;
            point.y = (rclRect.yTop+rclRect.yBottom)/2;
            GpiSetPel(hpsBeginPaint, &point);
            point.x = (rclRect.xRight+rclRect.xLeft)/2 - (iNumOfDots/2)*iSpaceBetweenDots + iDot*iSpaceBetweenDots;
            point.y = (rclRect.yTop+rclRect.yBottom)/2+1;
            GpiSetPel(hpsBeginPaint, &point);
                    point.x = (rclRect.xRight+rclRect.xLeft)/2 - (iNumOfDots/2)*iSpaceBetweenDots + iDot*iSpaceBetweenDots;
            point.y = (rclRect.yTop+rclRect.yBottom)/2-1;
            GpiSetPel(hpsBeginPaint, &point);
  
          }
          if (!iSizingFlag)
            GpiSetColor(hpsBeginPaint, SYSCLR_BUTTONLIGHT);
          else
            GpiSetColor(hpsBeginPaint, SYSCLR_BUTTONDARK);
          for (iDot = 0; iDot<iNumOfDots; iDot++)
          {
            point.x = (rclRect.xRight+rclRect.xLeft)/2 - (iNumOfDots/2)*iSpaceBetweenDots + iDot*iSpaceBetweenDots+1;
            point.y = (rclRect.yTop+rclRect.yBottom)/2;
            GpiSetPel(hpsBeginPaint, &point);
            point.x = (rclRect.xRight+rclRect.xLeft)/2 - (iNumOfDots/2)*iSpaceBetweenDots + iDot*iSpaceBetweenDots+1;
            point.y = (rclRect.yTop+rclRect.yBottom)/2+1;
            GpiSetPel(hpsBeginPaint, &point);
            point.x = (rclRect.xRight+rclRect.xLeft)/2 - (iNumOfDots/2)*iSpaceBetweenDots + iDot*iSpaceBetweenDots+1;
            point.y = (rclRect.yTop+rclRect.yBottom)/2-1;
            GpiSetPel(hpsBeginPaint, &point);
          }
        }
	WinEndPaint(hpsBeginPaint);
	return (MRESULT) FALSE;
      }
    default:
      break;
  }
  return WinDefWindowProc(hwnd, msg, mp1, mp2);
}

// ----------------- Header Background Window Class Stuff ---------------------

MRESULT EXPENTRY HeaderBgWindowProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  switch (msg)
  {
//    case WM_DESTROY:
//      {
//        printf("[HeaderBgWindowProc] : WM_DESTROY\n");
//        break;
//      }
//    case WM_CREATE:
//      {
//        printf("[HeaderBgWindowProc] : WM_CREATE\n");
//        break;
//      }


    case WM_BUTTON2CLICK:
      {
	WaWEChannelSet_p pChannelSet;
	WaWEChannel_p pChannel;
	ULONG ulHeaderBgType;

	ulHeaderBgType =                 WinQueryWindowULong(hwnd, HEADERBG_STORAGE_HEADERTYPE);
	pChannelSet = (WaWEChannelSet_p) WinQueryWindowULong(hwnd, HEADERBG_STORAGE_PCHANNELSET);
	pChannel    = (WaWEChannel_p)    WinQueryWindowULong(hwnd, HEADERBG_STORAGE_PCHANNEL);

        if (ulHeaderBgType==HEADERBG_HEADERTYPE_CHANNELSET)
        {
#ifdef DEBUG_BUILD
//          printf("[HeaderBgWindowProc] : WM_BUTTON2CLICK for chn-set %d!\n", pChannelSet->iChannelSetID);
//          fflush(stdout);
#endif
          CreatePopupMenu(NULL, pChannelSet);
        }
        else
        {
#ifdef DEBUG_BUILD
//          printf("[HeaderBgWindowProc] : WM_BUTTON2CLICK for chn %d!\n", pChannel->iChannelID);
//          fflush(stdout);
#endif
          CreatePopupMenu(pChannel, pChannelSet);
        }
        return (MRESULT) 1;
      }
    case WM_SIZE:
      {
	WaWEChannelSet_p pChannelSet;
	WaWEChannel_p pChannel;
	ULONG ulHeaderBgType;

	ulHeaderBgType =                 WinQueryWindowULong(hwnd, HEADERBG_STORAGE_HEADERTYPE);
	pChannelSet = (WaWEChannelSet_p) WinQueryWindowULong(hwnd, HEADERBG_STORAGE_PCHANNELSET);
	pChannel    = (WaWEChannel_p)    WinQueryWindowULong(hwnd, HEADERBG_STORAGE_PCHANNEL);

//	printf("[HeaderBgWindowProc] : WM_SIZE for %d\n", ulHeaderBgType);

        if (ulHeaderBgType==HEADERBG_HEADERTYPE_CHANNELSET)
	  ResizeAndRepositionChannelSetHeaderBgWindowChildren(hwnd, pChannelSet);
	else
          ResizeAndRepositionChannelHeaderBgWindowChildren(hwnd, pChannel, pChannelSet);
        break;
      }
    case WM_PAINT:
      {
	HPS hpsBeginPaint;
	RECTL rclRect;
        hpsBeginPaint = WinBeginPaint(hwnd, NULL, &rclRect);

        if (!WinIsRectEmpty(WinQueryAnchorBlock(hwnd), &rclRect))
        {
          // If there is something to repaint, repaint:
          WinQueryWindowRect(hwnd, &rclRect);

          // Draw a raised frame
          DrawFramedRect(hpsBeginPaint, &rclRect, TRUE);
        }
	WinEndPaint(hpsBeginPaint);
	return (MRESULT) FALSE;
      }
    case WM_COMMAND:
      {
	HWND hwndOwner;
	hwndOwner = WinQueryWindow(hwnd, QW_OWNER);
        return WinSendMsg(hwndOwner, msg, mp1, mp2);
      }
    default:
      break;
  }
  return WinDefWindowProc(hwnd, msg, mp1, mp2);
}

// ----------------- Channel-Draw Window Class Stuff ---------------------

MRESULT EXPENTRY ChannelDrawWindowProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  WaWEChannel_p pChannel;
  switch (msg)
  {
    case WM_DESTROY:                   // --- Window destroying:
      {
//        printf("[ChannelDrawProc] : WM_DESTROY\n");
        break;
      }
    case WM_CREATE:                    // --- Window creation:
      {
	PCREATESTRUCT pCreateSt;
//        printf("[ChannelDrawProc] : WM_CREATE\n");
        // We save the pointer to the WaWEChannel in our
        // window ptr.
        pCreateSt = (PCREATESTRUCT) mp2;
        WinSetWindowPtr(hwnd, 0, pCreateSt->pPresParams);
        break;
      }
    case WM_SIZE:                      // --- Resizing channeldraw window:
      {
//	printf("[ChannelDrawProc] : WM_SIZE\n");
        pChannel = WinQueryWindowPtr(hwnd, 0);
        if (pChannel)
	{
	  WaWEChannelSet_p pChannelSet = pChannel->pParentChannelSet;
	  WaWEWorker1Msg_ChannelDraw_p pWorkerMsg;
	  WaWEPosition_t FirstVisiblePosition, LastVisiblePosition;
	  int iPos;
	  
	  // Tell the worker thread to redraw the stuff!
	  if (DosRequestMutexSem(pChannel->hmtxUseRequiredFields, 1000)==NO_ERROR)
	  {
	    pChannel->RequiredWidth  = SHORT1FROMMP(mp2);
	    pChannel->RequiredHeight = SHORT2FROMMP(mp2);

	    iPos = (int) WinSendDlgItemMsg(pChannelSet->hwndChannelSet,
					   (USHORT) DID_CHANNELSET_HORZSCROLL,
					   SBM_QUERYPOS, (MPARAM) 0, (MPARAM) 0);
	    // Now update window content according to the new scrollbar position!

	    FirstVisiblePosition = pChannelSet->FirstVisiblePosition;
	    LastVisiblePosition =
	      pChannelSet->FirstVisiblePosition +
	      pChannel->RequiredWidth * pChannelSet->ZoomDenom / pChannelSet->ZoomCount
	      -1;

	    pChannel->RequiredFirstSamplePos = FirstVisiblePosition;
	    pChannel->RequiredLastSamplePos = LastVisiblePosition;
//          printf("Telling worker to redraw ch%d from %d to %d (ScrlbarPos = %d)\n",
//                 pChannel->iChannelID,
//                 (int) pChannel->RequiredFirstSamplePos,
//		 (int) pChannel->RequiredLastSamplePos,
//		 iPos);
	    pWorkerMsg = (WaWEWorker1Msg_ChannelDraw_p) dbg_malloc(sizeof(WaWEWorker1Msg_ChannelDraw_t));
	    if (pWorkerMsg)
	    {
              pWorkerMsg->pChannel = pChannel;
              pWorkerMsg->bDontOptimize = 0;
	      if (!AddNewWorker1Msg(WORKER1_MSG_CHANNELDRAW, sizeof(WaWEWorker1Msg_ChannelDraw_t), pWorkerMsg))
              {
                // Error adding the worker message to the queue, the queue might be full?
                dbg_free(pWorkerMsg);
              }
	    }
            DosReleaseMutexSem(pChannel->hmtxUseRequiredFields);
	  }
        }
        break;
      }
    case WM_PAINT:                       // --- Repainting parts of the window
      {
        HPS hpsBeginPaint;
        RECTL rclRect, rclChangedRect;
        SIZEL CurrentImageBufferSize;
        POINTL Points[4];
        int iDrawn;

        hpsBeginPaint = WinBeginPaint(hwnd, NULL, &rclChangedRect);
        if (!WinIsRectEmpty(WinQueryAnchorBlock(hwnd), &rclChangedRect))
        {
          // If the rectangle to repaint is not empty, then really repaint!
          WinQueryWindowRect(hwnd, &rclRect);

          pChannel = WinQueryWindowPtr(hwnd, 0);
          if (pChannel)
          {
            // If there is a current image buffer with the same width and height,
            // then simply show it again.
            // Otherwise fill it with a black stuff until it will be redrawn.
            iDrawn = 0;
	    if (DosRequestMutexSem(pChannel->hmtxUseImageBuffer, -1)==NO_ERROR) // With unlimited blocking
	    {
	      if (pChannel->hpsCurrentImageBuffer)
	      {
		// There is a current image buffer! Check its size!
		GpiQueryPS(pChannel->hpsCurrentImageBuffer, &CurrentImageBufferSize);
		if ((CurrentImageBufferSize.cx==(rclRect.xRight-rclRect.xLeft)) &&
		    (CurrentImageBufferSize.cy==(rclRect.yTop-rclRect.yBottom)))
		{
		  // Ok, the same size!
#ifdef DEBUG_BUILD
//		  printf("[ChannelDrawProc] : WM_PAINT : BitBlt for chn %d!\n", pChannel->iChannelID);
#endif
		  Points[0].x = 0;
		  Points[0].y = 0;
		  Points[1].x = CurrentImageBufferSize.cx;
		  Points[1].y = CurrentImageBufferSize.cy;
		  Points[2].x = 0;
		  Points[2].y = 0;
		  GpiBitBlt(hpsBeginPaint, pChannel->hpsCurrentImageBuffer,
			    3, &(Points[0]),
			    ROP_SRCCOPY,
			    BBO_IGNORE);
		  iDrawn = 1;
		} else
		{
#ifdef DEBUG_BUILD
//		  printf("[ChannelDrawProc] : WM_PAINT : Partial BitBlt for chn %d!\n", pChannel->iChannelID);
#endif

		  // Not the same size! So, reuse some of it!
                  Points[0].x = 0;                              // Target:
		  Points[0].y = 0;
		  Points[1].x = CurrentImageBufferSize.cx;
                  Points[1].y = (rclRect.yTop-rclRect.yBottom); //   Stretch!
                  Points[2].x = 0;                              // Source:
		  Points[2].y = 0;
		  Points[3].x = CurrentImageBufferSize.cx;
		  Points[3].y = CurrentImageBufferSize.cy;

		  GpiBitBlt(hpsBeginPaint, pChannel->hpsCurrentImageBuffer,
			    4, &(Points[0]),
			    ROP_SRCCOPY,
			    BBO_IGNORE);
		  // Also draw a black box if this drawing was smaller
		  // than the area available!
		  if (CurrentImageBufferSize.cx<(rclRect.xRight-rclRect.xLeft))
		  {
		    RECTL rcl;
#ifdef DEBUG_BUILD
//		    printf("[ChannelDrawProc] :            Plus some black!\n");
#endif
                    // Switch to RGB mode
		    GpiCreateLogColorTable(hpsBeginPaint, 0, LCOLF_RGB, 0, 0, NULL);
		    rcl.xLeft = CurrentImageBufferSize.cx;
		    rcl.xRight = rclRect.xRight;
		    rcl.yBottom = rclRect.yBottom;
                    rcl.yTop = rclRect.yTop;
		    WinFillRect(hpsBeginPaint, &rcl, RGB_BLACK);
		  }
		  iDrawn = 1;
		}
	      }
	      DosReleaseMutexSem(pChannel->hmtxUseImageBuffer);
	    }
            if (!iDrawn)
            {
              // Different size, or there is no current image buffer,
              // so draw a black stuff instead to the place which needs to be
	      // redrawn!
#ifdef DEBUG_BUILD
//              printf("[ChannelDrawProc] : WM_PAINT : Black background for chn %d\n", pChannel->iChannelID);
#endif
              // Switch to RGB mode
              GpiCreateLogColorTable(hpsBeginPaint, 0, LCOLF_RGB, 0, 0, NULL);
              WinFillRect(hpsBeginPaint, &rclChangedRect, RGB_BLACK);
            }
          }
        }
	WinEndPaint(hpsBeginPaint);
	return (MRESULT) FALSE;
      }
    case WM_MOUSEMOVE:
      pChannel = WinQueryWindowPtr(hwnd, 0);
      if (pChannel)
      {
	WaWEChannelSet_p pChannelSet;
        int iMousePosX;
        // Pointer moved above the channeldraw window. So, update pointer position
        // info text!
        iMousePosX = SIGNEDSHORT1FROMMP(mp1);

	pChannelSet = pChannel->pParentChannelSet;
	pChannelSet->PointerPosition = pChannelSet->FirstVisiblePosition +
          ((WaWEPosition_t) iMousePosX * pChannelSet->ZoomDenom / pChannelSet->ZoomCount);

        if (pChannelSet->PointerPosition<0)
          pChannelSet->PointerPosition = 0;

//        printf("PointerPos: %d (First is %d)\n", (int) pChannelSet->PointerPosition, (int) pChannelSet->FirstVisiblePosition);

        UpdateChannelSetPointerPositionString(pChannelSet);
        // Now update selected region list, if we're in selection mode!
        if (pChannel->bSelecting)
        {
          int iSelectionMode;
          WaWEPosition_t StartPos;
          WaWEWorker1Msg_UpdateShownSelection_p pWorkerMsg;
          int iWorkWithChannelSet;

          // Query if currently we're working with channelset or with individual channels!
          iWorkWithChannelSet =
            (int) WinQueryButtonCheckstate(WinWindowFromID(pChannelSet->hwndChannelSet,
                                                           DID_CHANNELSET_HEADERBG),
                                           DID_CHANNELSET_WORKWITHCHSET);

          // Get selected region parameters
          iSelectionMode = WAWE_SELECTIONMODE_REPLACE;
          if (SHORT2FROMMP(mp2) & KC_CTRL)
            iSelectionMode = WAWE_SELECTIONMODE_ADD;
          if (SHORT2FROMMP(mp2) & KC_ALT)
            iSelectionMode = WAWE_SELECTIONMODE_SUB;
          if ((SHORT2FROMMP(mp2) & (KC_ALT | KC_CTRL)) == (KC_ALT | KC_CTRL))
            iSelectionMode = WAWE_SELECTIONMODE_XOR;

          if (!iWorkWithChannelSet)
          {
            // Working with individual channel, so update selected region list only for this channel!
            // Redraw new selection
            if (SHORT2FROMMP(mp2) & KC_SHIFT)
              StartPos = pChannel->CurrentPosition;
            else
              StartPos = pChannel->SelectionStartSamplePos;

            CreateRequiredSelectedRegionList(pChannel,
                                             StartPos,
                                             pChannelSet->PointerPosition,
                                             iSelectionMode);
            pWorkerMsg = (WaWEWorker1Msg_UpdateShownSelection_p) dbg_malloc(sizeof(WaWEWorker1Msg_UpdateShownSelection_t));
            if (pWorkerMsg)
            {
              pWorkerMsg->pChannel = pChannel;
              if (!AddNewWorker1Msg(WORKER1_MSG_UPDATESHOWNSELECTION, sizeof(WaWEWorker1Msg_UpdateShownSelection_t), pWorkerMsg))
              {
                // Error adding the worker message to the queue, the queue might be full?
                dbg_free(pWorkerMsg);
              }
            }
          } else
          {
            // Working with every channel in channelset, so update selected region list for every channel in channelset!
            WaWEChannel_p pTempChannel;
            if (DosRequestMutexSem(ChannelSetListProtector_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
            {
              // For every channel in channelset
              pTempChannel = pChannelSet->pChannelListHead;
              while (pTempChannel)
              {
                if (SHORT2FROMMP(mp2) & KC_SHIFT)
                  StartPos = pTempChannel->CurrentPosition;
                else
                  StartPos = pTempChannel->SelectionStartSamplePos;

                // Redraw new selection
                CreateRequiredSelectedRegionList(pTempChannel,
                                                 StartPos,
                                                 pChannelSet->PointerPosition,
                                                 iSelectionMode);
                pWorkerMsg = (WaWEWorker1Msg_UpdateShownSelection_p) dbg_malloc(sizeof(WaWEWorker1Msg_UpdateShownSelection_t));
                if (pWorkerMsg)
                {
                  pWorkerMsg->pChannel = pTempChannel;
                  if (!AddNewWorker1Msg(WORKER1_MSG_UPDATESHOWNSELECTION, sizeof(WaWEWorker1Msg_UpdateShownSelection_t), pWorkerMsg))
                  {
                    // Error adding the worker message to the queue, the queue might be full?
                    dbg_free(pWorkerMsg);
                  }
                }
                pTempChannel = pTempChannel->pNext;
              }
              DosReleaseMutexSem(ChannelSetListProtector_Sem);
            }
          }
        }
      }
      break;
    case WM_BUTTON2CLICK:
      pChannel = WinQueryWindowPtr(hwnd, 0);
      if (pChannel)
      {
#ifdef DEBUG_BUILD
//        printf("[ChannelDrawProc] : WM_BUTTON2CLICK for chn %d!\n", pChannel->iChannelID);
//        fflush(stdout);
#endif
        CreatePopupMenu(pChannel, pChannel->pParentChannelSet);
      }
      return (MRESULT) 1;
    case WM_BUTTON1CLICK:
      pChannel = WinQueryWindowPtr(hwnd, 0);
      if (pChannel)
      {
	WaWEChannelSet_p pChannelSet;
	WaWEWorker1Msg_SetChannelCurrentPosition_p pWorkerMsg;
	int iWorkWithChannelSet;
	WaWEPosition_t NewPosition;

#ifdef DEBUG_BUILD
//	printf("[ChannelDrawProc] : WM_BUTTON1CLICK for chn %d!\n", pChannel->iChannelID);
#endif
	pChannelSet = pChannel->pParentChannelSet;

        // Query if currently we're working with channelset or with individual channels!
	iWorkWithChannelSet =
	  (int) WinQueryButtonCheckstate(WinWindowFromID(pChannelSet->hwndChannelSet,
							 DID_CHANNELSET_HEADERBG),
					 DID_CHANNELSET_WORKWITHCHSET);

        // Calculate new position in sample units
        NewPosition = pChannelSet->FirstVisiblePosition +
	  (WaWEPosition_t) (((POINTS *)(&mp1))->x) * pChannelSet->ZoomDenom / pChannelSet->ZoomCount;

        // If shift is not pressed, then we set current position!
        // If shift is pressed, then we set selected area from current pos to this pos!
        if (SHORT2FROMMP(mp2) & KC_SHIFT)
        {
          // Shift is pressed! So we have to change selected region!
          int iSelectionMode;
#ifdef DEBUG_BUILD
//          printf("[ChannelDrawProc] : Changing selection by click!\n");
#endif

          iSelectionMode = WAWE_SELECTIONMODE_REPLACE;
          if (SHORT2FROMMP(mp2) & KC_CTRL)
            iSelectionMode = WAWE_SELECTIONMODE_ADD;
          if (SHORT2FROMMP(mp2) & KC_ALT)
            iSelectionMode = WAWE_SELECTIONMODE_SUB;
          if ((SHORT2FROMMP(mp2) & (KC_ALT | KC_CTRL)) == (KC_ALT | KC_CTRL))
            iSelectionMode = WAWE_SELECTIONMODE_XOR;

          if (!iWorkWithChannelSet)
          {
            // Working with individual channel
#ifdef DEBUG_BUILD
//            printf("[ChannelDrawProc] : Changing selected region list for chn %d!\n", pChannel->iChannelID);
#endif
            // Mark flag that we're not selecting anymore, and save end pos
            pChannel->bSelecting = 0;
            // Redraw last settings
            CreateRequiredSelectedRegionList(pChannel,
                                             pChannel->CurrentPosition,
                                             NewPosition,
                                             iSelectionMode);
            pWorkerMsg = (WaWEWorker1Msg_UpdateShownSelection_p) dbg_malloc(sizeof(WaWEWorker1Msg_UpdateShownSelection_t));
            if (pWorkerMsg)
            {
              pWorkerMsg->pChannel = pChannel;
              if (!AddNewWorker1Msg(WORKER1_MSG_UPDATESHOWNSELECTION, sizeof(WaWEWorker1Msg_UpdateShownSelection_t), pWorkerMsg))
              {
                // Error adding the worker message to the queue, the queue might be full?
                dbg_free(pWorkerMsg);
              }
            }

            // Make this to be the last valid one
            MakeRequiredRegionListLastValid(pChannel);
          } else
          {
            // Working with every channel in channelset
            WaWEChannel_p pTempChannel;
            if (DosRequestMutexSem(ChannelSetListProtector_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
            {
              // For every channel in channelset
              pTempChannel = pChannelSet->pChannelListHead;
              while (pTempChannel)
              {
#ifdef DEBUG_BUILD
//                printf("[ChannelDrawProc] : Changing selected region list for chn %d!\n", pChannel->iChannelID);
#endif
                // Mark flag that we're not selecting anymore, and save end pos
                pTempChannel->bSelecting = 0;
                // Redraw last settings
                CreateRequiredSelectedRegionList(pTempChannel,
                                                 pTempChannel->CurrentPosition,
                                                 NewPosition,
                                                 iSelectionMode);
                pWorkerMsg = (WaWEWorker1Msg_UpdateShownSelection_p) dbg_malloc(sizeof(WaWEWorker1Msg_UpdateShownSelection_t));
                if (pWorkerMsg)
                {
                  pWorkerMsg->pChannel = pTempChannel;
                  if (!AddNewWorker1Msg(WORKER1_MSG_UPDATESHOWNSELECTION, sizeof(WaWEWorker1Msg_UpdateShownSelection_t), pWorkerMsg))
                  {
                    // Error adding the worker message to the queue, the queue might be full?
                    dbg_free(pWorkerMsg);
                  }
                }
    
                // Make this to be the last valid one
                MakeRequiredRegionListLastValid(pTempChannel);

                pTempChannel = pTempChannel->pNext;
              }
              DosReleaseMutexSem(ChannelSetListProtector_Sem);
            }
          }
        } else
        {
          // Shift is not pressed, so we have to change current position!
          if (!iWorkWithChannelSet)
          {
            // Working with individual channel, so set current position only for this channel!
  
            pWorkerMsg = (WaWEWorker1Msg_SetChannelCurrentPosition_p) dbg_malloc(sizeof(WaWEWorker1Msg_SetChannelCurrentPosition_t));
            if (pWorkerMsg)
            {
              pWorkerMsg->pChannel = pChannel;
              if (DosRequestMutexSem(pChannel->hmtxUseRequiredFields, 1000)==NO_ERROR)
              {
                pChannel->RequiredCurrentPosition = NewPosition;
                if (!AddNewWorker1Msg(WORKER1_MSG_SETCHANNELCURRENTPOSITION, sizeof(WaWEWorker1Msg_SetChannelCurrentPosition_t), pWorkerMsg))
                {
                  // Error adding the worker message to the queue, the queue might be full?
                  dbg_free(pWorkerMsg);
                }
                DosReleaseMutexSem(pChannel->hmtxUseRequiredFields);
              } else
              {
                dbg_free(pWorkerMsg);
              }
            }
          } else
          {
            // Working with every channel in channelset, so set current position for every channel in channelset!
            WaWEChannel_p pTempChannel;
            if (DosRequestMutexSem(ChannelSetListProtector_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
            {
              // For every channel in channelset, send a setchannelcurrentposition!
              pTempChannel = pChannelSet->pChannelListHead;
              while (pTempChannel)
              {
                pWorkerMsg = (WaWEWorker1Msg_SetChannelCurrentPosition_p) dbg_malloc(sizeof(WaWEWorker1Msg_SetChannelCurrentPosition_t));
                if (pWorkerMsg)
                {
                  pWorkerMsg->pChannel = pTempChannel;
                  if (DosRequestMutexSem(pTempChannel->hmtxUseRequiredFields, 1000)==NO_ERROR)
                  {
                    pTempChannel->RequiredCurrentPosition = NewPosition;
                    if (!AddNewWorker1Msg(WORKER1_MSG_SETCHANNELCURRENTPOSITION, sizeof(WaWEWorker1Msg_SetChannelCurrentPosition_t), pWorkerMsg))
                    {
                      // Error adding the worker message to the queue, the queue might be full?
                      dbg_free(pWorkerMsg);
                    }
                    DosReleaseMutexSem(pTempChannel->hmtxUseRequiredFields);
                  } else
                  {
                    dbg_free(pWorkerMsg);
                  }
                }
                pTempChannel = pTempChannel->pNext;
              }
              DosReleaseMutexSem(ChannelSetListProtector_Sem);
            }
          }
        }
      }
      break;
    case WM_BEGINSELECT:
      pChannel = WinQueryWindowPtr(hwnd, 0);
      if (pChannel)
      {
        WaWEChannelSet_p pChannelSet;
        POINTS *ppts;
        int iWorkWithChannelSet;

        pChannelSet = pChannel->pParentChannelSet;
        ppts = (POINTS *) (&mp1);

        // Query if currently we're working with channelset or with individual channels!
        iWorkWithChannelSet =
          (int) WinQueryButtonCheckstate(WinWindowFromID(pChannelSet->hwndChannelSet,
                                                         DID_CHANNELSET_HEADERBG),
                                         DID_CHANNELSET_WORKWITHCHSET);

	if (!iWorkWithChannelSet)
	{
	  // Working with individual channel
#ifdef DEBUG_BUILD
//          printf("[ChannelDrawProc] : WM_BEGINSELECT for chn %d!\n", pChannel->iChannelID);
#endif
          // Mark flag that we're selecting, and save start pos
          pChannel->bSelecting = 1;
          pChannel->SelectionStartSamplePos =
            pChannelSet->FirstVisiblePosition +
            (WaWEPosition_t) (ppts->x) * pChannelSet->ZoomDenom / pChannelSet->ZoomCount;
	} else
	{
	  // Working with every channel in channelset
	  WaWEChannel_p pTempChannel;
	  if (DosRequestMutexSem(ChannelSetListProtector_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
	  {
            // For every channel in channelset
	    pTempChannel = pChannelSet->pChannelListHead;
	    while (pTempChannel)
            {
#ifdef DEBUG_BUILD
//              printf("[ChannelDrawProc] : WM_BEGINSELECT for chn %d!\n", pTempChannel->iChannelID);
#endif
              // Mark flag that we're selecting, and save start pos
              pTempChannel->bSelecting = 1;
              pTempChannel->SelectionStartSamplePos =
                pChannelSet->FirstVisiblePosition +
                (WaWEPosition_t) (ppts->x) * pChannelSet->ZoomDenom / pChannelSet->ZoomCount;

              pTempChannel = pTempChannel->pNext;
	    }
	    DosReleaseMutexSem(ChannelSetListProtector_Sem);
	  }
        }

        // Capture mouse
        WinSetCapture(HWND_DESKTOP, hwnd);
        // Start a timer, so the channel-set window will
        // get events, and it can check if the mouse is
        // out of the window. If it is, then it will scroll the window.
        WinStartTimer(WinQueryAnchorBlock(hwnd),
                      pChannelSet->hwndChannelSet,
                      TID_AUTOSCROLLTIMER,
                      50);
      }
      break;
    case WM_ENDSELECT:
      pChannel = WinQueryWindowPtr(hwnd, 0);
      if (pChannel)
      {
        POINTS *ppts;
        WaWEPosition_t Pos;
        WaWEChannelSet_p pChannelSet;
        WaWEWorker1Msg_UpdateShownSelection_p pWorkerMsg;
        int iSelectionMode;
        WaWEPosition_t StartPos;
        int iWorkWithChannelSet;

        ppts = (POINTS *) (&mp1);
        if (ppts->x < 0) ppts->x = 0;
        pChannelSet = pChannel->pParentChannelSet;

        iSelectionMode = WAWE_SELECTIONMODE_REPLACE;
        if (SHORT2FROMMP(mp2) & KC_CTRL)
          iSelectionMode = WAWE_SELECTIONMODE_ADD;
        if (SHORT2FROMMP(mp2) & KC_ALT)
          iSelectionMode = WAWE_SELECTIONMODE_SUB;
        if ((SHORT2FROMMP(mp2) & (KC_ALT | KC_CTRL)) == (KC_ALT | KC_CTRL))
          iSelectionMode = WAWE_SELECTIONMODE_XOR;

        // Query if currently we're working with channelset or with individual channels!
        iWorkWithChannelSet =
          (int) WinQueryButtonCheckstate(WinWindowFromID(pChannelSet->hwndChannelSet,
                                                         DID_CHANNELSET_HEADERBG),
                                         DID_CHANNELSET_WORKWITHCHSET);

	if (!iWorkWithChannelSet)
	{
          // Working with individual channel
#ifdef DEBUG_BUILD
//          printf("[ChannelDrawProc] : WM_ENDSELECT for chn %d!\n", pChannel->iChannelID);
#endif
          if (pChannel->bSelecting)
          {
            // Mark flag that we're not selecting anymore, and save end pos
            pChannel->bSelecting = 0;
            Pos = pChannelSet->FirstVisiblePosition +
              (WaWEPosition_t) (ppts->x) * pChannelSet->ZoomDenom / pChannelSet->ZoomCount;

            if (SHORT2FROMMP(mp2) & KC_SHIFT)
              StartPos = pChannel->CurrentPosition;
            else
              StartPos = pChannel->SelectionStartSamplePos;

            // Redraw last settings
            CreateRequiredSelectedRegionList(pChannel,
                                             StartPos,
                                             Pos,
                                             iSelectionMode);
            pWorkerMsg = (WaWEWorker1Msg_UpdateShownSelection_p) dbg_malloc(sizeof(WaWEWorker1Msg_UpdateShownSelection_t));
            if (pWorkerMsg)
            {
              pWorkerMsg->pChannel = pChannel;
              if (!AddNewWorker1Msg(WORKER1_MSG_UPDATESHOWNSELECTION, sizeof(WaWEWorker1Msg_UpdateShownSelection_t), pWorkerMsg))
              {
                // Error adding the worker message to the queue, the queue might be full?
                dbg_free(pWorkerMsg);
              }
            }

            // Make this to be the last valid one
            MakeRequiredRegionListLastValid(pChannel);
          }
	} else
	{
	  // Working with every channel in channelset
	  WaWEChannel_p pTempChannel;
	  if (DosRequestMutexSem(ChannelSetListProtector_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
	  {
            // For every channel in channelset
	    pTempChannel = pChannelSet->pChannelListHead;
	    while (pTempChannel)
            {
              // Working with individual channel
#ifdef DEBUG_BUILD
//              printf("[ChannelDrawProc] : WM_ENDSELECT for chn %d!\n", pTempChannel->iChannelID);
#endif
              if (pTempChannel->bSelecting)
              {
                // Mark flag that we're not selecting anymore, and save end pos
                pTempChannel->bSelecting = 0;
                Pos = pChannelSet->FirstVisiblePosition +
                  (WaWEPosition_t) (ppts->x) * pChannelSet->ZoomDenom / pChannelSet->ZoomCount;
    
                if (SHORT2FROMMP(mp2) & KC_SHIFT)
                  StartPos = pTempChannel->CurrentPosition;
                else
                  StartPos = pTempChannel->SelectionStartSamplePos;
    
                // Redraw last settings
                CreateRequiredSelectedRegionList(pTempChannel,
                                                 StartPos,
                                                 Pos,
                                                 iSelectionMode);
                pWorkerMsg = (WaWEWorker1Msg_UpdateShownSelection_p) dbg_malloc(sizeof(WaWEWorker1Msg_UpdateShownSelection_t));
                if (pWorkerMsg)
                {
                  pWorkerMsg->pChannel = pTempChannel;
                  if (!AddNewWorker1Msg(WORKER1_MSG_UPDATESHOWNSELECTION, sizeof(WaWEWorker1Msg_UpdateShownSelection_t), pWorkerMsg))
                  {
                    // Error adding the worker message to the queue, the queue might be full?
                    dbg_free(pWorkerMsg);
                  }
                }
    
                // Make this to be the last valid one
                MakeRequiredRegionListLastValid(pTempChannel);
              }

              pTempChannel = pTempChannel->pNext;
	    }
	    DosReleaseMutexSem(ChannelSetListProtector_Sem);
	  }
        }
        // Release captured mouse
        WinSetCapture(HWND_DESKTOP, NULLHANDLE);
        // Stop autoscroll timer
        WinStopTimer(WinQueryAnchorBlock(hwnd),
                     pChannelSet->hwndChannelSet,
                     TID_AUTOSCROLLTIMER);
      }
      break;
    default:
      break;
  }
  return WinDefWindowProc(hwnd, msg, mp1, mp2);
}

// ----------------- Visible-Part selector Window Class Stuff ---------------------

static void VisiblePart_internal_CalculateNewValues(HWND hwnd, int iMouseButton,
                                                    VPNValuesChanged_p pNewValues,
                                                    int iMouseX, int iMouseY)
{
  // Get stored window values
  RECTL rclFullRect;
  ULONG ulMaxSampleLo;
  ULONG ulMaxSampleHi;
  ULONG ulStartFromLo;
  ULONG ulStartFromHi;
  ULONG ulZoomCountLo;
  ULONG ulZoomCountHi;
  ULONG ulZoomDenomLo;
  ULONG ulZoomDenomHi;
  ULONG ulVisiblePixels;
  WaWESize_t MaxSample, ZoomCount, ZoomDenom;
  WaWEPosition_t PointedSample, StartFrom, EndAt;
  ULONG ulWindowWidth;
  __int64 i64GCD;

  // Fill variables
  WinQueryWindowRect(hwnd, &rclFullRect);
  ulWindowWidth = rclFullRect.xRight - rclFullRect.xLeft;
  if (ulWindowWidth==0) ulWindowWidth = 1;
  ulStartFromLo = WinQueryWindowULong(hwnd, VISIBLEPART_STORAGE_ULSTARTFROMLO);
  ulStartFromHi = WinQueryWindowULong(hwnd, VISIBLEPART_STORAGE_ULSTARTFROMHI);
  ulMaxSampleLo = WinQueryWindowULong(hwnd, VISIBLEPART_STORAGE_ULMAXSAMPLELO);
  ulMaxSampleHi = WinQueryWindowULong(hwnd, VISIBLEPART_STORAGE_ULMAXSAMPLEHI);
  ulZoomCountLo = WinQueryWindowULong(hwnd, VISIBLEPART_STORAGE_ULZOOMCOUNTLO);
  ulZoomCountHi = WinQueryWindowULong(hwnd, VISIBLEPART_STORAGE_ULZOOMCOUNTHI);
  ulZoomDenomLo = WinQueryWindowULong(hwnd, VISIBLEPART_STORAGE_ULZOOMDENOMLO);
  ulZoomDenomHi = WinQueryWindowULong(hwnd, VISIBLEPART_STORAGE_ULZOOMDENOMHI);
  ulVisiblePixels = WinQueryWindowULong(hwnd, VISIBLEPART_STORAGE_ULVISIBLEPIXELS);

  MaxSample = ulMaxSampleHi;
  MaxSample = (MaxSample << 32) | ulMaxSampleLo;

  StartFrom = ulStartFromHi;
  StartFrom = (StartFrom << 32) | ulStartFromLo;

  ZoomCount = ulZoomCountHi;
  ZoomCount = (ZoomCount << 32) | ulZoomCountLo;

  ZoomDenom = ulZoomDenomHi;
  ZoomDenom = (ZoomDenom << 32) | ulZoomDenomLo;

  // Get number of visible samples
  EndAt = ulVisiblePixels;
  if (ZoomCount==0) ZoomCount = 1;
  EndAt = EndAt * ZoomDenom / ZoomCount;
  // Add to Start to get End
  EndAt += StartFrom;

  // Ok, so, in window, (left+1) pixel means 0, (right-1) pixel means MaxSample.
  // From this, we should calculate the sample number for current X position!
//  printf("MouseX is %d, WindowWidth is %d\n", iMouseX, ulWindowWidth);
  PointedSample = ((iMouseX-1) * MaxSample) / (ulWindowWidth-2);
//  printf("PointedSample is %I64d\n", PointedSample);
  if (PointedSample<0) PointedSample = 0;
  if (PointedSample>MaxSample) PointedSample = MaxSample;

  // Now update the startpos and/or zoom values!

  if (iMouseButton==1)
  {
    // Update the start position!
    StartFrom = PointedSample;
    if (StartFrom>EndAt)
      EndAt = StartFrom;
  }
  else
  {
    // Update the end position
    EndAt = PointedSample;
    if (EndAt<StartFrom)
      StartFrom = EndAt;
  }

  // Now recalculate zoom values from start/end values!
  EndAt-=StartFrom; // Now EndAt contains the number of visible samples
  if (EndAt==0) EndAt=1;

  i64GCD = GCD(ulVisiblePixels, EndAt);
  if (i64GCD==0) i64GCD = 1;
  ZoomCount = ulVisiblePixels / i64GCD;
  ZoomDenom = EndAt / i64GCD;

  // Store these new values in window storage
  ulStartFromLo = (ULONG) StartFrom;
  ulStartFromHi = (ULONG) (StartFrom >> 32);

  ulZoomCountLo = (ULONG) ZoomCount;
  ulZoomCountHi = (ULONG) (ZoomCount >> 32);

  ulZoomDenomLo = (ULONG) ZoomDenom;
  ulZoomDenomHi = (ULONG) (ZoomDenom >> 32);

  WinSetWindowULong(hwnd, VISIBLEPART_STORAGE_ULSTARTFROMLO, (ULONG) ulStartFromLo);
  WinSetWindowULong(hwnd, VISIBLEPART_STORAGE_ULSTARTFROMHI, (ULONG) ulStartFromHi);
  WinSetWindowULong(hwnd, VISIBLEPART_STORAGE_ULZOOMCOUNTLO, (ULONG) ulZoomCountLo);
  WinSetWindowULong(hwnd, VISIBLEPART_STORAGE_ULZOOMCOUNTHI, (ULONG) ulZoomCountHi);
  WinSetWindowULong(hwnd, VISIBLEPART_STORAGE_ULZOOMDENOMLO, (ULONG) ulZoomDenomLo);
  WinSetWindowULong(hwnd, VISIBLEPART_STORAGE_ULZOOMDENOMHI, (ULONG) ulZoomDenomHi);

  // Store these new values in NewValues struct!
  pNewValues->ulStartFromLo = ulStartFromLo;
  pNewValues->ulStartFromHi = ulStartFromHi;
  pNewValues->ulZoomCountLo = ulZoomCountLo;
  pNewValues->ulZoomCountHi = ulZoomCountHi;
  pNewValues->ulZoomDenomLo = ulZoomDenomLo;
  pNewValues->ulZoomDenomHi = ulZoomDenomHi;
}

MRESULT EXPENTRY VisiblePartWindowProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  USHORT usID;
  VPNValuesChanged_t NewValues;

  switch (msg)
  {
    case WM_DESTROY:                   // --- Window destroying:
      {
//        printf("[VisiblePartWindowProc] : WM_DESTROY\n");
        break;
      }
    case WM_CREATE:                    // --- Window creation:
      {
//        printf("[VisiblePartWindowProc] : WM_CREATE\n");
        // Initialize storage!
        WinSetWindowULong(hwnd, VISIBLEPART_STORAGE_ULSTARTFROMLO, 0);
        WinSetWindowULong(hwnd, VISIBLEPART_STORAGE_ULSTARTFROMHI, 0);
        WinSetWindowULong(hwnd, VISIBLEPART_STORAGE_ULMAXSAMPLELO, 0);
        WinSetWindowULong(hwnd, VISIBLEPART_STORAGE_ULMAXSAMPLEHI, 0);
        WinSetWindowULong(hwnd, VISIBLEPART_STORAGE_ULZOOMCOUNTLO, 1);
        WinSetWindowULong(hwnd, VISIBLEPART_STORAGE_ULZOOMCOUNTHI, 0);
        WinSetWindowULong(hwnd, VISIBLEPART_STORAGE_ULZOOMDENOMLO, 1);
        WinSetWindowULong(hwnd, VISIBLEPART_STORAGE_ULZOOMDENOMHI, 0);
        WinSetWindowULong(hwnd, VISIBLEPART_STORAGE_ULVISIBLEPIXELS, 1);
        WinSetWindowULong(hwnd, VISIBLEPART_STORAGE_ULMOUSECAPTURED, 0);
        break;
      }

    case WM_VISIBLEPART_SETSTARTFROM:
      WinSetWindowULong(hwnd, VISIBLEPART_STORAGE_ULSTARTFROMLO, (ULONG) mp1);
      WinSetWindowULong(hwnd, VISIBLEPART_STORAGE_ULSTARTFROMHI, (ULONG) mp2);
      WinInvalidateRect(hwnd, NULL, FALSE); // Redraw
      return (MRESULT) 1;

    case WM_VISIBLEPART_SETZOOMCOUNT:
      WinSetWindowULong(hwnd, VISIBLEPART_STORAGE_ULZOOMCOUNTLO, (ULONG) mp1);
      WinSetWindowULong(hwnd, VISIBLEPART_STORAGE_ULZOOMCOUNTHI, (ULONG) mp2);
      WinInvalidateRect(hwnd, NULL, FALSE); // Redraw
      return (MRESULT) 1;

    case WM_VISIBLEPART_SETZOOMDENOM:
      WinSetWindowULong(hwnd, VISIBLEPART_STORAGE_ULZOOMDENOMLO, (ULONG) mp1);
      WinSetWindowULong(hwnd, VISIBLEPART_STORAGE_ULZOOMDENOMHI, (ULONG) mp2);
      WinInvalidateRect(hwnd, NULL, FALSE); // Redraw
      return (MRESULT) 1;

    case WM_VISIBLEPART_SETVISIBLEPIXELS:
      WinSetWindowULong(hwnd, VISIBLEPART_STORAGE_ULVISIBLEPIXELS, (ULONG) mp1);
      WinInvalidateRect(hwnd, NULL, FALSE); // Redraw
      return (MRESULT) 1;

    case WM_VISIBLEPART_SETMAXSAMPLE:
      WinSetWindowULong(hwnd, VISIBLEPART_STORAGE_ULMAXSAMPLELO, (ULONG) mp1);
      WinSetWindowULong(hwnd, VISIBLEPART_STORAGE_ULMAXSAMPLEHI, (ULONG) mp2);
      WinInvalidateRect(hwnd, NULL, FALSE); // Redraw
      return (MRESULT) 1;

    case WM_BUTTON1DOWN:
      if (WinSetCapture(HWND_DESKTOP, hwnd))
      {
#ifdef DEBUG_BUILD
//        printf("Button1Down\n");
#endif

        WinSetWindowULong(hwnd, VISIBLEPART_STORAGE_ULMOUSECAPTURED, (ULONG) 1);
        // Calculate new values from current mouse position
        VisiblePart_internal_CalculateNewValues(hwnd, 1, &NewValues, SIGNEDSHORT1FROMMP(mp1), SIGNEDSHORT2FROMMP(mp1));
        // Notify entry fields of new values
        usID = WinQueryWindowUShort(hwnd, QWS_ID);
        WinSendMsg(WinQueryWindow(hwnd, QW_OWNER), WM_CONTROL,
                   MPFROM2SHORT(usID, VPN_VALUESCHANGED), (MPARAM) (&NewValues));
        // Redraw ourselves
        WinInvalidateRect(hwnd, NULL, FALSE);
      }
      break;
    case WM_BUTTON1UP:
#ifdef DEBUG_BUILD
//      printf("Button1UP\n");
#endif
      // Release the mouse
      WinSetCapture(HWND_DESKTOP, NULLHANDLE);
      // Store settings in window
      WinSetWindowULong(hwnd, VISIBLEPART_STORAGE_ULMOUSECAPTURED, (ULONG) 0);
      VisiblePart_internal_CalculateNewValues(hwnd, 1, &NewValues, SIGNEDSHORT1FROMMP(mp1), SIGNEDSHORT2FROMMP(mp1));
      // Notify entry fields of new values
      usID = WinQueryWindowUShort(hwnd, QWS_ID);
      WinSendMsg(WinQueryWindow(hwnd, QW_OWNER), WM_CONTROL,
                 MPFROM2SHORT(usID, VPN_VALUESCHANGED), (MPARAM) (&NewValues));
      // Redraw ourselves
      WinInvalidateRect(hwnd, NULL, FALSE);
      break;
    case WM_BUTTON2DOWN:
      if (WinSetCapture(HWND_DESKTOP, hwnd))
      {
#ifdef DEBUG_BUILD
//        printf("Button2Down\n");
#endif

        WinSetWindowULong(hwnd, VISIBLEPART_STORAGE_ULMOUSECAPTURED, (ULONG) 2);
        // Calculate new values from current mouse position
        VisiblePart_internal_CalculateNewValues(hwnd, 2, &NewValues, SIGNEDSHORT1FROMMP(mp1), SIGNEDSHORT2FROMMP(mp1));
        // Notify entry fields of new values
        usID = WinQueryWindowUShort(hwnd, QWS_ID);
        WinSendMsg(WinQueryWindow(hwnd, QW_OWNER), WM_CONTROL,
                   MPFROM2SHORT(usID, VPN_VALUESCHANGED), (MPARAM) (&NewValues));
        // Redraw ourselves
        WinInvalidateRect(hwnd, NULL, FALSE);
      }
      break;
    case WM_BUTTON2UP:
#ifdef DEBUG_BUILD
//      printf("Button2UP\n");
#endif
      // Release the mouse
      WinSetCapture(HWND_DESKTOP, NULLHANDLE);
      // Store settings in window
      WinSetWindowULong(hwnd, VISIBLEPART_STORAGE_ULMOUSECAPTURED, (ULONG) 0);
      VisiblePart_internal_CalculateNewValues(hwnd, 2, &NewValues, SIGNEDSHORT1FROMMP(mp1), SIGNEDSHORT2FROMMP(mp1));
      // Notify entry fields of new values
      usID = WinQueryWindowUShort(hwnd, QWS_ID);
      WinSendMsg(WinQueryWindow(hwnd, QW_OWNER), WM_CONTROL,
                 MPFROM2SHORT(usID, VPN_VALUESCHANGED), (MPARAM) (&NewValues));
      // Redraw ourselves
      WinInvalidateRect(hwnd, NULL, FALSE);
      break;
    case WM_MOUSEMOVE:
      {
        ULONG ulMouseCaptured;
        ulMouseCaptured = WinQueryWindowULong(hwnd, VISIBLEPART_STORAGE_ULMOUSECAPTURED);
        if (ulMouseCaptured)
        {
          // Calculate new values from current mouse position
          VisiblePart_internal_CalculateNewValues(hwnd, ulMouseCaptured, &NewValues, SIGNEDSHORT1FROMMP(mp1), SIGNEDSHORT2FROMMP(mp1));
          // Notify entry fields of new values
          usID = WinQueryWindowUShort(hwnd, QWS_ID);
          WinSendMsg(WinQueryWindow(hwnd, QW_OWNER), WM_CONTROL,
                     MPFROM2SHORT(usID, VPN_VALUESCHANGED), (MPARAM) (&NewValues));
          // Redraw ourselves
          WinInvalidateRect(hwnd, NULL, FALSE);
        }
      }
      break;
    case WM_PAINT:                     // --- Repainting parts of the window
      {
        HPS hpsBeginPaint;
        RECTL rclChangedRect;

        hpsBeginPaint = WinBeginPaint(hwnd, NULL, &rclChangedRect);
        if (!WinIsRectEmpty(WinQueryAnchorBlock(hwnd), &rclChangedRect))
        {
          RECTL rclFullRect;
          RECTL rclInnerRect;
          ULONG ulStartFromLo;
          ULONG ulStartFromHi;
          ULONG ulMaxSampleLo;
          ULONG ulMaxSampleHi;
	  ULONG ulZoomCountLo;
          ULONG ulZoomCountHi;
	  ULONG ulZoomDenomLo;
          ULONG ulZoomDenomHi;
          ULONG ulVisiblePixels;
          LONG lStartPos, lEndPos;
          WaWEPosition_t StartFrom;
          WaWEPosition_t EndAt;
	  WaWESize_t MaxSample;
          WaWESize_t ZoomCount, ZoomDenom;
          LONG rgbColor1, rgbColor2;
          PRGB2 pRGB2;

          // Switch to RGB mode
          GpiCreateLogColorTable(hpsBeginPaint, 0, LCOLF_RGB, 0, 0, NULL);

          // Fill variables
          WinQueryWindowRect(hwnd, &rclFullRect);
//          printf("FullRect: %d;%d %d;%d\n", rclFullRect.xLeft, rclFullRect.yBottom, rclFullRect.xRight, rclFullRect.yTop);
          ulStartFromLo = WinQueryWindowULong(hwnd, VISIBLEPART_STORAGE_ULSTARTFROMLO);
          ulStartFromHi = WinQueryWindowULong(hwnd, VISIBLEPART_STORAGE_ULSTARTFROMHI);
          ulMaxSampleLo = WinQueryWindowULong(hwnd, VISIBLEPART_STORAGE_ULMAXSAMPLELO);
          ulMaxSampleHi = WinQueryWindowULong(hwnd, VISIBLEPART_STORAGE_ULMAXSAMPLEHI);
	  ulZoomCountLo = WinQueryWindowULong(hwnd, VISIBLEPART_STORAGE_ULZOOMCOUNTLO);
          ulZoomCountHi = WinQueryWindowULong(hwnd, VISIBLEPART_STORAGE_ULZOOMCOUNTHI);
	  ulZoomDenomLo = WinQueryWindowULong(hwnd, VISIBLEPART_STORAGE_ULZOOMDENOMLO);
          ulZoomDenomHi = WinQueryWindowULong(hwnd, VISIBLEPART_STORAGE_ULZOOMDENOMHI);
          ulVisiblePixels = WinQueryWindowULong(hwnd, VISIBLEPART_STORAGE_ULVISIBLEPIXELS);

          StartFrom = ulStartFromHi;
          StartFrom = (StartFrom << 32) | ulStartFromLo;

          MaxSample = ulMaxSampleHi;
          MaxSample = (MaxSample << 32) | ulMaxSampleLo;

          ZoomCount = ulZoomCountHi;
          ZoomCount = (ZoomCount << 32) | ulZoomCountLo;

          ZoomDenom = ulZoomDenomHi;
          ZoomDenom = (ZoomDenom << 32) | ulZoomDenomLo;

          // Get number of visible samples
          EndAt = ulVisiblePixels;
          if (ZoomCount==0) ZoomCount = 1;
          EndAt = EndAt * ZoomDenom / ZoomCount;
          // Add to Start to get End
          EndAt += StartFrom;
          if (EndAt>MaxSample) EndAt = MaxSample;

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

          if (MaxSample==0)
          {
            lStartPos = 0;
            lEndPos = 0;
          }
          else
          {
            lStartPos =
              ((StartFrom * (rclFullRect.xRight-rclFullRect.xLeft-2) + MaxSample-1) / MaxSample) + 1;
            lEndPos =
              ((EndAt * (rclFullRect.xRight-rclFullRect.xLeft-2) + MaxSample-1) / MaxSample) + 1;
          }
//          printf("Start: %I64d (%d)  End: %I64d (%d)\n", StartFrom, lStartPos, EndAt, lEndPos);
          rclInnerRect.xLeft = rclFullRect.xLeft + lStartPos;
          if (rclInnerRect.xLeft <= rclFullRect.xLeft)
            rclInnerRect.xLeft = rclFullRect.xLeft+1;
          if (rclInnerRect.xLeft >= rclFullRect.xRight)
            rclInnerRect.xLeft = rclFullRect.xRight-1;
          rclInnerRect.yBottom = rclFullRect.yBottom+1;
          rclInnerRect.xRight = rclFullRect.xLeft + lEndPos;
          if (rclInnerRect.xRight == rclInnerRect.xLeft)
            rclInnerRect.xRight+=2;
          if (rclInnerRect.xRight >= rclFullRect.xRight)
            rclInnerRect.xRight = rclFullRect.xRight-1;
          if (rclInnerRect.xRight == rclInnerRect.xLeft)
            rclInnerRect.xLeft-=2;
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
	return (MRESULT) FALSE;
      }
    default:
      break;
  }
  return WinDefWindowProc(hwnd, msg, mp1, mp2);
}

MRESULT ProcessWMCHARMessage(MPARAM mp1, MPARAM mp2)
{
  USHORT fsflags = SHORT1FROMMP(mp1);
//  UCHAR  ucrepeat = CHAR3FROMMP(mp1);
//  UCHAR  ucscancode = CHAR4FROMMP(mp1);
//  USHORT usch = SHORT1FROMMP(mp2);
  USHORT usvk = SHORT2FROMMP(mp2);
  int iSelectionMode;
  int iShiftPressed;
  WaWEChannel_p pTempChannel;
  WaWEChannelSet_p pTempChannelSet;
  WaWEPosition_t StartPos, EndPos;
  WaWEWorker1Msg_UpdateShownSelection_p pWorkerMsg;
  POINTL ptlMousePos;

  if (fsflags & KC_VIRTUALKEY)
  {
    // Check if SHIFT, CTRL or ALT changed its state!
    // If so, we might update a selected region, if the user
    // is currently selecting one!
    if ((usvk==VK_SHIFT) || (usvk==VK_CTRL) || (usvk==VK_ALT))
    {
      iSelectionMode = WAWE_SELECTIONMODE_REPLACE;
      if (fsflags & KC_CTRL)
        iSelectionMode = WAWE_SELECTIONMODE_ADD;
      if (fsflags & KC_ALT)
        iSelectionMode = WAWE_SELECTIONMODE_SUB;
      if ((fsflags & (KC_ALT | KC_CTRL)) == (KC_ALT | KC_CTRL))
        iSelectionMode = WAWE_SELECTIONMODE_XOR;

      iShiftPressed = fsflags & KC_SHIFT;

//      printf("SelMode = %d   Shift = %d\n", iSelectionMode, iShiftPressed);

      // Go through all the channels, and if one is
      // being selected, update it!

      if (DosRequestMutexSem(ChannelSetListProtector_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
      {
        // For every channelset
        pTempChannelSet = ChannelSetListHead;
        while (pTempChannelSet)
        {
          // For every channel in channelset
          pTempChannel = pTempChannelSet->pChannelListHead;
          while (pTempChannel)
          {
            if (pTempChannel->bSelecting)
            {
              if (iShiftPressed)
                StartPos = pTempChannel->CurrentPosition;
              else
                StartPos = pTempChannel->SelectionStartSamplePos;

              WinQueryPointerPos(HWND_DESKTOP, &ptlMousePos);
              WinMapWindowPoints(HWND_DESKTOP,
                                 WinWindowFromID(pTempChannelSet->hwndChannelSet, DID_CHANNELSET_CH_EDITAREA_BASE + pTempChannel->iChannelID),
                                 &ptlMousePos, 1);
              
              EndPos =
                pTempChannelSet->FirstVisiblePosition +
                ptlMousePos.x * pTempChannelSet->ZoomDenom / pTempChannelSet->ZoomCount;

              // Redraw last settings
              CreateRequiredSelectedRegionList(pTempChannel,
                                               StartPos,
                                               EndPos,
                                               iSelectionMode);
              pWorkerMsg = (WaWEWorker1Msg_UpdateShownSelection_p) dbg_malloc(sizeof(WaWEWorker1Msg_UpdateShownSelection_t));
              if (pWorkerMsg)
              {
                pWorkerMsg->pChannel = pTempChannel;
                if (!AddNewWorker1Msg(WORKER1_MSG_UPDATESHOWNSELECTION, sizeof(WaWEWorker1Msg_UpdateShownSelection_t), pWorkerMsg))
                {
                  // Error adding the worker message to the queue, the queue might be full?
                  dbg_free(pWorkerMsg);
                }
              }
            }
            pTempChannel = pTempChannel->pNext;
          }
          pTempChannelSet = pTempChannelSet->pNext;
        }
        DosReleaseMutexSem(ChannelSetListProtector_Sem);
      }
    }
  }
  return (MRESULT) TRUE;
}

// ----------------- Others ---------------------------------------------------

long QueryFontHeight(HAB hab, char *pchFontFaceName, int iFontSize)
{
  long result = 10; // Default to 10, if something goes wrong...
  SIZEL sizel;
  HDC hdc;
  HPS hps;
  FATTRS fat;
  SIZEF sizef;
  FONTMETRICS fontmetrics;

  // Create a device context
  hdc=DevOpenDC(hab, OD_MEMORY, "*", 0L, NULL, NULLHANDLE);
  if (!hdc)
  {
#ifdef DEBUG_BUILD
    printf("[QueryFontHeight] : Could not create DC!\n");
#endif
    return result;
  }

  // Create a dummy presentation space, in units of Pixels
  sizel.cx = 10;
  sizel.cy = 10;
  hps = GpiCreatePS(hab, hdc, &sizel, PU_PELS | GPIT_NORMAL | GPIA_ASSOC);

  // Then create a logical font of the given font face
  fat.usRecordLength = sizeof(FATTRS);
  fat.fsSelection = 0;
  fat.lMatch = 0;
  fat.idRegistry = 0;
  fat.usCodePage = 850;
  fat.lMaxBaselineExt = 0;
  fat.lAveCharWidth = 0;
  fat.fsType = 0;
  fat.fsFontUse = FATTR_FONTUSE_NOMIX | FATTR_FONTUSE_OUTLINE;
  strcpy(fat.szFacename, pchFontFaceName);

  GpiCreateLogFont(hps, NULL, 1L, &fat);

  // Set this new logical font the current font for PS
  GpiSetCharSet(hps, 1L);

  // Set Truetype font size
  sizef.cx = MAKEFIXED(iFontSize, 0);
  sizef.cy = MAKEFIXED(iFontSize, 0);
  GpiSetCharBox(hps, &sizef);

  // Now query font metrics of the currently set font
  GpiQueryFontMetrics(hps, sizeof(FONTMETRICS), &fontmetrics);

#ifdef DEBUG_BUILD
  printf("FontMetrics:\n");
  printf("  Family: %s\n", fontmetrics.szFamilyname);
  printf("  Face  : %s\n", fontmetrics.szFacename);
  printf("  lEmHeight       : %d\n", fontmetrics.lEmHeight);
  printf("  lXHeight        : %d\n", fontmetrics.lXHeight);
  printf("  lMaxBaseLineExt : %d\n", fontmetrics.lMaxBaselineExt);
  printf("  lExternalLeading : %d\n", fontmetrics.lExternalLeading);
  printf("  lInternalLeading : %d\n", fontmetrics.lInternalLeading);
  printf("  sNominalPointSize : %d\n", fontmetrics.sNominalPointSize);
#endif

  // Ok, here is the needed height for this font
  result = fontmetrics.lMaxBaselineExt + fontmetrics.lInternalLeading;

  // Clean up stuffs not needed anymore
  GpiSetCharSet(hps, 0);
  GpiDeleteSetId(hps, 1);
  GpiDestroyPS(hps);
  DevCloseDC(hdc);
  return result;
}

int Initialize_Screen(HAB hab)
{

  // Register private window classes
  //  CHANNELSET_WINDOW_CLASS
  WinRegisterClass(hab, CHANNELSET_WINDOW_CLASS,
		   ChannelSetWindowProc,
                   CS_SIZEREDRAW |
                   CS_CLIPSIBLINGS |
                   CS_CLIPCHILDREN |
                   CS_PARENTCLIP,                      // Window style
                   8);   // 8 bytes of private storage for window of this class
  //  SIZER_WINDOW_CLASS
  WinRegisterClass(hab, SIZER_WINDOW_CLASS, 
		   SizerWindowProc,         
                   CS_SIZEREDRAW |
                   CS_CLIPSIBLINGS |
                   CS_CLIPCHILDREN |
                   CS_PARENTCLIP,                      // Window style
                   SIZER_STORAGE_NEEDED_BYTES);        // private storage for window of this class
  //  HEADERBG_WINDOW_CLASS
  WinRegisterClass(hab, HEADERBG_WINDOW_CLASS,
                   HeaderBgWindowProc,
                   CS_SIZEREDRAW |
                   CS_CLIPSIBLINGS |
                   CS_CLIPCHILDREN |
                   CS_PARENTCLIP,                      // Window style
                   HEADERBG_STORAGE_NEEDED_BYTES);     // private storage for window of this class

  //  CHANNELDRAW_WINDOW_CLASS
  WinRegisterClass(hab, CHANNELDRAW_WINDOW_CLASS,
                   ChannelDrawWindowProc,
                   CS_SIZEREDRAW |
                   CS_CLIPSIBLINGS |
                   CS_CLIPCHILDREN |
                   CS_PARENTCLIP,                      // Window style
                   CHANNELDRAW_STORAGE_NEEDED_BYTES);  // private storage for window of this class

  //  VISIBLEPART_WINDOW_CLASS
  WinRegisterClass(hab, VISIBLEPART_WINDOW_CLASS,
                   VisiblePartWindowProc,
                   CS_SIZEREDRAW |
                   CS_CLIPSIBLINGS |
                   CS_CLIPCHILDREN |
                   CS_PARENTCLIP,                      // Window style
                   VISIBLEPART_STORAGE_NEEDED_BYTES);  // private storage for window of this class

  // Setup scrollbar stuff
  iFirstScreenYPos = 0;
  iScrollBarShowsHeight_Client = 0;
  iScrollBarShowsHeight_Sets = 0;
  iScrollBarHeight = WinQuerySysValue(HWND_DESKTOP, SV_CYHSCROLL);

  // Sizer stuff:
  iSizerHeight = 5;

  // Get font height
  iDefaultFontHeight = QueryFontHeight(hab, DEFAULT_FONT_NAME, DEFAULT_FONT_SIZE);

  // This font will be put into windows, and the windows will be that high to
  // have the fonts fit, plus to have 1 pixel line at top and one pixel at bottom.
  iDefaultTextWindowHeight = iDefaultFontHeight + 2;

#ifdef DEBUG_BUILD
  printf("[Initialize_Screen] : Default font height is %d\n", iDefaultFontHeight);
#endif

  iChannelSetHeaderHeight = HEADERBG_FRAME_SPACE*2+HEADERBG_SPACE_BETWEEN_LINES*2+iDefaultTextWindowHeight*3;
  iChannelHeaderHeight = HEADERBG_FRAME_SPACE*2+HEADERBG_SPACE_BETWEEN_LINES+iDefaultTextWindowHeight*2;
  iChannelEditAreaHeight = 10 * iDefaultFontHeight;

  pActiveChannelSet = NULL;

  return 1;
}

void Uninitialize_Screen()
{
  return;
}
