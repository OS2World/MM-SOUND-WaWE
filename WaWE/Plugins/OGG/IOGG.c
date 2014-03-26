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
 * OGG Vorbis import plugin                                                  *
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

#include "vorbis\vorbisfile.h"

#include "WaWECommon.h"    // Common defines and structures

// Global variables
// (Per process, because the DLL is initinstance/terminstance!)
HMODULE hmodOurDLLHandle;
WaWEPluginHelpers_t PluginHelpers;
HMTX hmtxUsePlugin; // Mutex semaphore to serialize access to plugin!

char *pchPluginName = "OGG Vorbis Import plugin";
char *pchAboutTitle = "About OGG Vorbis Import plugin";
char *pchAboutMessage =
  "OGG Vorbis Import plugin v1.0 for WaWE\n"
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
                Msg, "OGG Vorbis Import Plugin Error", 0, MB_OK | MB_MOVEABLE | MB_ICONEXCLAMATION);
}


//----- Default callbacks for file reading: -----------

size_t file_read(void *ptr, size_t size, size_t nmemb, void *datasource)
{
#ifdef CALLBACK_DEBUG
  printf("file_read %p %d %d %p\n", ptr, size, nmemb, datasource); fflush(stdout);
#endif
  
  return fread(ptr, size, nmemb, (FILE *)datasource);
}

int file_seek(void *datasource, ogg_int64_t offset, int whence)
{
#ifdef CALLBACK_DEBUG
  printf("file_seek %p %d %d\n", datasource, offset, whence); fflush(stdout);
#endif

  return fseek((FILE *)datasource, offset, whence);
}

int file_close(void *datasource)
{
#ifdef CALLBACK_DEBUG
  printf("file_close %p\n", datasource); fflush(stdout);
#endif
  return fclose((FILE *) datasource);
}

long file_tell(void *datasource)
{
#ifdef CALLBACK_DEBUG
  printf("file_tell %p\n", datasource); fflush(stdout);
#endif
  return ftell((FILE *) datasource);
}

static ov_callbacks OggFileHandler = {file_read, file_seek, file_close, file_tell};

// Functions called from outside:

