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
 * Export plugin, using the built-in codecs of MMIO subsystem of OS/2        *
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

#define INCL_OS2MM
#define INCL_MMIOOS2
#define INCL_MMIO_CODEC
#define INCL_GPI
#include <os2.h>
#include <os2me.h>

#include "WaWECommon.h"    // Common defines and structures

// Global variables
// (Per process, because the DLL is initinstance/terminstance!)
HMODULE hmodOurDLLHandle;
WaWEPluginHelpers_t PluginHelpers;

char *pchPluginName = "MMIO ~Export plugin";
char *pchAboutTitle = "About MMIO Export plugin";
char *pchAboutMessage =
  "MMIO Export plugin v1.0 for WaWE\n"
  "Written by Doodle\n"
  "\n"
  "Compilation date: "__DATE__" "__TIME__"\n"
  "\n"
  "This plugin uses the installed MMIO codecs and IOProcs of OS/2 to encode files. "
  "This means, that it should be able to save in every audio format which can be saved by OS/2 itself.";



// Helper functions, called from inside:

void ErrorMsg(char *Msg)
{
#ifdef DEBUG_BUILD
  printf("ErrorMsg: %s\n",Msg); fflush(stdout);
#endif
  WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                Msg, "MMIO Export Plugin Error", 0, MB_OK | MB_MOVEABLE | MB_ICONEXCLAMATION);
}

static int MMIOIsFormatSupportedForWriting(char *pchFormatDesc, char *pchFourCC)
{
  MMFORMATINFO mmFormatInfo;
  MMFORMATINFO mmFormat;
  LONG         lFormatsRead;
  ULONG        rc;
  char         achFourCC[6];
  int          i;

  if (!pchFormatDesc) return 0;

  // Let's see if MMIO supports the given format!
#ifdef DEBUG_BUILD
  printf("[MMIOIsFormatSupportedForWriting] : Enter\n");
#endif

  // Copy out first some chars (until space or end) to get format specifier!
  i = 0;
  while ((i<5) && (pchFormatDesc[i]) &&
         (pchFormatDesc[i]!=' '))
  {
    achFourCC[i] = pchFormatDesc[i];
    i++;
  }
  if (i<6)
    achFourCC[i] = 0;
  else
    achFourCC[5] = 0;

  // Tell the caller the final FourCC if it's interested in it!
  if (pchFourCC)
    strcpy(pchFourCC, achFourCC);

  if (i>=5)
  {
#ifdef DEBUG_BUILD
    printf("[MMIOIsFormatSupportedForWriting] : Channel-set format ([%s] from [%s]) cannot be used as FourCC! Leaving!\n",
           achFourCC, pchFormatDesc); fflush(stdout);
#endif
    return 0;
  }

  // Now ask MMIO if it has an Audio ioproc for this fourCC!
  memset(&mmFormatInfo, 0, sizeof(mmFormatInfo));
  mmFormatInfo.ulMediaType |= MMIO_MEDIATYPE_AUDIO;
  mmFormatInfo.fccIOProc = mmioStringToFOURCC(achFourCC, 0);
  rc = mmioGetFormats(&mmFormatInfo, 1, &mmFormat, &lFormatsRead, 0, 0);
  if ((rc!=MMIO_SUCCESS) || (lFormatsRead==0))
  {
#ifdef DEBUG_BUILD
    printf("[MMIOIsFormatSupportedForWriting] : Could not find MMIOProc for fourCC [%s], Leaving!\n", achFourCC);
#endif
    return 0;
  }

  if (!(mmFormat.ulFlags & MMIO_CANWRITETRANSLATED))
  {
#ifdef DEBUG_BUILD
    printf("[MMIOIsFormatSupportedForWriting] : Found MMIOProc for fourCC [%s], but it cannot write. Leaving!\n", achFourCC);
#endif
    return 0;
  }

#ifdef DEBUG_BUILD
  printf("[MMIOIsFormatSupportedForWriting] : Found MMIOProc for fourCC [%s]!\n", achFourCC);
  printf("[MMIOIsFormatSupportedForWriting] : Leave (iResult=1)\n");
#endif

  return 1;
}

// Functions called from outside:

