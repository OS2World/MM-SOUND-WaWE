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
 * General MMIO import plugin                                                *
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
HMTX hmtxUsePlugin;

char *pchPluginName = "MMIO ~Import plugin";
char *pchAboutTitle = "About MMIO Import plugin";
char *pchAboutMessage =
  "MMIO Import plugin v1.0 for WaWE\n"
  "Written by Doodle\n"
  "\n"
  "Compilation date: "__DATE__" "__TIME__"\n"
  "\n"
  "This plugin uses the installed MMIO codecs and IOProcs of OS/2 to decode files. "
  "This means, that it should be able to open every audio file which can be played from OS/2.";



// Definitions:

typedef struct MMIOFileDesc_struct
{
  unsigned long  ulMMIOHandle;     // HMMIO of opened file
  FOURCC   fccType;                // Format FCC
  char achFormatName[1024];        // Format descriptor

  unsigned short usChannels;       // Info about opened file
  unsigned long  ulSamplesPerSec;
  unsigned short usBitsPerSample;

  long           lLastPCMBytePos;     // File size in bytes, of PCM stream

} tMMIOFileDesc, *pMMIOFileDesc;

// Helper functions, called from inside:

void ErrorMsg(char *Msg)
{
#ifdef DEBUG_BUILD
  printf("ErrorMsg: %s\n",Msg); fflush(stdout);
#endif
  WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                Msg, "MMIO Import Plugin Error", 0, MB_OK | MB_MOVEABLE | MB_ICONEXCLAMATION);
}


static ULONG MMIO_internal_FindRealFileSize(HMMIO hFile, ULONG ulPos1, ULONG ulPos2)
{
  LONG rc = 0;
  while (ulPos1<ulPos2-1)
  {
#ifdef DEBUG_BUILD
    printf("[MMIO_internal_FindRealFileSize] : Trying middle of %d and %d\n", (int) ulPos1, (int) ulPos2);
#endif
    rc = mmioSeek(hFile, (ulPos2 + ulPos1) / 2, SEEK_SET);
    if (rc<0)
      ulPos2 = (ulPos1 + ulPos2)/2;
    else
      ulPos1 = (ulPos1 + ulPos2)/2;
  }

  return ulPos1;
}

