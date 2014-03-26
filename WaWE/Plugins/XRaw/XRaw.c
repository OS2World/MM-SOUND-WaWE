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
 * RAW data export plugin                                                    *
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

char *WinStateSave_Name = "WaWE";
char *WinStateSave_Key  = "ExportPlugin_RAW_ExportPP";

char *pchPluginName = "RAW Export plugin";
char *pchAboutTitle = "About RAW Export plugin";
char *pchAboutMessage =
  "RAW Export plugin v1.0 for WaWE\n"
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
                Msg, "RAW Export Plugin Error", 0, MB_OK | MB_MOVEABLE | MB_ICONEXCLAMATION);
}

// Functions called from outside:

void *                WAWECALL plugin_Create(char *pchFileName,
                                             WaWEExP_Create_Desc_p pFormatDesc,
                                             HWND hwndOwner)
{
  FILE *hFile;
  char achTemp[64];
  int rc;

  strcpy(achTemp, ""); // Default
  rc = PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKey(&(pFormatDesc->ChannelSetFormat),
                                                            "FourCC", 0,
                                                            achTemp, sizeof(achTemp));

  if (stricmp(achTemp, "RAW"))
  {
    // Somebody wants us to save a audio data in RAW format, but the
    // source format is not RAW, or unset!
    // Make sure the user wants this!
    if (WinMessageBox(HWND_DESKTOP, hwndOwner,
                      "Are you sure you want to save in RAW format?", "RAW Export Plugin - Saving in RAW format", 0, MB_YESNO | MB_MOVEABLE | MB_ICONQUESTION)!=MBID_YES)
      return NULL;
  }
  // First try to open the file
  hFile = fopen(pchFileName, "wb+");
  if (!hFile)
  {
    ErrorMsg("Could not create file!\n");
    return NULL;
  }

  return (void *) hFile;
}

WaWESize_t            WAWECALL plugin_Write(void *pHandle,
                                            char *pchBuffer,
                                            WaWESize_t cBytes)
{
  FILE *hFile;

  if (!pHandle) return 0;

  hFile = (FILE *)pHandle;

  return fwrite(pchBuffer, 1, cBytes, hFile);
}

long                  WAWECALL plugin_Close(void *pHandle, WaWEChannelSet_p pSavedChannelSet)
{
  FILE *hFile;

  if (!pHandle) return 0;
  hFile = (FILE *)pHandle;

  fclose(hFile);

  if (pSavedChannelSet)
  {
    // Modify channel-set format, clear fourCC!
    PluginHelpers.fn_WaWEHelper_ChannelSetFormat_DeleteKeysAndSubkeys(&(pSavedChannelSet->ChannelSetFormat),
                                                                      "FourCC");
    PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pSavedChannelSet->ChannelSetFormat),
                                                          "FourCC",
                                                          "RAW");

  }

  return 1;
}

long                  WAWECALL plugin_IsFormatSupported(WaWEExP_Create_Desc_p pFormatDesc)
{
  if (!pFormatDesc) return 0;
  // We don't mind the format, we take everything! :)
  return 1;
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
  pPluginDesc->iPluginImportance = WAWE_PLUGIN_IMPORTANCE_MIN;

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