WaWEImP_Open_Handle_p WAWECALL plugin_Open(char *pchFileName, int iOpenMode, HWND hwndOwner)
{
  FILE *hFile;
  int i;
  WaWEImP_Open_Handle_p pResult;
  vorbis_comment *pComment;
  OggVorbis_File *povfile;
  char achTemp[64];

  pResult = NULL;

  if (DosRequestMutexSem(hmtxUsePlugin, SEM_INDEFINITE_WAIT) != NO_ERROR)
  {
#ifdef DEBUG_BUILD
    printf("[plugin_Open] : IOGG : Could not get mutex semaphore!\n"); fflush(stdout);
#endif
    return NULL;
  }

  // First try to open the file
#ifdef DEBUG_BUILD
  printf("[plugin_Open] : IOGG : Opening file\n"); fflush(stdout);
#endif
  hFile = fopen(pchFileName, "rb");
  if (!hFile)
  {
    DosReleaseMutexSem(hmtxUsePlugin);
    ErrorMsg("Could not open file!\n");
    return NULL;
  }

  if (iOpenMode==WAWE_OPENMODE_TEST)
  {
    int rc;

    povfile = (OggVorbis_File *) malloc(sizeof(OggVorbis_File));

    if (!povfile)
    {
      DosReleaseMutexSem(hmtxUsePlugin);
      ErrorMsg("Out of memory!\n");
#ifdef DEBUG_BUILD
      printf("[plugin_Open] : IOGG : Closing file\n"); fflush(stdout);
#endif
      fclose(hFile);
      return NULL;
    } else
    {
      // If we only had to test if we can handle it, ask OGG Vorbis to test it!
      rc = ov_test_callbacks(hFile, povfile, NULL, 0, OggFileHandler);
      if (!rc)
      {
        // Cool, Ogg Vorbis has identified it to be a valid stream!
        pResult = (WaWEImP_Open_Handle_p) malloc(sizeof(WaWEImP_Open_Handle_t));
        if (pResult)
        {
          // Set default stuffs, we'll fill these later, at real open!
          memset(pResult, 0, sizeof(WaWEImP_Open_Handle_t));
          pResult->iFrequency = 44100;
          pResult->iChannels = 2;
          pResult->iBits = 16;
          pResult->iIsSigned = 1;
  
          // We don't propose any channel or chanelset names.
          pResult->pchProposedChannelSetName = NULL;
          pResult->apchProposedChannelName = NULL;

          PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                                "FourCC", "OGG");
          pResult->pPluginSpecific = povfile;
  
          pResult->bTempClosed = 0;
        } else
        {
          DosReleaseMutexSem(hmtxUsePlugin);
          ErrorMsg("Out of memory!\n");
          ov_clear(povfile);
          free(povfile);
#ifdef DEBUG_BUILD
          printf("[plugin_Open] : IOGG : Closing file\n"); fflush(stdout);
#endif
          fclose(hFile);
          return NULL;
        }
      }
      else
      {
        // Not an OGG file
        DosReleaseMutexSem(hmtxUsePlugin);
#ifdef DEBUG_BUILD
        printf("[plugin_Open] : IOGG : Closing file\n"); fflush(stdout);
#endif
        fclose(hFile);
        return NULL;
      }
    }
  } else
  {
    // If we have to open it really, then tell OGG Vorbis to open it!
    int rc;

    povfile = (OggVorbis_File *) malloc(sizeof(OggVorbis_File));

    if (!povfile)
    {
      DosReleaseMutexSem(hmtxUsePlugin);
      ErrorMsg("Out of memory!\n");
#ifdef DEBUG_BUILD
      printf("[plugin_Open] : IOGG : Closing file\n"); fflush(stdout);
#endif
      fclose(hFile);
      return NULL;
    } else
    {
      // If we only had to test if we can handle it, ask OGG Vorbis to test it!
      rc = ov_open_callbacks(hFile, povfile, NULL, 0, OggFileHandler);
      if (!rc)
      {
        // Cool, Ogg Vorbis has identified it to be a valid stream, and opened it fully!
        pResult = (WaWEImP_Open_Handle_p) malloc(sizeof(WaWEImP_Open_Handle_t));
        if (pResult)
        {
          vorbis_info *vinfo;

          // Get stream information
          vinfo = ov_info(povfile, -1);
          if (!vinfo)
          {
            free(pResult); pResult = NULL;
            DosReleaseMutexSem(hmtxUsePlugin);
            ErrorMsg("Error while querying stream information!\n");
            ov_clear(povfile);
            free(povfile);
#ifdef DEBUG_BUILD
            printf("[plugin_Open] : IOGG : Closing file\n"); fflush(stdout);
#endif
            fclose(hFile);
            return NULL;
          }

          // Set stream properties
          memset(pResult, 0, sizeof(WaWEImP_Open_Handle_t));
          pResult->iFrequency = vinfo->rate;
          pResult->iChannels = vinfo->channels;
          pResult->iBits = 16;
          pResult->iIsSigned = 1;
          pResult->ChannelSetFormat = NULL;

          // We don't propose any channel or chanelset names by default
          pResult->pchProposedChannelSetName = NULL;
          pResult->apchProposedChannelName = NULL;

          // But check for special comments, and make channel-set name,
          // if found!
          // Also put comments into ChannelSet descriptor!
          pComment = ov_comment(povfile, -1);
          if (pComment)
          {
            char *pchTitle = NULL;
            char *pchArtist = NULL;
            char *pchAlbum = NULL;
            int iCHSetNameLen;
#ifdef DEBUG_BUILD
            printf(" OGG comments:\n");
#endif
            for (i=0; i<pComment->comments; i++)
            {
#ifdef DEBUG_BUILD
              printf("  Comment %d. : [%s]\n", i, pComment->user_comments[i]);
#endif
              if (strstr(pComment->user_comments[i],"TITLE=")==pComment->user_comments[i])
              {
                // Found Title!
                pchTitle = pComment->user_comments[i]+6;
                PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                                      "title", pchTitle);
              } else
              if (strstr(pComment->user_comments[i],"ARTIST=")==pComment->user_comments[i])
              {
                // Found Artist!
                pchArtist = pComment->user_comments[i]+7;
                PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                                      "artist", pchArtist);
              } else
              if (strstr(pComment->user_comments[i],"ALBUM=")==pComment->user_comments[i])
              {
                // Found Album!
                pchAlbum = pComment->user_comments[i]+6;
                PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                                      "album", pchAlbum);
              } else
              if (strstr(pComment->user_comments[i],"TRACKNUMBER=")==pComment->user_comments[i])
              {
                // Found track number!
                PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                                      "track number", pComment->user_comments[i]+12);
              } else
              if (strstr(pComment->user_comments[i],"PERFORMER=")==pComment->user_comments[i])
              {
                // Found performer!
                PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                                      "performer", pComment->user_comments[i]+10);
              } else
              if (strstr(pComment->user_comments[i],"COPYRIGHT=")==pComment->user_comments[i])
              {
                // Found copyright!
                PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                                      "copyright", pComment->user_comments[i]+10);
              } else
              if (strstr(pComment->user_comments[i],"LICENSE=")==pComment->user_comments[i])
              {
                // Found license!
                PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                                      "license", pComment->user_comments[i]+8);
              } else
              if (strstr(pComment->user_comments[i],"ORGANIZATION=")==pComment->user_comments[i])
              {
                // Found organization!
                PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                                      "organization", pComment->user_comments[i]+13);
              } else
              if (strstr(pComment->user_comments[i],"DESCRIPTION=")==pComment->user_comments[i])
              {
                // Found description!
                PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                                      "description", pComment->user_comments[i]+12);
              } else
              if (strstr(pComment->user_comments[i],"GENRE=")==pComment->user_comments[i])
              {
                // Found genre!
                PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                                      "genre", pComment->user_comments[i]+6);
              } else
              if (strstr(pComment->user_comments[i],"DATE=")==pComment->user_comments[i])
              {
                // Found date!
                PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                                      "date", pComment->user_comments[i]+5);
              } else
              if (strstr(pComment->user_comments[i],"LOCATION=")==pComment->user_comments[i])
              {
                // Found location!
                PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                                      "location", pComment->user_comments[i]+9);
              } else
              if (strstr(pComment->user_comments[i],"CONTACT=")==pComment->user_comments[i])
              {
                // Found contact!
                PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                                      "contact", pComment->user_comments[i]+8);
              } else
              if (strstr(pComment->user_comments[i],"ISRC=")==pComment->user_comments[i])
              {
                // Found isrc!
                PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                                      "ISRC", pComment->user_comments[i]+5);
              } else
              if (strstr(pComment->user_comments[i],"COMMENT=")==pComment->user_comments[i])
              {
                // Found comment
                PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                                      "comment", pComment->user_comments[i]+8);
              } else
                // Unknown comment
                PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                                      "comment", pComment->user_comments[i]);

            }
            // Prepare channel-set name
            iCHSetNameLen = 0;
            if (pchTitle) iCHSetNameLen+=strlen(pchTitle) + 10;
            if (pchArtist) iCHSetNameLen+=strlen(pchArtist) + 10;
            if (pchAlbum) iCHSetNameLen+=strlen(pchAlbum) + 10;

            if (iCHSetNameLen)
            {
              pResult->pchProposedChannelSetName = (char *) malloc(iCHSetNameLen);
              if (pResult->pchProposedChannelSetName)
              {
                strcpy(pResult->pchProposedChannelSetName, "");
                if (pchArtist)
                  strcat(pResult->pchProposedChannelSetName, pchArtist);
                if (pchAlbum)
                {
                  if (pchArtist)
                    strcat(pResult->pchProposedChannelSetName, " - ");
                  strcat(pResult->pchProposedChannelSetName, pchAlbum);
                }
                if (pchTitle)
                {
                  if ((pchArtist) || (pchAlbum))
                    strcat(pResult->pchProposedChannelSetName, " - ");
                  strcat(pResult->pchProposedChannelSetName, pchTitle);
                }
              }
            }