pMMIOFileDesc MMIOOpen(char *pchFileName, int bNeedPreciseSize)
{
  pMMIOFileDesc result = (pMMIOFileDesc) malloc(sizeof(tMMIOFileDesc));
  ULONG rc;
  MMIOINFO mmioInfo;
  MMFORMATINFO mmFormatInfo;
  HMMIO hFile;
  ULONG ulOpenFlags;
  MMAUDIOHEADER mmAudioHeader;
  ULONG ulBytesRead;
  ULONG ulAudioLengthInBytes; // From XWAV_HEADERINFO!


  if (!result) return result; // Could not allocate memory!

  memset(result, 0, sizeof(tMMIOFileDesc));

  // First check file, if we can use that, and what format it has!
  memset(&mmioInfo, 0, sizeof(mmioInfo));
  memset(&mmFormatInfo, 0, sizeof(mmFormatInfo));
  rc = mmioIdentifyFile(pchFileName, &mmioInfo, &mmFormatInfo, &(result->fccType), 0, 0);
  if (rc!=MMIO_SUCCESS)
  {
#ifdef DEBUG_BUILD
    // Could not identify file!
    printf("[mmioOpen] : Error identifying file: rc=%d...\n",rc);
#endif
    free(result); result = NULL;
    return result;
  }

  // Now, let's the MMIO open the file!

#ifdef DEBUG_BUILD
  printf("[mmioOpen] : Preparing mmioOpen()\n");
#endif


  // We already have a filled mmioInfo!
  mmioInfo.ulTranslate = MMIO_TRANSLATEDATA |
       		         MMIO_TRANSLATEHEADER;

  
  ulOpenFlags = MMIO_READ | MMIO_DENYWRITE | MMIO_VERTBAR;
  mmioInfo.ulFlags = ulOpenFlags;
#ifdef DEBUG_BUILD
  printf("[mmioOpen] : Opening %s\n", pchFileName);
#endif
  hFile = mmioOpen(pchFileName, &mmioInfo, mmioInfo.ulFlags);
  if (NULL == hFile)
  {
#ifdef DEBUG_BUILD
    printf("[mmioOpen] : Could not open file!\n");
    printf("[mmioOpen] : rc = %d\n", mmioInfo.ulErrorRet);
#endif
    free(result); result = NULL;
    return result;
  }
#ifdef DEBUG_BUILD  
  printf("[mmioOpen] : File opened, checking type...\n");
#endif
  rc = mmioGetHeader(hFile,
                     (PVOID)&mmAudioHeader,
                     sizeof(MMAUDIOHEADER),
                     (PLONG) &ulBytesRead,
                     NULL,
                     NULL);
  if (rc!=MMIO_SUCCESS)
  {
#ifdef DEBUG_BUILD
    printf("[mmioOpen] : Not an audio file!\n");
#endif
    mmioClose(hFile, 0);
    free(result); result = NULL;
    return result;
  }
  
  if (mmAudioHeader.ulMediaType!=MMIO_MEDIATYPE_AUDIO)
  {
#ifdef DEBUG_BUILD
    printf("[mmioOpen] : Not an audio file!\n");
    printf("             MediaType is %d\n", mmAudioHeader.ulMediaType);
#endif
    mmioClose(hFile, 0);
    free(result); result = NULL;
    return result;
  }
#ifdef DEBUG_BUILD
  printf("[mmioOpen] : Audio file opened!\n");
#endif
  if (mmAudioHeader.mmXWAVHeader.WAVEHeader.usFormatTag!=DATATYPE_WAVEFORM)
  {
#ifdef DEBUG_BUILD
    printf("[mmioOpen] : Not supported format (not PCM output)!\n");
#endif
    mmioClose(hFile, 0);
    free(result); result = NULL;
    return result;
  }

  ulAudioLengthInBytes = mmAudioHeader.mmXWAVHeader.XWAVHeaderInfo.ulAudioLengthInBytes;
#ifdef DEBUG_BUILD
  printf("[mmioOpen] : Audio length in bytes from header: %d\n", ulAudioLengthInBytes);
#endif

  result->usChannels = mmAudioHeader.mmXWAVHeader.WAVEHeader.usChannels;
  result->ulSamplesPerSec = mmAudioHeader.mmXWAVHeader.WAVEHeader.ulSamplesPerSec;
  result->usBitsPerSample = mmAudioHeader.mmXWAVHeader.WAVEHeader.usBitsPerSample;

#ifdef DEBUG_BUILD
  printf("  usChannels     : %d\n", result->usChannels);
  printf("  ulSamplesPerSec: %d\n", result->ulSamplesPerSec);
  printf("  usBitsPerSample: %d\n", result->usBitsPerSample);
#endif

  // Gather format specifier info
  rc = mmioGetData(hFile, &mmioInfo, 0);
  if (rc != MMIO_SUCCESS)
  {
#ifdef DEBUG_BUILD
    printf("[mmioOpen] : Could not get data! (rc=0x%x)\n", rc);
#endif
    mmioClose(hFile, 0);
    free(result); result = NULL;
    return result;
  } else
  {
    MMFORMATINFO mmFormatInfo;
    LONG lBytesRead;

    memset(result->achFormatName, 0, sizeof(result->achFormatName));
    mmFormatInfo.ulStructLen = sizeof(mmFormatInfo);
    mmFormatInfo.fccIOProc = mmioInfo.fccIOProc;
    mmFormatInfo.lNameLength = sizeof(result->achFormatName)-1;
    rc = mmioGetFormatName(&mmFormatInfo, &(result->achFormatName[0]),&lBytesRead, 0, 0);
    if (rc!=MMIO_SUCCESS)
    {
#ifdef DEBUG_BUILD
      printf("[mmioOpen] : Could not get format name! (rc=0x%x)\n", rc);
#endif
      mmioClose(hFile, 0);
      free(result); result = NULL;
      return result;
    } else
    {
#ifdef DEBUG_BUILD
      printf("  Format name: %s\n", result->achFormatName);
      printf("  fccIOProc: %.4s\n", &(mmFormatInfo.fccIOProc));
#endif
      result->fccType = mmFormatInfo.fccIOProc;

      if (result->fccType == mmioFOURCC('M', 'P', '3', ' '))
      {
        // Yikes, it's an MP3 file!
        // The MP3 decoder in mmos/2 is buggy, cannot be used for our
        // purposes, so disable it, let's say we don't support it!
#ifdef DEBUG_BUILD
        printf("[mmioOpen] : Detected MP3 format, which is disabled!\n");
#endif
        mmioClose(hFile, 0);
        free(result); result = NULL;
        return result;
      }
    }
  }

  result->ulMMIOHandle = (unsigned long) hFile;

#ifdef DEBUG_BUILD
  printf("[mmioOpen] : Getting PCM size\n");
#endif

  if (!bNeedPreciseSize)
    result->lLastPCMBytePos = ulAudioLengthInBytes-1;
  else
  {
    if (ulAudioLengthInBytes!=0)
    {
      // We know the length of stuff from header.
      // Make sure it's valid, and not just an approximated value!
      if (mmioSeek(hFile, ulAudioLengthInBytes-1, SEEK_SET)!=-1)
      {
        result->lLastPCMBytePos = ulAudioLengthInBytes-1;
      } else
      {
        LONG rc;
        ULONG ulPos1, ulPos2;
  
#ifdef DEBUG_BUILD
        rc = mmioGetLastError(result->ulMMIOHandle);
        printf("[mmioOpen] : mmioGetLastError rc = 0x%x\n", rc);
        printf("             Reported PCM size in header seems not to be valid!\n");
        printf("             Using binary search to find the real size.\n");
#endif
  
        ulPos2 = ulAudioLengthInBytes-1;
        ulPos1 = ulAudioLengthInBytes-1;
  
        rc = mmioSeek(hFile, ulPos1, SEEK_SET);
        while ((rc==-1) && (ulPos1>0))
        {
          if (ulPos1 >= 1024*1024*1)
            ulPos1 -= 1024*1024*1; // 1 meg
          else
            break;
#ifdef DEBUG_BUILD
          printf("[mmioOpen] : Trying %d\n", (int) ulPos1);
#endif
          rc = mmioSeek(hFile, ulPos1, SEEK_SET);
        }
  
        result->lLastPCMBytePos = MMIO_internal_FindRealFileSize(hFile, ulPos1, ulPos2);
      }
      mmioSeek(hFile, 0, SEEK_SET);
    } else
    {
#ifdef DEBUG_BUILD
      printf("[mmioOpen] : Header told it's 0 sized, so try to determine size by seeking to end...\n");
#endif
  
      // Could not get length from header, try to determine it by seeking!
      result->lLastPCMBytePos = mmioSeek(hFile, 0, SEEK_END);
  
      if (result->lLastPCMBytePos==-1)
      {
        // There was an error querying the length of file
        LONG rc;
        ULONG ulPos1, ulPos2;
  
#ifdef DEBUG_BUILD
        rc = mmioGetLastError(result->ulMMIOHandle);
        printf("[mmioOpen] : mmioGetLastError rc = 0x%x\n", rc);
        printf("             Trying to get length another way!\n");
#endif
  
        ulPos1 = 0;
        ulPos2 = 0;
  
        rc = mmioSeek(hFile, ulPos1, SEEK_SET);
  
        while (rc>-1)
        {
          ulPos1 = ulPos2;
          ulPos2 += 1024*1024*5; // 5 megs
#ifdef DEBUG_BUILD
          printf("[mmioOpen] : Trying %d\n", (int) ulPos2);
#endif
          rc = mmioSeek(hFile, ulPos2, SEEK_SET);
        }
        // Ok, found a place to we cannot seek to!
        // Now find the exact position betweek ulPos1 and ulPos2, where
        // we can still seek to.
        result->lLastPCMBytePos = MMIO_internal_FindRealFileSize(hFile, ulPos1, ulPos2);
      }
    }
  }
#ifdef DEBUG_BUILD
  printf("[mmioOpen] : PCM size is %d bytes\n", (result->lLastPCMBytePos)+1);
#endif

  // Go back to the beginning!
  mmioSeek(hFile, 0, SEEK_SET);

  return result;
}

