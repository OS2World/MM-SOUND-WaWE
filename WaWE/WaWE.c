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
 * Main code                                                                 *
 * Deals with main(), WindowProc of main window, and Load/Save dialogs       *
 *****************************************************************************/

#include "WaWE.h"

// --------------- Global variables: ----------------------------------------


unsigned long tidPM;               // PM Thread
BOOL GUI_Ready = FALSE;  // Indicates if the PM Thread has done its initialization

BOOL ShuttingDown = FALSE;      // Global Shut Down indicator

char *pchPrivateClassName = "WaWEClientWindow";
char *pchMainWindowTitle  = WAWE_SHORTAPPNAME" - "WAWE_LONGAPPNAME;
char *pchVer              = WAWE_APPVERSION;

char *WinStateSave_AppName = "WaWE";
char *WinStateSave_MainWindowKey  = "WaWE_MainWindowPP";
char *WinStateSave_CreateNewChannelSetWindowKey  = "WaWE_CreateNewChannelSetWindowPP";
char *WinStateSave_ZoomSetWindowKey = "WaWE_ZoomSetWindowPP";

HWND hwndFrame = NULL;
HWND hwndClient = NULL;

// Main thread global variables:
HAB hab;
HMQ hmq;

// --------------- Functions: -----------------------------------------------

#ifdef DEBUG_BUILD
void MorphToPM()
{
  PPIB pib;
  PTIB tib;

  DosGetInfoBlocks(&tib, &pib);

  // Change flag from VIO to PM:
  if (pib->pib_ultype==2) pib->pib_ultype = 3;
}
#endif

void ErrorMsg(char *Msg)
{
  HWND hwndOwner = hwndClient;

#ifdef DEBUG_BUILD
  printf("ErrorMsg: %s\n",Msg); fflush(stdout);
#endif

  if (!hwndOwner) hwndOwner = HWND_DESKTOP;
  WinMessageBox(HWND_DESKTOP, hwndOwner,
                Msg, "WaWE Error", 0, MB_OK | MB_MOVEABLE | MB_ICONEXCLAMATION);
}

void SetMainWindowTitleExtension(char *pchExtension)
{
  char *pchFullTitle;

  if (pchExtension)
  {
    pchFullTitle = dbg_malloc(strlen(pchMainWindowTitle) + strlen(pchExtension) + 1);
    if (pchFullTitle)
    {
      sprintf(pchFullTitle, "%s%s", pchMainWindowTitle, pchExtension);
      WinSetWindowText(hwndFrame, pchFullTitle);
      dbg_free(pchFullTitle);
    } else
      WinSetWindowText(hwndFrame, pchMainWindowTitle);
  } else
    WinSetWindowText(hwndFrame, pchMainWindowTitle);
}

HBITMAP DecodeBMPImage(char *pchBuffer, int iSize, HAB hab)
{
  HBITMAP result = NULL;
  ULONG rc;

  MMIOINFO mmioInfo;
  MMFORMATINFO mmFormatInfo;
  HMMIO hFile;
  MMIMAGEHEADER mmImageHeader;
  ULONG ulBytesRead;
  int iHeight;
  int iWidth;
  int iBitCount;
  int iRowBits;
  int iNumRowBytes;
  int iPadBytes;
  int iRowCount;
  HPS hps;
  HDC hdc;
  char *pRowBuffer;
  SIZEL ImageSize;

  if (!pchBuffer) return result;
#ifdef DEBUG_BUILD
//  printf("[DecodeBMPImage] : Preparing mmioOpen()\n");
#endif
  memset(&mmioInfo, 0, sizeof(mmioInfo));
  memset(&mmFormatInfo, 0, sizeof(mmFormatInfo));

  mmioInfo.ulTranslate =
    MMIO_TRANSLATEDATA |
    MMIO_TRANSLATEHEADER;

  mmioInfo.cchBuffer = iSize;
  mmioInfo.pchBuffer = pchBuffer;
  mmioInfo.fccChildIOProc = FOURCC_MEM;
  mmioInfo.fccIOProc = mmioFOURCC('O','S','2','0');

#ifdef DEBUG_BUILD
//  printf("[DecodeBMPImage] : Opening\n");
#endif
  hFile = mmioOpen(".BMP+.MEM",//NULL,//"a",
                   &mmioInfo,
                   MMIO_READ | MMIO_DENYWRITE);
  if (NULL == hFile)
  {
#ifdef DEBUG_BUILD
    printf("[DecodeBMPImage] : Could not open file!\n");
    printf("[DecodeBMPImage] : rc = %d\n", mmioInfo.ulErrorRet);
#endif
    return result;
  }
#ifdef DEBUG_BUILD
//  {
//    char fcc[5], fcc2[5];
//    mmioGetInfo(hFile, &mmioInfo, 0);
//    memcpy(fcc, &(mmioInfo.fccIOProc), 4); fcc[4] = 0;
//    memcpy(fcc2, &(mmioInfo.fccChildIOProc), 4); fcc2[4] = 0;
//
//    printf("[DecodeBMPImage] : File opened, checking type... [%s] [%s]\n", fcc, fcc2);
//  }
#endif

  rc = mmioGetHeader(hFile,
                     (PVOID)&mmImageHeader,
                     sizeof(MMIMAGEHEADER),
                     (PLONG) &ulBytesRead,
                     NULL,
                     NULL);
  if (rc!=MMIO_SUCCESS)
  {
#ifdef DEBUG_BUILD
    printf("[DecodeBMPImage] : Not an image file! rc = %d\n", rc);
#endif
    mmioClose(hFile, 0);
    return result;
  }
  
  if (mmImageHeader.ulMediaType!=MMIO_MEDIATYPE_IMAGE)
  {
#ifdef DEBUG_BUILD
    printf("[DecodeBMPImage] : Not an image file!\n");
    printf("             MediaType is %d\n", mmImageHeader.ulMediaType);
#endif
    mmioClose(hFile, 0);
    return result;
  }
#ifdef DEBUG_BUILD  
//  printf("[DecodeBMPImage] : Image file opened!\n");
#endif

#ifdef DEBUG_BUILD
//  rc = mmioGetData(hFile, &mmioInfo, 0);
//  if (rc == MMIO_SUCCESS)
//  {
//    char achFormatName[1024];
//    MMFORMATINFO mmFormatInfo;
//    LONG lBytesRead;
//    memset(achFormatName, 0, 1024);
//    mmFormatInfo.ulStructLen = sizeof(mmFormatInfo);
//    mmFormatInfo.fccIOProc = mmioInfo.fccIOProc;
//    mmFormatInfo.lNameLength = 1024-1;
//    rc = mmioGetFormatName(&mmFormatInfo, achFormatName,&lBytesRead, 0, 0);
//    if (rc==MMIO_SUCCESS)
//    {
//      printf("  Format name: %s\n", achFormatName);
//    }
//  }
#endif

  // Calculate the number of bytes required in a row
  iHeight = mmImageHeader.mmXDIBHeader.BMPInfoHeader2.cy;
  iWidth = mmImageHeader.mmXDIBHeader.BMPInfoHeader2.cx;
  iBitCount = mmImageHeader.mmXDIBHeader.BMPInfoHeader2.cBitCount;
  iRowBits = iWidth * iBitCount;
  iNumRowBytes = iRowBits >> 3;
  if (iRowBits % 8)
    iNumRowBytes++;

  // All bitmap data rows must are aligned on LONG/4-BYTE boundaries.
  iPadBytes = iNumRowBytes % 4;
  if (iPadBytes)
    iNumRowBytes += 4 - iPadBytes;

//  printf("%d %d %d %d %d %d\n", iHeight, iWidth, iBitCount, iRowBits, iNumRowBytes, iPadBytes);

  // Allocate space for one row
  pRowBuffer = dbg_malloc(iNumRowBytes);
  if (!pRowBuffer)
  {
#ifdef DEBUG_BUILD
    printf("[DecodeBMPImage] : Could not allocate memory for pRowBuffer (%d bytes)!\n", iNumRowBytes);
#endif
    mmioClose(hFile, 0);
    return result;
  }

  // Create a device context
  hdc=DevOpenDC(hab, OD_MEMORY,"*",0L, NULL, NULLHANDLE);
  if (!hdc)
  {
#ifdef DEBUG_BUILD
    printf("[DecodeBMPImage] : Could not create DC!\n");
#endif
    dbg_free(pRowBuffer);
    mmioClose(hFile, 0);
    return result;
  }

  // Create a memory PS

  ImageSize.cx = iWidth;
  ImageSize.cy = iHeight;
  hps = GpiCreatePS (hab,
                     hdc,
                     &ImageSize,
                     PU_PELS | GPIT_NORMAL | GPIA_ASSOC );
  if (!hps)
  {
#ifdef DEBUG_BUILD
    printf("[DecodeBMPImage] : Could not create PS!\n");
#endif
    DevCloseDC(hdc);
    dbg_free(pRowBuffer);
    mmioClose(hFile, 0);
    return result;
  }

  // Create an uninitialized bitmap.  This is where we
  // will put all of the bits once we read them in.
  result = GpiCreateBitmap (hps,
                            &(mmImageHeader.mmXDIBHeader.BMPInfoHeader2),
                            0,
                            NULL,
                            NULL);

  if (!result)
  {
#ifdef DEBUG_BUILD
    printf("[DecodeBMPImage] : Could not create Bitmap!\n");
#endif
    GpiDestroyPS(hps);
    DevCloseDC(hdc);
    dbg_free(pRowBuffer);
    mmioClose(hFile, 0);
    return result;
  }

  // Select the bitmap into the memory device context.
  GpiSetBitmap (hps,
                result);

  // And now load the bitmap data from the file, one line at a time,
  // starting from the bottom!
  for (iRowCount=0; iRowCount<iHeight; iRowCount++)
  {
    ulBytesRead = (ULONG) mmioRead(hFile,
                                   pRowBuffer,
                                   iNumRowBytes);
    if (!ulBytesRead)
    {
#ifdef DEBUG_BUILD
      printf("[DecodeBMPImage] : mmioRead error, rc = %d\n", mmioGetLastError(hFile));
#endif
      break;
    }
    GpiSetBitmapBits (hps,
                      (LONG) iRowCount,
                      (LONG) 1,
                      (PBYTE) pRowBuffer,
                      (PBITMAPINFO2) &(mmImageHeader.mmXDIBHeader.BMPInfoHeader2));
  }

  // Clean up stuffs not needed anymore
  GpiSetBitmap(hps,
               NULLHANDLE);
  dbg_free(pRowBuffer);
  GpiDestroyPS(hps);
  DevCloseDC(hdc);

  mmioClose(hFile, 0);
  // Return the new bitmap!
  return result;
}

//#define USE_PRIVATE_RLE_COMPRESSION

#ifdef USE_PRIVATE_RLE_COMPRESSION
int Decompress(char *pchCompBuffer, int iCompSize, char *pchDestBuffer, int iDestSize)
{
  int iDecSize = 0;
  int i, j;
  int iChunkSize;

  i = 0;
  while (i<iCompSize)
  {
    iChunkSize = pchCompBuffer[i] & 0x7f;
    if (iChunkSize<=0) iChunkSize = 1;
    if (pchCompBuffer[i] & 0x80)
    {
      for (j = 0; j<iChunkSize; j++)
      {
        pchDestBuffer[iDecSize] = pchCompBuffer[i+1]; iDecSize++;
        if (iDecSize>=iDestSize) return iDecSize;
      }
      i+=2;
    } else
    {
      i++; // Skip first byte (chunk size and type)
      for (j = 0; j<iChunkSize; j++)
      {
        pchDestBuffer[iDecSize] = pchCompBuffer[i]; iDecSize++; i++;
        if (iDecSize>=iDestSize) return iDecSize;
      }
    }
  }
  return iDecSize;
}
#endif

