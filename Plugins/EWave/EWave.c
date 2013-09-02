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
 * Wave Form editor plugin                                                   *
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <process.h>
#include <string.h>

#define INCL_TYPES
#define INCL_WIN
#define INCL_GPI
#define INCL_DOSRESOURCES
#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>

#include "WaWECommon.h"    // Common defines and structures

#define USE_LOCAL_CACHE_AT_READING

// Global variables
// (Per process, because the DLL is initinstance/terminstance!)
HMODULE hmodOurDLLHandle;
WaWEPluginHelpers_t PluginHelpers;

char *pchPluginName = "~WaveForm Editor plugin";
char *pchAboutTitle = "About WaveForm Editor plugin";
char *pchAboutMessage =
  "WaveForm Editor plugin v1.0 for WaWE\n"
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
                Msg, "WaveForm Editor Plugin Error", 0, MB_OK | MB_MOVEABLE | MB_ICONEXCLAMATION);
}

#ifdef USE_LOCAL_CACHE_AT_READING
#define LOCAL_CACHE_SIZE  1024

static WaWESample_p pSampleCache = NULL;
static int iSampleCacheSize = 0;
static WaWEPosition_t SampleCacheStartPos = 0;

void SetupSampleCache()
{
  pSampleCache = (WaWESample_p) malloc(LOCAL_CACHE_SIZE * sizeof(WaWESample_t));
  iSampleCacheSize = 0;
  SampleCacheStartPos = 0;
}

void ClearSampleCache()
{
  if (pSampleCache)
  {
    free(pSampleCache);
    pSampleCache = NULL;
  }
}

int ReadOneSample(WaWEEdP_Draw_Desc_p pDrawDesc,
                  WaWEPosition_t Pos,
                  WaWESample_p pSample,
                  WaWESize_p pReadedSamples)
{
  if (!pSampleCache)
  {
    // Eeek, no cache, so use the old method! Sigh.
    return PluginHelpers.fn_WaWEHelper_Channel_ReadSample(pDrawDesc->pChannelDesc,
                                                          Pos,
                                                          1,
                                                          pSample,
                                                          pReadedSamples);

  } else
  {
    if ((SampleCacheStartPos<=Pos) &&
        (SampleCacheStartPos+iSampleCacheSize-1>=Pos) &&
        (iSampleCacheSize>0))
    {
      *pReadedSamples = 1;
      *pSample = pSampleCache[Pos-SampleCacheStartPos];
      return WAWEHELPER_NOERROR;
    } else
    {
      // Hm, it's not in cache!
      int rc;

      // Let's see if it's in limits or not!
      // If position if out of channel, then don't even try
      // to decode anything!
      if (Pos>=pDrawDesc->pChannelDesc->Length)
      {
        *pReadedSamples = 0;
        *pSample = 0;
        return WAWEHELPER_ERROR_NODATA;
      }

#ifdef DEBUG_BUILD
//      printf("*** Not in cache: %d\n", (int) Pos);
#endif
      rc = PluginHelpers.fn_WaWEHelper_Channel_ReadSample(pDrawDesc->pChannelDesc,
                                                          Pos,
                                                          LOCAL_CACHE_SIZE,
                                                          pSampleCache,
                                                          pReadedSamples);
      iSampleCacheSize = *pReadedSamples;
      SampleCacheStartPos = Pos;

#ifdef DEBUG_BUILD
//      printf("*** Cached %d from %d\n", iSampleCacheSize, (int) Pos);
#endif

      if ((SampleCacheStartPos<=Pos) &&
          (SampleCacheStartPos+iSampleCacheSize-1>=Pos) &&
          (iSampleCacheSize>0))
      {
        *pReadedSamples = 1;
        *pSample = pSampleCache[Pos-SampleCacheStartPos];
        return WAWEHELPER_NOERROR;
      } else
        return WAWEHELPER_ERROR_INTERNALERROR;
    }
  }
}

#endif


// Functions called from outside:

void WAWECALL plugin_Draw(WaWEEdP_Draw_Desc_p pDrawDesc)
{
  HRGN hrgn, hrgnOld, hrgnTemp;
  RECTL rectl;
  long x;
  long height, middley, width, drawwidth;
  POINTL ptl;
  SIZEL ImageBufferSize;
  WaWESample_t Sample;
  WaWEPosition_t PosDiff;
  WaWEPosition_t Pos, PrevPosinmsec;
  WaWESize_t     ReadedSamples;
  WaWESize_t     ChannelSetLength;
  int            rc;
  signed __int64 BigSample;
  float          fPixelPerOneSec;
  float          fPixelPerOneMin;
  long long      llSampleRate;
  int            iPrevHadError;
  int            iVerticalLinesLevel;
#define VERTLINE_ATNEVER               0
#define VERTLINE_ATMINS                1
#define VERTLINE_ATHALFMINS            2
#define VERTLINE_ATSECS                3
#define VERTLINE_ATHALFSECS            4
#define VERTLINE_ATONETENTHSECS        5
#define VERTLINE_ATONEHUNDREDTHSECS    6

  if (!pDrawDesc) return;
//  printf("[plugin_Draw] : Enter\n"); fflush(stdout);

  GpiQueryPS(pDrawDesc->hpsImageBuffer, &ImageBufferSize);
  height = ImageBufferSize.cy;
  width = ImageBufferSize.cx;

  drawwidth = pDrawDesc->iDrawAreaRightX-pDrawDesc->iDrawAreaLeftX+1;
  middley = height/2;

  ChannelSetLength = pDrawDesc->pChannelDesc->pParentChannelSet->Length;
  llSampleRate = pDrawDesc->pChannelDesc->VirtualFormat.iFrequency;
  PosDiff = pDrawDesc->LastSamplePos - pDrawDesc->FirstSamplePos +1;

  fPixelPerOneSec = (float) (width * llSampleRate) / (float) PosDiff;
  fPixelPerOneMin = fPixelPerOneSec * 60.0;

  // Make sure we won't draw outside the draw area!
  // (Create a clip region)
  rectl.xLeft = pDrawDesc->iDrawAreaLeftX;
  rectl.xRight = pDrawDesc->iDrawAreaRightX+1;
  rectl.yBottom = 0;
  rectl.yTop = height;
  hrgn = GpiCreateRegion(pDrawDesc->hpsImageBuffer, 1, &rectl);
  GpiSetClipRegion(pDrawDesc->hpsImageBuffer, hrgn, &hrgnOld);

  //
  // --- Draw background in black ---
  //
  ptl.x = pDrawDesc->iDrawAreaLeftX;
  ptl.y = 0;
  GpiMove(pDrawDesc->hpsImageBuffer, &ptl);
  // Color is in RGB
  GpiSetColor(pDrawDesc->hpsImageBuffer, RGB_BLACK);
  ptl.x = pDrawDesc->iDrawAreaRightX;
  ptl.y = height-1;
  GpiBox(pDrawDesc->hpsImageBuffer, DRO_FILL, &ptl, 0, 0);

  //
  // --- Draw gray middle line and line at 1/4 and 3/4!
  //
  GpiSetLineType(pDrawDesc->hpsImageBuffer, LINETYPE_DEFAULT);
  // Color is in RGB
  GpiSetColor(pDrawDesc->hpsImageBuffer, 0x00585858); // Darker gray
  // Middle line
  ptl.x = pDrawDesc->iDrawAreaLeftX;
  ptl.y = height/2;
  GpiMove(pDrawDesc->hpsImageBuffer, &ptl);
  ptl.x = pDrawDesc->iDrawAreaRightX;
  ptl.y = height/2;
  GpiLine(pDrawDesc->hpsImageBuffer, &ptl);
  // Upper line
  ptl.x = pDrawDesc->iDrawAreaLeftX;
  ptl.y = height*3/4;
  GpiMove(pDrawDesc->hpsImageBuffer, &ptl);
  ptl.x = pDrawDesc->iDrawAreaRightX;
  ptl.y = height*3/4;
  GpiLine(pDrawDesc->hpsImageBuffer, &ptl);
  // Bottom line
  ptl.x = pDrawDesc->iDrawAreaLeftX;
  ptl.y = height/4;
  GpiMove(pDrawDesc->hpsImageBuffer, &ptl);
  ptl.x = pDrawDesc->iDrawAreaRightX;
  ptl.y = height/4;
  GpiLine(pDrawDesc->hpsImageBuffer, &ptl);

  //
  // --- Draw gray lines at every min, every sec, or half a sec, based on the zoom!
  //
  iVerticalLinesLevel = VERTLINE_ATNEVER;
  if (fPixelPerOneMin<200)
  {
    // Draw lines per minute
    iVerticalLinesLevel = VERTLINE_ATMINS;
  } else
  if (fPixelPerOneMin<400)
  {
    // Draw lines per half a minute
    iVerticalLinesLevel = VERTLINE_ATHALFMINS;
  } else
  {
    if (fPixelPerOneSec<200)
    {
      iVerticalLinesLevel = VERTLINE_ATSECS;
    } else
    if (fPixelPerOneSec<800)
    {
      iVerticalLinesLevel = VERTLINE_ATHALFSECS;
    } else
    if (fPixelPerOneSec<4000)
    {
      iVerticalLinesLevel = VERTLINE_ATONETENTHSECS;
    } else
    {
      iVerticalLinesLevel = VERTLINE_ATONEHUNDREDTHSECS;
    }
  }
#ifdef DEBUG_BUILD
//  printf("[Draw] : fPixelPerOneSec: %d  fPixelPerOneMin: %d  iLinesLevel: %d\n",
//         (int) fPixelPerOneSec, (int) fPixelPerOneMin, iVerticalLinesLevel);
#endif
  // Now draw vertical lines at given markpoints
  // Color is in RGB
  GpiSetColor(pDrawDesc->hpsImageBuffer, 0x00808080); // not so dark gray

  PrevPosinmsec = 
      (pDrawDesc->FirstSamplePos + PosDiff*(pDrawDesc->iDrawAreaLeftX-1)/width) * 1000 / llSampleRate;

  for (x=pDrawDesc->iDrawAreaLeftX; x<=pDrawDesc->iDrawAreaRightX; x++) // For every pixel we display
  {
    WaWEPosition_t Posinmsec, NewPosinmsec;

    NewPosinmsec =
      (pDrawDesc->FirstSamplePos + PosDiff*x/width) * 1000 / llSampleRate;

    if (NewPosinmsec>PrevPosinmsec)
    {
      for (Posinmsec = PrevPosinmsec+1; Posinmsec<=NewPosinmsec; Posinmsec++)
      {
        if ((iVerticalLinesLevel >= VERTLINE_ATMINS) &&
            ((Posinmsec % 60000)==0))
        {
          GpiSetLineType(pDrawDesc->hpsImageBuffer, LINETYPE_DEFAULT); // Solid line
          ptl.x = x;
          ptl.y = 0;
          GpiMove(pDrawDesc->hpsImageBuffer, &ptl);
          ptl.x = x;
          ptl.y = height;
          GpiLine(pDrawDesc->hpsImageBuffer, &ptl);
        } else
        if ((iVerticalLinesLevel >= VERTLINE_ATHALFMINS) &&
            ((Posinmsec % 30000)==0))
        {
          GpiSetLineType(pDrawDesc->hpsImageBuffer, LINETYPE_LONGDASH); // Long dashes
          ptl.x = x;
          ptl.y = 0;
          GpiMove(pDrawDesc->hpsImageBuffer, &ptl);
          ptl.x = x;
          ptl.y = height;
          GpiLine(pDrawDesc->hpsImageBuffer, &ptl);
        } else
        if ((iVerticalLinesLevel >= VERTLINE_ATSECS) &&
            ((Posinmsec % 1000)==0))
        {
          GpiSetLineType(pDrawDesc->hpsImageBuffer, LINETYPE_DASHDOT); // DashDotted
          ptl.x = x;
          ptl.y = 0;
          GpiMove(pDrawDesc->hpsImageBuffer, &ptl);
          ptl.x = x;
          ptl.y = height;
          GpiLine(pDrawDesc->hpsImageBuffer, &ptl);
        } else
        if ((iVerticalLinesLevel >= VERTLINE_ATHALFSECS) &&
            ((Posinmsec % 500)==0))
        {
          GpiSetLineType(pDrawDesc->hpsImageBuffer, LINETYPE_DASHDOUBLEDOT); // Dash + double dots
          ptl.x = x;
          ptl.y = 0;
          GpiMove(pDrawDesc->hpsImageBuffer, &ptl);
          ptl.x = x;
          ptl.y = height;
          GpiLine(pDrawDesc->hpsImageBuffer, &ptl);
        } else
        if ((iVerticalLinesLevel >= VERTLINE_ATONETENTHSECS) &&
            ((Posinmsec % 100)==0))
        {
          GpiSetLineType(pDrawDesc->hpsImageBuffer, LINETYPE_DOT);
          ptl.x = x;
          ptl.y = 0;
          GpiMove(pDrawDesc->hpsImageBuffer, &ptl);
          ptl.x = x;
          ptl.y = height;
          GpiLine(pDrawDesc->hpsImageBuffer, &ptl);
        } else
        if ((iVerticalLinesLevel >= VERTLINE_ATONEHUNDREDTHSECS) &&
            ((Posinmsec % 10)==0))
        {
          GpiSetLineType(pDrawDesc->hpsImageBuffer, LINETYPE_DOUBLEDOT);
          ptl.x = x;
          ptl.y = 0;
          GpiMove(pDrawDesc->hpsImageBuffer, &ptl);
          ptl.x = x;
          ptl.y = height;
          GpiLine(pDrawDesc->hpsImageBuffer, &ptl);
        }
      }
    }
    PrevPosinmsec = NewPosinmsec;
  }

//  printf("[plugin_Draw] : Draw waweform\n"); fflush(stdout);
//  printf("[plugin_Draw] : First=%d  Last=%d\n", (int) (pDrawDesc->FirstSamplePos),(int) pDrawDesc->LastSamplePos); fflush(stdout);
//  printf("[plugin_Draw] : Full width=%d   drawarea width=%d\n", width, drawwidth); fflush(stdout);

  //
  // --- Draw the waveform ---
  //

#ifdef USE_LOCAL_CACHE_AT_READING
  SetupSampleCache();
#endif

  iPrevHadError = 0;
  GpiSetLineType(pDrawDesc->hpsImageBuffer, LINETYPE_DEFAULT);
  for (x=-1; x<=drawwidth; x++)
  {
    Pos = pDrawDesc->FirstSamplePos +
      (PosDiff)*(pDrawDesc->iDrawAreaLeftX+x)/width;

    if (Pos<0)
    {
      rc = WAWEHELPER_ERROR_NODATA;
      Sample = 0;
    }
    else
    {
#ifdef USE_LOCAL_CACHE_AT_READING
      rc = ReadOneSample(pDrawDesc, Pos, &Sample, &ReadedSamples);
#else
      /*
       This is the original call to decode (read) one sample. Unfortunately,
       this is very slow if the decoder has to decode blocks for every
       call... So, we make a cache here!
       Well, we now have a cache in WaWEHelper level, but still...
       */
      rc = PluginHelpers.fn_WaWEHelper_Channel_ReadSample(pDrawDesc->pChannelDesc,
                                                          Pos,
                                                          1,
                                                          &Sample,
                                                          &ReadedSamples);
#endif
    }
    if (iPrevHadError)
      GpiSetColor(pDrawDesc->hpsImageBuffer, RGB_RED);   // Red
    else
      GpiSetColor(pDrawDesc->hpsImageBuffer, RGB_GREEN); // Green
    if (rc!=WAWEHELPER_NOERROR)
    {
      Sample=0;
      iPrevHadError = 1;
    } else
      iPrevHadError = 0;

    BigSample = Sample;
    BigSample = BigSample * (height/2) / MAX_WAWESAMPLE;
    ptl.x = pDrawDesc->iDrawAreaLeftX + x;
    ptl.y = middley + BigSample;
    if (x<0)
      GpiMove(pDrawDesc->hpsImageBuffer, &ptl);
    else
      GpiLine(pDrawDesc->hpsImageBuffer, &ptl);
  }

#ifdef USE_LOCAL_CACHE_AT_READING
  ClearSampleCache();
#endif

  // Destroy the clip region
  GpiSetClipRegion(pDrawDesc->hpsImageBuffer, hrgnOld, &hrgnTemp);
  GpiDestroyRegion(pDrawDesc->hpsImageBuffer, hrgn);
//  printf("[plugin_Draw] : Return\n"); fflush(stdout);
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

  pPluginDesc->iPluginType = WAWE_PLUGIN_TYPE_EDITOR;

  // Plugin functions:
  pPluginDesc->fnAboutPlugin = plugin_About;
  pPluginDesc->fnConfigurePlugin = NULL;
  pPluginDesc->fnInitializePlugin = plugin_Initialize;
  pPluginDesc->fnPrepareForShutdown = NULL;
  pPluginDesc->fnUninitializePlugin = NULL;

  pPluginDesc->TypeSpecificInfo.EditorPluginInfo.fnDraw = plugin_Draw;

  // Save module handle for later use
  // (We will need it when we want to load some resources from DLL)
  hmodOurDLLHandle = pPluginDesc->hmodDLL;

  // Return success!
  return 1;
}
