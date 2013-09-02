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
 * WAV format export plugin                                                  *
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
#include <os2.h>

#include "WaWECommon.h"    // Common defines and structures

// Global variables
// (Per process, because the DLL is initinstance/terminstance!)
HMODULE hmodOurDLLHandle;
WaWEPluginHelpers_t PluginHelpers;

char *pchPluginName = "WAV Export plugin";
char *pchAboutTitle = "About WAV Export plugin";
char *pchAboutMessage =
  "WAV Export plugin v1.0 for WaWE\n"
  "Written by Doodle\n"
  "\n"
  "Compilation date: "__DATE__" "__TIME__;

typedef struct XWAV_FileDesc_t_struct {

  FILE *hFile;
  WaWESize_t WrittenBytes;
} XWAV_FileDesc_t, *XWAV_FileDesc_p;

// Helper functions, called from inside:

void ErrorMsg(char *Msg)
{
#ifdef DEBUG_BUILD
  printf("ErrorMsg: %s\n",Msg); fflush(stdout);
#endif
  WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                Msg, "WAV Export Plugin Error", 0, MB_OK | MB_MOVEABLE | MB_ICONEXCLAMATION);
}

// Functions called from outside:

void *                WAWECALL plugin_Create(char *pchFileName,
                                             WaWEExP_Create_Desc_p pFormatDesc,
                                             HWND hwndOwner)
{
  FILE *hFile;
  long lTemp;
  XWAV_FileDesc_p pHandle;

  if (((pFormatDesc->iBits<=8) && (pFormatDesc->iIsSigned==1)) ||
      ((pFormatDesc->iBits>8) && (pFormatDesc->iIsSigned==0)))
  {
    ErrorMsg("One of the following WAV file requirements are not met:\n"
             "- For bit-depths of 8 and lower the samples have to be unsigned\n"
             "- For bit-depths above 8 the samples have to be signed\n"
             "\n"
             "The export plugin cannot save this channel-set into WAV container!\n");
    return NULL;
  }

  // First try to open the file
  hFile = fopen(pchFileName, "wb+");
  if (!hFile)
  {
    ErrorMsg("Could not create file!\n");
    return NULL;
  }

  // File opened, fine, so create our own structure!
  pHandle = (XWAV_FileDesc_p) malloc(sizeof(XWAV_FileDesc_t));
  if (!pHandle)
  {
    fclose(hFile);
    ErrorMsg("Out of memory!\n");
    return NULL;
  }

  pHandle->hFile = hFile;
  pHandle->WrittenBytes = 0;

  // Prepare the beginning of the file!

  fwrite("RIFF", 1, 4, hFile); // Write RIFF header
  lTemp = 0; fwrite(&lTemp, 1, 4, hFile); // Write ChunkSize (will be filled later)
  fwrite("WAVE", 1, 4, hFile); // Write format

  // Subchunk
  fwrite("fmt ", 1, 4, hFile); // subchunk ID
  lTemp = 16; fwrite(&lTemp, 1, 4, hFile); // Write SubChunkSize (always 16 for PCM)
  lTemp = 1; fwrite(&lTemp, 1, 2, hFile); // Write AudioFormat (1 for PCM)
  lTemp = pFormatDesc->iChannels; fwrite(&lTemp, 1, 2, hFile); // Write NumChannels
  lTemp = pFormatDesc->iFrequency; fwrite(&lTemp, 1, 4, hFile); // Write SampleRate
  lTemp = pFormatDesc->iFrequency * pFormatDesc->iChannels * (pFormatDesc->iBits+7);
  fwrite(&lTemp, 1, 4, hFile); // Write ByteRate
  lTemp = pFormatDesc->iChannels * (pFormatDesc->iBits+7);
  fwrite(&lTemp, 1, 2, hFile); // Write BlockAlign
  lTemp = pFormatDesc->iBits; fwrite(&lTemp, 1, 2, hFile); // Write BitsPerSample

  // Subchunk2
  fwrite("data", 1, 4, hFile); // subchunk ID
  lTemp = 0; fwrite(&lTemp, 1, 4, hFile); // Write SubChunkSize (will be filled later)
  // Then come the audio data.

  return (void *) pHandle;
}

WaWESize_t            WAWECALL plugin_Write(void *pHandle,
                                            char *pchBuffer,
                                            WaWESize_t cBytes)
{
  WaWESize_t Written;
  XWAV_FileDesc_p pXWAVHandle = pHandle;

  if (!pHandle) return 0;

  Written = fwrite(pchBuffer, 1, cBytes, pXWAVHandle->hFile);
  pXWAVHandle->WrittenBytes+=Written;
  return Written;
}

long                  WAWECALL plugin_Close(void *pHandle, WaWEChannelSet_p pSavedChannelSet)
{
  FILE *hFile;
  XWAV_FileDesc_p pXWAVHandle;
  long lChunkSize;

  if (!pHandle) return 0;
  pXWAVHandle = (XWAV_FileDesc_p) pHandle;

  hFile = pXWAVHandle->hFile;

  lChunkSize = pXWAVHandle->WrittenBytes + 36;
  // Fill the missing stuffs
  fseek(hFile, 4, SEEK_SET);
  fwrite(&lChunkSize, 1, 4, hFile);
  fseek(hFile, 40, SEEK_SET);
  lChunkSize = pXWAVHandle->WrittenBytes;
  fwrite(&lChunkSize, 1, 4, hFile);

  fclose(hFile);
  free(pXWAVHandle);

  if (pSavedChannelSet)
  {
    // Modify channel-set format, set to "WAVE"!
    PluginHelpers.fn_WaWEHelper_ChannelSetFormat_DeleteKeysAndSubkeys(&(pSavedChannelSet->ChannelSetFormat),
                                                                      "FourCC");
    PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pSavedChannelSet->ChannelSetFormat),
                                                          "FourCC",
                                                          "WAVE");
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
  printf("[XWAV@plugin_IsFormatSupported] : rc = %d, achTemp = [%s]\n", rc, achTemp);
#endif

  if ((rc==WAWEHELPER_NOERROR) &&
      (!strcmp(achTemp, "WAVE")))
  {
    // Seems to be a good format, but WAV also needs unsigned for 8 bits!
    if (((pFormatDesc->iBits<=8) && (pFormatDesc->iIsSigned==0)) ||
        ((pFormatDesc->iBits>8) && (pFormatDesc->iIsSigned==1)))
    {
#ifdef DEBUG_BUILD
      printf("[XWAV@plugin_IsFormatSupported] : Yes!\n");
#endif
      return 1;
    }
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

  // We can handle WAV files, nobody else should if we're here,
  // so make out importance quite high!
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