void AnimationThreadFunc(void *p)
{
  HWND hwnd = (HWND) p;
  FILE *hFile;
  int i, iFrame;
  int *piSizeTable;
  char *pchOneFrame;
#ifdef USE_PRIVATE_RLE_COMPRESSION

#define DEC_FRAME_BUF_SIZE 32768
  char *pchDecompressedFrame;
  int  iDecompressedFrameSize;
#endif
  int iNumOfFrames;
  PVOID pWinPtr;
  HAB hab;
  HMQ hmq;
  HBITMAP hbmOriginalHandle;
  HBITMAP hbmNewFrame, hbmOldFrame;
  int iUseFrameRegulation;
  int iErrorInFrameRegulation;
  HEV hevBlitEvent;

#ifdef DEBUG_BUILD
  printf("[Animation] : Started!\n");
  printf("[Animation] : Setting priority\n");
#endif
  DosSetPriority(PRTYS_THREAD, PRTYC_REGULAR, 15L, 0);
#ifdef DEBUG_BUILD
  printf("[Animation] : Starting Frame Regulator\n");
#endif

  iUseFrameRegulation = 1;
  iErrorInFrameRegulation = 0;
  if (DosCreateEventSem(NULL, &hevBlitEvent, 0, FALSE)!=NO_ERROR)
  {
#ifdef DEBUG_BUILD
    printf("[Animation] : Could not create event semaphore!\n");
#endif
    // Falling back to DosSleep() API:
    iUseFrameRegulation = 0;
  } else
  {
    if (!StartFrameRegulation(hevBlitEvent, 30)) // Set up Frame Regulator to 30 FPS
    {
#ifdef DEBUG_BUILD
      printf("[Animation] : Could not start frame regulation!\n");
#endif
      // Falling back to DosSleep() API:
      iUseFrameRegulation = 0;
      DosCloseEventSem(hevBlitEvent);
    }
  }

  hFile = fopen("WaWELogo.dat", "rb");
  if (!hFile)
  {
#ifdef DEBUG_BUILD
    printf("[Animation] : Could not open WaWELogo.dat file!\n");
#endif
  } else
  {
    // Skip the info part
    fseek(hFile, 16, SEEK_SET);
    // Read the number of frames
    i = fread(&iNumOfFrames, sizeof(iNumOfFrames), 1, hFile);
    if (i!=1)
    {
#ifdef DEBUG_BUILD
      printf("[Animation] : Could not read iNumOfFrames!\n");
#endif
    } else
    {
      // Allocate memory for size table
      piSizeTable = (int *) dbg_malloc(iNumOfFrames*sizeof(int));
      if (!piSizeTable)
      {
#ifdef DEBUG_BUILD
        printf("[Animation] : Not enough memory for piSizeTable!\n");
#endif
      } else
      {
        // Read size table
        i = fread(piSizeTable, iNumOfFrames*sizeof(int), 1, hFile);
        if (i!=1)
        {
#ifdef DEBUG_BUILD
          printf("[Animation] : Could not read piSizeTable!\n");
#endif
        } else
        {
          // Ok, size table readed, now read frames, and display them, as
          // long as the about window is visible

          // Initialize PM for this thread
          hab = WinInitialize(0);
          hmq = WinCreateMsgQueue(hab, 0);
          // Query original bitmap handle for dialog!
          hbmOriginalHandle = (HBITMAP)WinSendMsg(WinWindowFromID(hwnd, BM_ANIMATION),
                                                                  SM_QUERYHANDLE,
                                                                  (MPARAM) NULL,
                                                                  (MPARAM) NULL);

#ifdef DEBUG_BUILD
          printf("[Animation] : Starting animation of %d frames\n", iNumOfFrames);
#endif
          hbmNewFrame = hbmOldFrame = NULL;
          pWinPtr = WinQueryWindowPtr(hwnd, QWP_PFNWP);
          iFrame = 0;
#ifdef USE_PRIVATE_RLE_COMPRESSION
          pchDecompressedFrame = (char *) dbg_malloc(DEC_FRAME_BUF_SIZE);
          if (!pchDecompressedFrame)
          {
            printf("[Animation] : Could not allocate memory for dec frame!\n");
          } else
#endif
          while ((pWinPtr) && (iFrame<iNumOfFrames))
          {
            // Read frame
            pchOneFrame = (char *) dbg_malloc(piSizeTable[iFrame]);
            if (!pchOneFrame)
            {
#ifdef DEBUG_BUILD
              printf("[Animation] : Could not allocate memory for frame!\n");
#endif
              iFrame = iNumOfFrames; // Exit loop
            } else
            {
              i = fread(pchOneFrame, piSizeTable[iFrame], 1, hFile);
              if (i!=1)
              {
#ifdef DEBUG_BUILD
                printf("[Animation] : Could not read frame!\n");
#endif
                iFrame = iNumOfFrames; // Exit loop
              } else
              {
                // Decode new frame
                if (hbmNewFrame)
                  hbmOldFrame = hbmNewFrame;
#ifdef USE_PRIVATE_RLE_COMPRESSION
                iDecompressedFrameSize = Decompress(pchOneFrame, piSizeTable[iFrame], pchDecompressedFrame, DEC_FRAME_BUF_SIZE);
                hbmNewFrame = DecodeBMPImage(pchDecompressedFrame, iDecompressedFrameSize, hab);
#else
                hbmNewFrame = DecodeBMPImage(pchOneFrame, piSizeTable[iFrame], hab);
#endif
                // Show new frame
                if (hbmNewFrame)
                {
                  WinSendMsg(WinWindowFromID(hwnd, BM_ANIMATION),
                             SM_SETHANDLE,
                             (MPARAM) hbmNewFrame,
                             (MPARAM) NULL);
                }
                // Destroy old bitmap
                if (hbmOldFrame)
                  GpiDeleteBitmap(hbmOldFrame);
              }
              dbg_free(pchOneFrame);
            }

            // Check for window existance!
            pWinPtr = WinQueryWindowPtr(hwnd, QWP_PFNWP);
            if ((iUseFrameRegulation) && (!iErrorInFrameRegulation))
            {
              if (DosWaitEventSem(hevBlitEvent, 128)==NO_ERROR)
              {
                ULONG ulPostCount;
                ULONG ulTemp;
                DosResetEventSem(hevBlitEvent, &ulPostCount);
                if (ulPostCount>1)
                {
                  ulPostCount--;
                  if (ulPostCount>20) ulPostCount = 20;
                  for (ulTemp=0; ulTemp<ulPostCount; ulTemp++)
                  {
                    DosPostEventSem(hevBlitEvent);
                  }
                }
              } else
                iErrorInFrameRegulation = 1;
            } else
              DosSleep(1);
            iFrame++; // Advance to next frame
            // Loop!
            if (iFrame == iNumOfFrames)
            {
              // Reached the last frame
              iFrame = 0;
#ifdef DEBUG_BUILD
//              printf("[Animation] : Looping animation!\n");
#endif
              fseek(hFile, 16 + sizeof(iNumOfFrames) + iNumOfFrames*sizeof(int), SEEK_SET);
            }
          }
#ifdef USE_PRIVATE_RLE_COMPRESSION
          if (pchDecompressedFrame)
          {
            dbg_free(pchDecompressedFrame); pchDecompressedFrame = NULL;
          }
#endif

#ifdef DEBUG_BUILD
          printf("[Animation] : Restoring original bitmap!\n");
#endif
          // Set back original bitmap
          WinSendMsg(WinWindowFromID(hwnd, BM_ANIMATION),
                     SM_SETHANDLE,
                     (MPARAM) hbmOriginalHandle,
                     (MPARAM) NULL);

#ifdef DEBUG_BUILD
          printf("[Animation] : Destroying last animation frame\n");
#endif
          // Destroy last shown bitmap
          if (hbmOldFrame)
            GpiDeleteBitmap(hbmOldFrame);

          // Uninitialize PM
          WinDestroyMsgQueue(hmq);
          WinTerminate(hab);
        }
        // dbg_free size table
        dbg_free(piSizeTable);
      }
    }
  }
  fclose(hFile);

  if (iUseFrameRegulation)
  {
#ifdef DEBUG_BUILD
    printf("[Animation] : Stopping Frame Regulation!\n");
#endif
    StopFrameRegulation();
    DosCloseEventSem(hevBlitEvent);
  }

#ifdef DEBUG_BUILD
  printf("[Animation] : Stopped!\n");
#endif

  _endthread();
}

MRESULT EXPENTRY AboutDialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  SWP mainswp, swp, swp2;
  char achBuffer[128];
  switch(msg)
  {
    case WM_INITDLG:
      // First rename and arrange controls!
      WinQueryWindowPos(hwnd, &mainswp);
      // OK Button
      WinQueryWindowPos(WinWindowFromID(hwnd, DID_OK), &swp);
      WinSetWindowPos(WinWindowFromID(hwnd, DID_OK), HWND_TOP,
		      (mainswp.cx-swp.cx)/2,
		      WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME),
		      0,
		      0,
		      SWP_MOVE);
      // Then the compilation time
      sprintf(achBuffer, "Compilation date: %s %s", __DATE__, __TIME__);
      WinSetWindowText(WinWindowFromID(hwnd, ST_COMPILATIONTIME), achBuffer);
      WinQueryWindowPos(WinWindowFromID(hwnd, ST_COMPILATIONTIME), &swp2);
      WinSetWindowPos(WinWindowFromID(hwnd, ST_COMPILATIONTIME), HWND_TOP,
		      100,
                      swp.y+swp.cy,
		      mainswp.cx-100 - 2*WinQuerySysValue(HWND_DESKTOP, SV_CXBORDER),
		      swp2.cy,
		      SWP_MOVE | SWP_SIZE);
      // Then the written by stuff
      WinQueryWindowPos(WinWindowFromID(hwnd, ST_COMPILATIONTIME), &swp);
      WinQueryWindowPos(WinWindowFromID(hwnd, ST_WRITTENBYDOODLE), &swp2);
      WinSetWindowPos(WinWindowFromID(hwnd, ST_WRITTENBYDOODLE), HWND_TOP,
		      swp.x,
                      swp.y+swp.cy,
		      swp.cx,
		      swp2.cy,
		      SWP_MOVE | SWP_SIZE);
      // Then the long name
      sprintf(achBuffer, "%s v%s", WAWE_LONGAPPNAME, pchVer);
      WinSetWindowText(WinWindowFromID(hwnd, ST_THEWARPEDWAVEEDITOR), achBuffer);
      WinQueryWindowPos(WinWindowFromID(hwnd, ST_WRITTENBYDOODLE), &swp);
      WinQueryWindowPos(WinWindowFromID(hwnd, ST_THEWARPEDWAVEEDITOR), &swp2);
      WinSetWindowPos(WinWindowFromID(hwnd, ST_THEWARPEDWAVEEDITOR), HWND_TOP,
		      swp.x,
                      swp.y+swp.cy,
		      swp.cx,
		      swp2.cy,
		      SWP_MOVE | SWP_SIZE);
      // Then the short name
      sprintf(achBuffer, "%s", WAWE_SHORTAPPNAME, pchVer);
      WinSetWindowText(WinWindowFromID(hwnd, ST_WAWE), achBuffer);
      WinQueryWindowPos(WinWindowFromID(hwnd, ST_THEWARPEDWAVEEDITOR), &swp);
      WinQueryWindowPos(WinWindowFromID(hwnd, ST_WAWE), &swp2);
      WinSetWindowPos(WinWindowFromID(hwnd, ST_WAWE), HWND_TOP,
		      swp.x,
                      swp.y+swp.cy,
		      swp.cx,
		      swp2.cy,
		      SWP_MOVE | SWP_SIZE);
      // Then resize the dialog window to be just as high as needed!
      WinSetWindowPos(hwnd, HWND_TOP,
		      0, 0,
                      mainswp.cx,
		      swp.y+swp.cy+2*WinQuerySysValue(HWND_DESKTOP, SV_CYBORDER)
		      *WinQuerySysValue(HWND_DESKTOP, SV_CYTITLEBAR),
		      SWP_SIZE);
      // Then reposition the bitmap control, where the animation will be shown!
      WinQueryWindowPos(hwnd, &mainswp);
      WinQueryWindowPos(WinWindowFromID(hwnd, BM_ANIMATION), &swp);
      WinSetWindowPos(WinWindowFromID(hwnd, BM_ANIMATION), HWND_TOP,
                      10,
                      (mainswp.cy-
                       WinQuerySysValue(HWND_DESKTOP, SV_CYTITLEBAR)-
                       swp.cy)/2,
                      0, 0,
		      SWP_MOVE);

#ifdef DEBUG_BUILD
      printf("Starting animation thread!\n");
#endif
      _beginthread(AnimationThreadFunc, 0, 16384, (VOID *) hwnd);
      break;
    default:
      break;
  }
  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}