int MMIOClose(pMMIOFileDesc Handle)
{
  if (!Handle) return 0;

  mmioClose((HMMIO) (Handle->ulMMIOHandle), 0);
  free(Handle);
  DosSleep(100); // Hm, seems to be needed sometimes to give time to
  // the MMIO subsystem to close the file.
  return 1;
}

int MMIODecode(pMMIOFileDesc Handle,
               unsigned char *pBuffer, unsigned long ulToRead,
               unsigned long *pulReaded)
{
  int rc;

  if ((!Handle) || (!pulReaded)) return 0;

#ifdef DEBUG_BUILD
//  printf("[MMIODecode] : Entered (ToRead=%d)\n", (int) ulToRead);
#endif
  *pulReaded = 0;
  rc = 0;
  while ((rc!=MMIO_ERROR) && ((*pulReaded)<ulToRead))
  {
    rc = mmioRead((HMMIO)(Handle->ulMMIOHandle), pBuffer, ulToRead-(*pulReaded));

    if (rc == 0)
    {
      // reached end of file/stream!
      break;
    } else
    if (rc!=MMIO_ERROR)
    { // Readed some bytes!
      (*pulReaded) += rc;
      pBuffer += rc;
    }
  } // end of while

#ifdef DEBUG_BUILD
//  printf("[MMIODecode] : Out of loop, rc=%d,  ulReaded=%d\n", rc, (int) *pulReaded);
#endif

  // Return error, if there was any.
  if ((rc==0) || (rc==MMIO_ERROR)) return 0;

  return 1;
}