#ifdef DEBUG_BUILD
            printf("  Vendor: %s\n", pComment->vendor);
#endif
          }

          PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                                "FourCC", "OGG");

          // Add general bitrate info into formatdesc
          sprintf(achTemp, "%d", vinfo->bitrate_nominal);
          PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                                "bitrate", achTemp);
          // Add detailed bitrate info
          PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                                "bitrate/nominal bitrate", achTemp);
          sprintf(achTemp, "%d", vinfo->bitrate_upper);
          PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                                "bitrate/upper bitrate", achTemp);
          sprintf(achTemp, "%d", vinfo->bitrate_lower);
          PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                                "bitrate/lower bitrate", achTemp);

          pResult->pPluginSpecific = povfile;
  
          pResult->bTempClosed = 0;
        } else
        {
          DosReleaseMutexSem(hmtxUsePlugin);
          ErrorMsg("Out of memory!\n");
          ov_clear(povfile);
          free(povfile);
#ifdef DEBUG_BUILD
          printf("[plugin_Open] : IOGG : Closing file\n"); fflush(stdout);
#endif
          fclose(hFile);
          return NULL;
        }
      }
      else
      {
        // Not an OGG file
        DosReleaseMutexSem(hmtxUsePlugin);
#ifdef DEBUG_BUILD
        printf("[plugin_Open] : IOGG : Closing file\n"); fflush(stdout);
#endif
        fclose(hFile);
        return NULL;
      }
    }
  }

  DosReleaseMutexSem(hmtxUsePlugin);

  return pResult;
}