void ResizeCustomFileDialogControls(HWND hwnd)
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

  // First: Set position of Ok, Cancel, and Help buttons!
  WinQueryWindowPos(hwnd, &Swp1);
  WinQueryWindowPos(WinWindowFromID(hwnd, DID_OK), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, DID_OK), HWND_TOP,
                  CXDLGFRAME*3,
                  CYDLGFRAME*3,
                  0,
                  0,
                  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, DID_OK), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, DID_CANCEL), HWND_TOP,
                  Swp2.x+Swp2.cx+CXDLGFRAME,
                  Swp2.y,
                  0,
                  0,
                  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, DID_CANCEL), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, DID_HELP_PB), HWND_TOP,
                  Swp2.x+Swp2.cx+CXDLGFRAME,
                  Swp2.y,
                  0,
                  0,
                  SWP_MOVE);

  // Ok, pushbuttons are positioned.
  // Now the filename text!
  WinQueryWindowPos(WinWindowFromID(hwnd, DID_FILENAME_TXT), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, DID_FILENAME_TXT), HWND_TOP,
                  CXDLGFRAME*3,
                  Swp1.cy - CYTITLEBAR - CYDLGFRAME*3 - Swp2.cy,
                  0,
                  0,
                  SWP_MOVE);

  // Now the filename edit
  WinQueryWindowPos(WinWindowFromID(hwnd, DID_FILENAME_TXT), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, DID_FILENAME_ED), HWND_TOP,
                  Swp2.x + Swp2.cx + 2*CXDLGFRAME,
                  Swp2.y,
                  Swp1.cx - Swp2.x - Swp2.cx - 2*CXDLGFRAME - CXDLGFRAME*3 - CXDLGFRAME,
                  Swp2.cy,
                  SWP_SIZE | SWP_MOVE);

  // Drive text
  WinQueryWindowPos(WinWindowFromID(hwnd, DID_FILENAME_TXT), &Swp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, DID_DRIVE_TXT), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, DID_DRIVE_TXT), HWND_TOP,
                  Swp2.x,
                  Swp2.y - Swp2.cy - 3*CYDLGFRAME,
                  0,
                  0,
                  SWP_MOVE);
  // Drive combo:
  WinQueryWindowPos(WinWindowFromID(hwnd, DID_FILENAME_ED), &Swp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, DID_DRIVE_CB), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, DID_DRIVE_CB), HWND_TOP,
                  Swp2.x,
                  Swp2.y - Swp3.cy - CXDLGFRAME,
                  Swp1.cx/2 - Swp2.x - CXDLGFRAME,
                  Swp3.cy,
                  SWP_SIZE | SWP_MOVE);

  // Open As text
  WinQueryWindowPos(WinWindowFromID(hwnd, DID_DRIVE_TXT), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_OPENAS), HWND_TOP,
                  Swp1.cx/2 + CXDLGFRAME,
                  Swp2.y,
                  0,
                  0,
                  SWP_MOVE);
  // Open As combo:
  WinQueryWindowPos(WinWindowFromID(hwnd, DID_DRIVE_CB), &Swp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_OPENAS), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, CX_OPENAS), HWND_TOP,
                  Swp3.x + Swp3.cx + CXDLGFRAME,
                  Swp2.y,
                  Swp1.cx - Swp3.cx - Swp3.x - 3*CXDLGFRAME - CXDLGFRAME,
                  Swp2.cy,
                  SWP_SIZE | SWP_MOVE);

  // Directory text
  WinQueryWindowPos(WinWindowFromID(hwnd, DID_DRIVE_TXT), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, DID_DIRECTORY_TXT), HWND_TOP,
                  Swp2.x,
                  Swp2.y - Swp2.cy - 3*CXDLGFRAME,
                  0,
                  0,
                  SWP_MOVE);
  // File text
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_OPENAS), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, DID_FILES_TXT), HWND_TOP,
                  Swp2.x,
                  Swp2.y - Swp2.cy - 3*CXDLGFRAME,
                  0,
                  0,
                  SWP_MOVE);

  // Directory listbox
  WinQueryWindowPos(WinWindowFromID(hwnd, DID_DIRECTORY_TXT), &Swp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, DID_OK), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, DID_DIRECTORY_LB), HWND_TOP,
                  Swp2.x,
                  Swp3.y + Swp3.cy + 2*CXDLGFRAME,
                  Swp1.cx/2 - Swp2.x - 2*CXDLGFRAME,
                  Swp2.y - Swp3.y - Swp3.cy - 4*CXDLGFRAME,
                  SWP_SIZE | SWP_MOVE);

  // Files listbox
  WinQueryWindowPos(WinWindowFromID(hwnd, DID_DIRECTORY_LB), &Swp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, DID_FILES_TXT), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, DID_FILES_LB), HWND_TOP,
                  Swp3.x,
                  Swp2.y+1,
                  Swp2.cx - CXDLGFRAME,
                  Swp2.cy,
                  SWP_SIZE | SWP_MOVE);

  if (iDisabledWindowUpdate)
    WinEnableWindowUpdate(hwnd, TRUE);
}

MRESULT EXPENTRY CustomOpenDialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  MRESULT rc;
  FILEDLG *fd;
  WaWEPluginList_p pImportPluginList;
  LONG lID;

  switch(msg)
  {
    case WM_DESTROY:
     {
       // Set in filedlg structure the selected plugin too!
       fd = (FILEDLG *)WinQueryWindowULong(hwnd, QWL_USER);
       if (fd)
       {
         // Query currently selected item index!
         lID = (LONG) WinSendDlgItemMsg(hwnd, CX_OPENAS, LM_QUERYSELECTION,
                                          (MPARAM) LIT_CURSOR,
                                          (MPARAM) 0);
         // And query handle of selected item
         fd->ulUser = (ULONG) WinSendDlgItemMsg(hwnd, CX_OPENAS, LM_QUERYITEMHANDLE,
                                                (MPARAM) lID,
                                                (MPARAM) 0);
       }
       // Then let the default stuff run!
       break;
     }
    case WM_INITDLG:
        {
          char *pchAutomatic = "<Automatic>";

          // Let's resize the controls inside the window!
          ResizeCustomFileDialogControls(hwnd);

          WinSetDlgItemText(hwnd, ST_OPENAS, "Open with");
          // Fill 'Open With' listbox with elements
          WinSendDlgItemMsg(hwnd, CX_OPENAS, LM_DELETEALL, (MPARAM) 0, (MPARAM) 0);
          // Add default item, and set handle (plugin pointer) to null!
          lID = (LONG) WinSendDlgItemMsg(hwnd, CX_OPENAS, LM_INSERTITEM,
                                         (MPARAM) LIT_SORTASCENDING,
                                         (MPARAM) pchAutomatic);
          WinSendDlgItemMsg(hwnd, CX_OPENAS, LM_SETITEMHANDLE,
                            (MPARAM) lID,
                            (MPARAM) NULL);

          // Now go through all plugins, and add them into listbox!
          fd = (FILEDLG *)WinQueryWindowULong(hwnd, QWL_USER);
          if (fd)
          {
            pImportPluginList = (WaWEPluginList_p) (fd->ulUser);
            while (pImportPluginList)
            {
              // Add item into listbox
              lID = (LONG) WinSendDlgItemMsg(hwnd, CX_OPENAS, LM_INSERTITEM,
                                             (MPARAM) LIT_SORTASCENDING,
                                             (MPARAM) (pImportPluginList->pPlugin->achName));
              // And set handle of item to pointer to plugin descriptor
              WinSendDlgItemMsg(hwnd, CX_OPENAS, LM_SETITEMHANDLE,
                                (MPARAM) lID,
                                (MPARAM) (pImportPluginList->pPlugin));
              pImportPluginList = pImportPluginList->pNext;
            }
          }
          // Select the first one!
          WinSendDlgItemMsg(hwnd, CX_OPENAS, LM_SELECTITEM, (MPARAM) 0, (MPARAM) TRUE);
        
          rc=WinDefFileDlgProc(hwnd, msg, mp1, mp2);
          WinEnableWindow(WinWindowFromID(hwnd, DID_OK), TRUE);
          return rc;
	}
    case WM_FORMATFRAME:
        {
          // Let's resize the controls inside the window!
	  ResizeCustomFileDialogControls(hwnd);
          break;
        }
    default:
        break;
  }
  return WinDefFileDlgProc(hwnd, msg, mp1, mp2);
}

MRESULT EXPENTRY CustomSaveAsDialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  MRESULT rc;
  FILEDLG *fd;
  WaWEPluginList_p pImportPluginList;
  LONG lID;

  switch(msg)
  {
    case WM_DESTROY:
     {
       // Set in filedlg structure the selected plugin too!
       fd = (FILEDLG *)WinQueryWindowULong(hwnd, QWL_USER);
       if (fd)
       {
         // Query currently selected item index!
         lID = (LONG) WinSendDlgItemMsg(hwnd, CX_OPENAS, LM_QUERYSELECTION,
                                          (MPARAM) LIT_CURSOR,
                                          (MPARAM) 0);
         // And query handle of selected item
         fd->ulUser = (ULONG) WinSendDlgItemMsg(hwnd, CX_OPENAS, LM_QUERYITEMHANDLE,
                                                (MPARAM) lID,
                                                (MPARAM) 0);
       }
       // Then let the default stuff run!
       break;
     }
    case WM_INITDLG:
        {
          char *pchAutomatic = "<Automatic>";

          // Let's resize the controls inside the window!
          ResizeCustomFileDialogControls(hwnd);

          WinSetDlgItemText(hwnd, ST_OPENAS, "Save with");
          // Fill 'Save With' listbox with elements
          WinSendDlgItemMsg(hwnd, CX_OPENAS, LM_DELETEALL, (MPARAM) 0, (MPARAM) 0);
          // Add default item, and set handle (plugin pointer) to null!
          lID = (LONG) WinSendDlgItemMsg(hwnd, CX_OPENAS, LM_INSERTITEM,
                                         (MPARAM) LIT_SORTASCENDING,
                                         (MPARAM) pchAutomatic);
          WinSendDlgItemMsg(hwnd, CX_OPENAS, LM_SETITEMHANDLE,
                            (MPARAM) lID,
                            (MPARAM) NULL);

          // Now go through all plugins, and add them into listbox!
          fd = (FILEDLG *)WinQueryWindowULong(hwnd, QWL_USER);
          if (fd)
          {
            pImportPluginList = (WaWEPluginList_p) (fd->ulUser);
            while (pImportPluginList)
            {
              // Add item into listbox
              lID = (LONG) WinSendDlgItemMsg(hwnd, CX_OPENAS, LM_INSERTITEM,
                                             (MPARAM) LIT_SORTASCENDING,
                                             (MPARAM) (pImportPluginList->pPlugin->achName));
              // And set handle of item to pointer to plugin descriptor
              WinSendDlgItemMsg(hwnd, CX_OPENAS, LM_SETITEMHANDLE,
                                (MPARAM) lID,
                                (MPARAM) (pImportPluginList->pPlugin));
              pImportPluginList = pImportPluginList->pNext;
            }
          }
          // Select the first one!
          WinSendDlgItemMsg(hwnd, CX_OPENAS, LM_SELECTITEM, (MPARAM) 0, (MPARAM) TRUE);
        
          rc=WinDefFileDlgProc(hwnd, msg, mp1, mp2);
          WinEnableWindow(WinWindowFromID(hwnd, DID_OK), TRUE);
          return rc;
	}
    case WM_FORMATFRAME:
        {
          // Let's resize the controls inside the window!
	  ResizeCustomFileDialogControls(hwnd);
          break;
        }
    default:
        break;
  }
  return WinDefFileDlgProc(hwnd, msg, mp1, mp2);
}