WaWEPosition_t MMIOSeek(pMMIOFileDesc Handle, WaWEPosition_t Pos, int iOrigin)
{
  if (!Handle) return 0;

  switch (iOrigin)
  {
    case WAWE_SEEK_END:
      return mmioSeek((HMMIO) (Handle->ulMMIOHandle), Handle->lLastPCMBytePos+Pos, SEEK_SET);
    case WAWE_SEEK_SET:
      return mmioSeek((HMMIO) (Handle->ulMMIOHandle), Pos, SEEK_SET);
    case WAWE_SEEK_CUR:
      return mmioSeek((HMMIO) (Handle->ulMMIOHandle), Pos, SEEK_CUR);
    default:
      return mmioSeek((HMMIO) (Handle->ulMMIOHandle), Pos, SEEK_SET);
  }
}


// Functions called from outside:
WaWEImP_Open_Handle_p WAWECALL plugin_Open(char *pchFileName, int iOpenMode, HWND hwndOwner)
{
  WaWEImP_Open_Handle_p pResult;
  pMMIOFileDesc pHandle;
  char achTemp[16];

  // Check parameters
  if (!pchFileName) return NULL;

  if (DosRequestMutexSem(hmtxUsePlugin, SEM_INDEFINITE_WAIT) != NO_ERROR)
  {
#ifdef DEBUG_BUILD
    printf("[plugin_Open] : IMMIO : Could not get mutex semaphore!\n"); fflush(stdout);
#endif
    return NULL;
  }

#ifdef DEBUG_BUILD
//  printf("[IMMIO]: plugin_Open() for [%s]\n", pchFileName);
#endif

  pResult = NULL;

  // First try to open the file
  if (iOpenMode == WAWE_OPENMODE_TEST)
    pHandle = MMIOOpen(pchFileName, FALSE);
  else
    pHandle = MMIOOpen(pchFileName, TRUE);

  if (!pHandle)
  {
    DosReleaseMutexSem(hmtxUsePlugin);

    if (iOpenMode!=WAWE_OPENMODE_TEST)
      ErrorMsg("Could not open file!\n");
#ifdef DEBUG_BUILD
//    else
//      printf("[IMMIO] : plugin_Open() : Could not open file!\n");
#endif
    return NULL;
  }

  // We don't care iOpenMode, as we have to do the same even if
  // we have to open the file or if we only have to open it to test if
  // we can handle it.

  pResult = (WaWEImP_Open_Handle_p) malloc(sizeof(WaWEImP_Open_Handle_t));
  
  if (pResult)
  {
    // Make it empty first!
    memset(pResult, 0, sizeof(WaWEImP_Open_Handle_p));

    // Set file info stuffs
    pResult->iFrequency = pHandle->ulSamplesPerSec;
    pResult->iChannels = pHandle->usChannels;
    pResult->iBits = pHandle->usBitsPerSample;
    if (pResult->iBits==8)
      pResult->iIsSigned = 0;
    else
      pResult->iIsSigned = 1;

    // We don't propose any channel or chanelset names.
    pResult->pchProposedChannelSetName = NULL;
    pResult->apchProposedChannelName = NULL;

    sprintf(achTemp, "%.4s", &(pHandle->fccType));
    PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pResult->ChannelSetFormat),
                                                          "FourCC", achTemp);