WaWEPosition_t        WAWECALL plugin_Seek(WaWEImP_Open_Handle_p pHandle,
                                           WaWEPosition_t Offset,
                                           int iOrigin)
{
  OggVorbis_File * povfile;

  if (!pHandle) return 0;

  if (DosRequestMutexSem(hmtxUsePlugin, SEM_INDEFINITE_WAIT) != NO_ERROR)
  {
#ifdef DEBUG_BUILD
    printf("[plugin_Seek] : IOGG : Could not get mutex semaphore!\n"); fflush(stdout);
#endif
    return 0;
  }

  if (pHandle->bTempClosed)
  {
    DosReleaseMutexSem(hmtxUsePlugin);
    return 0; // Hey, it's TempClosed!
  }

  
  povfile = (OggVorbis_File *)(pHandle->pPluginSpecific);
  if (!povfile)
  {
    DosReleaseMutexSem(hmtxUsePlugin);
    return 0;
  }

#ifdef DEBUG_BUILD
//  printf("[plugin_Seek] : Seek to %d (origin %d)\n", (int) Offset, iOrigin);
//  printf("                Curr pos: (samples) %d\n", (int) ov_pcm_tell(povfile));
#endif

  switch (iOrigin)
  {
    default:
    case WAWE_SEEK_SET:
        ov_pcm_seek(povfile, Offset/((pHandle->iBits+7)/8)/(pHandle->iChannels));
        break;
    case WAWE_SEEK_CUR:
        ov_pcm_seek(povfile,
                    ov_pcm_tell(povfile) + Offset/((pHandle->iBits+7)/8)/(pHandle->iChannels));
        break;
    case WAWE_SEEK_END:
        ov_pcm_seek(povfile,
                    ov_pcm_total(povfile, -1) + Offset/((pHandle->iBits+7)/8)/(pHandle->iChannels));
        break;
  }

#ifdef DEBUG_BUILD
//  printf("[plugin_Seek] : New pos: (samples)  %d\n", (int) ov_pcm_tell(povfile));
//  printf("                New pos: (bytes)    %d\n", ov_pcm_tell(povfile) * ((pHandle->iBits+7)/8)*(pHandle->iChannels));
#endif

  DosReleaseMutexSem(hmtxUsePlugin);

  return (ov_pcm_tell(povfile) * ((pHandle->iBits+7)/8)*(pHandle->iChannels));
}