void ResizeCreateNewChannelSetDialogControls(HWND hwnd)
{
  SWP Swp1, Swp2, Swp3;
  ULONG CXDLGFRAME, CYDLGFRAME, CYTITLEBAR;
  int iTopGBHeight;
  int iDisabledWindowUpdate=0;

  if (WinIsWindowVisible(hwnd))
  {
    WinEnableWindowUpdate(hwnd, FALSE);
    iDisabledWindowUpdate = 1;
  }
  CXDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
  CYDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);
  CYTITLEBAR = WinQuerySysValue(HWND_DESKTOP, SV_CYTITLEBAR);

  // First: Set position of Create, Cancel buttons!
  WinQueryWindowPos(hwnd, &Swp1);
  WinQueryWindowPos(WinWindowFromID(hwnd, PB_CREATE), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, PB_CREATE), HWND_TOP,
                  Swp1.cx/4 - Swp2.cx/2,
                  CYDLGFRAME*2,
                  0,
                  0,
                  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, PB_CANCEL), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, PB_CANCEL), HWND_TOP,
                  Swp1.cx/4*3 - Swp2.cx/2,
                  CYDLGFRAME*2,
                  0,
                  0,
                  SWP_MOVE);


  // Ok, pushbuttons are positioned.
  // Now the two groupboxes!

  // First calculate the height needed for the top one!
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_NAMEOFCHANNELSET), &Swp3);
  iTopGBHeight = 3*Swp3.cy + 6*CYDLGFRAME;

  // Now set position and size of bottom one!
  WinQueryWindowPos(WinWindowFromID(hwnd, PB_CREATE), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, GB_FORMATOFCHANNELSET), HWND_TOP,
                  CXDLGFRAME*2,                                  // Pos
		  Swp2.y + Swp2.cy + CYDLGFRAME,
                  Swp1.cx - 4*CXDLGFRAME,                        // Size
		  Swp1.cy - CYTITLEBAR - CYDLGFRAME*3 - iTopGBHeight
                  - (Swp2.y + Swp2.cy + CYDLGFRAME),
                  SWP_MOVE | SWP_SIZE);

  // And the top one!
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_FORMATOFCHANNELSET), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, GB_GENERALPROPERTIES), HWND_TOP,
                  CXDLGFRAME*2,                                  // Pos
		  Swp2.y + Swp2.cy + CXDLGFRAME*1,
                  Swp1.cx - 4*CXDLGFRAME,                        // Size
                  iTopGBHeight,
		  SWP_MOVE | SWP_SIZE);

  // Now position the controls inside the groupboxes!
  // First the top one
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_GENERALPROPERTIES), &Swp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_NAMEOFCHANNELSET), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, EF_CHANNELSETNAME), HWND_TOP,
                  Swp2.x + 2*CXDLGFRAME,                         // Pos
		  Swp2.y + 2*CYDLGFRAME,
                  Swp2.cx - 4*CXDLGFRAME,                        // Size
                  Swp3.cy,
                  SWP_MOVE | SWP_SIZE);
  WinQueryWindowPos(WinWindowFromID(hwnd, EF_CHANNELSETNAME), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_NAMEOFCHANNELSET), HWND_TOP,
                  Swp3.x,                                        // Pos
		  Swp3.y + Swp3.cy + 2*CYDLGFRAME,
                  0,                        // Size
                  0,
                  SWP_MOVE);

  // Then the bottom one
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_FORMATOFCHANNELSET), &Swp2);
  //   Initial number of channels:
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_INITIALNUMBEROFCHANNELS), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_INITIALNUMBEROFCHANNELS), HWND_TOP,
                  Swp2.x + 1*CXDLGFRAME,                         // Pos
		  Swp2.y + Swp2.cy - 2*Swp3.cy - 2*CYDLGFRAME,
                  0,                        // Size
                  0,
                  SWP_MOVE);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_INITIALNUMBEROFCHANNELS), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, EF_NUMBEROFCHANNELS), HWND_TOP,
                  Swp3.x + Swp3.cx + 2*CXDLGFRAME,               // Pos
		  Swp3.y,
                  Swp2.x+Swp2.cx - (Swp3.x + Swp3.cx + 2*CXDLGFRAME) - 2*CXDLGFRAME, // Size
                  Swp3.cy,
                  SWP_MOVE | SWP_SIZE);
  //   Sampling frequency:
  WinQueryWindowPos(WinWindowFromID(hwnd, EF_NUMBEROFCHANNELS), &Swp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_INITIALNUMBEROFCHANNELS), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_FREQUENCY), HWND_TOP,
                  Swp3.x,                         // Pos
		  Swp3.y - Swp3.cy - 2*CYDLGFRAME,
                  0,                             // Size
                  0,
                  SWP_MOVE);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_FREQUENCY), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, CX_FREQUENCY), HWND_TOP,
                  Swp2.x,               // Pos
		  Swp3.y - (Swp3.cy*3+2*CYDLGFRAME),
                  Swp2.cx, // Size
                  Swp3.cy*4 + 3*CYDLGFRAME,
                  SWP_MOVE | SWP_SIZE);
  //   BitsPerSample:
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_FREQUENCY), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_BITS), HWND_TOP,
                  Swp3.x,                         // Pos
		  Swp3.y - Swp3.cy - 2*CYDLGFRAME,
                  0,                             // Size
                  0,
                  SWP_MOVE);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_BITS), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, CX_BITS), HWND_TOP,
                  Swp2.x,               // Pos
		  Swp3.y - (Swp3.cy*3+2*CYDLGFRAME),
                  Swp2.cx, // Size
                  Swp3.cy*4 + 3*CYDLGFRAME,
                  SWP_MOVE | SWP_SIZE);
  //   SamplesAre:
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_BITS), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_SAMPLESARE), HWND_TOP,
                  Swp3.x,                         // Pos
		  Swp3.y - Swp3.cy - 2*CYDLGFRAME,
                  0,                             // Size
                  0,
                  SWP_MOVE);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_SAMPLESARE), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, CX_SIGNED), HWND_TOP,
                  Swp2.x,               // Pos
		  Swp3.y - (Swp3.cy*3+2*CYDLGFRAME),
                  Swp2.cx, // Size
                  Swp3.cy*4 + 3*CYDLGFRAME,
                  SWP_MOVE | SWP_SIZE);
  

  if (iDisabledWindowUpdate)
    WinEnableWindowUpdate(hwnd, TRUE);
}


MRESULT EXPENTRY CreateNewChannelSetDialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  LONG lID, lDefaultID;

  switch(msg)
  {
    case WM_FORMATFRAME:
      {
        ResizeCreateNewChannelSetDialogControls(hwnd);
        break;
      }
    case WM_INITDLG:
      {
        char achName[WAWE_CHANNELSET_NAME_LEN];
        // Set the limits of entry fields
        WinSendDlgItemMsg(hwnd, EF_NUMBEROFCHANNELS,
                          EM_SETTEXTLIMIT,
                          (MPARAM) (WAWE_CHANNELSET_NAME_LEN-1),
                          (MPARAM) 0);
	// Clear entry fields
	WinSendDlgItemMsg(hwnd, CX_FREQUENCY,
			  LM_DELETEALL,
			  (MPARAM) 0,
                          (MPARAM) 0);
	WinSendDlgItemMsg(hwnd, EF_NUMBEROFCHANNELS,
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
        WinSetDlgItemText(hwnd, CX_FREQUENCY, "44100");

        // Prepare new channel-set name
        snprintf(achName, WAWE_CHANNELSET_NAME_LEN,
                 "Channel-set %d", iNextChannelSetID);
        WinSetDlgItemText(hwnd, EF_CHANNELSETNAME, achName);

	WinSetDlgItemText(hwnd, EF_NUMBEROFCHANNELS, "2");
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
				       (MPARAM) "Signed");
	WinSendDlgItemMsg(hwnd, CX_SIGNED,
			  LM_SETITEMHANDLE,
			  (MPARAM) lID,
                          (MPARAM) 1);

	lID = (LONG) WinSendDlgItemMsg(hwnd, CX_SIGNED,
				       LM_INSERTITEM,
				       (MPARAM) LIT_END,
				       (MPARAM) "Unsigned");
	WinSendDlgItemMsg(hwnd, CX_SIGNED,
			  LM_SETITEMHANDLE,
			  (MPARAM) lID,
                          (MPARAM) 0);

	WinSendDlgItemMsg(hwnd, CX_SIGNED,
			  LM_SELECTITEM,
			  (MPARAM) lDefaultID,
                          (MPARAM) TRUE);
        // Then the Frequency
	lID = (LONG) WinSendDlgItemMsg(hwnd, CX_FREQUENCY,
				       LM_INSERTITEM,
				       (MPARAM) LIT_END,
				       (MPARAM) "48000");
	WinSendDlgItemMsg(hwnd, CX_FREQUENCY,
			  LM_SETITEMHANDLE,
			  (MPARAM) lID,
                          (MPARAM) 48000);

        lDefaultID =
	lID = (LONG) WinSendDlgItemMsg(hwnd, CX_FREQUENCY,
				       LM_INSERTITEM,
				       (MPARAM) LIT_END,
				       (MPARAM) "44100");
	WinSendDlgItemMsg(hwnd, CX_FREQUENCY,
			  LM_SETITEMHANDLE,
			  (MPARAM) lID,
                          (MPARAM) 44100);

	lID = (LONG) WinSendDlgItemMsg(hwnd, CX_FREQUENCY,
				       LM_INSERTITEM,
				       (MPARAM) LIT_END,
				       (MPARAM) "22050");
	WinSendDlgItemMsg(hwnd, CX_FREQUENCY,
			  LM_SETITEMHANDLE,
			  (MPARAM) lID,
                          (MPARAM) 22050);

        lID = (LONG) WinSendDlgItemMsg(hwnd, CX_FREQUENCY,
				       LM_INSERTITEM,
				       (MPARAM) LIT_END,
				       (MPARAM) "11025");
	WinSendDlgItemMsg(hwnd, CX_FREQUENCY,
			  LM_SETITEMHANDLE,
			  (MPARAM) lID,
                          (MPARAM) 11025);


	WinSendDlgItemMsg(hwnd, CX_FREQUENCY,
			  LM_SELECTITEM,
			  (MPARAM) lDefaultID,
                          (MPARAM) TRUE);
        // Ok, stuffs set up!
	return (MRESULT) 0;
      }
    default:
      break;
  }
  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}

static void GetFilePath(char *pchPathBuf, unsigned int uiPathBufSize, char *pchFilePath)
{
  int iFilePathSize;
  int iPos;
  int iLastPathCharPos;

  if ((!pchPathBuf) || (!pchFilePath)) return;

  iFilePathSize = strlen(pchFilePath);
  iLastPathCharPos = -1;
  // Search for last '\' or '/' or ':' character in string!
  iPos = iFilePathSize;
  while ((iPos>=0) &&
         (pchFilePath[iPos]!='\\') &&
         (pchFilePath[iPos]!='/') &&
         (pchFilePath[iPos]!=':'))
    iPos--;

  if (iPos < (uiPathBufSize-1))
    iLastPathCharPos = iPos;
  else
    iLastPathCharPos = uiPathBufSize-2;

  // Copy characters!
  for (iPos = 0; iPos<=iLastPathCharPos; iPos++)
    pchPathBuf[iPos] = pchFilePath[iPos];

  pchPathBuf[iLastPathCharPos+1] = 0;
}

static void internal_Open_core(char *pchFullFileName, WaWEPlugin_p pImportPlugin)
{
  WaWEWorker2Msg_Open_p pOpenMsgParm;
  FILE *hFile;
  char *pchErrorMsg;

  // Store the folder of the file first in achLastOpenFolder!
  GetFilePath(WaWEConfiguration.achLastOpenFolder, sizeof(WaWEConfiguration.achLastOpenFolder), pchFullFileName);
#ifdef DEBUG_BUILD
//  printf("[internal_Open_core] : Saved last open folder as [%s]\n", WaWEConfiguration.achLastOpenFolder);
#endif
  // Send a message to Worker to try to open this file with that plugin!
  if (GetChannelSetFromFileName(pchFullFileName))
  {
    ErrorMsg("This file is already opened!");
  } else
  {
    // Now let's see if such a file really exists or not!
    hFile = fopen(pchFullFileName, "rb");
    if (!hFile)
    {
      // File does not exist, don't try to open it, but give an error message!
      pchErrorMsg = dbg_malloc(strlen(pchFullFileName)+256);
      if (pchErrorMsg)
      {
        sprintf(pchErrorMsg, "Could not open the following file:\n%s", pchFullFileName);
        ErrorMsg(pchErrorMsg);
        dbg_free(pchErrorMsg);
      }
    } else
    {
      // File exists, so close file handle, and open it with plugin!
      fclose(hFile);

      pOpenMsgParm = (WaWEWorker2Msg_Open_p) dbg_malloc(sizeof(WaWEWorker2Msg_Open_t));
      if (pOpenMsgParm)
      {
        pOpenMsgParm->pImportPlugin = pImportPlugin;
        strncpy(pOpenMsgParm->achFileName, pchFullFileName, sizeof(pOpenMsgParm->achFileName));
  
        if (!AddNewWorker2Msg(WORKER2_MSG_OPEN, sizeof(WaWEWorker2Msg_Open_t), pOpenMsgParm))
        {
          // Error adding the worker message to the queue, the queue might be full?
          dbg_free(pOpenMsgParm);
        }
      }
    }
  }
}