#ifdef DEBUG_BUILD
    printf("  ChannelSetFormat FourCC [%s]\n", achTemp);
#endif

    pResult->pPluginSpecific = pHandle;

    pResult->bTempClosed = 0;
  } else
    MMIOClose(pHandle);

  DosReleaseMutexSem(hmtxUsePlugin);

  return pResult;
}

WaWEPosition_t        WAWECALL plugin_Seek(WaWEImP_Open_Handle_p pHandle,
                                           WaWEPosition_t Offset,
                                           int iOrigin)
{
  pMMIOFileDesc pMMIOHandle;
  int iTranslatedOrigin;
  WaWEPosition_t Result;

  if (!pHandle) return 0;

  if (DosRequestMutexSem(hmtxUsePlugin, SEM_INDEFINITE_WAIT) != NO_ERROR)
  {
#ifdef DEBUG_BUILD
    printf("[plugin_Seek] : IMMIO : Could not get mutex semaphore!\n"); fflush(stdout);
#endif
    return 0;
  }

  if (pHandle->bTempClosed)
  {
    DosReleaseMutexSem(hmtxUsePlugin);
    return 0; // Hey, it's TempClosed!
  }

  pMMIOHandle = (pMMIOFileDesc)(pHandle->pPluginSpecific);
  if (!pMMIOHandle)
  {
    DosReleaseMutexSem(hmtxUsePlugin);
    return 0;
  }

  iTranslatedOrigin = SEEK_SET;
  if (iOrigin == WAWE_SEEK_CUR) iTranslatedOrigin = SEEK_CUR;
  if (iOrigin == WAWE_SEEK_END) iTranslatedOrigin = SEEK_END;

  Result = MMIOSeek(pMMIOHandle, Offset, iTranslatedOrigin);
  DosReleaseMutexSem(hmtxUsePlugin);

  return Result;
}

WaWESize_t            WAWECALL plugin_Read(WaWEImP_Open_Handle_p pHandle,
                                           char *pchBuffer,
                                           WaWESize_t cBytes)
{
  unsigned long ulReaded;
  pMMIOFileDesc pMMIOHandle;

  if (!pHandle) return 0;

  if (DosRequestMutexSem(hmtxUsePlugin, SEM_INDEFINITE_WAIT) != NO_ERROR)
  {
#ifdef DEBUG_BUILD
    printf("[plugin_Read] : IMMIO : Could not get mutex semaphore!\n"); fflush(stdout);
#endif
    return 0;
  }

  if (pHandle->bTempClosed)
  {
    DosReleaseMutexSem(hmtxUsePlugin);
    return 0; // Hey, it's TempClosed!
  }

  pMMIOHandle = (pMMIOFileDesc)(pHandle->pPluginSpecific);
  if (!pMMIOHandle)
  {
    DosReleaseMutexSem(hmtxUsePlugin);
    return 0;
  }

  MMIODecode(pMMIOHandle, pchBuffer, cBytes, &ulReaded);

  DosReleaseMutexSem(hmtxUsePlugin);

  return ulReaded;
}

