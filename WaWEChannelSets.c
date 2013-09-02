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
 * Code dealing with list of channel-sets                                    *
 *****************************************************************************/

#include "WaWE.h"

HMTX ChannelSetListProtector_Sem = -1;
WaWEChannelSet_p ChannelSetListHead = NULL;
WaWEChannelSet_p pLastChannelSet = NULL;
int  iNextChannelSetID;
int  bChannelSetListChanged = 0;

int GetNumOfChannelsFromChannelSet(WaWEChannelSet_p pChannelSet)
{
  int iResult;
  WaWEChannel_p pChannel;

  if (!pChannelSet) return 0;

  iResult = 0;
  pChannel = pChannelSet->pChannelListHead;
  while (pChannel)
  {
    iResult++;
    pChannel = pChannel->pNext;
  }

  return iResult;
}

WaWEChannelSet_p GetChannelSetFromFileName(char *pchFileName)
{
  WaWEChannelSet_p pChannelSet = NULL;

  if (!pchFileName) return NULL;

  if (DosRequestMutexSem(ChannelSetListProtector_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    pChannelSet = ChannelSetListHead;
    while ((pChannelSet) && (stricmp(pChannelSet->achFileName, pchFileName)))
      pChannelSet = pChannelSet->pNext;

    // Return the result
    DosReleaseMutexSem(ChannelSetListProtector_Sem);
  }

  return pChannelSet;
}

// Therea are some redraws not needed when we're called from Open dialog.
// This function is called from the Open dialog, from the Worker thread.
// The opening itself puts a lot of DrawChannel messages into the queue of the
// Worker thread, no need for more...
// Change: Now that there are two separate worker threads, we *do* need these
// extra redraws, because the screen-updater-worker thread processes the
// channeldraw requests by the end of this, so we have to tell it again to
// redraw now that we've fully opened the channel-set!
//#define OPTIMIZE_REDRAW_AT_OPEN

WaWEChannelSet_p CreateChannelSetFromOpenHandle(char *pchFileName,          // The filename
                                                WaWEPlugin_p pPlugin,       // The plugin which opened it
                                                WaWEImP_Open_Handle_p hFile,// The handle returned by the plugin
                                                int bRedrawNow)             // If True, then a screen redraw will be called
{
  WaWEChannelSet_p result;
  WaWEFormat_t OriginalFormat;
  WaWEChannel_p pChannel;
  WaWESize_t ChannelLength;
  int iChannelNum;
  int rc;

  OriginalFormat.iFrequency = hFile->iFrequency;
  OriginalFormat.iBits      = hFile->iBits;
  OriginalFormat.iIsSigned  = hFile->iIsSigned;

  rc = WaWEHelper_Global_CreateChannelSet(&result,
                                          &OriginalFormat,
                                          hFile->iChannels,
                                          hFile->pchProposedChannelSetName,
                                          pchFileName);
  if (rc!=WAWEHELPER_NOERROR)
  {
    return result; // If all goes well, it's already NULL
  }

  // Ok, an empty ChannelSet has been created, now
  // fill it with data!
  if (DosRequestMutexSem(ChannelSetListProtector_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    // Save plugin and file handle, used to create the channel, to be able
    // to close file handle later!
    result->pOriginalPlugin = pPlugin;
    result->hOriginalFile   = hFile;

    // Initialize read-cache
    result->ChannelSetCache.bUseCache = WaWEConfiguration.bUseCache;
    result->ChannelSetCache.uiCachePageSize = WaWEConfiguration.uiCachePageSize;
    result->ChannelSetCache.uiNumOfCachePages = WaWEConfiguration.uiNumOfCachePages;
    result->ChannelSetCache.FileSize = 0; // Will be filled later!
    if (result->ChannelSetCache.bUseCache)
    {
      // Initialize cache pages
      result->ChannelSetCache.CachePages =
        (WaWEChannelSetCachePage_p) dbg_malloc(sizeof(WaWEChannelSetCachePage_t) * result->ChannelSetCache.uiNumOfCachePages);

      if (!result->ChannelSetCache.CachePages)
      {
#ifdef DEBUG_BUILD
        printf("[CreateChannelSetFromOpenHandle] : Out of memory, cache disabled for this channel-set!\n");
#endif
        result->ChannelSetCache.bUseCache = FALSE;
      } else
      {
        unsigned int uiTemp;
        WaWEChannelSetCachePage_p pCachePages;

        pCachePages = result->ChannelSetCache.CachePages;
        // Initialize cache-pages to be empty
        for (uiTemp = 0; uiTemp<result->ChannelSetCache.uiNumOfCachePages; uiTemp++)
        {
          pCachePages[uiTemp].PageNumber = -1;
          pCachePages[uiTemp].uiNumOfBytesOnPage = 0;
          pCachePages[uiTemp].puchCachedData = NULL;
        }
      }
    } else
    {
      // No cache pages to use, so set cache pages to NULL
      result->ChannelSetCache.CachePages = NULL;
    }

    // Move (!!) the channel-set format pointer to channel-set level from
    // file handle level. This makes it sure that bad import plugins
    // will not free it at close-time, and we won't try to free it again
    // when destroying the channel-set structure.
    WaWEHelper_ChannelSet_StartChSetFormatUsage(result);
    result->ChannelSetFormat = hFile->ChannelSetFormat;
    hFile->ChannelSetFormat = NULL;
    WaWEHelper_ChannelSet_StopChSetFormatUsage(result, TRUE);
    // Remove "modified" flag from channel-set!
    result->bModified = FALSE;

    // Calculate number of samples in one channel!
    ChannelLength = pPlugin->TypeSpecificInfo.ImportPluginInfo.fnSeek(hFile, 0, WAWE_SEEK_END);
    ChannelLength++; // Last position + 1 will mean the number of bytes in file.
    result->ChannelSetCache.FileSize = ChannelLength;
#ifdef DEBUG_BUILD
    printf("FileSize = %d\n", ChannelLength);
#endif
    pPlugin->TypeSpecificInfo.ImportPluginInfo.fnSeek(hFile, 0, WAWE_SEEK_SET);
    ChannelLength /= (result->OriginalFormat.iBits/8);
    ChannelLength /= hFile->iChannels;
    result->Length = ChannelLength;
#ifdef DEBUG_BUILD
    printf("ChannelLength = %d\n", ChannelLength);
#endif

    // Setup the list of channels inside the channel-set!
    pChannel = result->pChannelListHead;
    iChannelNum = 0;
    while ((pChannel) && (iChannelNum<hFile->iChannels))
    {
      char *pchProposedName;

      if ((hFile->apchProposedChannelName) &&
          (hFile->apchProposedChannelName[iChannelNum]))
        pchProposedName = hFile->apchProposedChannelName[iChannelNum];
      else
        pchProposedName = NULL;

      // Set initial values of channel
      memcpy(&(pChannel->VirtualFormat), &(result->OriginalFormat), sizeof(pChannel->VirtualFormat));

      if (pchProposedName)
        strncpy(pChannel->achName, pchProposedName, sizeof(pChannel->achName));

      pChannel->Length = ChannelLength;
      pChannel->FirstSamplePos = 0;
      pChannel->LastSamplePos = result->iWidth-1;

      if (DosRequestMutexSem(pChannel->hmtxUseChunkList, SEM_INDEFINITE_WAIT)==NO_ERROR)
      {

#ifdef DEBUG_BUILD
        if (pChannel->pChunkListHead)
        {
          printf("Warning: Empty channel already has a chunk!!!\n"); fflush(stdout);
        }
#endif
        pChannel->pChunkListHead = (WaWEChunk_p) dbg_malloc(sizeof(WaWEChunk_t));
        if (pChannel->pChunkListHead)
        {
          // At the beginning, the channel will contain one big chunk.
          pChannel->pChunkListHead->iChunkType = WAWE_CHUNKTYPE_ORIGINAL;
          pChannel->pChunkListHead->VirtPosStart = 0;
          pChannel->pChunkListHead->VirtPosEnd = ChannelLength-1;
#ifdef DEBUG_BUILD
          printf("Chunk is from %d to %d\n", 0, ChannelLength-1);
#endif
          pChannel->pChunkListHead->pPlugin = pPlugin;
          pChannel->pChunkListHead->hFile = hFile;
          pChannel->pChunkListHead->iChannelNum = iChannelNum;
          pChannel->pChunkListHead->RealPosStart = 0;
          pChannel->pChunkListHead->pSampleBuffer = NULL;
          pChannel->pChunkListHead->pPrev = NULL;
          pChannel->pChunkListHead->pNext = NULL;
        }

        DosReleaseMutexSem(pChannel->hmtxUseChunkList);
      }
#ifndef OPTIMIZE_REDRAW_AT_OPEN
      // Note that the channel has been changed, so
      // redraw the channel.
      pChannel->bRedrawNeeded = 1;
#endif

      iChannelNum++;
      pChannel = pChannel->pNext;
    }

    // Notify that the channel-set has been changed
    // This will update channel-set length
    result->bRedrawNeeded = 1;

    // Return the result
    DosReleaseMutexSem(ChannelSetListProtector_Sem);

    if (bRedrawNow)
    {
      // Redraw everything that's changed!
      RedrawChangedChannelsAndChannelSets();
    }
  }

  return result;
}

int UpdateReOpenedChannelSet(WaWEChannelSet_p pChannelSet, int bRedrawNow)
{
  WaWEPlugin_p pPlugin;
  WaWEImP_Open_Handle_p  hFile;
  WaWEChannel_p pChannel;
  WaWESize_t ChannelLength;
  int iChannelNum;
  int bOldModifiedFlag;
  int rc;

  pPlugin = pChannelSet->pOriginalPlugin;
  hFile = pChannelSet->hOriginalFile;

  pChannelSet->OriginalFormat.iFrequency = hFile->iFrequency;
  pChannelSet->OriginalFormat.iBits      = hFile->iBits;
  pChannelSet->OriginalFormat.iIsSigned  = hFile->iIsSigned;

  bOldModifiedFlag = pChannelSet->bModified;
  WaWEHelper_ChannelSet_StartChSetFormatUsage(pChannelSet);
  WaWEHelper_ChannelSetFormat_DeleteAllKeys(&(pChannelSet->ChannelSetFormat));
  pChannelSet->ChannelSetFormat = hFile->ChannelSetFormat;
  hFile->ChannelSetFormat = NULL;
  WaWEHelper_ChannelSet_StopChSetFormatUsage(pChannelSet, TRUE);
  pChannelSet->bModified = bOldModifiedFlag;

  // Make the channel-set cache empty, if turned on!
  if (pChannelSet->ChannelSetCache.bUseCache)
  {
    WaWEChannelSetCachePage_p pCachePages;
    unsigned int uiTemp;

    pCachePages = pChannelSet->ChannelSetCache.CachePages;

    for (uiTemp=0; uiTemp<pChannelSet->ChannelSetCache.uiNumOfCachePages; uiTemp++)
    {
      if (pCachePages->puchCachedData)
      {
        dbg_free(pCachePages->puchCachedData);
        pCachePages->puchCachedData = NULL;
      }
      pCachePages->uiNumOfBytesOnPage = 0;
      pCachePages->PageNumber = -1;
    }
  }

  // Calculate number of samples in one channel!
  ChannelLength = pPlugin->TypeSpecificInfo.ImportPluginInfo.fnSeek(hFile, 0, WAWE_SEEK_END);
  ChannelLength++; // Last position + 1 will mean the number of bytes in file.
  pChannelSet->ChannelSetCache.FileSize = ChannelLength; // Also update cache!
#ifdef DEBUG_BUILD
  printf("[UpdateReOpenedChannelSet] : Enter\n");
  printf("FileSize = %d\n", ChannelLength);
#endif
  pPlugin->TypeSpecificInfo.ImportPluginInfo.fnSeek(hFile, 0, WAWE_SEEK_SET);
  ChannelLength /= (pChannelSet->OriginalFormat.iBits/8);
  ChannelLength /= hFile->iChannels;
  pChannelSet->Length = ChannelLength;

#ifdef DEBUG_BUILD
  printf("ChannelLength = %d\n", ChannelLength);
  printf("Number of channels = %d\n", hFile->iChannels);
#endif

  // Setup the list of channels inside the channel-set!
  pChannel = pChannelSet->pChannelListHead;
  iChannelNum = 0;
  while ((pChannel) && (iChannelNum<hFile->iChannels))
  {
#ifdef DEBUG_BUILD
    printf("Updating channel %p (Channel num %d)\n", pChannel, hFile->iChannels); fflush(stdout);
#endif

    memcpy(&(pChannel->VirtualFormat), &(pChannelSet->OriginalFormat), sizeof(pChannel->VirtualFormat));

    pChannel->Length = ChannelLength;

    if (DosRequestMutexSem(pChannel->hmtxUseChunkList, SEM_INDEFINITE_WAIT)==NO_ERROR)
    {
      WaWEChunk_p pToDelete;

      while (pChannel->pChunkListHead)
      {
        pToDelete = pChannel->pChunkListHead;
        pChannel->pChunkListHead = pChannel->pChunkListHead->pNext;

        if (pToDelete->iChunkType == WAWE_CHUNKTYPE_MODIFIED)
        {
          // Delete the modified chunk
          if (pToDelete->pSampleBuffer)
          {
            dbg_free(pToDelete->pSampleBuffer); pToDelete->pSampleBuffer = NULL;
          }
        } else
        {
          // Delete the original chunk
          // Nothing special to do here, anyway, we'll recreate one original chunk.
        }
        dbg_free(pToDelete);
      }

      pChannel->pChunkListHead = (WaWEChunk_p) dbg_malloc(sizeof(WaWEChunk_t));
      if (pChannel->pChunkListHead)
      {
        // At the beginning, the channel will contain one big chunk.
        pChannel->pChunkListHead->iChunkType = WAWE_CHUNKTYPE_ORIGINAL;
        pChannel->pChunkListHead->VirtPosStart = 0;
        pChannel->pChunkListHead->VirtPosEnd = ChannelLength-1;
#ifdef DEBUG_BUILD
        printf("Chunk is from %d to %d\n", 0, ChannelLength-1);
#endif
        pChannel->pChunkListHead->pPlugin = pPlugin;
        pChannel->pChunkListHead->hFile = hFile;
        pChannel->pChunkListHead->iChannelNum = iChannelNum;
        pChannel->pChunkListHead->RealPosStart = 0;
        pChannel->pChunkListHead->pSampleBuffer = NULL;
        pChannel->pChunkListHead->pPrev = NULL;
        pChannel->pChunkListHead->pNext = NULL;
      }

      DosReleaseMutexSem(pChannel->hmtxUseChunkList);
    }

#ifndef OPTIMIZE_REDRAW_AT_OPEN
    // Note that the channel has been changed, so
    // redraw the channel.
    pChannel->bRedrawNeeded = 1;
#endif

    iChannelNum++;
    pChannel = pChannel->pNext;
  }

  // Ok, out of loop.
  // Now, delete every extra channel which remained in channel-set!
  while (pChannel)
  {
    WaWEChannel_p pToDelete;
    pToDelete = pChannel;
    pChannel = pChannel->pNext;
#ifdef DEBUG_BUILD
    printf("Deleting old channel %p\n", pToDelete);
#endif
    WaWEHelper_ChannelSet_DeleteChannel(pToDelete);
  }
  // Now, create every extra channel which appeared in file!
  while (iChannelNum<hFile->iChannels)
  {
#ifdef DEBUG_BUILD
    printf("Creating new channel num %d\n", hFile->iChannels);
#endif

    rc = WaWEHelper_ChannelSet_CreateChannel(pChannelSet,
                                             WAWECHANNEL_BOTTOM,
                                             NULL,
                                             &pChannel);
    if (rc==WAWEHELPER_NOERROR)
    {
      // Fill new channel with info!
      char *pchProposedName;

      if ((hFile->apchProposedChannelName) &&
          (hFile->apchProposedChannelName[iChannelNum]))
        pchProposedName = hFile->apchProposedChannelName[iChannelNum];
      else
        pchProposedName = NULL;

      // Set initial values of channel
      memcpy(&(pChannel->VirtualFormat), &(pChannelSet->OriginalFormat), sizeof(pChannel->VirtualFormat));

      if (pchProposedName)
        strncpy(pChannel->achName, pchProposedName, sizeof(pChannel->achName));

      pChannel->Length = ChannelLength;
      if (pChannelSet->pChannelListHead)
      {
        // Copy settings from other channels
        pChannel->FirstSamplePos = pChannelSet->pChannelListHead->FirstSamplePos;
        pChannel->LastSamplePos = pChannelSet->pChannelListHead->LastSamplePos;
      } else
      {
        pChannel->FirstSamplePos = 0;
        pChannel->LastSamplePos = pChannelSet->iWidth-1;
      }

      if (DosRequestMutexSem(pChannel->hmtxUseChunkList, SEM_INDEFINITE_WAIT)==NO_ERROR)
      {

        WaWEChunk_p pToDelete;
  
        while (pChannel->pChunkListHead)
        {
          pToDelete = pChannel->pChunkListHead;
          pChannel->pChunkListHead = pChannel->pChunkListHead->pNext;
  
          if (pToDelete->pSampleBuffer)
            dbg_free(pToDelete->pSampleBuffer);
          dbg_free(pToDelete);
        }

        pChannel->pChunkListHead = (WaWEChunk_p) dbg_malloc(sizeof(WaWEChunk_t));
        if (pChannel->pChunkListHead)
        {
          // At the beginning, the channel will contain one big chunk.
          pChannel->pChunkListHead->iChunkType = WAWE_CHUNKTYPE_ORIGINAL;
          pChannel->pChunkListHead->VirtPosStart = 0;
          pChannel->pChunkListHead->VirtPosEnd = ChannelLength-1;
#ifdef DEBUG_BUILD
          printf("Chunk is from %d to %d\n", 0, ChannelLength-1);
#endif
          pChannel->pChunkListHead->pPlugin = pPlugin;
          pChannel->pChunkListHead->hFile = hFile;
          pChannel->pChunkListHead->iChannelNum = iChannelNum;
          pChannel->pChunkListHead->RealPosStart = 0;
          pChannel->pChunkListHead->pSampleBuffer = NULL;
          pChannel->pChunkListHead->pPrev = NULL;
          pChannel->pChunkListHead->pNext = NULL;
        }

        DosReleaseMutexSem(pChannel->hmtxUseChunkList);
      }
#ifndef OPTIMIZE_REDRAW_AT_OPEN
      // Note that the channel has been changed, so
      // redraw the channel.
      pChannel->bRedrawNeeded = 1;
#endif
    }
    iChannelNum++;
  }

  // Notify that the channel-set has been changed
  // This will update channel-set length
  pChannelSet->bRedrawNeeded = 1;

  if (bRedrawNow)
  {
    // Redraw everything that's changed!
    RedrawChangedChannelsAndChannelSets();
  }
#ifdef DEBUG_BUILD
  printf("[UpdateReOpenedChannelSet] : Leave\n");
#endif
  return 1;
}


int DeleteChannelSet(WaWEChannelSet_p pChannelSet)
{
  int rc;

  rc = WaWEHelper_Global_DeleteChannelSet(pChannelSet);

  return (rc==WAWEHELPER_NOERROR);
}

int GetNumOfModifiedChannelSets()
{
  int iResult = 0;
  WaWEChannelSet_p pChannelSet;

  if (DosRequestMutexSem(ChannelSetListProtector_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    pChannelSet = ChannelSetListHead;
    while (pChannelSet)
    {
      if (pChannelSet->bModified) iResult++;
      pChannelSet = pChannelSet->pNext;
    }
    DosReleaseMutexSem(ChannelSetListProtector_Sem);
  }
  return iResult;
}

// Try to read 128K chunks
#define READ_BUF_SIZE (128*1024)

int SaveChannelSetToOpenHandle(WaWEChannelSet_p pChannelSet, WaWEPlugin_p pPlugin, void *hFile, HWND hwndSavingDlg)
{
  char *pchReadBuffer;
  int   iBytesPerSample;
  int   iNumOfChannels;
  WaWEPosition_t  CurrPos;
  WaWESize_t      SamplesToRead;
  WaWESize_t      ReadedSamples;
  int rc;

  printf("[SaveChannelSetToOpenHandle] : Enter\n");

  pchReadBuffer = (char *) dbg_malloc(READ_BUF_SIZE);
  if (!pchReadBuffer)
  {
    printf("[SaveChannelSetToOpenHandle] : Out of memory to allocate pchReadBuffer!\n");
    return 0;
  }

  iNumOfChannels = GetNumOfChannelsFromChannelSet(pChannelSet);
  CurrPos = 0;
  iBytesPerSample = (pChannelSet->OriginalFormat.iBits + 7)/8;
  SamplesToRead = READ_BUF_SIZE / iBytesPerSample / iNumOfChannels;
  while (CurrPos<pChannelSet->Length)
  {
    ReadedSamples = 0;
    WaWEHelper_ChannelSet_ReadPackedSamples(pChannelSet,
                                            CurrPos,
                                            SamplesToRead,
                                            pchReadBuffer,
                                            &(pChannelSet->OriginalFormat),
                                            &ReadedSamples);
    if (ReadedSamples==0) break;
    CurrPos+=ReadedSamples;

    if (pChannelSet->Length>0)
    {
#ifdef DEBUG_BUILD
//      printf("[SaveChannelSetToOpenHandle] : Progress: %d%%\n", CurrPos * 100 / pChannelSet->Length);
#endif

      WinSendDlgItemMsg(hwndSavingDlg, DID_SLIDER, SLM_SETSLIDERINFO,
                        MPFROM2SHORT(SMA_SLIDERARMPOSITION, SMA_INCREMENTVALUE),
                        (MPARAM) (CurrPos * 99 / pChannelSet->Length));
    }

    rc = pPlugin->TypeSpecificInfo.ExportPluginInfo.fnWrite(hFile, pchReadBuffer, ReadedSamples*iBytesPerSample*iNumOfChannels);
    if (!rc)
    {
      dbg_free(pchReadBuffer);
      printf("[SaveChannelSetToOpenHandle] : Error from fnWrite() of export plugin!\n");
      return 0;
    }
  }

  dbg_free(pchReadBuffer);

  printf("[SaveChannelSetToOpenHandle] : Done\n");
  return 1; // Success
}

int Initialize_ChannelSetList()
{
  int rc;

  if (ChannelSetListProtector_Sem != -1) return TRUE;

  ChannelSetListHead = NULL;
  pLastChannelSet = NULL;
  iNextChannelSetID = 1;
  bChannelSetListChanged = 0;

  rc = DosCreateMutexSem(NULL, &ChannelSetListProtector_Sem, 0, FALSE);
  return (rc == NO_ERROR);
}

void EraseChannelSetList()
{
  while (ChannelSetListHead)
  {
#ifdef DEBUG_BUILD
    printf("Erasing channel set: %s\n", ChannelSetListHead->achName);
#endif
    DeleteChannelSet(ChannelSetListHead);
  }
}

void Uninitialize_ChannelSetList()
{
  if (ChannelSetListProtector_Sem==-1) return;

  EraseChannelSetList();

  DosCloseMutexSem(ChannelSetListProtector_Sem); ChannelSetListProtector_Sem = -1;
}