MRESULT MenuSelected_Open(HWND hwnd)
{
  FILEDLG fd = { 0 };
  WaWEPluginList_p pImportPluginList;

  fd.cbSize = sizeof( fd );
  // It's a centered 'Open'-dialog
  fd.fl = FDS_CUSTOM | FDS_OPEN_DIALOG | FDS_CENTER;
  fd.pfnDlgProc = CustomOpenDialogProc;
  fd.usDlgId = DLG_OPENIMPORTFILE;
  fd.hMod = 0; // Read from exe
  fd.pszTitle = "Open/Import file...";

  pImportPluginList = CreatePluginList(WAWE_PLUGIN_TYPE_IMPORT);
  fd.ulUser = (ULONG) pImportPluginList;

  // Initial path, file name, or file filter
  snprintf(fd.szFullFile, sizeof(fd.szFullFile), "%s*.*", WaWEConfiguration.achLastOpenFolder);

  if(WinFileDlg( HWND_DESKTOP, hwnd, &fd ) && (fd.lReturn == DID_OK) )
  {
    // Ok, user selected a filename and an import plugin (or null).
    internal_Open_core(fd.szFullFile,
                      (WaWEPlugin_p) (fd.ulUser));
  }
  // That was all, dbg_free the plugin list we used here...
  FreePluginList(pImportPluginList);

  return (MRESULT) FALSE;
}

MRESULT MenuSelected_NewChannel(HWND hwnd)
{
  int rc;
  WaWEChannel_p pNewChannel;

#ifdef DEBUG_BUILD
  printf("[MenuSelected_NewChannel] : Called!\n");
#endif

  if (!pActiveChannelSet)
  {
    ErrorMsg("To create a Channel, one needs a Channel-Set first!");
  } else
  {

    rc = WaWEHelper_ChannelSet_CreateChannel(pActiveChannelSet,  // Insert into this channel-set
					     WAWECHANNEL_BOTTOM, // Insert here
					     NULL,               // No proposed name
					     &pNewChannel);
    if (rc!=WAWEHELPER_NOERROR)
      ErrorMsg("Could not create channel!");
    else
      RedrawChangedChannelsAndChannelSets();
  }
  return (MRESULT) FALSE;
}