WaWESize_t            WAWECALL plugin_Read(WaWEImP_Open_Handle_p pHandle,
                                           char *pchBuffer,
                                           WaWESize_t cBytes)
{
  OggVorbis_File *povfile;
  int rc;
  int bitstream;
  WaWESize_t readed;

  if (!pHandle) return 0;

  if (DosRequestMutexSem(hmtxUsePlugin, SEM_INDEFINITE_WAIT) != NO_ERROR)
  {
#ifdef DEBUG_BUILD
    printf("[plugin_Read] : IOGG : Could not get mutex semaphore!\n"); fflush(stdout);
#endif
    return 0;
  }

  if (pHandle->bTempClosed)
  {
    DosReleaseMutexSem(hmtxUsePlugin);
    return 0; // Hey, it's TempClosed!
  }

  povfile = (OggVorbis_File *)(pHandle->pPluginSpecific);
  if (!povfile)
  {
    DosReleaseMutexSem(hmtxUsePlugin);
    return 0;
  }

  readed = 0;
  rc = 0;
  while ((rc!=OV_HOLE) && (rc!=OV_EBADLINK) && (readed<cBytes))
  {
    rc = ov_read(povfile, pchBuffer, cBytes-readed,
                 0, 2, 1,
                 &bitstream);
    if (rc == 0)
    {
#ifdef DEBUG_BUILD
//      printf("[plugin_Read] : EOF\n"); fflush(stdout);
#endif
      // reached end of file/stream!
      break;
    } else
    if ((rc!=OV_HOLE) && (rc!=OV_EBADLINK))
    { // Readed some bytes!
      readed += rc;
      pchBuffer += rc;
#ifdef DEBUG_BUILD
//      printf("[plugin_Read] : Readed %d\n", (int) rc); fflush(stdout);
#endif
    }
  } // end of while

  DosReleaseMutexSem(hmtxUsePlugin);

#ifdef DEBUG_BUILD
//  printf("[plugin_Read] : Wanted %d, readed %d\n", (int) cBytes, (int) readed); fflush(stdout);
#endif
  return readed;
}

long                  WAWECALL plugin_Close(WaWEImP_Open_Handle_p pHandle)
{
  OggVorbis_File *povfile;

  if (!pHandle) return 0;

  if (DosRequestMutexSem(hmtxUsePlugin, SEM_INDEFINITE_WAIT) != NO_ERROR)
  {
#ifdef DEBUG_BUILD
    printf("[plugin_Close] : IOGG : Could not get mutex semaphore!\n"); fflush(stdout);
#endif
    return NULL;
  }

  if (!(pHandle->bTempClosed))
  {
    povfile = (OggVorbis_File *)(pHandle->pPluginSpecific);
    if (povfile)
    {
      ov_clear(povfile);
      free(povfile);
    }
  }

  if (pHandle->pchProposedChannelSetName)
    free(pHandle->pchProposedChannelSetName);
  free(pHandle);

  DosReleaseMutexSem(hmtxUsePlugin);

  return 1;
}