long                  WAWECALL plugin_Close(WaWEImP_Open_Handle_p pHandle)
{
  pMMIOFileDesc pMMIOHandle;

  if (!pHandle) return 0;

  if (DosRequestMutexSem(hmtxUsePlugin, SEM_INDEFINITE_WAIT) != NO_ERROR)
  {
#ifdef DEBUG_BUILD
    printf("[plugin_Close] : IMMIO : Could not get mutex semaphore!\n"); fflush(stdout);
#endif
    return 0;
  }

  pMMIOHandle = (pMMIOFileDesc)(pHandle->pPluginSpecific);
  if (!(pHandle->bTempClosed))
  {
    if (pMMIOHandle)
      MMIOClose(pMMIOHandle);
  }

  // Free the WaWE file handle
  free(pHandle);

  DosReleaseMutexSem(hmtxUsePlugin);

  return 1;
}

long                  WAWECALL plugin_TempClose(WaWEImP_Open_Handle_p pHandle)
{
  pMMIOFileDesc pMMIOHandle;

  if (!pHandle) return 0;

  if (DosRequestMutexSem(hmtxUsePlugin, SEM_INDEFINITE_WAIT) != NO_ERROR)
  {
#ifdef DEBUG_BUILD
    printf("[plugin_TempClose] : IMMIO : Could not get mutex semaphore!\n"); fflush(stdout);
#endif
    return 0;
  }

  if (pHandle->bTempClosed)
  {
#ifdef DEBUG_BUILD
    printf("[plugin_TempClose] : Hm, already tempclosed!\n");
#endif
    DosReleaseMutexSem(hmtxUsePlugin);
    return 1; // No need to do anything, it's already TempClosed.
  }

  pMMIOHandle = (pMMIOFileDesc)(pHandle->pPluginSpecific);
  if (!pMMIOHandle)
  {
    DosReleaseMutexSem(hmtxUsePlugin);
    return 0;
  }

  MMIOClose(pMMIOHandle);
  pHandle->bTempClosed = 1; // Note that it's temporarily closed

  DosReleaseMutexSem(hmtxUsePlugin);

  return 1;
}

long                  WAWECALL plugin_ReOpen(char *pchFileName, WaWEImP_Open_Handle_p pHandle)
{
  pMMIOFileDesc pMMIOHandle;
  char achTemp[16];

  if (!pHandle) return 0;

  if (DosRequestMutexSem(hmtxUsePlugin, SEM_INDEFINITE_WAIT) != NO_ERROR)
  {
#ifdef DEBUG_BUILD
    printf("[plugin_ReOpen] : IMMIO : Could not get mutex semaphore!\n"); fflush(stdout);
#endif
    return 0;
  }

  if (!(pHandle->bTempClosed))
  {
    DosReleaseMutexSem(hmtxUsePlugin);
    return 1; // No need to do anything, it's already opened.
  }

  pMMIOHandle = MMIOOpen(pchFileName, TRUE);
  pHandle->pPluginSpecific =  pMMIOHandle;

  if (!pMMIOHandle)
  {
    DosReleaseMutexSem(hmtxUsePlugin);
    return 0;
  }

  // Update file info stuff
  pHandle->iFrequency = pMMIOHandle->ulSamplesPerSec;
  pHandle->iChannels = pMMIOHandle->usChannels;
  pHandle->iBits = pMMIOHandle->usBitsPerSample;
  if (pHandle->iBits==8)
    pHandle->iIsSigned = 0;
  else
    pHandle->iIsSigned = 1;

  sprintf(achTemp, "%.4s", &(pMMIOHandle->fccType));
  PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pHandle->ChannelSetFormat),
                                                        "FourCC", achTemp);

  pHandle->bTempClosed = 0; // Note that it's not closed temporarily anymore!

  DosReleaseMutexSem(hmtxUsePlugin);

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

  // Set plugin importance to average
  pPluginDesc->iPluginImportance = (WAWE_PLUGIN_IMPORTANCE_MAX + WAWE_PLUGIN_IMPORTANCE_MIN)/2;

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