MRESULT MenuSelected_NewChannelSet(HWND hwnd)
{
  HWND hwndDlg;
  SWP swap, DlgSwap, ParentSwap;

#ifdef DEBUG_BUILD
  printf("[MenuSelected_NewChannelSet] : Called!\n");
#endif

  // Load plugins, and create an info window for it!
  hwndDlg = WinLoadDlg(HWND_DESKTOP, hwnd,
                       CreateNewChannelSetDialogProc, NULLHANDLE,
                       DLG_CREATENEWCHANNELSET, NULL);
  if (hwndDlg)
  {
    // Now try to reload size and position information from INI file
    if (!WinRestoreWindowPos(WinStateSave_AppName, WinStateSave_CreateNewChannelSetWindowKey, hwndDlg))
    {
      // If could not, then center the window to its parent!
#ifdef DEBUG_BUILD
      printf("[MenuSelected_NewChannelSet] : Centering window to parent\n");
#endif

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
#ifdef DEBUG_BUILD
    else
      printf("[MenuSelected_NewChannelSet] : Restored window pos from INI file\n");
#endif

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
    ResizeCreateNewChannelSetDialogControls(hwndDlg);

    // Activate and make it visible
    WinSetWindowPos(hwndDlg, HWND_TOP, 0, 0, 0, 0,
		    SWP_SHOW | SWP_ACTIVATE | SWP_ZORDER);
    WinSetFocus(HWND_DESKTOP, hwndDlg);


    // Now process the window
    if (WinProcessDlg(hwndDlg)==PB_CREATE)
    {
      WaWEFormat_t NewFormat;
      int          iNumOfChannels;
      char         achEntered[WAWE_CHANNELSET_NAME_LEN];
      char        *pchName;
      unsigned long ulSelected;

      // Ok, we're about to create channel-set!
#ifdef DEBUG_BUILD
      printf("[MenuSelected_NewChannelSet] : About to create new Channel-Set!\n");
#endif

      // Set default stuffs
      NewFormat.iFrequency = 44100;
      iNumOfChannels = 2;
      NewFormat.iBits = 16;
      NewFormat.iIsSigned = 1;
      pchName = NULL;

      WinQueryDlgItemText(hwndDlg, CX_FREQUENCY, WAWE_CHANNELSET_NAME_LEN, achEntered);
      NewFormat.iFrequency = atoi(achEntered);
      WinQueryDlgItemText(hwndDlg, EF_NUMBEROFCHANNELS, WAWE_CHANNELSET_NAME_LEN, achEntered);
      iNumOfChannels = atoi(achEntered);
      ulSelected = (LONG) WinSendDlgItemMsg(hwndDlg, CX_BITS, LM_QUERYSELECTION, (MPARAM) 0, (MPARAM) 0);
      NewFormat.iBits = (int) WinSendDlgItemMsg(hwndDlg, CX_BITS, LM_QUERYITEMHANDLE, (MPARAM) ulSelected, (MPARAM) 0);
      ulSelected = (ULONG) WinSendDlgItemMsg(hwndDlg, CX_SIGNED, LM_QUERYSELECTION, (MPARAM) 0, (MPARAM) 0);
      NewFormat.iIsSigned = (int) WinSendDlgItemMsg(hwndDlg, CX_SIGNED, LM_QUERYITEMHANDLE, (MPARAM) ulSelected, (MPARAM) 0);

      if (WinQueryDlgItemText(hwndDlg, EF_CHANNELSETNAME, WAWE_CHANNELSET_NAME_LEN, achEntered))
        pchName = &(achEntered[0]);

      // Now check the results!
      if ((NewFormat.iFrequency<100) ||
          (NewFormat.iBits<8) ||
          (NewFormat.iBits>32) ||
          (NewFormat.iBits%8 != 0) ||
          (iNumOfChannels<0) ||
          (iNumOfChannels>64))
      {
        ErrorMsg("Invalid Channel-Set format settings!");
      } else
      {
        WaWEChannelSet_p pNewChannelSet;
        int rc;

        rc = WaWEHelper_Global_CreateChannelSet(&pNewChannelSet,
                                                &NewFormat,
                                                iNumOfChannels,
                                                pchName,
                                                NULL);
        if (rc!=WAWEHELPER_NOERROR)
          ErrorMsg("Could not create new Channel-Set!\n");
      }
    }

    // Now save size and position info into INI file
    WinStoreWindowPos(WinStateSave_AppName, WinStateSave_CreateNewChannelSetWindowKey, hwndDlg);

    WinDestroyWindow(hwndDlg);
  }
  return (MRESULT) FALSE;
}

MRESULT MenuSelected_DeleteChannel(HWND hwnd, WaWEChannelSet_p pCurrChannelSet, WaWEChannel_p pCurrChannel)
{
  int rc;

#ifdef DEBUG_BUILD
  printf("[MenuSelected_DeleteChannel] : Called!\n");
#endif

  // Check parameters!
  if (!pCurrChannel) return (MRESULT) FALSE;

  rc = WaWEHelper_ChannelSet_DeleteChannel(pCurrChannel);
  if (rc!=WAWEHELPER_NOERROR)
  {
    if (rc == WAWEHELPER_ERROR_ELEMENTINUSE)
      ErrorMsg("The selected channel or channel-set is currently in use.\n"
               "The deletion has been aborted. Please try again later!");
    else
      ErrorMsg("Could not delete channel!");
  }
  else
    RedrawChangedChannelsAndChannelSets();

  return (MRESULT) FALSE;
}

MRESULT MenuSelected_DeleteChannelSet(HWND hwnd, WaWEChannelSet_p pCurrChannelSet, WaWEChannel_p pCurrChannel)
{
  int bDeleteConfirmed;
  char *pchMessage;

#ifdef DEBUG_BUILD
  printf("[MenuSelected_DeleteChannelSet] : Called!\n");
#endif

  if (pCurrChannelSet)
  {
    pchMessage = dbg_malloc(1024);

    // Ask the user if he really wants to delete the channel-set!
    // Give info about modified/unmodified state, too!
    if (pCurrChannelSet->bModified)
    {
      if (pchMessage)
      {
        snprintf(pchMessage,
                 1024,
                 "You are about to delete a Channel-Set (%s) which is not saved yet.\n\nAre you sure you want to delete it?\n",
                 pCurrChannelSet->achName);
      } else
        pchMessage = "This Channel-Set is not saved yet.\n\nAre you sure you want to delete it?";

      bDeleteConfirmed = (WinMessageBox(HWND_DESKTOP, hwnd,
                                        pchMessage,
                                        "Deleting Modified Channel-Set", 0, MB_YESNO | MB_MOVEABLE | MB_ICONQUESTION)==MBID_YES);
    } else
    {
      if (pchMessage)
      {
        snprintf(pchMessage,
                 1024,
                 "You are about to delete a Channel-Set (%s).\n\nAre you sure you want to delete it?\n",
                 pCurrChannelSet->achName);
      } else
        pchMessage = "Are you sure you want to delete this Channel-Set?";

      bDeleteConfirmed = (WinMessageBox(HWND_DESKTOP, hwnd,
                                        pchMessage,
                                        "Deleting Channel-Set", 0, MB_YESNO | MB_MOVEABLE | MB_ICONQUESTION)==MBID_YES);
    }

    if (pchMessage)
      dbg_free(pchMessage);

    // Now delete the channel-set if the user really wants to!
    if (bDeleteConfirmed)
    {
      int rc;
      rc = WaWEHelper_Global_DeleteChannelSet(pCurrChannelSet);
#ifdef DEBUG_BUILD
      if (rc!=WAWEHELPER_NOERROR)
      {
        printf("DeleteChannel reported rc=%d\n", rc); fflush(stdout);
      }
#endif
      if (rc == WAWEHELPER_ERROR_ELEMENTINUSE)
        ErrorMsg("The selected channel-set is currently in use.\n"
                 "The deletion has been aborted. Please try again later!");

    }
  }

  return (MRESULT) FALSE;
}

void GenerateTempFileFromRealFileName(char *pchOriginalFileName, char *pchTempFileName)
{
  int iLen;

  strcpy(pchTempFileName, pchOriginalFileName);
  // Cut the filename from the end
  iLen = strlen(pchTempFileName);
  while ((iLen>0) &&
         (pchTempFileName[iLen-1]!='\\') &&
         (pchTempFileName[iLen-1]!='/') &&
         (pchTempFileName[iLen-1]!=':'))
  {
    pchTempFileName[iLen-1] = 0;
    iLen--;
  }
  // Add temp filename
  strcat(pchTempFileName, "WaWETemp.$$$");
}

void MenuSelected_Save_core(HWND hwnd, WaWEChannelSet_p pChannelSet, char *pchFileName, WaWEPlugin_p pExportPlugin, WaWEExP_Create_Desc_p pFormatDesc)
{
  void *hFile;
  char achTempFileName[TEMPFILENAME_LEN];
  HWND hwndSaving;
  WaWEWorker2Msg_SaveChannelSet_p pWorkerMsg;
  SWP DlgSwp, OwnerSwp;

  // Check parameters!
  if ((!pChannelSet) || (!pchFileName) || (!pExportPlugin)) return;

  // So:
  // - Use the Export plugin to save the channel-set into a temporary file
  // - Close temporarily every file handle (so if user overwrites one of the files,
  //   they will be reopened well)
  // - Rename the temp file to target filename
  // - Reopen every channel-set
  //   - If it doesn't succeed: Inform user and close channel-set

  // Save the channel-set using the export plugin
  // First, create a temporary file next to the real file
  GenerateTempFileFromRealFileName(pchFileName, achTempFileName);

  // Try to open the new file using the plugin
  hFile = pExportPlugin->TypeSpecificInfo.ExportPluginInfo.fnCreate(achTempFileName, pFormatDesc, hwnd);
  if (!hFile)
    return; // Could not create file! The plugin should notify the user about the origin of the problem.

  // Otherwise the plugin could create the temp file, so let's tell the worker thread
  // to save the contents to the file!
  // Meanwhile we'll show a window about the progress.

  hwndSaving = WinLoadDlg(HWND_DESKTOP, // Parent
                          hwnd,         // Owner
                          WinDefDlgProc, // DlgProc
                          NULLHANDLE,   // hmod
                          DLG_SAVING,   // iddlg
                          NULL);        // Dialog parameters
  if (!hwndSaving)
  {
    // Close the file
    pExportPlugin->TypeSpecificInfo.ExportPluginInfo.fnClose(hFile, NULL);
    // Delete temporary file if exists
    DosDelete(achTempFileName);

    ErrorMsg("Could not create saving progress window!");
    return;
  }

  // Center the dialog window to frame window
  if (WinQueryWindowPos(hwndSaving, &DlgSwp))
    if (WinQueryWindowPos(hwndFrame, &OwnerSwp))
    {
      // Center dialog box within its owner window
      int ix, iy;
      ix = OwnerSwp.x + (OwnerSwp.cx - DlgSwp.cx)/2;
      iy = OwnerSwp.y + (OwnerSwp.cy - DlgSwp.cy)/2;
      WinSetWindowPos(hwndSaving, HWND_TOP, ix, iy, 0, 0,
                      SWP_MOVE | SWP_SHOW | SWP_ACTIVATE);
    }


  // Okay, now tell the worker to save the file, and use this
  // progress window!
  pWorkerMsg = (WaWEWorker2Msg_SaveChannelSet_p) dbg_malloc(sizeof(WaWEWorker2Msg_SaveChannelSet_t));
  if (pWorkerMsg)
  {
    pWorkerMsg->pChannelSet = pChannelSet;
    pWorkerMsg->pExportPlugin = pExportPlugin;
    pWorkerMsg->hFile = hFile;
    pWorkerMsg->hwndSavingDlg = hwndSaving;
    strcpy(pWorkerMsg->achTempFileName, achTempFileName);
    strcpy(pWorkerMsg->achDestFileName, pchFileName);
    if (!AddNewWorker2Msg(WORKER2_MSG_SAVECHANNELSET, sizeof(WaWEWorker2Msg_SaveChannelSet_t), pWorkerMsg))
    {
      // Error adding the worker message to the queue, the queue might be full?
      dbg_free(pWorkerMsg);
    }
  }

  // Now process the dialog window. The worker thread will
  // close it when it's not needed anymore, signalling that it's done
  // with saving.

  WinProcessDlg(hwndSaving);

  WinDestroyWindow(hwndSaving);
}

MRESULT MenuSelected_SaveAs(HWND hwnd)
{
  WaWEChannelSet_p pChannelSet = pActiveChannelSet;
  WaWEExP_Create_Desc_t FormatDesc;

  // Cannot save if there is no channel-set...
  if (!pChannelSet)
  {
    ErrorMsg("Hey! You should have a channel-set before saving it! :)");
  } else
  {
    FILEDLG fd = { 0 };
    WaWEPluginList_p pExportPluginList;

    fd.cbSize = sizeof( fd );
    // It's a centered 'Save As'-dialog
    fd.fl = FDS_CUSTOM | FDS_SAVEAS_DIALOG | FDS_ENABLEFILELB | FDS_CENTER;
    fd.pfnDlgProc = CustomSaveAsDialogProc;
    fd.usDlgId = DLG_OPENIMPORTFILE;
    fd.hMod = 0; // Read from exe
    fd.pszTitle = "Save/Export Channel-Set As...";
  
    pExportPluginList = CreatePluginList(WAWE_PLUGIN_TYPE_EXPORT);
    fd.ulUser = (ULONG) pExportPluginList;

    // Initial path, file name, or file filter
    if (!strcmp(pChannelSet->achFileName, ""))
      snprintf(fd.szFullFile, sizeof(fd.szFullFile), "%s*.*", WaWEConfiguration.achLastOpenFolder);
    else
      strcpy(fd.szFullFile, pChannelSet->achFileName);
  
    if(WinFileDlg( HWND_DESKTOP, hwnd, &fd ) && (fd.lReturn == DID_OK) )
    {
      // Ok, user selected a filename and an export plugin (or null).
      WaWEPlugin_p pExportPlugin;

      // Store the folder of the file first in WaWEConfiguration.achLastOpenFolder!
      GetFilePath(WaWEConfiguration.achLastOpenFolder, sizeof(WaWEConfiguration.achLastOpenFolder), fd.szFullFile);
#ifdef DEBUG_BUILD
      printf("[MenuSelected_SaveAs] : Saved last open folder as [%s]\n", WaWEConfiguration.achLastOpenFolder);
#endif

      // Get selected export plugin
      pExportPlugin = (WaWEPlugin_p) (fd.ulUser);

      // Fill format desc structure
      WaWEHelper_ChannelSet_StartChSetFormatUsage(pChannelSet);
      FormatDesc.ChannelSetFormat = pChannelSet->ChannelSetFormat;
      WaWEHelper_ChannelSet_StopChSetFormatUsage(pChannelSet, FALSE);
      FormatDesc.iFrequency = pChannelSet->OriginalFormat.iFrequency;
      FormatDesc.iBits = pChannelSet->OriginalFormat.iBits;
      FormatDesc.iIsSigned = pChannelSet->OriginalFormat.iIsSigned;
      FormatDesc.iChannels = pChannelSet->iNumOfChannels;

      // If there is no export plugin, let's try to find one automatically!
      if (!pExportPlugin)
      {
        pExportPlugin = GetExportPluginForFormat(&FormatDesc);
        if (!pExportPlugin)
        {
          ErrorMsg("Could not find an export plugin which can save in the required format!\n");
        }
      }

      if (pExportPlugin)
      {
#ifdef DEBUG_BUILD
        printf("[MenuSelected_SaveAs] : Save file [%s] with plugin %p\n", fd.szFullFile, pExportPlugin);
#endif
        MenuSelected_Save_core(hwnd, pChannelSet, fd.szFullFile, pExportPlugin, &FormatDesc);
      }
    }
    // That was all, dbg_free the plugin list we used here...
    FreePluginList(pExportPluginList);
  }
  return (MRESULT) 0;
}

MRESULT MenuSelected_Save(HWND hwnd)
{
  WaWEChannelSet_p pChannelSet = pActiveChannelSet;
  WaWEExP_Create_Desc_t FormatDesc;
  WaWEPlugin_p pExportPlugin;

  // Cannot save if there is no channel-set...
  if (!pChannelSet)
  {
    ErrorMsg("Hey! You should have a channel-set before saving it! :)");
  } else
  {
    // Okay, there is a channel-set, now let's see if its format is known or not!
    if (!(pChannelSet->ChannelSetFormat))
      // Unknown format, so we should actually be a Save As dialog, not a Save dialog!
      return MenuSelected_SaveAs(hwnd);

    // Known format, so we should find a plugin which knows this format.
    WaWEHelper_ChannelSet_StartChSetFormatUsage(pChannelSet);
    FormatDesc.ChannelSetFormat = pChannelSet->ChannelSetFormat;
    WaWEHelper_ChannelSet_StopChSetFormatUsage(pChannelSet, FALSE);
    FormatDesc.iFrequency = pChannelSet->OriginalFormat.iFrequency;
    FormatDesc.iBits = pChannelSet->OriginalFormat.iBits;
    FormatDesc.iIsSigned = pChannelSet->OriginalFormat.iIsSigned;
    FormatDesc.iChannels = pChannelSet->iNumOfChannels;

    pExportPlugin = GetExportPluginForFormat(&FormatDesc);

    // If we find it, then we can save using that plugin, and using the original
    // filename. If we don't find it, then we fall back to the Save As dialog, again.
    if (!pExportPlugin)
      return MenuSelected_SaveAs(hwnd);

    // Ok, we have the filename and the export plugin to use.
    MenuSelected_Save_core(hwnd, pChannelSet, pChannelSet->achFileName, pExportPlugin, &FormatDesc);
    
  }
  return (MRESULT) FALSE;
}

// This is needed for bad plugins which do a lot :)
#define USE_WORKER_FOR_POPUPMENU

MRESULT EXPENTRY MainWindowProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  switch (msg)
  {
    case WM_REDRAWCHANGEDCHANNELSANDCHANNELSETS:
      // Redraw changed channels and channel-sets!
      RedrawChangedChannelsAndChannelSets_PMThreadWorker();
      return (MRESULT) TRUE;
    case WM_CREATECSWINDOW:
      {
        // Create a ChannelSet window

	// We have to create every window from the PM-Thread, so we need
        // only one WinGetMsg() loop...
	if (mp1)
	{
	  WaWEChannelSet_p pChannelSet;
          pChannelSet = (WaWEChannelSet_p) mp1;
          CreateNewChannelSetWindow(pChannelSet);
	}
        return (MRESULT) TRUE;
      }
    case WM_CREATECWINDOW:
      {
        // Create a Channel window

	// We have to create every window from the PM-Thread, so we need
        // only one WinGetMsg() loop...
	if (mp1)
	{
	  WaWEChannel_p pChannel;
          int           iEditAreaHeight;
	  pChannel = (WaWEChannel_p) mp1;
          iEditAreaHeight = (int) mp2;
          CreateNewChannelWindow(pChannel, iEditAreaHeight);
	}
        return (MRESULT) TRUE;
      }
    case WM_DESTROYWINDOW:
      return (MRESULT) WinDestroyWindow((HWND) mp1);
    case WM_LOADPLUGINS:
      {
        HWND dlg;
        SWP DlgSwap, ParentSwap;

        // Load plugins, and create an info window for it!
        dlg = WinLoadDlg(HWND_DESKTOP, hwnd,
                         LoadingPluginsDlgProc, NULLHANDLE,
                         DLG_LOADINGPLUGINS, NULL);
        if (dlg)
        {
          if (WinQueryWindowPos(dlg, &DlgSwap))
            if (WinQueryWindowPos(hwndFrame, &ParentSwap))
            {
              // Center dialog box within the parent window
              int ix, iy;
              ix = ParentSwap.x + (ParentSwap.cx - DlgSwap.cx)/2;
              iy = ParentSwap.y + (ParentSwap.cy - DlgSwap.cy)/2;
              WinSetWindowPos(dlg, HWND_TOP, ix, iy, 0, 0,
                              SWP_MOVE | SWP_SHOW | SWP_ACTIVATE);
            }
          // Spawn a thread which will load the plugins!
          _beginthread( LoadPluginsFunc, 0, 1024*1024, (void *) dlg);
          WinProcessDlg(dlg);
          WinDestroyWindow(dlg);
          return (MRESULT) TRUE;
        }
      }
      break;
    case WM_UPDATEWINDOWTITLE:
      switch (iWorker1State)
      {
	case WORKER1STATE_REDRAWING:
	  SetMainWindowTitleExtension(" - Redrawing");
	  break;
	default:
	  SetMainWindowTitleExtension(NULL);
          break;
      }
      return (MRESULT) TRUE;
    case WM_VSCROLL:
      {
        if ((USHORT)mp1 == FID_VERTSCROLL)
        {
          // This is a message from out vertical scrollbar!
          // Let's see what command it is!
          switch (SHORT2FROMMP(mp2))
          {
            case SB_LINEUP:
              {
                int iPos;
//                printf("SB_LINEUP of scrollbar at %d\n",
//                       SHORT1FROMMP(mp2));
                iPos = (int) WinSendDlgItemMsg(hwndFrame, FID_VERTSCROLL,
                                               SBM_QUERYPOS, (MPARAM) 0, (MPARAM) 0);
                iPos-=5; if (iPos<0) iPos = 0;
                WinSendDlgItemMsg(hwndFrame, FID_VERTSCROLL,
                                  SBM_SETPOS, (MPARAM) iPos, (MPARAM) 0);
                // Now move windows according to the new scrollbar position!
                UpdateChannelSetsByMainScrollbar();
                break;
              }
            case SB_LINEDOWN:
              {
                int iPos;
//                printf("SB_LINEDOWN of scrollbar at %d\n",
//                       SHORT1FROMMP(mp2));
                iPos = (int) WinSendDlgItemMsg(hwndFrame, FID_VERTSCROLL,
                                               SBM_QUERYPOS, (MPARAM) 0, (MPARAM) 0);
                iPos+=5;
                WinSendDlgItemMsg(hwndFrame, FID_VERTSCROLL,
                                  SBM_SETPOS, (MPARAM) iPos, (MPARAM) 0);
                // Now move windows according to the new scrollbar position!
                UpdateChannelSetsByMainScrollbar();
                break;
              }
            case SB_PAGEUP:
              {
                int iPos;
//                printf("SB_PAGEUP of scrollbar at %d\n",
//                       SHORT1FROMMP(mp2));
                iPos = (int) WinSendDlgItemMsg(hwndFrame, FID_VERTSCROLL,
                                               SBM_QUERYPOS, (MPARAM) 0, (MPARAM) 0);
                iPos-=iChannelEditAreaHeight; if (iPos<0) iPos = 0;
                WinSendDlgItemMsg(hwndFrame, FID_VERTSCROLL,
                                  SBM_SETPOS, (MPARAM) iPos, (MPARAM) 0);
                // Now move windows according to the new scrollbar position!
                UpdateChannelSetsByMainScrollbar();
                break;
              }
            case SB_PAGEDOWN:
              {
                int iPos;
//                printf("SB_PAGEDOWN of scrollbar at %d\n",
//                       SHORT1FROMMP(mp2));
                iPos = (int) WinSendDlgItemMsg(hwndFrame, FID_VERTSCROLL,
                                               SBM_QUERYPOS, (MPARAM) 0, (MPARAM) 0);
                iPos+=iChannelEditAreaHeight;
                WinSendDlgItemMsg(hwndFrame, FID_VERTSCROLL,
                                  SBM_SETPOS, (MPARAM) iPos, (MPARAM) 0);
                // Now move windows according to the new scrollbar position!
                UpdateChannelSetsByMainScrollbar();
                break;
              }
            case SB_SLIDERPOSITION:
            case SB_ENDSCROLL:
              {
//                printf("SB_ENDSCROLL or SB_SLIDERPOSITION of scrollbar at %d\n",
//                       SHORT1FROMMP(mp2));
                // Now move windows according to the new scrollbar position!
                UpdateChannelSetsByMainScrollbar();
                break;
              }
            case SB_SLIDERTRACK:
              {
//                printf("SB_SLIDERTRACK of scrollbar at %d\n",
//                       SHORT1FROMMP(mp2));
                // The slidertrack does not change scrollbar position, so
                // we do it, so the UpdateChannelSetsByMainScrollbar() will
                // get the new position!
                WinSendDlgItemMsg(hwndFrame, FID_VERTSCROLL,
                                  SBM_SETPOS,
                                  (MPARAM) SHORT1FROMMP(mp2),
                                  (MPARAM) 0);
                // Now move windows according to the new scrollbar position!
                UpdateChannelSetsByMainScrollbar();
                break;
              }
            default:
//              printf("  WM_SCROLL message of %d\n", SHORT2FROMMP(mp2));
              break;

          }
          return (MRESULT) TRUE;
        }
        break;
      }
    case WM_COMMAND:
      {
	switch (SHORT1FROMMP(mp2))
	{
	  case CMDSRC_ACCELERATOR:
	  case CMDSRC_MENU:
            {
              if ((SHORT1FROMMP(mp1)>=ABOUTMENU_ID_BASE) &&
                  (SHORT1FROMMP(mp1)<CONFIGUREMENU_ID_BASE))
              {
                // It's an ABOUT menu item!
                WaWEPlugin_p pPlugin;
                pPlugin = GetPluginByIndex(SHORT1FROMMP(mp1)-ABOUTMENU_ID_BASE);
                if (pPlugin)
                {
                  if (pPlugin->fnAboutPlugin)
                    pPlugin->fnAboutPlugin(hwnd);
                }
              } else
              if ((SHORT1FROMMP(mp1)>=CONFIGUREMENU_ID_BASE) &&
                  (SHORT1FROMMP(mp1)<CONFIGUREMENU_ID_BASE+1000))
              {
                // It's a CONFIGURE menu item!
                WaWEPlugin_p pPlugin;
                pPlugin = GetPluginByIndex(SHORT1FROMMP(mp1)-CONFIGUREMENU_ID_BASE);
                if (pPlugin)
                {
                  if (pPlugin->fnConfigurePlugin)
                    pPlugin->fnConfigurePlugin(hwnd);
                }
	      } else
              if ((SHORT1FROMMP(mp1)>=DID_POPUPMENU_FIRST) &&
                  (SHORT1FROMMP(mp1)<=DID_POPUPMENU_LAST))
	      {
		// A popup-menu-item has been selected
		WaWEPlugin_p pPlugin;
		int          iItemID;
		WaWEChannelSet_p pCurrChannelSet;
		WaWEChannel_p    pCurrChannel;
	
#ifdef DEBUG_BUILD
		printf("[MainWindowProc] : Selected popup menu entry id %d\n", SHORT1FROMMP(mp1)); fflush(stdout);
#endif
		if (GetPopupMenuItem(SHORT1FROMMP(mp1),
				     &pPlugin, &iItemID,
				     &pCurrChannelSet, &pCurrChannel))
		{
		  // Yes, it's ours!
		  if (pPlugin)
                  {
#ifdef USE_WORKER_FOR_POPUPMENU
                    // It's an entry of a plugin
                    WaWEWorker2Msg_DoPopupMenuAction_p pWorkerMsg;

                    pWorkerMsg = (WaWEWorker2Msg_DoPopupMenuAction_p) dbg_malloc(sizeof(WaWEWorker2Msg_DoPopupMenuAction_t));
                    if (pWorkerMsg)
                    {
                      pWorkerMsg->WaWEState.pCurrentChannelSet = pCurrChannelSet;
		      pWorkerMsg->WaWEState.pCurrentChannel = pCurrChannel;
                      pWorkerMsg->WaWEState.pChannelSetListHead = ChannelSetListHead;
                      pWorkerMsg->pPlugin = pPlugin;
                      pWorkerMsg->iItemID = iItemID;
                      if (!AddNewWorker2Msg(WORKER2_MSG_DOPOPUPMENUACTION, sizeof(WaWEWorker2Msg_DoPopupMenuAction_t), pWorkerMsg))
                      {
                        // Error adding the worker message to the queue, the queue might be full?
                        dbg_free(pWorkerMsg);
                      }
                    }
                    return (MRESULT) 1;
#else
                    WaWEState_t WaWEState;
                    WaWEState.pCurrentChannelSet = pCurrChannelSet;
		    WaWEState.pCurrentChannel = pCurrChannel;
                    WaWEState.pChannelSetListHead = ChannelSetListHead;

                    pPlugin->TypeSpecificInfo.ProcessorPluginInfo.fnDoPopupMenuAction(iItemID,
                                                                                        &WaWEState,
                                                                                        hwndFrame);
                    // Check all the channels and channel-sets, and redraw what changed!
                    RedrawChangedChannelsAndChannelSets();
#endif
		  } else
		  {
		    // It's a base entry, must be handled by WaWE itself!
		    switch (iItemID)
		    {
		      case DID_POPUPMENU_NEWCHANNEL:
			// Emulate the New->Channel menuitem from main menu!
                        MenuSelected_NewChannel(hwnd);
			break;
		      case DID_POPUPMENU_NEWCHANNELSET:
			// Emulate the New->ChannelSet menuitem from main menu!
                        MenuSelected_NewChannelSet(hwnd);
			break;
		      case DID_POPUPMENU_DELETECHANNEL:
                        MenuSelected_DeleteChannel(hwnd, pCurrChannelSet, pCurrChannel);
			break;
		      case DID_POPUPMENU_DELETECHANNELSET:
                        MenuSelected_DeleteChannelSet(hwnd, pCurrChannelSet, pCurrChannel);
			break;

		      default:
#ifdef DEBUG_BUILD
			printf("[MainWindowProc] : Unknown base popupmenu entry id (%d)!\n", iItemID);
#endif
			break;
		    }
		    return (MRESULT) 1;
		  }
		}
		return (MRESULT) 0;
	      } else
                // It's something else
	      switch (SHORT1FROMMP(mp1))
              {
		case IDM_GETWINDOWCONTEXTMENU: /* simulate MB2 with keyboard      */
		  {                            /* maybe it's a hack, but it works */
		    POINTL ptl;                /* sugestions to fbakan@gmx.net    */
		    HWND   hwndFound;
		    WinQueryPointerPos(HWND_DESKTOP, &ptl);
		    hwndFound = WinWindowFromPoint(HWND_DESKTOP,
						 		      &ptl,
								      TRUE);
		    return WinSendMsg(hwndFound,
					       WM_BUTTON2CLICK,
					       MPVOID,
					       MPVOID);
		  }
		case IDM_OPEN:
		  {
		    return MenuSelected_Open(hwnd);
		  }
		case IDM_NEWCHANNELSET:
		  {
                    return MenuSelected_NewChannelSet(hwnd);
		  }
		case IDM_NEWCHANNEL:
		  {
                    return MenuSelected_NewChannel(hwnd);
		  }
		case IDM_PRINTCHANNELSET:
		  {
#ifdef DEBUG_BUILD
		    printf("[MainWindowProc] : Print ChannelSet!\n");
#endif
                    ErrorMsg("This part of WaWE is not implemented yet!");
                    return (MRESULT) 0;
		  }
		case IDM_PRINTCHANNEL:
		  {
#ifdef DEBUG_BUILD
		    printf("[MainWindowProc] : Print Channel!\n");
#endif
                    ErrorMsg("This part of WaWE is not implemented yet!");
                    return (MRESULT) 0;
		  }
		case IDM_SAVE:
		  {
#ifdef DEBUG_BUILD
		    printf("[MainWindowProc] : Save Channel-Set!\n");
#endif
                    return MenuSelected_Save(hwnd);
                  }
		case IDM_SAVEAS:
		  {
#ifdef DEBUG_BUILD
		    printf("[MainWindowProc] : Save Channel-Set As...!\n");
#endif
                    return MenuSelected_SaveAs(hwnd);
                  }
		case IDM_ABOUTWAWE:
		  {
		    HWND dlg;
		    SWP DlgSwap, ParentSwap;

		    dlg = WinLoadDlg(HWND_DESKTOP, hwndFrame,
				     AboutDialogProc, NULLHANDLE,
				     DLG_ABOUT, NULL);
		    if (dlg)
		    {
		      if (WinQueryWindowPos(dlg, &DlgSwap))
			if (WinQueryWindowPos(hwndFrame, &ParentSwap))
			{
			  // Center dialog box within then parent window
			  int ix, iy;
			  ix = ParentSwap.x + (ParentSwap.cx - DlgSwap.cx)/2;
			  iy = ParentSwap.y + (ParentSwap.cy - DlgSwap.cy)/2;
			  WinSetWindowPos(dlg, HWND_TOP, ix, iy, 0, 0,
					  SWP_MOVE | SWP_SHOW | SWP_ACTIVATE);
			}
		      WinProcessDlg(dlg);
		      WinDestroyWindow(dlg);
		      return (MRESULT) TRUE;
                      }
                    break;
		  }
                case IDM_CONFIGUREWAWE:
                  DoWaWEConfigureWindow(hwnd);
                  return (MRESULT) TRUE;
                case IDM_CLOSE:
		  {
		    WinPostMsg(hwndFrame, WM_CLOSE, 0, 0);
		    return (MRESULT) TRUE;
		  }
	      } // end of processing switch of menu commands
	    } // end of CASE CMDSRC_MENU
	} // end of switch
      } // End of processing WM_COMMAND
      break;
    case WM_SIZE: // The main window has been resized!
      {
        // Window resized.
        // The size and position of client window and the scrollbar is updated
        // automatically, all we have to do is to update the scrollbar's values,
        // and the position and width of the ChannelSet windows.
        // The changing of width of ChannelSet window can result in redrawing
        // the whole waveform, but it's done by the worker thread anyway, so resizing
        // does not take too much time, do it here!
        UpdateChannelSetWindowSizes();
        break;
      }
    case WM_PAINT:
      {
	HPS hpsBeginPaint;
        RECTL rclRect;
      
        hpsBeginPaint = WinBeginPaint(hwnd, NULL, &rclRect);
	// Redraw the background!
	WinFillRect(hpsBeginPaint, &rclRect, SYSCLR_DIALOGBACKGROUND);
        // All done!
	WinEndPaint(hpsBeginPaint);
	return (MRESULT) FALSE;
      }
    case WM_BUTTON2CLICK:
      {
#ifdef DEBUG_BUILD
        printf("[MainWindowProc] : WM_BUTTON2CLICK!\n");
        fflush(stdout);
#endif
        CreatePopupMenu(NULL, NULL);
        return (MRESULT) 1;
      }
    case WM_CHAR:
      return ProcessWMCHARMessage(mp1, mp2);
    case WM_CLOSE:
      {
        int iNumOfModifiedChannelSets;
	char achMessage[512];

	iNumOfModifiedChannelSets = GetNumOfModifiedChannelSets();
	if (iNumOfModifiedChannelSets>0)
	{
          if (iNumOfModifiedChannelSets==1)
	    sprintf(achMessage, "There is one modified channel-set. Are you sure you want to quit WaWE without saving it?");
	  else
	    sprintf(achMessage, "There are %d modified channel-sets. Are you sure you want to quit WaWE without saving them?",
		    iNumOfModifiedChannelSets);

	  if (WinMessageBox(HWND_DESKTOP, hwnd,
			achMessage,
                            "Closing WaWE", 0, MB_YESNO | MB_MOVEABLE | MB_ICONQUESTION)==MBID_YES)

            WinPostMsg(hwnd, WM_QUIT, 0L, 0L);
        } else
          WinPostMsg(hwnd, WM_QUIT, 0L, 0L);
      }
      return 0L;

    case WM_SAVEAPPLICATION:
      WinStoreWindowPos(WinStateSave_AppName, WinStateSave_MainWindowKey, hwndFrame);
      break;
    case WM_QUIT:
#ifdef DEBUG_BUILD
      printf("[MainWindowProc] : WM_QUIT!\n");
#endif
      // Call Uninitialize for the plugins, before this window is destroyed!
      // But this never seems to be called...
      UninitializeAllPlugins(hwnd);
      break;
  }
  return WinDefWindowProc(hwnd, msg, mp1, mp2);
}