long                  WAWECALL plugin_TempClose(WaWEImP_Open_Handle_p pHandle)
{
  OggVorbis_File *povfile;

  if (!pHandle) return 0;

  if (DosRequestMutexSem(hmtxUsePlugin, SEM_INDEFINITE_WAIT) != NO_ERROR)
  {
#ifdef DEBUG_BUILD
    printf("[plugin_TempClose] : IOGG : Could not get mutex semaphore!\n"); fflush(stdout);
#endif
    return 0;
  }

  if (pHandle->bTempClosed)
  {
    DosReleaseMutexSem(hmtxUsePlugin);
    return 1; // No need to do anything, it's already TempClosed.
  }

  povfile = (OggVorbis_File *)(pHandle->pPluginSpecific);
  if (!povfile)
  {
    DosReleaseMutexSem(hmtxUsePlugin);
    return 0;
  }

  ov_clear(povfile); free(povfile); pHandle->pPluginSpecific = NULL;

  if (pHandle->pchProposedChannelSetName)
  {
    free(pHandle->pchProposedChannelSetName);
    pHandle->pchProposedChannelSetName = NULL;
  }

  pHandle->bTempClosed = 1; // Note that it's temporarily closed

  DosReleaseMutexSem(hmtxUsePlugin);

  return 1;
}

long                  WAWECALL plugin_ReOpen(char *pchFileName, WaWEImP_Open_Handle_p pHandle)
{
  WaWEImP_Open_Handle_p pNewHandle;

  if (!pHandle) return 0;
  if (!(pHandle->bTempClosed)) return 1; // No need to do anything, it's already opened.

  pNewHandle = plugin_Open(pchFileName, WAWE_OPENMODE_NORMAL, HWND_DESKTOP);

  if (!pNewHandle) return 0;
  memcpy(pHandle, pNewHandle, sizeof(WaWEImP_Open_Handle_t));
  free(pNewHandle);

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

  DosCreateMutexSem(NULL, // Name
                    &hmtxUsePlugin,
                    0L, // Non-shared
                    FALSE); // Unowned

  // Return success!
  return 1;
}

void WAWECALL plugin_Uninitialize(HWND hwndOwner)
{
  DosRequestMutexSem(hmtxUsePlugin, SEM_INDEFINITE_WAIT); // Make sure that nobody uses the plugin now.
  DosCloseMutexSem(hmtxUsePlugin);
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

  // Make the importance of this plugin high, so if we report that we can handle
  // a file, we'll be used. Hey, who else would handle OGG files if not us?
  pPluginDesc->iPluginImportance = WAWE_PLUGIN_IMPORTANCE_MAX - 5;

  pPluginDesc->iPluginType = WAWE_PLUGIN_TYPE_IMPORT;

  // Plugin functions:
  pPluginDesc->fnAboutPlugin = plugin_About;
  pPluginDesc->fnConfigurePlugin = NULL;
  pPluginDesc->fnInitializePlugin = plugin_Initialize;
  pPluginDesc->fnPrepareForShutdown = NULL;
  pPluginDesc->fnUninitializePlugin = plugin_Uninitialize;

  pPluginDesc->TypeSpecificInfo.ImportPluginInfo.fnOpen = plugin_Open;
  pPluginDesc->TypeSpecificInfo.ImportPluginInfo.fnSeek = plugin_Seek;
  pPluginDesc->TypeSpecificInfo.ImportPluginInfo.fnRead = plugin_Read;
  pPluginDesc->TypeSpecificInfo.ImportPluginInfo.fnClose = plugin_Close;
  pPluginDesc->TypeSpecificInfo.ImportPluginInfo.fnTempClose = plugin_TempClose;
  pPluginDesc->TypeSpecificInfo.ImportPluginInfo.fnReOpen = plugin_ReOpen;

  // Save module handle for later use
  // (We will need it when we want to load some resources from DLL)
  hmodOurDLLHandle = pPluginDesc->hmodDLL;

  // Return success!
  return 1;
}