void *                WAWECALL plugin_Create(char *pchFileName,
                                             WaWEExP_Create_Desc_p pFormatDesc,
                                             HWND hwndOwner)
{
  HMMIO hFile;
  ULONG rc;
  MMIOINFO mmioInfo;
  MMAUDIOHEADER mmAudioHeader;
  LONG lBytesWritten;
  char achFourCC[6];
  char achTemp[64];


  if ((!pchFileName) || (!pFormatDesc)) return 0;

  // Try to get the FourCC key from formatdesc.
  // If there is no such stuff, then we cannot do anything..

  if (PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKey(&(pFormatDesc->ChannelSetFormat),
                                                           "FourCC", 0,
                                                           achTemp, sizeof(achTemp))!=WAWEHELPER_NOERROR)
  {
    ErrorMsg("The file format (FourCC of the Channel-Set) is not set!");
    return 0;
  }

  // Let's see if MMIO supports the given format!
  if (!MMIOIsFormatSupportedForWriting(achTemp, achFourCC))
  {
    ErrorMsg("The file format (FourCC) of the Channel-Set is not supported by this plugin!");
    return 0;
  }

  // Ok, it seems that MMIO supports it, and we have the FourCC also.
  // Now, try to create the file with MMIO!

#ifdef DEBUG_BUILD
  printf("[plugin_Create] : Preparing mmioOpen()\n");
#endif
  memset(&mmioInfo, 0, sizeof(mmioInfo));
  mmioInfo.ulFlags = MMIO_READWRITE | MMIO_DENYWRITE | MMIO_CREATE | MMIO_VERTBAR;
  mmioInfo.ulTranslate = MMIO_TRANSLATEDATA |
       		         MMIO_TRANSLATEHEADER;
  mmioInfo.fccIOProc = mmioStringToFOURCC(achFourCC, 0);

#ifdef DEBUG_BUILD
  printf("[plugin_Create] : Opening %s\n", pchFileName);
#endif
  hFile = mmioOpen(pchFileName, &mmioInfo, mmioInfo.ulFlags);

  if (NULL == hFile)
  {
#ifdef DEBUG_BUILD
    printf("[plugin_Create] : Could not mmioOpen() the file!\n");
    printf("[plugin_Create] : rc = %d\n", mmioInfo.ulErrorRet);
#endif
    return NULL;
  }

#ifdef DEBUG_BUILD
  printf("[plugin_Create] : Setting header\n");
#endif

  mmAudioHeader.ulHeaderLength = sizeof(mmAudioHeader);
  mmAudioHeader.ulContentType = 0;
  mmAudioHeader.ulMediaType = MMIO_MEDIATYPE_AUDIO;
  mmAudioHeader.mmXWAVHeader.WAVEHeader.usFormatTag = DATATYPE_WAVEFORM;
  mmAudioHeader.mmXWAVHeader.WAVEHeader.usChannels = pFormatDesc->iChannels;
  mmAudioHeader.mmXWAVHeader.WAVEHeader.ulSamplesPerSec = pFormatDesc->iFrequency;
  mmAudioHeader.mmXWAVHeader.WAVEHeader.usBitsPerSample = pFormatDesc->iBits;
  mmAudioHeader.mmXWAVHeader.WAVEHeader.ulAvgBytesPerSec =
    pFormatDesc->iChannels * pFormatDesc->iFrequency * (pFormatDesc->iBits+7)/8;
  mmAudioHeader.mmXWAVHeader.WAVEHeader.usBlockAlign =
    pFormatDesc->iChannels * (pFormatDesc->iBits+7)/8;
  mmAudioHeader.mmXWAVHeader.XWAVHeaderInfo.ulAudioLengthInMS = 0;
  mmAudioHeader.mmXWAVHeader.XWAVHeaderInfo.ulAudioLengthInBytes = 0;
  mmAudioHeader.mmXWAVHeader.XWAVHeaderInfo.ulAudioLengthInBytes = NULL;

  rc = mmioSetHeader(hFile, &mmAudioHeader, sizeof(mmAudioHeader),
                     &lBytesWritten, 0, 0);

  if (rc!=MMIO_SUCCESS)
  {
#ifdef DEBUG_BUILD
    printf("[plugin_Create] : Error at mmioSetHeader(), rc=0x%x\n", rc);
#endif
    mmioClose(hFile, 0);
    DosDelete(pchFileName);
    return NULL;
  }

  return (void *) hFile;

}

WaWESize_t            WAWECALL plugin_Write(void *pHandle,
                                            char *pchBuffer,
                                            WaWESize_t cBytes)
{
  HMMIO hFile;
  if (!pHandle) return 0;

  hFile = (HMMIO) pHandle;

  return mmioWrite(hFile, pchBuffer, cBytes);
}

long                  WAWECALL plugin_Close(void *pHandle, WaWEChannelSet_p pSavedChannelSet)
{
  HMMIO hFile;
  if (!pHandle) return 0;

  hFile = (HMMIO) pHandle;
  mmioClose(hFile, 0);

  // No need to modify channel-set format, as we never change it.
  return 1;
}

long                  WAWECALL plugin_IsFormatSupported(WaWEExP_Create_Desc_p pFormatDesc)
{
  char achTemp[64];

  if (!pFormatDesc) return 0;

  // Try to get FourCC from formatDesc
  if (PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKey(&(pFormatDesc->ChannelSetFormat),
                                                           "FourCC", 0,
                                                           achTemp, sizeof(achTemp))!=WAWEHELPER_NOERROR)
    return 0;

  // Let's see if MMIO supports the given format!
  if (MMIOIsFormatSupportedForWriting(achTemp, NULL))
    return 1;
  else
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

  // Use this plugin to save files if no other plugin accepted the file:
  pPluginDesc->iPluginImportance = (WAWE_PLUGIN_IMPORTANCE_MIN + WAWE_PLUGIN_IMPORTANCE_MAX)/2;

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