MRESULT EXPENTRY NewFrameProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  PFNWP pOldFrameProc;
  MRESULT result;
  PTRACKINFO ti;

  pOldFrameProc = (PFNWP) WinQueryWindowULong(hwnd, QWL_USER);

  result = (*pOldFrameProc)(hwnd, msg, mp1, mp2);

  if (msg == WM_QUERYTRACKINFO)
  {
    ti = (PTRACKINFO) mp2;
    ti->ptlMinTrackSize.x = 300;
    ti->ptlMinTrackSize.y = 200;
  }
  return result;
}

void PMThreadFunc(void *p)
{
  HAB hab;
  HMQ hmq;
  QMSG msg;
  ULONG flFrameStyle, flClientStyle, flCreaFlags;
  SWP swap;
  PFNWP pOldFrameProc;

// accel table
  HACCEL haccelAccel;
  BOOL rc;

#ifdef DEBUG_BUILD
  printf("[PMThread] : Starting\n");
#endif
  // Setup this thread for using PM (sending messages etc...)
  hab = WinInitialize(0);
  hmq = WinCreateMsgQueue(hab, 0);

  // Create main window
  WinRegisterClass(hab, pchPrivateClassName, // Register a private class, so
		   MainWindowProc,           // we can use our own window procedure for frame
		   0, 0);

  // Set defaults for main window:
  flCreaFlags = 
    FCF_TASKLIST | FCF_SYSMENU | FCF_TITLEBAR | FCF_SIZEBORDER |
    FCF_DLGBORDER | FCF_MINMAX | FCF_MENU | FCF_ICON | FCF_VERTSCROLL |
    FCF_SHELLPOSITION | FCF_NOBYTEALIGN | FCF_AUTOICON | FCF_ACCELTABLE;
  flFrameStyle =
    FS_NOBYTEALIGN | FS_BORDER;
  flClientStyle =
      WS_CLIPCHILDREN;

  hwndFrame = WinCreateStdWindow(HWND_DESKTOP,
				 flFrameStyle,       // Frame window style
				 &flCreaFlags,
                                 pchPrivateClassName,
                                 pchMainWindowTitle,
				 flClientStyle,//0,   // Client window style
				 NULLHANDLE,
				 WIN_WARPEDWAVEEDITOR,
                                 &hwndClient);

  pOldFrameProc = WinSubclassWindow(hwndFrame, NewFrameProc); // Subclass window to be able
                                                              // to handle resize events
  // Store old window proc in frame window's ULONG
  WinSetWindowULong(hwndFrame, QWL_USER, (ULONG) pOldFrameProc);

  // Now try to reaload size and position information from INI file
  WinRestoreWindowPos(WinStateSave_AppName, WinStateSave_MainWindowKey, hwndFrame);

  // Move the window more down if it would be out of the screen
  WinQueryWindowPos(hwndFrame, &swap);
  if (swap.y+swap.cy > WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN))
  {
    WinSetWindowPos(hwndFrame, HWND_TOP, swap.x,
		    WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN) - swap.cy, 0, 0, SWP_MOVE);
    WinQueryWindowPos(hwndFrame, &swap);
  }
  if (swap.x+swap.cx > WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN))
  {
    if (WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN) - swap.cx >= 0)
      WinSetWindowPos(hwndFrame, HWND_TOP, 
		      WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN) - swap.cx, swap.y,  0, 0, SWP_MOVE);
  }


// accel table

haccelAccel = WinLoadAccelTable(hab, NULLHANDLE, WIN_WARPEDWAVEEDITOR);
#ifdef DEBUG_BUILD
 printf("[PMThread] : load acceltable error=%lu\n", WinGetLastError(hab));
#endif
rc = WinSetAccelTable(hab, haccelAccel, hwndFrame);

#ifdef DEBUG_BUILD
 printf("[PMThread] : set acceltable\n");
 if (rc == FALSE)  printf("[PMThread] : set accel failed\n");
#endif

  WinShowWindow(hwndFrame, TRUE);

  GUI_Ready = TRUE;

#ifdef DEBUG_BUILD
  printf("[PMThread] : Reached WinProcessDlg\n");
#endif
  while (WinGetMsg(hab, &msg, 0, 0, 0))
    WinDispatchMsg(hab, &msg);
#ifdef DEBUG_BUILD
  printf("[PMThread] : Shutting down!\n");
#endif
  ShuttingDown = TRUE;

  GUI_Ready = FALSE;

  WinDestroyWindow(hwndFrame); // Destroy windows

  // Uninitialize PM
  WinDestroyMsgQueue(hmq);
  WinTerminate(hab);

  tidPM = -1;
#ifdef DEBUG_BUILD
  printf("[PMThread] : Stopped\n");
#endif

  _endthread();
}

int Initialize_Variables()
{
  // Initialize global variables

  tidPM = -1; // Indicate that PMThread is not running
  GUI_Ready = FALSE;

  ShuttingDown = FALSE;

  return TRUE;
}

void Cleanup_Variables()
{
  return;
}

void Shutdown_PMThread()
{
  if (tidPM!=-1)
  {
    WinPostMsg(hwndFrame, WM_QUIT, 0, 0);
    DosWaitThread(&tidPM, DCWW_WAIT);
  }
}

// ---------- Main function -------------------------------------------------

int main(int argc, char *argv[])
{
#ifdef DEBUG_BUILD
  MorphToPM();
  // Make stdout and stderr unbuffered
  setbuf( stdout, NULL );
  setbuf( stderr, NULL );
  
  printf("Starting %s version %s\n",
         pchMainWindowTitle,
         pchVer);
  printf("Compilation date: %s %s\n", __DATE__, __TIME__);
#ifdef __WATCOMC__
#if __WATCOMC__<1200
  printf("Compiled with WatcomC v%d.%02d\n", (__WATCOMC__)/100, (__WATCOMC__)%100);
#else
  printf("Compiled with OpenWatcom v%d.%02d\n", (__WATCOMC__-1100)/100, (__WATCOMC__-1100)%100);
#endif
#endif
#endif

  // Initialize memory leak detection
  Initialize_MemoryUsageDebug();

#ifdef DEBUG_BUILD
  printf("Loading configuration...\n");
#endif
  LoadWaWEConfig();

  // Setup this thread for using PM (sending messages etc...)
  hab = WinInitialize(0);
  hmq = WinCreateMsgQueue(hab, 0);

  if (!Initialize_Variables())
  {
    ErrorMsg("Error while initializing variables!");
  } else
  {

#ifdef DEBUG_BUILD
    printf("Initializing modules!\n");
#endif
    if (!Initialize_EventCallbackList())
    {
      ErrorMsg("Warning!\nCould not initialize Event Callback list!");
    }

    if (!Initialize_FrameRegulation())
    {
      ErrorMsg("Warning!\nCould not initialize Frame Regulation library!");
    }

    if (!Initialize_PluginList())
    {
      ErrorMsg("Warning!\nCould not initialize Plugin list!");
    }
    if (!Initialize_ChannelSetList())
    {
      ErrorMsg("Warning!\nCould not initialize list of Channel-sets!");
    }
    if (!Initialize_Screen(hab))
    {
      ErrorMsg("Warning!\nCould not initialize Screen module!");
    }
    if (!Initialize_Worker1())
    {
      ErrorMsg("Warning!\nCould not initialize Worker-1 thread!");
    }
    if (!Initialize_Worker2())
    {
      ErrorMsg("Warning!\nCould not initialize Worker-2 thread!");
    }
    if (!Initialize_PopupMenu())
    {
      ErrorMsg("Warning!\nCould not initialize Popup Menu module!");
    }

    // Start PM-Thread:
    //  Build GUI, and process GUI messages, then cleanup PM resources
    tidPM = _beginthread( PMThreadFunc, 0,
                          1024*1024, NULL);
#ifdef DEBUG_BUILD
    printf("PM thread created, tid = %d\n", tidPM);
#endif

    while ((!GUI_Ready) && (tidPM>0) && (!ShuttingDown)) DosSleep(32); // Wait until the GUI is ready

    if (tidPM>0)
    {
      // Raise priority of PM thread, so it will respond quickly for user actions
      DosSetPriority(PRTYS_THREAD, PRTYC_REGULAR, +5, tidPM);
#ifdef DEBUG_BUILD
      printf("Main Initialization done, loading plugins...\n");
#endif

      // Send a command to start loading the plugins!
      WinSendMsg(hwndFrame, WM_LOADPLUGINS, 0, 0);

      if (argc>1)
      {
        int iArg;
#ifdef DEBUG_BUILD
        printf("Processing parameters (%d)...\n", argc);
#endif
        for (iArg = 1; iArg<argc; iArg++)
        {
#ifdef DEBUG_BUILD
          printf("  Trying to open [%s] with automatic plugin selection.\n", argv[iArg]);
#endif
          internal_Open_core(argv[iArg], NULL); // Open with automatic plugin selection
        }
      }


      if ((tidPM!=-1) && (GUI_Ready) && (!ShuttingDown))
        DosWaitThread(&tidPM, DCWW_WAIT); // Wait for shutdown

#ifdef DEBUG_BUILD
      printf("Shutting down\n");
#endif

      Shutdown_PMThread();
    }
  
    // Cleanup global variables
    Cleanup_Variables();
  }

#ifdef DEBUG_BUILD
  printf("Preparing plugins for shutdown.\n");
#endif
  // This should make pending plugin-operations stop, like playback
  // and others...
  PrepareAllPluginsForShutdown();

#ifdef DEBUG_BUILD
  printf("Uninitializing modules!\n");
#endif
  Uninitialize_PopupMenu();
  Uninitialize_Worker2();
  Uninitialize_Worker1();
  Uninitialize_Screen();
  Uninitialize_ChannelSetList();
  Uninitialize_PluginList();
  Uninitialize_FrameRegulation();
  Uninitialize_EventCallbackList();

#ifdef DEBUG_BUILD
  printf("Saving configuration.\n");
#endif
  SaveWaWEConfig();

#ifdef DEBUG_BUILD
  printf("Uninitializing PM!\n");
#endif

  // Uninitialize PM
  WinDestroyMsgQueue(hmq);
  WinTerminate(hab);

#ifdef DEBUG_BUILD
  printf("Maximum amount of memory used (not counting the plugins):\n  %d Bytes = %d KBytes = %d MBytes\n",
        dbg_max_allocated_memory, dbg_max_allocated_memory/1024, dbg_max_allocated_memory/1024/1024);
#endif

  // Check for memory leak:
  dbg_ReportMemoryLeak();
  Uninitialize_MemoryUsageDebug();

#ifdef DEBUG_BUILD
  printf("Everything done! Bye!\n");
#endif

  return 0;
}
