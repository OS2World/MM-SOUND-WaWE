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
 * Helper functions for plugins                                              *
 *****************************************************************************/

#include "WaWE.h"

// To debug the read-cache, define this, so
// requests to read biiiig chunks (for example from
// clipboard plugin) will result in reading biiig
// chunks from read-cache. Otherwise big chunks
// will be cutted into smaller ones to save
// memory.
//#define DONT_OPTIMIZE_READSAMPLE_FOR_MEMORY

// If the following is defined, its value is used as a maximum
// number of samples of a modified chunk. If a modified chunk would
// be bigger than this, it will be splitted to more smaller chunks.
#define MAX_MODIFIED_CHUNK_SAMPLES  65536

static void PMThread_WinDestroyWindow(HWND hwnd)
{
  // We must make sure that the window will be destroyed from the
  // same thread which has created it, because WinDestroyWindow() only
  // works from that thread. Sigh.

#ifdef DEBUG_BUILD
  printf("  Asking PM-Thread to destroy window (0x%x)\n", hwnd); fflush(stdout);
#endif
  WinSendMsg(hwndClient, WM_DESTROYWINDOW, (MPARAM) (hwnd), 0);
}

// This stuff returns the current TID
static unsigned long GetCurrentThreadID(void)
{
  PTIB tib;
  DosGetInfoBlocks(&tib, NULL);
  return((unsigned long) (tib->tib_ptib2->tib2_ultid));
}

// This function checks if a string starts with another, but case insensitive.
static int strstartswith(char *pchString, char *pchSubString)
{
  int iLen1, iLen2, i;
  unsigned char c1, c2;

  if ((!pchString) || (!pchSubString))
    return 0;

  iLen1 = strlen(pchString);
  iLen2 = strlen(pchSubString);

  if (iLen2>iLen1) return 0;

  for (i=0; i<iLen2; i++)
  {
    c1 = pchString[i];
    c2 = pchSubString[i];
    if( c1 >= 'A'  &&  c1 <= 'Z' )  c1 += 'a' - 'A';
    if( c2 >= 'A'  &&  c2 <= 'Z' )  c2 += 'a' - 'A';
    if( c1 != c2 ) return 0;
  }

  return 1;
}



// This stuff goes through all chunks, and checks for overlapped ones.
// Repairs them, if finds any!
// (Repairing means, that the previous chunk is treated to be good, so it
//  cuts the beginning of the chunk to be after the prev chunk.)
static void internal_RepairOverlappedChunks(WaWEChannel_p pChannel, WaWEChunk_p pChunk)
{
  while (pChunk)
  {
    WaWEChunk_p pPrevChunk = pChunk->pPrev;

    if (pPrevChunk)
    {
      if (pChunk->VirtPosStart<=pPrevChunk->VirtPosEnd)
      {
        WaWEPosition_t Shift;

        /* The previous chunk overlaps this! So make this chunk smaller! */

        /* Count how much we have to shift on its startpos! */
        Shift = pPrevChunk->VirtPosEnd - pChunk->VirtPosStart + 1;

        /* Check if the whole chunk has to be destroyed, or just modified! */
        if (pChunk->VirtPosStart + Shift > pChunk->VirtPosEnd)
        {
          /* This chunk has been fully overlapped! */
          /* So, destroy this chunk! */
          WaWEChunk_p pNextChunk;

          pNextChunk = pChunk->pNext;
          pPrevChunk->pNext = pChunk->pNext;
          if (pNextChunk)
            pNextChunk->pPrev = pPrevChunk;


          if (pChunk->iChunkType == WAWE_CHUNKTYPE_MODIFIED)
          {
            if (pChunk->pSampleBuffer)
              dbg_free(pChunk->pSampleBuffer);
          }

          dbg_free(pChunk);
          pChunk = pPrevChunk;

        } else
        {
          /* This chunk has to be modified to be smaller! */
          if (pChunk->iChunkType == WAWE_CHUNKTYPE_ORIGINAL)
          {
            /* This chunk is an ORIGINAL chunk */

            pChunk->VirtPosStart += Shift;
            pChunk->RealPosStart += Shift*
              (pChunk->hFile->iBits/8) *   /* == iBytesPerSample */
              (pChunk->hFile->iChannels);  /* == iNumOfChannels */
          } else
          {
            /* This chunk is a MODIFIED chunk */
            WaWESample_p pNewSampleBuffer;
            WaWESize_t NewSize;

            pChunk->VirtPosStart += Shift;

            NewSize = (pChunk->VirtPosEnd - pChunk->VirtPosStart + 1);

            memcpy(pChunk->pSampleBuffer,
                   pChunk->pSampleBuffer+Shift,
                   NewSize * sizeof(WaWESample_t));

            pNewSampleBuffer = dbg_realloc(pChunk->pSampleBuffer,
                                           NewSize * sizeof(WaWESample_t));

            if (pNewSampleBuffer)
              pChunk->pSampleBuffer = pNewSampleBuffer;
          }
        }
        pChannel->bRedrawNeeded = 1; // Note that channel data changed, redraw needed!
      }
    }
    pChunk = pChunk->pNext;
  }
}

// This stuff goes through all chunks, and deletes invalid (zero length) chunks!
static void internal_DeleteInvalidChunks(WaWEChannel_p pChannel)
{
  WaWEChunk_p pChunk, pNextChunk, pPrevChunk;

  pChunk = pChannel->pChunkListHead;

  while (pChunk)
  {
    if (pChunk->VirtPosStart>pChunk->VirtPosEnd)
    {
      pNextChunk = pChunk->pNext;
      pPrevChunk = pChunk->pPrev;
      if (pPrevChunk)
	pPrevChunk->pNext = pNextChunk;
      else
	pChannel->pChunkListHead = pNextChunk;

      if (pNextChunk)
	pNextChunk->pPrev = pPrevChunk;

      if (pChunk->iChunkType == WAWE_CHUNKTYPE_MODIFIED)
      {
	if (pChunk->pSampleBuffer)
	  dbg_free(pChunk->pSampleBuffer);
      }

      dbg_free(pChunk);
      pChunk = pNextChunk;
    } else
      pChunk = pChunk->pNext;
  }
}

// The following checks a list of chunks for neightbourous chunks with the same type,
// and concatenates them, if possible!
// Update: (See definition of MAX_MODIFIED_CHUNK_SAMPLES)
// It has been extended to make it that way, that modified chunks will never
// be bigger than a given value. It splits them if it has to. This makes it sure that
// the modified chunk will never be one big chunk, so inserting into it will not
// require moving megs of memory for one sample...

static void internal_ConcatenateNeightbourousChunks(WaWEChannel_p pChannel)
{
  WaWEChunk_p pChunk, pNextChunk;

  pChunk = pChannel->pChunkListHead;
  while (pChunk)
  {
#ifdef MAX_MODIFIED_CHUNK_SAMPLES
    // MAX_MODIFIED_CHUNK_SAMPLES is defined, so do the following:
    //  - Check for too big chunk, split to smaller ones if required
    //  - Check for neightbourous modified chunks, concatenate, if they
    //    will not be too big!
    if ((pChunk->iChunkType==WAWE_CHUNKTYPE_MODIFIED) &&
        ((pChunk->VirtPosEnd - pChunk->VirtPosStart + 1) > MAX_MODIFIED_CHUNK_SAMPLES))
    {
#ifdef DEBUG_BUILD
//      printf("[internal_ConcatenateNeightbourousChunks] : Too big modified chunk (%I64d), splitting! (0x%p)\n",
//             (pChunk->VirtPosEnd - pChunk->VirtPosStart + 1),
//             pChunk);
//      fflush(stdout);
#endif
      // Too many samples in this modified chunk, let's split it into smaller ones!
      while ((pChunk->VirtPosEnd - pChunk->VirtPosStart + 1) > MAX_MODIFIED_CHUNK_SAMPLES)
      {
        // Cut one part from the beginning
        WaWEChunk_p pNewChunk, pPrevChunk;
        WaWESample_p pNewSampleBuffer;

        pNewChunk = (WaWEChunk_p) dbg_malloc(sizeof(WaWEChunk_t));
        if (!pNewChunk)
          // Out of memory, cannot split this chunk.
          break;

        pNewSampleBuffer = (WaWESample_p) dbg_malloc_huge(MAX_MODIFIED_CHUNK_SAMPLES * sizeof(WaWESample_t));
        if (!pNewSampleBuffer)
        {
          // Out of memory, cannot split this chunk.
          dbg_free(pNewChunk);
          break;
        }

        // Ok, could allocate new memory!
        memcpy(pNewSampleBuffer,
               pChunk->pSampleBuffer,
               MAX_MODIFIED_CHUNK_SAMPLES * sizeof(WaWESample_t));
        // Prepare the new chunk
        pNewChunk->iChunkType = WAWE_CHUNKTYPE_MODIFIED;
        pNewChunk->pSampleBuffer = pNewSampleBuffer;
        pNewChunk->VirtPosStart = pChunk->VirtPosStart;
        pNewChunk->VirtPosEnd = pChunk->VirtPosStart + MAX_MODIFIED_CHUNK_SAMPLES-1;
        // Link the new chunk into list
        pNewChunk->pNext = pChunk;
        pPrevChunk = pNewChunk->pPrev = pChunk->pPrev;
        if (pPrevChunk)
          pPrevChunk->pNext = pNewChunk;
        else
          pChannel->pChunkListHead = pNewChunk;

        pChunk->pPrev = pNewChunk;
#ifdef DEBUG_BUILD
//        printf("[internal_ConcatenateNeightbourousChunks] : New chunk inserted (0x%p).\n", pNewChunk); fflush(stdout);
#endif

        // Now modify the old chunk to be smaller
        pChunk->VirtPosStart += MAX_MODIFIED_CHUNK_SAMPLES;
        memcpy(pChunk->pSampleBuffer,
               pChunk->pSampleBuffer + MAX_MODIFIED_CHUNK_SAMPLES,
               (pChunk->VirtPosEnd - pChunk->VirtPosStart + 1) * sizeof(WaWESample_t));

        pNewSampleBuffer = (WaWESample_p) dbg_realloc(pChunk->pSampleBuffer,
                                                      (pChunk->VirtPosEnd - pChunk->VirtPosStart + 1) * sizeof(WaWESample_t));
        if (pNewSampleBuffer)
          pChunk->pSampleBuffer = pNewSampleBuffer;

      }
#ifdef DEBUG_BUILD
//      printf("[internal_ConcatenateNeightbourousChunks] : Splitting done\n"); fflush(stdout);
#endif
    }
#endif

    pNextChunk = pChunk->pNext;
    if (pNextChunk)
    {
      // If it has a next chunk, we can check if we can concatenate them!
      // (based on the chunk type and the chunk type of the next chunk)
#ifdef DEBUG_BUILD
//      printf("[internal_ConcatenateNeightbourousChunks] : It has a next, check it!\n"); fflush(stdout);
#endif
      if (pChunk->iChunkType==WAWE_CHUNKTYPE_MODIFIED)
      {
        // Check if the neightbourous chunks are really neightbourous, and
        // concatenate them if needed!
        if ((pNextChunk->iChunkType == WAWE_CHUNKTYPE_MODIFIED) &&
            (pNextChunk->VirtPosStart == pChunk->VirtPosEnd+1)
#ifdef MAX_MODIFIED_CHUNK_SAMPLES
            // but only if they won't make a too big modified chunk!
            && ((pChunk->VirtPosEnd - pChunk->VirtPosStart + 1 +
                 pNextChunk->VirtPosEnd - pNextChunk->VirtPosStart + 1) <= MAX_MODIFIED_CHUNK_SAMPLES)

#endif
           )
        {
          /* Found a neightbourous modified chunk! */
          WaWESample_p pNewSampleBuffer;
          pNewSampleBuffer = (WaWESample_p) dbg_realloc(pChunk->pSampleBuffer,
                                                        ((pChunk->VirtPosEnd-pChunk->VirtPosStart+1) +
                                                         (pNextChunk->VirtPosEnd-pNextChunk->VirtPosStart+1)) * sizeof(WaWESample_t));
          if (pNewSampleBuffer)
          {
            /* Ok, could allocate new memory! */
            memcpy(pNewSampleBuffer + (pChunk->VirtPosEnd-pChunk->VirtPosStart+1),
                   pNextChunk->pSampleBuffer,
                   (pNextChunk->VirtPosEnd-pNextChunk->VirtPosStart+1) * sizeof(WaWESample_t));
            pChunk->pSampleBuffer = pNewSampleBuffer;
            pChunk->VirtPosEnd = pNextChunk->VirtPosEnd;
            pChunk->pNext = pNextChunk->pNext;
            dbg_free(pNextChunk->pSampleBuffer);
            dbg_free(pNextChunk);
            pNextChunk = pChunk->pNext;
            if (pNextChunk)
              pNextChunk->pPrev = pChunk;

//            pChannel->bRedrawNeeded = 1; // Note that channel data changed, redraw needed!
          } else
            pChunk = pChunk->pNext;
        } else
          pChunk = pChunk->pNext;

      } else
      {
        // pChunk->iChunkType==WAWE_CHUNKTYPE_ORIGINAL
        if ((pNextChunk->iChunkType == WAWE_CHUNKTYPE_ORIGINAL) &&
            (pNextChunk->VirtPosStart == pChunk->VirtPosEnd+1))
        {
          // Neightbourous Original chunks.
          // Let's see if they are really neightbourous, so their
          // real positions are after each other!
          if (pChunk->RealPosStart +
              (pChunk->VirtPosEnd-pChunk->VirtPosStart+1) *
              (pChunk->hFile->iBits/8) *
              (pChunk->hFile->iChannels) == pNextChunk->RealPosStart)
          {
#ifdef DEBUG_BUILD
//          printf("  Found really neightbourous Original chunks!\n"); fflush(stdout);
#endif
            pChunk->VirtPosEnd = pNextChunk->VirtPosEnd;
            pChunk->pNext = pNextChunk->pNext;
            dbg_free(pNextChunk);
            pNextChunk = pChunk->pNext;
            if (pNextChunk)
              pNextChunk->pPrev = pChunk;

//            pChannel->bRedrawNeeded = 1; // Note that channel data changed, redraw needed!
          } else
            pChunk = pChunk->pNext;

        } else
          pChunk = pChunk->pNext;
      }
    } else
      pChunk = pChunk->pNext;
  }
#ifdef DEBUG_BUILD
//  printf("[internal_ConcatenateNeightbourousChunks] : Return\n"); fflush(stdout);
#endif

}

static void internal_UpdateChannelsetLength(WaWEChannelSet_p pChannelSet)
{
  WaWEChannel_p pChannel;
  if (pChannelSet)
  {
    if (DosRequestMutexSem(pChannelSet->hmtxUseChannelList, SEM_INDEFINITE_WAIT)==NO_ERROR)
    {
      pChannel = pChannelSet->pChannelListHead;
      pChannelSet->Length = 0;
      while (pChannel)
      {
        if (pChannelSet->Length<pChannel->Length)
          pChannelSet->Length = pChannel->Length;

        pChannel = pChannel->pNext;
      }

      DosReleaseMutexSem(pChannelSet->hmtxUseChannelList);
    }
  }
}


#ifdef DEBUG_BUILD
/*
static void debug_ShowChunkList(WaWEChannel_p pChannel)
{
  WaWEChunk_p pChunk;

  if (!pChannel)
  {
    printf("[debug_ShowChunkList] : pChannel is NULL!\n");
    fflush(stdout);
    return;
  }

  if (!(pChannel->pChunkListHead))
  {
    printf("[debug_ShowChunkList] : ChunkList if EMPTY!\n");
    fflush(stdout);
    return;
  }

  printf("[debug_ShowChunkList] : List of chunks:\n");
  pChunk = pChannel->pChunkListHead;
  while (pChunk)
  {
    printf("  %07d -> %07d (Type: %s)",
           (int) (pChunk->VirtPosStart), (int) (pChunk->VirtPosEnd),
           (pChunk->iChunkType==WAWE_CHUNKTYPE_ORIGINAL)?"Original":"Modified");
    if (pChunk->iChunkType==WAWE_CHUNKTYPE_ORIGINAL)
      printf(" [RealPosStart = %07d]\n", (int) (pChunk->RealPosStart));
    else
      printf(" [pSampleBuffer = %08p]\n", pChunk->pSampleBuffer);

    pChunk = pChunk->pNext;
  }
  printf("[debug_ShowChunkList] : End\n");
  fflush(stdout);
}
*/
#endif

#define PROCESS_CACHE_PAGE(uiPage, iMarker) \
            if (pCachePages[uiPage].PageNumber==FirstPageNum)                                                                              \
            {                                                                                                                              \
              /* This is the first required page, so be careful to return */                                                               \
              /* only the required number of bytes! */                                                                                     \
                                                                                                                                           \
              if (pCachePages[uiPage].uiNumOfBytesOnPage >= uiReadFromFirstPageFrom+uiToReadFromFirstPage)                                 \
              {                                                                                                                            \
/*                printf("FirstPage@1(0x%p->0x%p)... ", */                                                                                 \
/*                       pCachePages[uiPage].puchCachedData + uiReadFromFirstPageFrom, */                                                  \
/*                       pchBuffer */                                                                                                      \
/*                      ); */                                                                                                              \
                                                                                                                                           \
                /* Ok, there is enough data on this page */                                                                                \
                memcpy(pchBuffer,                                                                                                          \
                       pCachePages[uiPage].puchCachedData + uiReadFromFirstPageFrom,                                                       \
                       uiToReadFromFirstPage);                                                                                             \
                Result += uiToReadFromFirstPage;                                                                                           \
              } else                                                                                                                       \
              {                                                                                                                            \
                /* Oooops, not enough data on this page!*/                                                                                 \
                unsigned int uiCanReadFromThisPage;                                                                                        \
                                                                                                                                           \
                printf("!!!!!!   FirstPage@2... !!!!!!!\n");                                                                               \
                                                                                                                                           \
                if (uiReadFromFirstPageFrom < pCachePages[uiPage].uiNumOfBytesOnPage)                                                      \
                {                                                                                                                          \
                  /* but we can take some... */                                                                                            \
                  uiCanReadFromThisPage =                                                                                                  \
                    pCachePages[uiPage].uiNumOfBytesOnPage - uiReadFromFirstPageFrom;                                                      \
                  memcpy(pchBuffer,                                                                                                        \
                         pCachePages[uiPage].puchCachedData + uiReadFromFirstPageFrom,                                                     \
                         uiCanReadFromThisPage);                                                                                           \
                  Result += uiCanReadFromThisPage;                                                                                         \
                }                                                                                                                          \
              }                                                                                                                            \
            } else                                                                                                                         \
            if (pCachePages[uiPage].PageNumber==FirstPageNum+NumOfPagesToGet-1)                                                            \
            {                                                                                                                              \
              /* This is the last required page, so be careful to return */                                                                \
              /* only the required number of bytes!                      */                                                                \
              if (pCachePages[uiPage].uiNumOfBytesOnPage >= uiToReadFromLastPage)                                                          \
              {                                                                                                                            \
/*                printf("LastPage@1(0x%p->0x%p)... ",                                                 */                                  \
/*                       pCachePages[uiPage].puchCachedData,                                           */                                  \
/*                       pchBuffer + cBytes - uiToReadFromLastPage                                     */                                  \
/*                       );                                                                            */                                  \
/*                printf("Ok, can read all the %d bytes from the last page\n", uiToReadFromLastPage);  */                                  \
                                                                                                                                           \
                /* Ok, there is enough data on this page */                                                                                \
                memcpy(pchBuffer + cBytes - uiToReadFromLastPage,                                                                          \
                       pCachePages[uiPage].puchCachedData,                                                                                 \
                       uiToReadFromLastPage);                                                                                              \
                Result += uiToReadFromLastPage;                                                                                            \
              } else                                                                                                                       \
              {                                                                                                                            \
                /* Oooops, not enough data on this page! */                                                                                \
                /* but we can take some... */                                                                                              \
                                                                                                                                           \
                printf("!!!!!!! LastPage@2... !!!!!!\n");                                                                                  \
/*                printf("Can read only %d bytes from the last page!\n", pCachePages[uiPage].uiNumOfBytesOnPage);*/                        \
                                                                                                                                           \
                memcpy(pchBuffer + cBytes - uiToReadFromLastPage,                                                                          \
                       pCachePages[uiPage].puchCachedData,                                                                                 \
                       pCachePages[uiPage].uiNumOfBytesOnPage);                                                                            \
                Result += pCachePages[uiPage].uiNumOfBytesOnPage;                                                                          \
              }                                                                                                                            \
            } else                                                                                                                         \
            {                                                                                                                              \
/*              printf("MidPage(0x%p->0x%p)... ",                      */                                                                  \
/*                     pCachePages[uiPage].puchCachedData,             */                                                                  \
/*                     pchBuffer + uiToReadFromFirstPage +             */                                                                  \
/*                       pCache->uiCachePageSize * (pCachePages[uiPage].PageNumber - FirstPageNum - 1) */                                  \
/*                    ); */                                                                                                                \
                                                                                                                                           \
              /* This is an intermediate page, so we have to return the*/                                                                  \
              /* whole page (if it's a full page).  */                                                                                     \
              memcpy(pchBuffer + uiToReadFromFirstPage + pCache->uiCachePageSize * (pCachePages[uiPage].PageNumber - FirstPageNum - 1),    \
                     pCachePages[uiPage].puchCachedData,                                                                                   \
                     pCachePages[uiPage].uiNumOfBytesOnPage);                                                                              \
              Result += pCachePages[uiPage].uiNumOfBytesOnPage;                                                                            \
            }                                                                                                                              \
                                                                                                                                           \
/*            printf("Marking"); */                                                                                                        \
/*            printf("(%I64d)... ", pCachePages[uiPage].PageNumber - FirstPageNum);*/                                                      \
                                                                                                                                           \
            /* Note that we've found one page! */                                                                                          \
            NumOfPagesGot++;                                                                                                               \
            /* Also mark the page we got! */                                                                                               \
            pchPagesGot[pCachePages[uiPage].PageNumber - FirstPageNum] = iMarker;                                                          \
                                                                                                                                           \
/*            printf("Rearrange cache... "); */                                                                                            \
                                                                                                                                           \
/*            printf("Done.\n"); */


// The following function implements cached reading from a channel-set, if the cache is
// turned on for the channel-set.
static WaWESize_t internal_SeekNRead(WaWEChannelSet_p pChannelSet,   // ChannelSet
                                     pfn_WaWEImP_Seek pfnSeek,       // Seek and
                                     pfn_WaWEImP_Read pfnRead,       // Read callbacks
                                     WaWEImP_Open_Handle_p pHandle,  // File handle for callbacks
                                     WaWEPosition_t ReadFrom,        // Position to seek to
                                     char *pchBuffer,                // Buffer to read into
                                     WaWESize_t cBytes)              // Number of bytes to read there
{
  WaWESize_t Result = 0;

  if ((!pfnSeek) || (!pfnRead) || (!cBytes)) return 0;

#ifdef DEBUG_BUILD
//  printf("[internal_SeekNRead] : Enter (cBytes=%I64d  ReadFrom=%I64d)\n", cBytes, ReadFrom);
#endif

  if (!pChannelSet)
  {
    // Something is wrong, invalid parameter!
    // Anyway, do something as if there would be no cache!
    pfnSeek(pHandle, ReadFrom, SEEK_SET);
    Result = pfnRead(pHandle, pchBuffer, cBytes);
  } else
  {
    // Fine, we have good parameters!

    // Get the semaphore to protect the list and to make seek and read atomic together!
    if (DosRequestMutexSem(pChannelSet->hmtxUseChannelSetCache, SEM_INDEFINITE_WAIT)!=NO_ERROR)
    {
#ifdef DEBUG_BUILD
      printf("[internal_SeekNRead] : Warning, could not get mutex semaphore!\n");
#endif
      // Something is wrong, could not get semaphore!
      // Anyway, do something as if there would be no cache!
      pfnSeek(pHandle, ReadFrom, SEEK_SET);
      Result = pfnRead(pHandle, pchBuffer, cBytes);
      return Result;
    }

    // Now, let's see if this channel-set is using cache, or not!
    // Do a simple SeekNRead if it's not cached!
    if ((pChannelSet->ChannelSetCache.bUseCache) && (pChannelSet->ChannelSetCache.CachePages))
    {
      // Fine, the channel-set is cached!
      // Now, go through the required pages, and
      // copy the data from cached pages, and/or read the data from non-available
      // pages.

      // First do the copy, so if a page would been dropped because of full
      // cache, it will be used first!

      WaWEChannelSetCache_p pCache = &(pChannelSet->ChannelSetCache);
      WaWEChannelSetCachePage_p pCachePages = pCache->CachePages;
      WaWEPosition_t FirstPageNum, LastPageNum;
      unsigned int   uiPage;
      WaWESize_t     NumOfPagesToGet, PageToLoad;
      char          *pchPagesGot;

      FirstPageNum = ReadFrom / pCache->uiCachePageSize;
      LastPageNum = (ReadFrom+cBytes-1) / pCache->uiCachePageSize;
      NumOfPagesToGet = LastPageNum - FirstPageNum + 1;
#ifdef DEBUG_BUILD
//      printf("Will need %I64d pages (from %I64d to %I64d)\n",
//             NumOfPagesToGet,
//             FirstPageNum,
//             LastPageNum);
#endif
      pchPagesGot = (char *) dbg_malloc(NumOfPagesToGet);
      if (!pchPagesGot)
      {
        // Out of memory. So, do a simple seek then read.
        pfnSeek(pHandle, ReadFrom, SEEK_SET);
        Result = pfnRead(pHandle, pchBuffer, cBytes);
      } else
      {
        WaWESize_t NumOfPagesGot;
        unsigned int uiReadFromFirstPageFrom;
        unsigned int uiToReadFromFirstPage;
        unsigned int uiToReadFromLastPage;
        unsigned int uiPageToCopy;
        WaWEChannelSetCachePage_t TempCachePage;

        NumOfPagesGot = 0;
        memset(pchPagesGot, 0, NumOfPagesToGet); // None of the pages are loaded yet!

        // Calculate some common values, like the amount of data to
        // read from first page, and from last page.
        uiReadFromFirstPageFrom =
          ReadFrom % pCache->uiCachePageSize;
        uiToReadFromFirstPage = pCache->uiCachePageSize - uiReadFromFirstPageFrom;
        if (uiToReadFromFirstPage>cBytes)
          uiToReadFromFirstPage = cBytes;

        if (cBytes - uiToReadFromFirstPage)
        {
          // There are more bytes to read.
          // So, there will be CachePageSize sized middle chunks, and
          // there will be a last chunk, with size from 1 to CachePageSize.
          // Let's see how many bytes remain for the last page!
          uiToReadFromLastPage = (cBytes - uiToReadFromFirstPage) % pCache->uiCachePageSize;
          // If it says that 0 bytes remain for the last page, it means that
          // it just got a page border, so the last page is a middle (full) page,
          // in reality. So, the last page is full then.
          if (uiToReadFromLastPage==0)
            uiToReadFromLastPage = pCache->uiCachePageSize;
        } else
          uiToReadFromLastPage = 0;

#ifdef DEBUG_BUILD
//        printf("To read %I64d bytes, I'll read\n", cBytes);
//        printf("%d bytes from first page (from pos %d), %d from middle ones, and %d from last page\n",
//               uiToReadFromFirstPage, uiReadFromFirstPageFrom,
//               pCache->uiCachePageSize,
//               uiToReadFromLastPage);
//        printf("Check cached pages first...\n");
#endif
        // First, go through the list of cached pages, and
        // copy the data from there, if any of them are needed!

        uiPage = 0;
        while ((uiPage<pCache->uiNumOfCachePages) &&    // All the cache pages
               (pCachePages[uiPage].PageNumber>-1) &&   // until we reach an empty one
               (Result<cBytes))                         // or until we got all the bytes we wanted
        {
          // There is a cached page here. Let's see if it's
          // needed now, and copy the data from it, if it can be used now!

          if ((pCachePages[uiPage].PageNumber>=FirstPageNum) &&
              (pCachePages[uiPage].PageNumber<FirstPageNum+NumOfPagesToGet))
          {
            // Fine, this is a page in cache, which is required now!
            // So, copy the data from it!
#ifdef DEBUG_BUILD
//            printf("Found page %I64d in cache, copy... ", pCachePages[uiPage].PageNumber);
#endif
            PROCESS_CACHE_PAGE(uiPage, 1);
            /* Also move this page to the beginning of cached pages, */
            /* so it will be sorted by usage. */
            if (uiPage>0)
            {
              memcpy(&TempCachePage, &(pCachePages[uiPage]), sizeof(WaWEChannelSetCachePage_t));
              for (uiPageToCopy = uiPage; uiPageToCopy>0; uiPageToCopy--)
                memcpy(&(pCachePages[uiPageToCopy]), &(pCachePages[uiPageToCopy-1]), sizeof(WaWEChannelSetCachePage_t));
              memcpy(&(pCachePages[0]), &TempCachePage, sizeof(WaWEChannelSetCachePage_t));
            }

          }

          // Go for next page!
          uiPage++;
        }
#ifdef DEBUG_BUILD
//        printf("Cached pages checked.\n");
#endif
        if (NumOfPagesGot<NumOfPagesToGet)
        {
          // Then, go through the pchPagesGot array, and read the
          // pages from disk, as they are not in the cache.
          // Also, put the loaded pages into the cache.

#ifdef DEBUG_BUILD
//          printf("Reading non-cached pages.\n");
#endif

          for (PageToLoad = 0; PageToLoad<NumOfPagesToGet; PageToLoad++)
          {
            if (!pchPagesGot[PageToLoad])
            {
              // This page is not loaded yet, so try to load it, and
              // put into the cache, if we could load it!
              char *pchPageInMem;
              WaWEPosition_t PageStartPos;
              WaWESize_t ReadFromPage;
#ifdef DEBUG_BUILD
//              printf("Page %I64d (%I64d) is not in cache, loading it... ",
//                     PageToLoad, FirstPageNum+PageToLoad);
#endif

              PageStartPos = (FirstPageNum + PageToLoad) * pCache->uiCachePageSize;
              pchPageInMem = dbg_malloc(pCache->uiCachePageSize);
              if (pchPageInMem)
              {
                if (PageStartPos == pfnSeek(pHandle, PageStartPos, SEEK_SET))
                {
                  // Read the max
                  ReadFromPage = pfnRead(pHandle, pchPageInMem, pCache->uiCachePageSize);
                  // Ok, now put this entry into the page-cache!
                  // First of all destroy the last cache page
                  if (pCachePages[pCache->uiNumOfCachePages-1].puchCachedData)
                  {
#ifdef DEBUG_BUILD
//                    printf("DROP cache page ");
#endif

                    dbg_free(pCachePages[pCache->uiNumOfCachePages-1].puchCachedData);
                  }
                  pCachePages[pCache->uiNumOfCachePages-1].puchCachedData = NULL;
                  pCachePages[pCache->uiNumOfCachePages-1].PageNumber = -1;
                  // Then move the pages to the right!
                  for (uiPage = pCache->uiNumOfCachePages-1; uiPage>0; uiPage--)
                    memcpy(&(pCachePages[uiPage]),
                           &(pCachePages[uiPage-1]),
                           sizeof(WaWEChannelSetCachePage_t));
                  // Now fill the first entry with new data!
                  pCachePages[0].PageNumber = FirstPageNum + PageToLoad;
                  pCachePages[0].uiNumOfBytesOnPage = ReadFromPage;
                  pCachePages[0].puchCachedData = pchPageInMem;

                  // Ok, now that it's in the cache, we can use this cache page to
                  // fill the user buffer
#ifdef DEBUG_BUILD
//                  printf("Page %I64d cached (%d bytes on page), now use it.\n",
//                         FirstPageNum + PageToLoad,
//                         pCachePages[0].uiNumOfBytesOnPage);
#endif

                  PROCESS_CACHE_PAGE(0, 2);
                }
                else
                {
#ifdef DEBUG_BUILD
                  printf("[internal_SeekNRead] : Could not seek to required page, page is not loaded!\n");
#endif
                  dbg_free(pchPageInMem);
                }
              }
#ifdef DEBUG_BUILD
              else
              {
                printf("[internal_SeekNRead] : Out of memory, page could not be loaded!\n");
              }
#endif
#ifdef DEBUG_BUILD
//              printf("Done.\n");
#endif

            }
          }
        }
#ifdef DEBUG_BUILD
//        printf("Data read. Wanted: %I64d  Got: %I64d\n", cBytes, Result);
//        printf("Wanted %I64d pages, got %I64d pages\n", NumOfPagesToGet, NumOfPagesGot);
//        {
//          int i;
//          printf("Pages: ");
//          for (i=0; i<NumOfPagesToGet; i++)
//          {
//            printf("%2d", pchPagesGot[i]);
//          }
//          printf("\n");
//        }
#endif

        dbg_free(pchPagesGot);
      }
    } else
    {
      // The channel-set is not cached, so do a simple seek then read.
      pfnSeek(pHandle, ReadFrom, SEEK_SET);
      Result = pfnRead(pHandle, pchBuffer, cBytes);
    }

    // Release seeknread semaphore
    DosReleaseMutexSem(pChannelSet->hmtxUseChannelSetCache);
  }

#ifdef DEBUG_BUILD
//  printf("[internal_SeekNRead] : Leave (%I64d)\n", Result);
#endif

  return Result;
}

int  WAWECALL WaWEHelper_Channel_ReadSample(WaWEChannel_p  pChannel,
                                            WaWEPosition_t StartPos,
                                            WaWESize_t     ReadSize,
                                            WaWESample_p   pSampleBuffer,
                                            WaWESize_p     pCouldReadSize)
{
  WaWEChunk_p pChunk;
  WaWESize_t ToRead, Readed, l;
  char *pTempBuffer;
  int iResult = WAWEHELPER_NOERROR; // No error by default


  if ((!pChannel) || (!pSampleBuffer) || (!pCouldReadSize))
    return WAWEHELPER_ERROR_INVALIDPARAMETER; // Invalid parameter!

  if (ReadSize==0)
  {
    *pCouldReadSize = 0;
    return WAWEHELPER_NOERROR;
  }

  if (DosRequestMutexSem(pChannel->hmtxUseChunkList, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
#ifdef DEBUG_BUILD
//    printf("[ReadSample] : Request to read %d samples from %d pos\n", (int) ReadSize, (int) StartPos); fflush(stdout);
#endif
    *pCouldReadSize = 0;

    // While there is something more to read
    while (ReadSize>0)
    {
      // Find a chunk which contains the starting position we want to read from!
      pChunk = pChannel->pChunkListHead;
      while ((pChunk) && (pChunk->VirtPosEnd<StartPos))
      {
        pChunk = pChunk->pNext;
      }

      // Ok, found a chunk, conatining some data!
      if ((pChunk) && (pChunk->VirtPosStart<=StartPos) && (pChunk->VirtPosEnd>=StartPos))
      {
        // Found the first chunk to contain some bytes!
        // Calculate how many data we can read from this chunk!
        ToRead = (pChunk->VirtPosEnd) - StartPos+1;
        if (ToRead>ReadSize) ToRead = ReadSize;

        // Read data from this chunk!
#ifdef DEBUG_BUILD
//        printf("[ReadSample] : Read %d from chunk\n",(int) ToRead); fflush(stdout);
#endif

        // Use different method, based on chunk type!
        // If it's an original chunk, then we have to use the plugin
        // to read some bytes, if it's a modified chunk, then we can access the
        // memory buffer directly!
        if (pChunk->iChunkType==WAWE_CHUNKTYPE_ORIGINAL)
        {
          int iBytesPerSample = pChunk->hFile->iBits/8;
          int iIsSigned = pChunk->hFile->iIsSigned;

          // ToRead contains the number of Samples to read from this chunk.
          // StartPos contains the start position to read from
          // pChunk is the chunk to read from

          // This chunk is an original chunk, so the
          // import plugin has to be used!

#ifdef DONT_OPTIMIZE_READSAMPLE_FOR_MEMORY
          // To read X samples from this chunk, we have to read
          // X * pChunk->hFile->iChannels * iBytesPerSample
          // bytes, and collect the samples from this data.

          pTempBuffer = (char *) dbg_malloc(ToRead *
                                            pChunk->hFile->iChannels*
                                            iBytesPerSample);
#else
          // To read X samples from this chunk, we have to read
          // X * pChunk->hFile->iChannels * iBytesPerSample
          // bytes, and collect the samples from this data.
          // This can be quite a big amount of memory to be allocated
          // temporary, so to minimize it, we split this to
          // a number of smaller chunks.
          // These smaller chunks will contain a maximum of 4096 samples.

          pTempBuffer = (char *) dbg_malloc(4096*
                                            pChunk->hFile->iChannels*
                                            iBytesPerSample);
#endif
          if (pTempBuffer)
          {
            // Ok, allocated temp buffer!
#ifdef DEBUG_BUILD
//            printf("[ReadSample] : Trying to read %d bytes from original chunk\n",
//                   (int) ToRead*
//                   pChunk->hFile->iChannels*
//                   iBytesPerSample); fflush(stdout);
#endif
            WaWESize_t LocalToRead;

            while (ToRead>0)
            {
#ifdef DONT_OPTIMIZE_READSAMPLE_FOR_MEMORY
              LocalToRead = ToRead;
#else
              LocalToRead = 4096;
              if (LocalToRead>ToRead)
                LocalToRead = ToRead;
#endif

              Readed = internal_SeekNRead(pChannel->pParentChannelSet,
                                          pChunk->pPlugin->TypeSpecificInfo.ImportPluginInfo.fnSeek,
                                          pChunk->pPlugin->TypeSpecificInfo.ImportPluginInfo.fnRead,
                                          pChunk->hFile,
                                          pChunk->RealPosStart +
                                            (StartPos-(pChunk->VirtPosStart))*
                                            iBytesPerSample*
                                            (pChunk->hFile->iChannels),
                                          pTempBuffer,
                                          LocalToRead*
                                            pChunk->hFile->iChannels*
                                            iBytesPerSample);

              // Now, take the sample data of important channel from this stream!
#ifdef DEBUG_BUILD
//              printf("[ReadSample] : Could read: %I64d bytes from original chunk (chn %d)\n",
//                     Readed, pChunk->iChannelNum); fflush(stdout);
#endif

              l=0;
              while (l<Readed)
              {
                WaWESample_t Sample = 0;
  
                // Get one sample
                memcpy(&Sample, pTempBuffer+l+pChunk->iChannelNum*iBytesPerSample, iBytesPerSample);
  
                // Convert it to 32bits signed
                Sample = Sample<<(32-pChunk->hFile->iBits);
                if (!iIsSigned)
                  Sample += 0x80000000;
  
                *pSampleBuffer = Sample;
                pSampleBuffer++;
                StartPos++;
                (*pCouldReadSize)++;
                ReadSize--; LocalToRead--;
  
                // Go to next sample!
                l = l+pChunk->hFile->iChannels*iBytesPerSample;
              }
              // If could not read all we wanted:
              if (LocalToRead>0)
              {
#ifdef DEBUG_BUILD
//                printf("[ReadSample] : Could not read all, zeroing out remaining.\n");
#endif
                memset(pSampleBuffer, 0, LocalToRead*sizeof(WaWESample_t));
                pSampleBuffer+=LocalToRead;
                StartPos+=LocalToRead;
                ReadSize-=LocalToRead;
                LocalToRead=0;
                iResult = WAWEHELPER_ERROR_PARTIALDATA;
              }

#ifdef DONT_OPTIMIZE_READSAMPLE_FOR_MEMORY
              ToRead = 0;
#else
              // Ok, processed one small chunk of this Original chunk
              if (ToRead>=4096)
                ToRead-=4096;
              else
                ToRead = 0;
#endif
            }
#ifdef DEBUG_BUILD
//            printf("[ReadSample] : Freeing pTempBuffer (0x%p)\n", pTempBuffer);
#endif

            dbg_free(pTempBuffer); pTempBuffer = NULL;
#ifdef DEBUG_BUILD
//            printf("[ReadSample] : Separated samples.\n");
#endif

          } else
          {
            // There was not enough memory to allocate temporary buffer,
            // so returning zeros!
            memset(pSampleBuffer, 0, ToRead*sizeof(WaWESample_t));
            ReadSize -= ToRead;
            pSampleBuffer+=ToRead;
            StartPos+=ToRead;
            iResult = WAWEHELPER_ERROR_OUTOFMEMORY; // Return error
          }
        } else
        {
          // This chunk is a modified chunk,
          // so it contains normal WaWE Samples in memory!

          // ToRead contains the number of Samples to read from this chunk.
          // StartPos contains the start position to read from
          // pChunk is the chunk to read from
          memcpy(pSampleBuffer,
                 pChunk->pSampleBuffer + (StartPos-(pChunk->VirtPosStart)),
                 ToRead*sizeof(WaWESample_t));
          ReadSize -= ToRead;
          pSampleBuffer+=ToRead;
          StartPos+=ToRead;
          (*pCouldReadSize)+=ToRead;
        }
      } else
      {
        // Did not find chunk for requested data!
        // Return zero samples then!
        memset(pSampleBuffer, 0, ReadSize*sizeof(WaWESample_t));
        ReadSize = 0;
        pSampleBuffer+=ReadSize;
        StartPos+=ReadSize;

        if (*pCouldReadSize)
          iResult = WAWEHELPER_ERROR_PARTIALDATA;
        else
          iResult = WAWEHELPER_ERROR_NODATA;
      }
    }
#ifdef DEBUG_BUILD
//    printf("[ReadSample] : Return!\n"); fflush(stdout);
#endif
    DosReleaseMutexSem(pChannel->hmtxUseChunkList);
  } else iResult = WAWEHELPER_ERROR_INTERNALERROR;


  // Call registered event callbacks
  if (*pCouldReadSize)
  {
    WaWEEventDesc_t EventDesc;

    EventDesc.iEventType = WAWE_EVENT_POSTSAMPLEREAD;
    EventDesc.EventSpecificInfo.SampleModInfo.pModifiedChannel = pChannel;
    EventDesc.EventSpecificInfo.SampleModInfo.ModificationStartPos = StartPos;
    EventDesc.EventSpecificInfo.SampleModInfo.ModificationSize = *pCouldReadSize;

    CallEventCallbacks(&EventDesc);
  }

  return iResult;
}


int  WAWECALL WaWEHelper_Channel_WriteSample(WaWEChannel_p  pChannel,
                                             WaWEPosition_t StartPos,
                                             WaWESize_t     WriteSize,
                                             WaWESample_p   pSampleBuffer,
                                             WaWESize_p     pCouldWriteSize)
{
  int iResult = WAWEHELPER_NOERROR;

#ifdef DEBUG_BUILD
//  printf("[WaWEHelper] : WriteSample! CurrPos = %d\n", (int) pChannel->CurrentPosition); fflush(stdout);
#endif

  if ((!pChannel) || (!pSampleBuffer) || (!pCouldWriteSize))
    return WAWEHELPER_ERROR_INVALIDPARAMETER; // Invalid parameter!

  if (WriteSize==0)
  {
    *pCouldWriteSize = 0;
    return WAWEHELPER_NOERROR;
  }

  if (DosRequestMutexSem(pChannel->hmtxUseChunkList, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    WaWEChunk_p pChunk;

#ifdef DEBUG_BUILD
//    printf("[WriteSample] : Before\n");
//    debug_ShowChunkList(pChannel);
#endif

    (*pCouldWriteSize) = 0;

    // Find chunk for this data!
    // Find a chunk which contains the starting position we want to write to!
    pChunk = pChannel->pChunkListHead;
    while ((pChunk) && (pChunk->VirtPosEnd<StartPos))
    {
      pChunk = pChunk->pNext;
    }

    // Let's see what we've found!
    if ((pChunk) && (pChunk->VirtPosStart<=StartPos) && (pChunk->VirtPosEnd>=StartPos))
    {
      // Found a chunk which contains this position!
#ifdef DEBUG_BUILD
//      printf("[WriteSample] :   Found chunk containing position!\n"); fflush(stdout);
#endif

      // If this chunk is a MODIFIED chunk, then simply increase it if needed,
      // and modify data inside.
      // If this chunk is an ORIGINAL chunk, then split it, and insert a MODIFIED
      // chunk into the middle!

      if (pChunk->iChunkType == WAWE_CHUNKTYPE_MODIFIED)
      {
        // This chunk is a MODIFIED chunk!
        // Simply increase it if needed,
        // and modify data inside.

#ifdef DEBUG_BUILD
//        printf("[WriteSample] :   Chunk if MODIFIED\n"); fflush(stdout);
#endif

        // Increase chunk size, if needed!
        if ((StartPos + WriteSize - 1)>pChunk->VirtPosEnd)
        {
          WaWESample_p pNewSampleBuffer;
          WaWESize_t NewSize;

          NewSize = (StartPos + WriteSize - 1) - pChunk->VirtPosStart + 1;

#ifdef DEBUG_BUILD
//          printf("[WriteSample] :   Increasing size of MODIFIED chunk\n"); fflush(stdout);
//          printf("[WriteSample] :   New size is %d samples\n", (int) NewSize); fflush(stdout);
#endif

          pNewSampleBuffer = dbg_realloc(pChunk->pSampleBuffer,
                                         NewSize * sizeof(WaWESample_t));
          if (!pNewSampleBuffer)
          {
            // Not enough memory to increase sample buffer!
            iResult = WAWEHELPER_ERROR_OUTOFMEMORY;
          } else
          {
#ifdef DEBUG_BUILD
//            printf("[WriteSample] :   OldBuf: %p, NewBuf: %p\n", pChunk->pSampleBuffer, pNewSampleBuffer); fflush(stdout);
//            printf("[WriteSample] :   OldEnd: %d, NewEnd: %d\n", (int) pChunk->VirtPosEnd, (int) (StartPos+WriteSize-1)); fflush(stdout);
#endif
            pChunk->pSampleBuffer = pNewSampleBuffer;
            pChunk->VirtPosEnd = StartPos + WriteSize - 1;
          }
        }

        if (iResult == WAWEHELPER_NOERROR)
        {
          // If there was no error yet (with possible resizing)
          // then copy the data here!

#ifdef DEBUG_BUILD
//          printf("[WriteSample] :   Overwriting data in MODIFIED chunk\n"); fflush(stdout);
#endif

          memcpy(pChunk->pSampleBuffer + (StartPos - pChunk->VirtPosStart),
                 pSampleBuffer,
                 WriteSize * sizeof(WaWESample_t));
          (*pCouldWriteSize) = WriteSize;
        }
      } else
      {
        // This chunk is an ORIGINAL chunk!
        // Split it, and insert a MODIFIED
        // chunk into the middle!

        WaWEChunk_p pNewChunk1, pNewChunk2, pNextChunk;

#ifdef DEBUG_BUILD
//        printf("[WriteSample] :   Chunk is ORIGINAL\n"); fflush(stdout);
#endif


        pNewChunk1 = (WaWEChunk_p) dbg_malloc(sizeof(WaWEChunk_t));
        pNewChunk2 = (WaWEChunk_p) dbg_malloc(sizeof(WaWEChunk_t));
        if ((!pNewChunk1) || (!pNewChunk2))
        {
          // Not enough memory to create new chunks!
          if (pNewChunk1)
            dbg_free(pNewChunk1);
          if (pNewChunk2)
            dbg_free(pNewChunk2);
          iResult = WAWEHELPER_ERROR_OUTOFMEMORY;
        } else
        {
          pNewChunk1->iChunkType = WAWE_CHUNKTYPE_MODIFIED;
          pNewChunk1->VirtPosStart = StartPos;
          pNewChunk1->VirtPosEnd = StartPos+WriteSize-1;
          pNewChunk1->pSampleBuffer = (WaWESample_p) dbg_malloc_huge(WriteSize * sizeof(WaWESample_t));
          if (!pSampleBuffer)
          {
            // Not enough memory to create sample buffer!
            if (pNewChunk1)
              dbg_free(pNewChunk1);
            if (pNewChunk2)
              dbg_free(pNewChunk2);
            iResult = WAWEHELPER_ERROR_OUTOFMEMORY;
          } else
          {
#ifdef DEBUG_BUILD
//            printf("[WriteSample] :   Copy data into new modified chunk\n"); fflush(stdout);
#endif

            memcpy(pNewChunk1->pSampleBuffer, pSampleBuffer, WriteSize * sizeof(WaWESample_t));

            (*pCouldWriteSize) = WriteSize;

            pNewChunk1->pPrev = pChunk;
            pNewChunk1->pNext = pChunk->pNext;
            pChunk->pNext = pNewChunk1;
            pNextChunk = pNewChunk1->pNext;
            if (pNextChunk)
              pNextChunk->pPrev = pNewChunk1;

            if (pChunk->VirtPosEnd>pNewChunk1->VirtPosEnd)
            {

#ifdef DEBUG_BUILD
//              printf("[WriteSample] :   Creating new ORIGINAL chunk\n"); fflush(stdout);
#endif


              // The old chunk have been splitted, so a new chunk has to be
              // created for the remaining end!
              pNewChunk2->iChunkType = WAWE_CHUNKTYPE_ORIGINAL;
              pNewChunk2->VirtPosStart = pNewChunk1->VirtPosEnd+1;
              pNewChunk2->VirtPosEnd = pChunk->VirtPosEnd;
              pNewChunk2->pPlugin = pChunk->pPlugin;
              pNewChunk2->hFile = pChunk->hFile;
              pNewChunk2->iChannelNum = pChunk->iChannelNum;
              pNewChunk2->RealPosStart =
                pChunk->RealPosStart +
                (pNewChunk2->VirtPosStart - pChunk->VirtPosStart) *
                (pChunk->hFile->iBits/8) *
                (pChunk->hFile->iChannels);

              pNewChunk2->pPrev = pNewChunk1;
              pNewChunk2->pNext = pNewChunk1->pNext;
              pNewChunk1->pNext = pNewChunk2;

              pNextChunk = pNewChunk2->pNext;
              if (pNextChunk)
                pNextChunk->pPrev = pNewChunk2;

            } else
            {
              // The old chunk's end have been overwritten, no need for a new
              // chunk for the end!

#ifdef DEBUG_BUILD
//              printf("[WriteSample] :   No need for new ORIGINAL chunk\n"); fflush(stdout);
#endif

              dbg_free(pNewChunk2); pNewChunk2 = NULL;
              pNextChunk = pNewChunk1->pNext;
              if (pNextChunk)
                pNextChunk->pPrev = pNewChunk1;
            }
            // Now we can set the new end-of-chunk for old chunk!
            pChunk->VirtPosEnd = StartPos - 1;
          }
        }
      }

      if (iResult == WAWEHELPER_NOERROR)
      {
        // Ok, the chunk has been successfully modified!
        // Now check later chunks, if this chunk overlaps them or not, and so on.
        // So, delete/modify wrong chunks!

	internal_RepairOverlappedChunks(pChannel, pChunk);
        internal_DeleteInvalidChunks(pChannel);
        // Now check for neightbourous modified chunks, and concatenate them, if found!
        internal_ConcatenateNeightbourousChunks(pChannel);
        // Ok, all done!
      }
    } else
    if (pChunk)
    {
      // The first chunk does not contain this position!
      // We have to insert a new chunk before pChunk!
      // (This should not happen, but who knows!)

      WaWEChunk_p pNewChunk;

      // Now create the new chunk
      pNewChunk = (WaWEChunk_p) dbg_malloc(sizeof(WaWEChunk_t));
      if (!pNewChunk)
      {
        // Not enough memory for new chunk!
        iResult = WAWEHELPER_ERROR_OUTOFMEMORY;
      } else
      {
        pNewChunk->iChunkType = WAWE_CHUNKTYPE_MODIFIED;
        pNewChunk->VirtPosStart = 0;
        pNewChunk->VirtPosEnd = StartPos + WriteSize-1;

        pNewChunk->pSampleBuffer = (WaWESample_p) dbg_malloc_huge((pNewChunk->VirtPosEnd - pNewChunk->VirtPosStart + 1) * sizeof(WaWESample_t));
        if (!(pNewChunk->pSampleBuffer))
        {
          // Not enough memory for sample buffer!
          dbg_free(pNewChunk);
          iResult = WAWEHELPER_ERROR_OUTOFMEMORY;
        } else
        {
          // Ok, we have the buffer
          // Fill it with zeros until start pos,
          // and memcpy data from startpos to virtposend!
          if (pNewChunk->VirtPosStart!=StartPos)
            memset(pNewChunk->pSampleBuffer, 0, (StartPos - pNewChunk->VirtPosStart) * sizeof(WaWESample_t));

          memcpy(pNewChunk->pSampleBuffer + (StartPos - pNewChunk->VirtPosStart), pSampleBuffer, WriteSize * sizeof(WaWESample_t));

          (*pCouldWriteSize) = WriteSize;

          pNewChunk->pPrev = NULL;
          pNewChunk->pNext = pChunk;

          // New chunk is ready, now link it into list!
          if (pChunk)
            pChunk->pPrev = pNewChunk;
          pChannel->pChunkListHead = pNewChunk;

          // Ok, linked. Now make sure that it does not overlap some
          // chunks after it!
	  internal_RepairOverlappedChunks(pChannel, pChunk);
	  internal_DeleteInvalidChunks(pChannel);
          internal_ConcatenateNeightbourousChunks(pChannel);
        }
      }
    } else
    // pChunk == NULL
    {
      // The last chunk is still before the position we want to
      // write, so we have to create a new last chunk!
      WaWEChunk_p pNewChunk;
      WaWEChunk_p pLastChunk;

      // Find the last chunk, and have a pointer for it in pLastChunk!
      pLastChunk = pChannel->pChunkListHead;
      while ((pLastChunk) && (pLastChunk->pNext))
        pLastChunk = pLastChunk->pNext;

      // Now create the new chunk
      pNewChunk = (WaWEChunk_p) dbg_malloc(sizeof(WaWEChunk_t));
      if (!pNewChunk)
      {
        // Not enough memory for new chunk!
        iResult = WAWEHELPER_ERROR_OUTOFMEMORY;
      } else
      {
        pNewChunk->iChunkType = WAWE_CHUNKTYPE_MODIFIED;
        if (pLastChunk)
          pNewChunk->VirtPosStart = pLastChunk->VirtPosEnd+1;
        else
          // There is no last chunk, so there is no chunk list yet!!
          pNewChunk->VirtPosStart = 0;
        pNewChunk->VirtPosEnd = StartPos + WriteSize-1;

        pNewChunk->pSampleBuffer = (WaWESample_p) dbg_malloc_huge((pNewChunk->VirtPosEnd - pNewChunk->VirtPosStart + 1) * sizeof(WaWESample_t));
        if (!(pNewChunk->pSampleBuffer))
        {
          // Not enough memory for sample buffer!
          dbg_free(pNewChunk);
          iResult = WAWEHELPER_ERROR_OUTOFMEMORY;
        } else
        {
          // Ok, we have the buffer
          // Fill it with zeros until start pos,
          // and memcpy data from startpos to virtposend!
          if (pNewChunk->VirtPosStart!=StartPos)
          {
#ifdef DEBUG_BUILD
//            printf("Setting zeros from %d to %d\n", 0, (int) (StartPos - pNewChunk->VirtPosStart)); fflush(stdout);
#endif
            memset(pNewChunk->pSampleBuffer, 0, (StartPos - pNewChunk->VirtPosStart) * sizeof(WaWESample_t));
          }

#ifdef DEBUG_BUILD
//          printf("memcpy to %d, %d samples\n", (int) (StartPos - pNewChunk->VirtPosStart), (int) WriteSize); fflush(stdout);
#endif

          memcpy(pNewChunk->pSampleBuffer + (StartPos - pNewChunk->VirtPosStart),
                 pSampleBuffer,
                 WriteSize * sizeof(WaWESample_t));

          (*pCouldWriteSize) = WriteSize;

          pNewChunk->pPrev = pLastChunk;
          pNewChunk->pNext = NULL;

          // New chunk is ready, now link it into list!
          if (pLastChunk)
            pLastChunk->pNext = pNewChunk;
          else
            pChannel->pChunkListHead = pNewChunk;

          // Now check for neightbourous modified chunks, and concatenate them, if found!
	  internal_RepairOverlappedChunks(pChannel, pChunk);
          internal_DeleteInvalidChunks(pChannel);
          internal_ConcatenateNeightbourousChunks(pChannel);
        }
      }
    }

#ifdef DEBUG_BUILD
//    printf("[WriteSample] : After\n");
//    debug_ShowChunkList(pChannel);
#endif

    if (*pCouldWriteSize)
    {
      // Update channel length!
      pChannel->Length = 0;
      pChunk = pChannel->pChunkListHead;
      while ((pChunk) && (pChunk->pNext)) pChunk = pChunk->pNext;
      if (pChunk)
        pChannel->Length = pChunk->VirtPosEnd+1;

      pChannel->bRedrawNeeded = 1; // Note that channel data changed, redraw needed!
      pChannel->bModified = 1;
      pChannel->pParentChannelSet->bModified = 1;
    }

    DosReleaseMutexSem(pChannel->hmtxUseChunkList);
  } else iResult = WAWEHELPER_ERROR_INTERNALERROR;

  internal_UpdateChannelsetLength(pChannel->pParentChannelSet);

  // Call registered event callbacks
  if (*pCouldWriteSize)
  {
    WaWEEventDesc_t EventDesc;

    EventDesc.iEventType = WAWE_EVENT_POSTSAMPLEWRITE;
    EventDesc.EventSpecificInfo.SampleModInfo.pModifiedChannel = pChannel;
    EventDesc.EventSpecificInfo.SampleModInfo.ModificationStartPos = StartPos;
    EventDesc.EventSpecificInfo.SampleModInfo.ModificationSize = *pCouldWriteSize;

    CallEventCallbacks(&EventDesc);
  }

  return iResult;
}

int  WAWECALL WaWEHelper_Channel_InsertSample(WaWEChannel_p  pChannel,
                                              WaWEPosition_t StartPos,
                                              WaWESize_t     InsertSize,
                                              WaWESample_p   pSampleBuffer,
                                              WaWESize_p     pCouldInsertSize)
{
  int iResult = WAWEHELPER_NOERROR;

#ifdef DEBUG_BUILD
//  printf("[WaWEHelper] : InsertSample! StartPos = %d\n", (int) StartPos); fflush(stdout);
#endif

  if ((!pChannel) || (!pSampleBuffer) || (!pCouldInsertSize))
    return WAWEHELPER_ERROR_INVALIDPARAMETER; // Invalid parameter!

  if (InsertSize==0)
  {
    *pCouldInsertSize = 0;
    return WAWEHELPER_NOERROR;
  }

  // We have to grab two semaphores, the ChunkList, and the ShownSelectedRegionList, because
  // we'll tamper with those two linked lists
  if (DosRequestMutexSem(pChannel->hmtxUseShownSelectedRegionList, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    if (DosRequestMutexSem(pChannel->hmtxUseChunkList, SEM_INDEFINITE_WAIT)==NO_ERROR)
    {
      WaWEChunk_p pChunk;
  
#ifdef DEBUG_BUILD
//      printf("[InsertSample] : Before\n");
//      debug_ShowChunkList(pChannel);
#endif
  
      (*pCouldInsertSize) = 0;
  
      // Find chunk for this data!
      // Find a chunk which contains the starting position we want to insert to!
      pChunk = pChannel->pChunkListHead;
      while ((pChunk) && (pChunk->VirtPosEnd<StartPos))
      {
        pChunk = pChunk->pNext;
      }
  
      // Let's see what we've found!
      if ((pChunk) && (pChunk->VirtPosStart<=StartPos) && (pChunk->VirtPosEnd>=StartPos))
      {
        // Found a chunk which contains this position!
  
        // If this chunk is a MODIFIED chunk, then simply increase it,
        // and modify data inside.
        // If this chunk is an ORIGINAL chunk, then split it, and insert a MODIFIED
        // chunk into the middle!
  
        if (pChunk->iChunkType == WAWE_CHUNKTYPE_MODIFIED)
        {
          // This chunk is a MODIFIED chunk!
          // Simply increase it,
          // and modify data inside.
  
          WaWESample_p pNewSampleBuffer;
          WaWESize_t NewSize;
          WaWEChunk_p pShiftStartChunk;
  
          pShiftStartChunk = pChunk->pNext;
  
          NewSize = pChunk->VirtPosEnd - pChunk->VirtPosStart + 1 + InsertSize;
  
          pNewSampleBuffer = dbg_realloc(pChunk->pSampleBuffer,
                                         NewSize * sizeof(WaWESample_t));
          if (!pNewSampleBuffer)
          {
            // Not enough memory to increase sample buffer!
            iResult = WAWEHELPER_ERROR_OUTOFMEMORY;
          } else
          {
            WaWESize_t i;
            WaWESize_t Last, CopySize;
  
            pChunk->pSampleBuffer = pNewSampleBuffer;
            pChunk->VirtPosEnd += InsertSize;

            // Copy old data to the end
            Last = pChunk->VirtPosEnd - pChunk->VirtPosStart;
            CopySize = pChunk->VirtPosEnd + 1 - (StartPos + InsertSize);
            for (i=0; i<CopySize; i++)
            {
              pChunk->pSampleBuffer[Last-i] =pChunk->pSampleBuffer[Last-i-InsertSize];
            }

            // Copy new data into the hole!
            memcpy(pChunk->pSampleBuffer + (StartPos - pChunk->VirtPosStart),
                   pSampleBuffer,
                   InsertSize * sizeof(WaWESample_t));

            (*pCouldInsertSize) = InsertSize;

            // Ok, data inserted!
            // Now go through all the next chunks, and shift their virtual position!
            pChunk = pShiftStartChunk;
            while (pChunk)
            {
              pChunk->VirtPosStart += InsertSize;
              pChunk->VirtPosEnd   += InsertSize;
              pChunk = pChunk->pNext;
            }
          }
        } else
        {
          // This chunk is an ORIGINAL chunk!
          // Split it, and insert a MODIFIED
          // chunk into the middle!
  
          WaWEChunk_p pNewChunk1, pNewChunk2;
          WaWEChunk_p pNextChunk;
          WaWEChunk_p pShiftStartChunk;
  
          pShiftStartChunk = pChunk->pNext;
  
          pNewChunk1 = (WaWEChunk_p) dbg_malloc(sizeof(WaWEChunk_t));
          pNewChunk2 = (WaWEChunk_p) dbg_malloc(sizeof(WaWEChunk_t));
          if ((!pNewChunk1) || (!pNewChunk2))
          {
            // Not enough memory to create new chunks!
            if (pNewChunk1)
              dbg_free(pNewChunk1);
            if (pNewChunk2)
              dbg_free(pNewChunk2);
            iResult = WAWEHELPER_ERROR_OUTOFMEMORY;
          } else
          {
            pNewChunk1->iChunkType = WAWE_CHUNKTYPE_MODIFIED;
            pNewChunk1->VirtPosStart = StartPos;
            pNewChunk1->VirtPosEnd = StartPos+InsertSize-1;
            pNewChunk1->pSampleBuffer = (WaWESample_p) dbg_malloc_huge(InsertSize * sizeof(WaWESample_t));
            if (!pSampleBuffer)
            {
              // Not enough memory to create sample buffer!
              if (pNewChunk1)
                dbg_free(pNewChunk1);
              if (pNewChunk2)
                dbg_free(pNewChunk2);
              iResult = WAWEHELPER_ERROR_OUTOFMEMORY;
            } else
            {
              memcpy(pNewChunk1->pSampleBuffer, pSampleBuffer, InsertSize * sizeof(WaWESample_t));
  
              (*pCouldInsertSize) = InsertSize;
  
              pNewChunk1->pPrev = pChunk;
              pNewChunk1->pNext = pChunk->pNext;
              pChunk->pNext = pNewChunk1;
  
              // The old chunk have been splitted, so a new chunk has to be
              // created for the remaining end!
              pNewChunk2->iChunkType = WAWE_CHUNKTYPE_ORIGINAL;
              pNewChunk2->VirtPosStart = pNewChunk1->VirtPosEnd+1;
              pNewChunk2->VirtPosEnd = pChunk->VirtPosEnd + InsertSize;
              pNewChunk2->pPlugin = pChunk->pPlugin;
              pNewChunk2->hFile = pChunk->hFile;
              pNewChunk2->iChannelNum = pChunk->iChannelNum;
              pNewChunk2->RealPosStart =
                  pChunk->RealPosStart +
                  (StartPos - pChunk->VirtPosStart) *
                  (pChunk->hFile->iBits/8) *
                  (pChunk->hFile->iChannels);
  
              pNewChunk2->pPrev = pNewChunk1;
              pNewChunk2->pNext = pNewChunk1->pNext;
              pNewChunk1->pNext = pNewChunk2;
  
              // Modify old original chunk!
              pChunk->VirtPosEnd = StartPos-1;
  
              pNextChunk = pNewChunk2->pNext;
              if (pNextChunk)
                pNextChunk->pPrev = pNewChunk2;
  
              // Ok, data inserted!
              // Now go through all the next chunks, and shift their virtual position!
              pChunk = pShiftStartChunk;
              while (pChunk)
              {
                pChunk->VirtPosStart += InsertSize;
                pChunk->VirtPosEnd   += InsertSize;
                pChunk = pChunk->pNext;
              }
            }
          }
        }
      } else
      if (pChunk)
      {
        // The first chunk does not contain this position!
        // We have to insert a new chunk before pChunk!
        // (This should not happen, but who knows!)
  
        WaWEChunk_p pNewChunk;
        WaWEChunk_p pShiftStartChunk;
  
        pShiftStartChunk = pChunk;
  
        // Now create the new chunk
        pNewChunk = (WaWEChunk_p) dbg_malloc(sizeof(WaWEChunk_t));
        if (!pNewChunk)
        {
          // Not enough memory for new chunk!
          iResult = WAWEHELPER_ERROR_OUTOFMEMORY;
        } else
        {
          pNewChunk->iChunkType = WAWE_CHUNKTYPE_MODIFIED;
          pNewChunk->VirtPosStart = 0;
          pNewChunk->VirtPosEnd = StartPos + InsertSize-1;
  
          pNewChunk->pSampleBuffer = (WaWESample_p) dbg_malloc_huge((pNewChunk->VirtPosEnd - pNewChunk->VirtPosStart + 1) * sizeof(WaWESample_t));
          if (!(pNewChunk->pSampleBuffer))
          {
            // Not enough memory for sample buffer!
            dbg_free(pNewChunk);
            iResult = WAWEHELPER_ERROR_OUTOFMEMORY;
          } else
          {
            // Ok, we have the buffer
            // Fill it with zeros until start pos,
            // and memcpy data from startpos to virtposend!
            if (pNewChunk->VirtPosStart!=StartPos)
              memset(pNewChunk->pSampleBuffer, 0, (StartPos - pNewChunk->VirtPosStart) * sizeof(WaWESample_t));
  
            memcpy(pNewChunk->pSampleBuffer + (StartPos - pNewChunk->VirtPosStart), pSampleBuffer, InsertSize * sizeof(WaWESample_t));
  
            (*pCouldInsertSize) = InsertSize;
  
            pNewChunk->pPrev = NULL;
            pNewChunk->pNext = pChunk;
  
            // New chunk is ready, now link it into list!
            if (pChunk)
              pChunk->pPrev = pNewChunk;
            pChannel->pChunkListHead = pNewChunk;
  
            // Ok, data inserted!
            // Now go through all the next chunks, and shift their virtual position!
            pChunk = pShiftStartChunk;
            while (pChunk)
            {
              pChunk->VirtPosStart += InsertSize;
              pChunk->VirtPosEnd   += InsertSize;
              pChunk = pChunk->pNext;
            }
          }
        }
      } else
      // pChunk == NULL
      {
        // The last chunk is still before the position we want to
        // write, so we have to create a new last chunk!
        WaWEChunk_p pNewChunk;
        WaWEChunk_p pLastChunk;
  
        // Find the last chunk, and have a pointer for it in pLastChunk!
        pLastChunk = pChannel->pChunkListHead;
        while ((pLastChunk) && (pLastChunk->pNext))
          pLastChunk = pLastChunk->pNext;
  
        // Now create the new chunk
        pNewChunk = (WaWEChunk_p) dbg_malloc(sizeof(WaWEChunk_t));
        if (!pNewChunk)
        {
          // Not enough memory for new chunk!
          iResult = WAWEHELPER_ERROR_OUTOFMEMORY;
        } else
        {
          pNewChunk->iChunkType = WAWE_CHUNKTYPE_MODIFIED;
          if (pLastChunk)
            pNewChunk->VirtPosStart = pLastChunk->VirtPosEnd+1;
          else
            // There is no last chunk, so there is no chunk list yet!!
            pNewChunk->VirtPosStart = 0;
          pNewChunk->VirtPosEnd = StartPos + InsertSize-1;
  
          pNewChunk->pSampleBuffer = (WaWESample_p) dbg_malloc_huge((pNewChunk->VirtPosEnd - pNewChunk->VirtPosStart + 1) * sizeof(WaWESample_t));
          if (!(pNewChunk->pSampleBuffer))
          {
            // Not enough memory for sample buffer!
            dbg_free(pNewChunk);
            iResult = WAWEHELPER_ERROR_OUTOFMEMORY;
          } else
          {
            // Ok, we have the buffer
            // Fill it with zeros until start pos,
            // and memcpy data from startpos to virtposend!
            if (pNewChunk->VirtPosStart!=StartPos)
              memset(pNewChunk->pSampleBuffer, 0, (StartPos - pNewChunk->VirtPosStart) * sizeof(WaWESample_t));
  
            memcpy(pNewChunk->pSampleBuffer + (StartPos - pNewChunk->VirtPosStart), pSampleBuffer, InsertSize * sizeof(WaWESample_t));
  
            (*pCouldInsertSize) = InsertSize;
  
            pNewChunk->pPrev = pLastChunk;
            pNewChunk->pNext = NULL;
  
            // New chunk is ready, now link it into list!
            if (pLastChunk)
              pLastChunk->pNext = pNewChunk;
            else
              pChannel->pChunkListHead = pNewChunk;
          }
        }
      }
      // Simplify chunklist
      internal_DeleteInvalidChunks(pChannel);
      internal_ConcatenateNeightbourousChunks(pChannel);
#ifdef DEBUG_BUILD
//      printf("[InsertSample] : After\n");
//      debug_ShowChunkList(pChannel);
#endif
  
      if (*pCouldInsertSize)
      {
        // Update channel length!
        pChannel->Length = 0;
        pChunk = pChannel->pChunkListHead;
        while ((pChunk) && (pChunk->pNext)) pChunk = pChunk->pNext;
        if (pChunk)
          pChannel->Length = pChunk->VirtPosEnd+1;
  
        pChannel->bRedrawNeeded = 1; // Note that channel data changed, redraw needed!
        pChannel->bModified = 1;
        pChannel->pParentChannelSet->bModified = 1;

        // Now update selected region list, if there is any!
        if (pChannel->pShownSelectedRegionListHead)
        {
          WaWESelectedRegion_p pRegion = pChannel->pShownSelectedRegionListHead;
          while (pRegion)
          {
            if ((pRegion->Start<=StartPos) && (pRegion->End>=StartPos))
            {
              // The insert has been happened in this region!
              pChannel->ShownSelectedLength+=*pCouldInsertSize;
              if (pRegion->Start == StartPos)
                pRegion->Start+=*pCouldInsertSize;

              pRegion->End+=*pCouldInsertSize;

            } else
            if (pRegion->End>StartPos)
            {
              // The insert has been happened in a region before this one, so
              // all of this region has to be shifted!
              pRegion->Start+=*pCouldInsertSize;
              pRegion->End+=*pCouldInsertSize;
            }
            pRegion = pRegion->pNext;
          }

        }
      }
      DosReleaseMutexSem(pChannel->hmtxUseChunkList);
    } else iResult = WAWEHELPER_ERROR_INTERNALERROR;
    DosReleaseMutexSem(pChannel->hmtxUseShownSelectedRegionList);
  } else iResult = WAWEHELPER_ERROR_INTERNALERROR;

  internal_UpdateChannelsetLength(pChannel->pParentChannelSet);

  // Call registered event callbacks
  if (*pCouldInsertSize)
  {
    WaWEEventDesc_t EventDesc;

    EventDesc.iEventType = WAWE_EVENT_POSTSAMPLEINSERT;
    EventDesc.EventSpecificInfo.SampleModInfo.pModifiedChannel = pChannel;
    EventDesc.EventSpecificInfo.SampleModInfo.ModificationStartPos = StartPos;
    EventDesc.EventSpecificInfo.SampleModInfo.ModificationSize = *pCouldInsertSize;

    CallEventCallbacks(&EventDesc);
  }

  return iResult;
}

int  WAWECALL WaWEHelper_Channel_DeleteSample(WaWEChannel_p  pChannel,
                                              WaWEPosition_t StartPos,
                                              WaWESize_t     DeleteSize,
                                              WaWESize_p     pCouldDeleteSize)
{
  int iResult = WAWEHELPER_NOERROR;

#ifdef DEBUG_BUILD
//  printf("[WaWEHelper] : DeleteSample (%d samples at %d)!\n", (int) DeleteSize, (int) StartPos); fflush(stdout);
#endif

  if ((!pChannel) || (!pCouldDeleteSize))
    return WAWEHELPER_ERROR_INVALIDPARAMETER; // Invalid parameter!

  if (DeleteSize==0)
  {
    *pCouldDeleteSize = 0;
    return WAWEHELPER_NOERROR;
  }

  // We have to grab two semaphores, the ChunkList, and the ShownSelectedRegionList, because
  // we'll tamper with those two linked lists
  if (DosRequestMutexSem(pChannel->hmtxUseShownSelectedRegionList, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    if (DosRequestMutexSem(pChannel->hmtxUseChunkList, SEM_INDEFINITE_WAIT)==NO_ERROR)
    {
      WaWEChunk_p pChunk;
      WaWEPosition_t IntersectStart, IntersectEnd, DeleteStart, DeleteEnd;
  
#ifdef DEBUG_BUILD
//      printf("[DeleteSample] : Before\n");
//      debug_ShowChunkList(pChannel);
#endif
  
      (*pCouldDeleteSize) = 0;
  
      // Find chunk for this data!
      // Find a chunk which contains the starting position we want to delete from!
      DeleteStart = StartPos;
      DeleteEnd = StartPos + DeleteSize - 1;
      pChunk = pChannel->pChunkListHead;
      while ((pChunk) && (iResult == WAWEHELPER_NOERROR))
      {
        if (CalculateRegionIntersect(pChunk->VirtPosStart, pChunk->VirtPosEnd,
                                     DeleteStart, DeleteEnd,
                                     &IntersectStart, &IntersectEnd))
        {
          // Oh, this chunk contains some from the region to be deleted!
          // So, modify this chunk!
  
          if ((IntersectStart == pChunk->VirtPosStart) &&
              (IntersectEnd == pChunk->VirtPosEnd))
          {
            // The whole chunk has to be deleted!
            WaWEChunk_p pToDelete;
  
#ifdef DEBUG_BUILD
  //	  printf("Deleting the whole chunk!\n"); fflush(stdout);
#endif
  
            pToDelete = pChunk;
            pChunk = pChunk->pNext;
  
            if (pChunk)
              pChunk->pPrev = pToDelete->pPrev;
            if (pToDelete->pPrev)
            {
              WaWEChunk_p pPrev = pToDelete->pPrev;
              pPrev->pNext = pChunk;
            }
            else
              pChannel->pChunkListHead = pChunk;
  
            if (pToDelete->iChunkType == WAWE_CHUNKTYPE_MODIFIED)
            {
              if (pToDelete->pSampleBuffer)
                dbg_free(pToDelete->pSampleBuffer);
            }
  
            dbg_free(pToDelete);

            (*pCouldDeleteSize)+= (IntersectEnd - IntersectStart + 1);
          } else
          if (IntersectStart == pChunk->VirtPosStart)
          {
            // Not the whole chunk has to be deleted, but an area from its beginning!
  
#ifdef DEBUG_BUILD
  //	  printf("Deleting an area from the beginning of the chunk!\n"); fflush(stdout);
#endif
  
            pChunk->VirtPosStart += (IntersectEnd - IntersectStart + 1);
            // Modify data in chunk!
            if (pChunk->iChunkType == WAWE_CHUNKTYPE_ORIGINAL)
            {
#ifdef DEBUG_BUILD
  //	    printf("  (Original chunk)\n"); fflush(stdout);
#endif
  
              // Original chunk!
              pChunk->RealPosStart +=
                (IntersectEnd - IntersectStart + 1) *
                (pChunk->hFile->iBits/8) *
                (pChunk->hFile->iChannels);
            } else
            {
              // Modified chunk!
              WaWESample_p pNewSampleBuffer;
              WaWESize_t NewSize;
  
#ifdef DEBUG_BUILD
  //	    printf("  (Modified chunk)\n"); fflush(stdout);
#endif
  
              NewSize = (pChunk->VirtPosEnd - pChunk->VirtPosStart + 1);
  
              memcpy(pChunk->pSampleBuffer,
                     pChunk->pSampleBuffer + (IntersectEnd-IntersectStart + 1),
                     NewSize * sizeof(WaWESample_t));
              pNewSampleBuffer = dbg_realloc(pChunk->pSampleBuffer, NewSize * sizeof(WaWESample_t));
              if (!pNewSampleBuffer)
              {
                iResult = WAWEHELPER_ERROR_OUTOFMEMORY;
              } else
              {
                pChunk->pSampleBuffer = pNewSampleBuffer;
              }
            }
            (*pCouldDeleteSize)+= (IntersectEnd - IntersectStart + 1);
            pChunk = pChunk->pNext;
          } else
          if (IntersectEnd == pChunk->VirtPosEnd)
          {
            // Not the whole chunk has to be deleted, but an area from its end!
#ifdef DEBUG_BUILD
  //	  printf("Deleting an area from the end of the chunk!\n"); fflush(stdout);
#endif
  
            pChunk->VirtPosEnd -= (IntersectEnd - IntersectStart + 1);
            // Modify data in chunk!
            if (pChunk->iChunkType == WAWE_CHUNKTYPE_ORIGINAL)
            {
              // Nothing to do!
            } else
            {
              // Modified chunk!
              WaWESample_p pNewSampleBuffer;
              WaWESize_t NewSize;
  
#ifdef DEBUG_BUILD
  //	    printf("  (Modified chunk)\n"); fflush(stdout);
#endif
  
              NewSize = (pChunk->VirtPosEnd - pChunk->VirtPosStart + 1);
  
              pNewSampleBuffer = dbg_realloc(pChunk->pSampleBuffer, NewSize * sizeof(WaWESample_t));
#ifdef DEBUG_BUILD
  //	    printf("  OldP = %p NewP = %p\n", pChunk->pSampleBuffer, pNewSampleBuffer); fflush(stdout);
#endif
  
              if (!pNewSampleBuffer)
              {
                iResult = WAWEHELPER_ERROR_OUTOFMEMORY;
              } else
              {
                pChunk->pSampleBuffer = pNewSampleBuffer;
              }
            }
            (*pCouldDeleteSize)+= (IntersectEnd - IntersectStart + 1);
            pChunk = pChunk->pNext;
          } else
          {
            // Not the whole chunk has to be deleted, but an area from its middle!
            // This means, that a new chunk has to be created!
            WaWEChunk_p pNewChunk, pNext;
  
#ifdef DEBUG_BUILD
//            printf("Deleting an area from the middle of the chunk!\n"); fflush(stdout);
#endif
  
            pNewChunk = (WaWEChunk_p) dbg_malloc(sizeof(WaWEChunk_t));
            if (!pNewChunk)
            {
              iResult = WAWEHELPER_ERROR_OUTOFMEMORY;
            } else
            {
              // Link the new chunk into list
#ifdef DEBUG_BUILD
//              printf("Link the new chunk into list\n"); fflush(stdout);
#endif
  
              pNewChunk->pNext = pChunk->pNext;
              pNewChunk->pPrev = pChunk;
              pChunk->pNext = pNewChunk;
              pNext = pNewChunk->pNext;
              if (pNext)
                pNext->pPrev = pNewChunk;
  
              // Fill entries of new chunk
#ifdef DEBUG_BUILD
//              printf("Fill entries of new chunk\n"); fflush(stdout);
#endif
              pNewChunk->VirtPosEnd = pChunk->VirtPosEnd;
              pNewChunk->VirtPosStart = IntersectEnd+1;
              pNewChunk->iChunkType = pChunk->iChunkType;
              // Modify entries of old chunk
#ifdef DEBUG_BUILD
//              printf("Modify entries of old chunk\n"); fflush(stdout);
#endif
  
              pChunk->VirtPosEnd = IntersectStart - 1;
  
              // Now create data of new chunk, and modify data of old chunk!
              if (pChunk->iChunkType == WAWE_CHUNKTYPE_MODIFIED)
              {
                WaWESample_p pNewChunkSampleBuffer, pOldChunkSampleBuffer;
  
                pNewChunkSampleBuffer = (WaWESample_p) dbg_malloc_huge(sizeof(WaWESample_t) * (pNewChunk->VirtPosEnd - pNewChunk->VirtPosStart + 1));
                if (!pNewChunkSampleBuffer)
                {
                  iResult = WAWEHELPER_ERROR_OUTOFMEMORY;
                  // Cleanup mess
                  pChunk->pNext = pNewChunk->pNext;
                  if (pNext)
                    pNext->pPrev = pChunk;
                  dbg_free(pNewChunk);
                } else
                {
                  pNewChunk->pSampleBuffer = pNewChunkSampleBuffer;
                  memcpy(pNewChunk->pSampleBuffer,
                         pChunk->pSampleBuffer + (IntersectEnd+1 - pChunk->VirtPosStart),
                         (pNewChunk->VirtPosEnd - pNewChunk->VirtPosStart + 1) * sizeof(WaWESample_t));
                  // New chunk is ready, now modify old chunk!
                  pOldChunkSampleBuffer = dbg_realloc(pChunk->pSampleBuffer,
                                                      (pChunk->VirtPosEnd - pChunk->VirtPosStart + 1) * sizeof(WaWESample_t));
                  if (!pOldChunkSampleBuffer)
                  {
                    // Not enough memory! Ouch!
                    // Ok, no problem, the memory will not be resized to smaller.. Sigh.
                  } else
                  {
                    pChunk->pSampleBuffer = pOldChunkSampleBuffer;
                  }
                }
              } else
              {
                // Modify Original chunk
#ifdef DEBUG_BUILD
  //	      printf("Modify Original chunk!\n"); fflush(stdout);
#endif
                pNewChunk->pPlugin = pChunk->pPlugin;
                pNewChunk->hFile = pChunk->hFile;
                pNewChunk->iChannelNum = pChunk->iChannelNum;
                pNewChunk->RealPosStart =
                  pChunk->RealPosStart +
                  (pNewChunk->VirtPosStart - pChunk->VirtPosStart) *
                  (pChunk->hFile->iBits/8) *
                  (pChunk->hFile->iChannels);
              }
            }
            (*pCouldDeleteSize)+= (IntersectEnd - IntersectStart + 1);
            pChunk = pChunk->pNext;
          }
        } else
        pChunk = pChunk->pNext;
      }
  
#ifdef DEBUG_BUILD
  //    printf("[DeleteSample] : Before checks\n");
  //    debug_ShowChunkList(pChannel);
#endif
  
  
      // Now make sure the list of chunks remains continuous!
#ifdef DEBUG_BUILD
  //    printf("Make sure the list of chunks remains continuous!\n"); fflush(stdout);
#endif
  
      pChunk = pChannel->pChunkListHead;
      if (pChunk)
      {
        // Check if the first chunk starts from 0!
        if (pChunk->VirtPosStart!=0)
        {
          pChunk->VirtPosEnd-=pChunk->VirtPosStart;
          pChunk->VirtPosStart = 0;
        }
        pChunk = pChunk->pNext;
      }
#ifdef DEBUG_BUILD
  //    printf("Now check all the next chunks!\n"); fflush(stdout);
#endif
  
      // Now check all the next chunks!
      while (pChunk)
      {
        WaWEChunk_p pPrev;
        WaWEPosition_t NeededStart;
  
        pPrev = pChunk->pPrev;
        if (pPrev)
        {
          NeededStart = pPrev->VirtPosEnd + 1;
          if (pChunk->VirtPosStart > NeededStart)
          {
            pChunk->VirtPosEnd -= (pChunk->VirtPosStart - NeededStart);
            pChunk->VirtPosStart = NeededStart;
          }
        }
        pChunk = pChunk->pNext;
      }
  
#ifdef DEBUG_BUILD
  //    printf("[DeleteSample] : Before simplify\n");
  //    debug_ShowChunkList(pChannel);
#endif
  
      // Simplify chunklist
      internal_DeleteInvalidChunks(pChannel);
      internal_ConcatenateNeightbourousChunks(pChannel);
#ifdef DEBUG_BUILD
//      printf("[DeleteSample] : After\n");
//      debug_ShowChunkList(pChannel);
#endif
  
      if (*pCouldDeleteSize)
      {
        // Update channel length!
        pChannel->Length = 0;
        pChunk = pChannel->pChunkListHead;
        while ((pChunk) && (pChunk->pNext)) pChunk = pChunk->pNext;
        if (pChunk)
          pChannel->Length = pChunk->VirtPosEnd+1;
  
        pChannel->bRedrawNeeded = 1; // Note that channel data changed, redraw needed!
        pChannel->bModified = 1;
        pChannel->pParentChannelSet->bModified = 1;

        // Now update selected region list, if there is any!
        if (pChannel->pShownSelectedRegionListHead)
        {
          WaWESelectedRegion_p pRegion = pChannel->pShownSelectedRegionListHead;
          int irc;
          WaWEPosition_t IntersectStart, IntersectEnd;

          while (pRegion)
          {
            irc=CalculateRegionIntersect(pRegion->Start, pRegion->End,
                                         StartPos, StartPos+*pCouldDeleteSize-1,
                                         &IntersectStart, &IntersectEnd);
            if (!irc)
            {
              // No intersect with this selected region. Let's see if we have to shift it or not!
              if (pRegion->Start>=StartPos)
              {
                pRegion->Start-=*pCouldDeleteSize;
                pRegion->End-=*pCouldDeleteSize;
              }
            } else
            if (irc)
            {
              pChannel->ShownSelectedLength-=(IntersectEnd-IntersectStart+1);

              if ((IntersectStart == pRegion->Start) && (IntersectEnd == pRegion->End))
              {
                // The whole selected region has to be deleted!
                WaWESelectedRegion_p pPrev, pNext;
                pPrev = pRegion->pPrev;
                pNext = pRegion->pNext;

                // Unchain from linked list
                if (pNext)
                  pNext->pPrev = pRegion->pPrev;

                if (pPrev)
                  pPrev->pNext = pRegion->pNext;
                else
                  pChannel->pShownSelectedRegionListHead = pRegion->pNext;

                dbg_free(pRegion);
                pChannel->iShownSelectedPieces--;
              } else
              {
                // Some has to be deleted from region, but not all
                if (pRegion->Start>=IntersectStart)
                {
                  pRegion->Start-=(IntersectEnd-IntersectStart+1);
                  if (pRegion->Start<StartPos)
                    pRegion->Start = StartPos;
                }
                pRegion->End-=(IntersectEnd-IntersectStart+1);
              }

            }

            pRegion = pRegion->pNext;
          }

        }

      }
  
      DosReleaseMutexSem(pChannel->hmtxUseChunkList);
    } else iResult = WAWEHELPER_ERROR_INTERNALERROR;
    DosReleaseMutexSem(pChannel->hmtxUseShownSelectedRegionList);
  } else iResult = WAWEHELPER_ERROR_INTERNALERROR;

  internal_UpdateChannelsetLength(pChannel->pParentChannelSet);

  // Call registered event callbacks
  if (*pCouldDeleteSize)
  {
    WaWEEventDesc_t EventDesc;

    EventDesc.iEventType = WAWE_EVENT_POSTSAMPLEDELETE;
    EventDesc.EventSpecificInfo.SampleModInfo.pModifiedChannel = pChannel;
    EventDesc.EventSpecificInfo.SampleModInfo.ModificationStartPos = StartPos;
    EventDesc.EventSpecificInfo.SampleModInfo.ModificationSize = *pCouldDeleteSize;

    CallEventCallbacks(&EventDesc);
  }

  return iResult;
}

int  WAWECALL WaWEHelper_Channel_SetCurrentPosition(WaWEChannel_p  pChannel,
                                                   WaWEPosition_t NewCurrentPosition)
{
  WaWEPosition_t OldCurrentPos;
  int iResult = WAWEHELPER_NOERROR;
  WaWEWorker1Msg_SetChannelCurrentPosition_p pWorkerMsg;

  if (!pChannel)
    return WAWEHELPER_ERROR_INVALIDPARAMETER;

  pWorkerMsg = (WaWEWorker1Msg_SetChannelCurrentPosition_p) dbg_malloc(sizeof(WaWEWorker1Msg_SetChannelCurrentPosition_t));
  if (pWorkerMsg)
  {
    pWorkerMsg->pChannel = pChannel;
    if (DosRequestMutexSem(pChannel->hmtxUseRequiredFields, 1000)==NO_ERROR)
    {
      OldCurrentPos = pChannel->RequiredCurrentPosition;
      pChannel->RequiredCurrentPosition = NewCurrentPosition;
      if (!AddNewWorker1Msg(WORKER1_MSG_SETCHANNELCURRENTPOSITION, sizeof(WaWEWorker1Msg_SetChannelCurrentPosition_t), pWorkerMsg))
      {
        // Error adding the worker message to the queue, the queue might be full?
        dbg_free(pWorkerMsg);
        iResult = WAWEHELPER_ERROR_INTERNALERROR;
      }
      DosReleaseMutexSem(pChannel->hmtxUseRequiredFields);
    } else
    {
      dbg_free(pWorkerMsg);
      iResult = WAWEHELPER_ERROR_OUTOFMEMORY;
    }
  } else
  {
    iResult = WAWEHELPER_ERROR_INTERNALERROR;
  }

  // Call registered event callbacks
  if (iResult==WAWEHELPER_NOERROR)
  {
    WaWEEventDesc_t EventDesc;

    EventDesc.iEventType = WAWE_EVENT_POSTCHANNELPOSITIONCHANGE;
    EventDesc.EventSpecificInfo.ChannelPosChangeInfo.pChannel = pChannel;
    EventDesc.EventSpecificInfo.ChannelPosChangeInfo.NewCurrentPos = NewCurrentPosition;
    EventDesc.EventSpecificInfo.ChannelPosChangeInfo.OldCurrentPos = OldCurrentPos;

    CallEventCallbacks(&EventDesc);
  }

  return iResult;
}

int  WAWECALL WaWEHelper_Channel_Changed(WaWEChannel_p pChannel)
{
  if (pChannel)
  {
    WaWEEventDesc_t EventDesc;

    // Note that the channel and the channel-set have been modified!
#ifdef DEBUG_BUILD
//    printf("[WaWEHelper_Channel_Changed] : Channel [%s] has changed!\n", pChannel->achName); fflush(stdout);
#endif
    pChannel->bModified = TRUE;
    pChannel->bRedrawNeeded = TRUE;
    pChannel->pParentChannelSet->bModified = TRUE;

    EventDesc.iEventType = WAWE_EVENT_POSTCHANNELCHANGE;
    EventDesc.EventSpecificInfo.ChannelModInfo.pModifiedChannel = pChannel;
    EventDesc.EventSpecificInfo.ChannelModInfo.pParentChannelSet = pChannel->pParentChannelSet;

    CallEventCallbacks(&EventDesc);
    return WAWEHELPER_NOERROR;
  }
  return WAWEHELPER_ERROR_INVALIDPARAMETER;
}

// Functions related to channel-sets:

int  WAWECALL WaWEHelper_ChannelSet_CreateChannel(WaWEChannelSet_p pChannelSet,
                                                  WaWEChannel_p  pInsertAfterThis,
                                                  char *pchProposedName,
                                                  WaWEChannel_p *ppChannel)
{
  WaWEChannel_p pPrevChannel, pTempChannel;
  int iCurrentEditAreaHeight;
  int iNewID;
  int iNumOfChannelsInChannelSet;

#ifdef DEBUG_BUILD
  printf("[WaWEHelper] : CreateChannel: Enter\n"); fflush(stdout);
#endif

  // First check parameters!
  if ((!pChannelSet) || (!ppChannel))
    return WAWEHELPER_ERROR_INVALIDPARAMETER;

  if (pChannelSet->lUsageCounter>0)
    return WAWEHELPER_ERROR_ELEMENTINUSE;

  if (DosRequestMutexSem(pChannelSet->hmtxUseChannelList, SEM_INDEFINITE_WAIT)!=NO_ERROR)
    return WAWEHELPER_ERROR_INTERNALERROR;

  // Get current number of channels in Channel-Set
  iNumOfChannelsInChannelSet = GetNumOfChannelsFromChannelSet(pChannelSet);

  // Find previous channel in channel-set
  if (pInsertAfterThis == WAWECHANNEL_TOP)
  {
    // Will insert to the beginning
    pPrevChannel = NULL;
  } else
  if (pInsertAfterThis == WAWECHANNEL_BOTTOM)
  {
    // Will insert to the end!
    pPrevChannel = pChannelSet->pChannelListHead;
    while ((pPrevChannel) && (pPrevChannel->pNext))
      pPrevChannel = pPrevChannel->pNext;
  } else
  {
    // Find the one we have to insert after!
    pPrevChannel = pChannelSet->pChannelListHead;
    while ((pPrevChannel) && (pPrevChannel!=pInsertAfterThis))
      pPrevChannel = pPrevChannel->pNext;
  }

  // Allocate memory for structure!
  *ppChannel = (WaWEChannel_p) dbg_malloc(sizeof(WaWEChannel_t));
  if (!(*ppChannel))
    return WAWEHELPER_ERROR_OUTOFMEMORY;

  // Fill entries of structure!

  (*ppChannel)->usStructureSize = sizeof(WaWEChannel_t);
  (*ppChannel)->pParentChannelSet = pChannelSet;
  memcpy(&((*ppChannel)->VirtualFormat), &(pChannelSet->OriginalFormat), sizeof((*ppChannel)->VirtualFormat));

  // Set the channel ID
  // For this, we find an unused ID
  iNewID=1;
  while (iNewID<10000)
  {
    pTempChannel = pChannelSet->pChannelListHead;
    while ((pTempChannel) && (pTempChannel->iChannelID!=iNewID))
      pTempChannel = pTempChannel->pNext;
    // If there is no channel with ID 'i', then break!
    if (!pTempChannel) break;
    iNewID++;
  }
  (*ppChannel)->iChannelID = iNewID;

  // Set the channel name!
  // Use the proposed name, if available, or generate one if there is no
  // proposed name.
  if (pchProposedName)
    snprintf((*ppChannel)->achName, WAWE_CHANNEL_NAME_LEN,
             "%s", pchProposedName);
  else
    snprintf((*ppChannel)->achName, WAWE_CHANNEL_NAME_LEN,
             "Channel %d", (*ppChannel)->iChannelID);

  (*ppChannel)->CurrentPosition = 0;
  DosCreateMutexSem(NULL,                                   // Name
                    &((*ppChannel)->hmtxUseChunkList),          // Pointer to handle
                    0L,                                     // Non-shared
                    FALSE);                                 // Unowned
  DosCreateMutexSem(NULL,                                   // Name
                    &((*ppChannel)->hmtxUseShownSelectedRegionList), // Pointer to handle
                    0L,                                     // Non-shared
                    FALSE);                                 // Unowned
  DosCreateMutexSem(NULL,                                   // Name
                    &((*ppChannel)->hmtxUseRequiredShownSelectedRegionList), // Pointer to handle
                    0L,                                     // Non-shared
                    FALSE);                                 // Unowned
  DosCreateMutexSem(NULL,                                   // Name
                    &((*ppChannel)->hmtxUseLastValidSelectedRegionList), // Pointer to handle
                    0L,                                     // Non-shared
                    FALSE);                                 // Unowned
  DosCreateMutexSem(NULL,                                   // Name
                    &((*ppChannel)->hmtxUseRequiredFields),     // Pointer to handle
                    0L,                                     // Non-shared
                    FALSE);                                 // Unowned
  (*ppChannel)->pShownSelectedRegionListHead = NULL;
  (*ppChannel)->ShownSelectedLength = 0;
  (*ppChannel)->iShownSelectedPieces = 0;
  (*ppChannel)->pRequiredShownSelectedRegionListHead = NULL;
  (*ppChannel)->pLastValidSelectedRegionListHead = NULL;
  (*ppChannel)->bSelecting = FALSE; // Currently not in selection mode
  (*ppChannel)->SelectionStartSamplePos = 0;
  (*ppChannel)->bRedrawNeeded = 0;

  (*ppChannel)->Length = 0;

  // GUI stuff
  (*ppChannel)->pEditorPlugin = GetDefaultEditorPlugin();
  DosCreateMutexSem(NULL,                                   // Name
                    &((*ppChannel)->hmtxUseImageBuffer),    // Pointer to handle
                    0L,                                     // Non-shared
                    FALSE);                                 // Unowned
  (*ppChannel)->FirstSamplePos = -1; // Nothing is shown yet!
  (*ppChannel)->LastSamplePos = -1;
  (*ppChannel)->RequiredFirstSamplePos = 0;  // We'd like all to be shown,
  (*ppChannel)->RequiredLastSamplePos = -1;  // but these will be set at a different place.
  (*ppChannel)->hpsCurrentImageBuffer = NULLHANDLE;
  (*ppChannel)->hwndEditAreaWindow = NULLHANDLE;
  (*ppChannel)->bWindowCreated = 0;

  (*ppChannel)->bModified = 0;

  (*ppChannel)->pChunkListHead = NULL; // Empty!

  (*ppChannel)->pNext = NULL;
  (*ppChannel)->pPrev = NULL;

  // and link into list of channels
  if (!pPrevChannel)
  {
    (*ppChannel)->pNext = pChannelSet->pChannelListHead;
    if (pChannelSet->pChannelListHead)
      pChannelSet->pChannelListHead->pPrev = (*ppChannel);

    pChannelSet->pChannelListHead = (*ppChannel);
  } else
  {
    WaWEChannel_p pNext = pPrevChannel->pNext;

    pPrevChannel->pNext = (*ppChannel);
    (*ppChannel)->pPrev = pPrevChannel;
    (*ppChannel)->pNext = pNext;
    if (pNext)
      pNext->pPrev = (*ppChannel);
  }

  // Calculate Edit Area height!
  if (iNumOfChannelsInChannelSet>0)
  {
    // Use the same channel height as we already have in the channel-set!
    iCurrentEditAreaHeight =
      (pChannelSet->iHeight -
       iChannelSetHeaderHeight -
       iScrollBarHeight-
       iSizerHeight-
       iNumOfChannelsInChannelSet*iChannelHeaderHeight) / iNumOfChannelsInChannelSet;
#ifdef DEBUG_BUILD
    printf("[WaWEHelper] : Using editareaheight of prevs: %d\n", iCurrentEditAreaHeight); fflush(stdout);
#endif
    // Make channel-set a bit bigger!
    pChannelSet->iHeight+=iChannelHeaderHeight + iCurrentEditAreaHeight;
  } else
  {
    // Use the default height, if there is no channel in channel-set yet!
    iCurrentEditAreaHeight = iChannelEditAreaHeight;
#ifdef DEBUG_BUILD
    printf("[WaWEHelper] : Using default editareaheight: %d\n", iCurrentEditAreaHeight); fflush(stdout);
#endif

    // Make channel-set that big to contain exactly one default sized channel
    pChannelSet->iHeight=
      iChannelSetHeaderHeight +
      iScrollBarHeight+
      iSizerHeight+
      iChannelHeaderHeight + iCurrentEditAreaHeight;
  }

  // The creation of the window itself will be done by the
  // PM thread, so do it there!
  if (GetCurrentThreadID() == tidPM)
  {
#ifdef DEBUG_BUILD
    printf("[WaWEHelper] : Calling CreateNewChannelWindow to create window\n"); fflush(stdout);
#endif
    CreateNewChannelWindow(*ppChannel, iCurrentEditAreaHeight);
  }
  else
  {
#ifdef DEBUG_BUILD
    printf("[WaWEHelper] : Asking PM-thread to create window\n"); fflush(stdout);
#endif
    WinPostMsg(hwndClient, WM_CREATECWINDOW, (MPARAM) (*ppChannel), (MRESULT) iCurrentEditAreaHeight);
    // Wait until that thread creates the channel window!
    while ((!(*ppChannel)->bWindowCreated))
      DosSleep(32);
  }

#ifdef DEBUG_BUILD
  printf("[WaWEHelper] : Ok, PM-thread created window\n"); fflush(stdout);
#endif

  pChannelSet->iNumOfChannels = iNumOfChannelsInChannelSet+1;

  // Note that the channel-set have been changed!
  pChannelSet->bRedrawNeeded = 1;
  pChannelSet->bModified = 1;

  DosReleaseMutexSem(pChannelSet->hmtxUseChannelList);

#ifdef DEBUG_BUILD
  printf("[WaWEHelper] : CreateChannel: Done\n"); fflush(stdout);
#endif

  // Call registered event callbacks
  {
    WaWEEventDesc_t EventDesc;

    EventDesc.iEventType = WAWE_EVENT_POSTCHANNELCREATE;
    EventDesc.EventSpecificInfo.ChannelModInfo.pModifiedChannel = *ppChannel;
    EventDesc.EventSpecificInfo.ChannelModInfo.pParentChannelSet = pChannelSet;

    CallEventCallbacks(&EventDesc);
  }

  return WAWEHELPER_NOERROR;
}

int  WAWECALL WaWEHelper_ChannelSet_DeleteChannel(WaWEChannel_p pChannel)
{
  WaWEChannelSet_p pChannelSet;
  WaWEChannel_p pTempChannel;
  WaWESelectedRegion_p pSRegionTemp;
  WaWEChunk_p pChunkTemp;
  SWP swpEditAreaWindow;
  SWP swpHeaderBgWindow;

#ifdef DEBUG_BUILD
  printf("[WaWEHelper] : DeleteChannel: Enter\n"); fflush(stdout);
#endif

  // First check parameters!
  if (!pChannel)
    return WAWEHELPER_ERROR_INVALIDPARAMETER;

  // Call registered event callbacks
  {
    WaWEEventDesc_t EventDesc;

    EventDesc.iEventType = WAWE_EVENT_PRECHANNELDELETE;
    EventDesc.EventSpecificInfo.ChannelModInfo.pModifiedChannel = pChannel;
    EventDesc.EventSpecificInfo.ChannelModInfo.pParentChannelSet = pChannel->pParentChannelSet;

    CallEventCallbacks(&EventDesc);
  }

  pChannelSet = pChannel->pParentChannelSet;

  if (pChannelSet->lUsageCounter>0)
    return WAWEHELPER_ERROR_ELEMENTINUSE;

  if (DosRequestMutexSem(pChannelSet->hmtxUseChannelList, SEM_INDEFINITE_WAIT)!=NO_ERROR)
    return WAWEHELPER_ERROR_INTERNALERROR;


#ifdef DEBUG_BUILD
//  printf("    Unlinking channel from channel-list\n"); fflush(stdout);
#endif

  // First unlink channel from channel-list!
  if (pChannel->pPrev)
  {
    pTempChannel = pChannel->pPrev;
    pTempChannel->pNext = pChannel->pNext;
  } else
    pChannelSet->pChannelListHead = pChannel->pNext;
  if (pChannel->pNext)
  {
    pTempChannel = pChannel->pNext;
    pTempChannel->pPrev = pChannel->pPrev;
  }

  // Then free resources of channel!

#ifdef DEBUG_BUILD
//        printf("    Free list of shown selected regions!\n"); fflush(stdout);
#endif

  // dbg_free list of shown selected regions in this channel
  while (pChannel->pShownSelectedRegionListHead)
  {
    pSRegionTemp = pChannel->pShownSelectedRegionListHead;
    pChannel->pShownSelectedRegionListHead = pChannel->pShownSelectedRegionListHead->pNext;
    dbg_free(pSRegionTemp);
  }

#ifdef DEBUG_BUILD
//        printf("    Free list of req. shown selected regions!\n"); fflush(stdout);
#endif

  // dbg_free list of required shown selected regions in this channel
  while (pChannel->pRequiredShownSelectedRegionListHead)
  {
    pSRegionTemp = pChannel->pRequiredShownSelectedRegionListHead;
    pChannel->pRequiredShownSelectedRegionListHead = pChannel->pRequiredShownSelectedRegionListHead->pNext;
    dbg_free(pSRegionTemp);
  }

#ifdef DEBUG_BUILD
//        printf("    Free list of last valid selected regions!\n"); fflush(stdout);
#endif

  // dbg_free list of last valid selected regions in this channel
  while (pChannel->pLastValidSelectedRegionListHead)
  {
    pSRegionTemp = pChannel->pLastValidSelectedRegionListHead;
    pChannel->pLastValidSelectedRegionListHead = pChannel->pLastValidSelectedRegionListHead->pNext;
    dbg_free(pSRegionTemp);
  }

#ifdef DEBUG_BUILD
//  printf("    Free chunks!\n"); fflush(stdout);
#endif

  // dbg_free list of chunks in this channel
  while (pChannel->pChunkListHead)
  {
    pChunkTemp = pChannel->pChunkListHead;
    pChannel->pChunkListHead = pChannel->pChunkListHead->pNext;
    if (pChunkTemp->iChunkType == WAWE_CHUNKTYPE_MODIFIED)
    {
      if (pChunkTemp->pSampleBuffer)
        dbg_free(pChunkTemp->pSampleBuffer);
    }
    dbg_free(pChunkTemp);
  }
#ifdef DEBUG_BUILD
//  printf("    Delete other resources!\n"); fflush(stdout);
#endif

  // Delete other resources in Channel:
  DosCloseMutexSem(pChannel->hmtxUseRequiredFields);
  DosCloseMutexSem(pChannel->hmtxUseLastValidSelectedRegionList);
  DosCloseMutexSem(pChannel->hmtxUseRequiredShownSelectedRegionList);
  DosCloseMutexSem(pChannel->hmtxUseShownSelectedRegionList);
  DosCloseMutexSem(pChannel->hmtxUseImageBuffer);
  DosCloseMutexSem(pChannel->hmtxUseChunkList);

  if (pChannel->hpsCurrentImageBuffer)
  {
    HDC hdc;
    HBITMAP hbmp;
    hdc = GpiQueryDevice(pChannel->hpsCurrentImageBuffer);
    hbmp = GpiSetBitmap(pChannel->hpsCurrentImageBuffer, NULLHANDLE);
    if (hbmp)
    {
      GpiDeleteBitmap(hbmp);
    }
    GpiDestroyPS(pChannel->hpsCurrentImageBuffer); pChannel->hpsCurrentImageBuffer=NULLHANDLE;
    DevCloseDC(hdc);
  }

  // Destroy draw window and channel header window
  // Meanwhile remember its heights
  WinQueryWindowPos(pChannel->hwndEditAreaWindow, &swpEditAreaWindow);
  WinQueryWindowPos(WinWindowFromID(pChannelSet->hwndChannelSet, DID_CHANNELSET_CH_HEADERBG_BASE + pChannel->iChannelID),
                    &swpHeaderBgWindow);

#ifdef DEBUG_BUILD
//  printf("    Destroy windows!\n"); fflush(stdout);
#endif

  PMThread_WinDestroyWindow(pChannel->hwndEditAreaWindow);
  // The next will destroy the subwindows of headerbackground, too!
  PMThread_WinDestroyWindow(WinWindowFromID(pChannelSet->hwndChannelSet, DID_CHANNELSET_CH_HEADERBG_BASE + pChannel->iChannelID));

#ifdef DEBUG_BUILD
//  printf("    Free pChannel!\n"); fflush(stdout);
#endif

  // Everything dbg_freed, so dbg_free the structure itself!
  dbg_free(pChannel);

#ifdef DEBUG_BUILD
//  printf("    Modify pChannelSet!\n"); fflush(stdout);
#endif

  // Decrease channel-set height by header height and draw-window height!
  pChannelSet->iHeight-=swpHeaderBgWindow.cy;
  pChannelSet->iHeight-=swpEditAreaWindow.cy;
  pChannelSet->iNumOfChannels = GetNumOfChannelsFromChannelSet(pChannelSet);

  // Note that the channel-set have been changed!
  pChannelSet->bRedrawNeeded = 1;
  pChannelSet->bModified = 1;

  DosReleaseMutexSem(pChannelSet->hmtxUseChannelList);

  internal_UpdateChannelsetLength(pChannelSet);

#ifdef DEBUG_BUILD
  printf("[WaWEHelper] : DeleteChannel: Done\n"); fflush(stdout);
#endif

  // Call registered event callbacks
  {
    WaWEEventDesc_t EventDesc;

    EventDesc.iEventType = WAWE_EVENT_POSTCHANNELDELETE;
    EventDesc.EventSpecificInfo.ChannelModInfo.pModifiedChannel = pChannel;
    EventDesc.EventSpecificInfo.ChannelModInfo.pParentChannelSet = pChannelSet;

    CallEventCallbacks(&EventDesc);
  }

  return WAWEHELPER_NOERROR;
}

int  WAWECALL WaWEHelper_ChannelSet_MoveChannel(WaWEChannel_p pChannel,
                                                WaWEChannel_p pMoveAfterThis)
{
  WaWEChannel_p pTempChannel;
  WaWEChannelSet_p pChannelSet;

  if ((!pChannel) || (!pMoveAfterThis))
    return WAWEHELPER_ERROR_INVALIDPARAMETER; // Invalid parameter!

  pChannelSet = pChannel->pParentChannelSet;

  if (pChannelSet->lUsageCounter>0)
    return WAWEHELPER_ERROR_ELEMENTINUSE;

  if (DosRequestMutexSem(pChannelSet->hmtxUseChannelList, SEM_INDEFINITE_WAIT)!=NO_ERROR)
  {
    return WAWEHELPER_ERROR_INTERNALERROR;
  }

  // First unlink from list
  if (pChannel->pPrev)
  {
    pTempChannel = pChannel->pPrev;
    pTempChannel->pNext = pChannel->pNext;
  } else
    pChannelSet->pChannelListHead = pChannel->pNext;

  if (pChannel->pNext)
  {
    pTempChannel = pChannel->pNext;
    pTempChannel->pPrev = pChannel->pPrev;
  }

  // Then relink it at the new position!
  if (pMoveAfterThis == WAWECHANNEL_TOP)
  {
    // Move to top!
    pChannel->pPrev = NULL;
    pChannel->pNext = pChannelSet->pChannelListHead;
    pTempChannel = pChannelSet->pChannelListHead;
    if (pTempChannel)
      pTempChannel->pPrev = pChannel;
    pChannelSet->pChannelListHead = pChannel;
  } else
  if (pMoveAfterThis == WAWECHANNEL_BOTTOM)
  {
    WaWEChannel_p pLast, pPrev;
    pPrev = NULL;
    pLast = pChannelSet->pChannelListHead;
    while (pLast)
    {
      pPrev = pLast;
      pLast = pLast->pNext;
    }

    if (pPrev)
    {
      pPrev->pNext = pChannel;
      pChannel->pPrev = pPrev;
      pChannel->pNext = NULL;
    } else
    {
      pChannel->pPrev = NULL;
      pChannel->pNext = pChannelSet->pChannelListHead;
      pChannelSet->pChannelListHead = pChannel;
    }
  } else
  {
    WaWEChannel_p pNext;

    // Move after a given one!
    pNext = pMoveAfterThis->pNext;
    pChannel->pPrev = pMoveAfterThis;
    pChannel->pNext = pNext;
    pMoveAfterThis->pNext = pChannel;
    if (pNext)
      pNext->pPrev = pChannel;
  }

  // Note that the channel-set have been changed!
  pChannelSet->bRedrawNeeded = 1;
  pChannelSet->bModified = 1;

  DosReleaseMutexSem(pChannelSet->hmtxUseChannelList);

  // Call registered event callbacks
  {
    WaWEEventDesc_t EventDesc;

    EventDesc.iEventType = WAWE_EVENT_POSTCHANNELMOVE;
    EventDesc.EventSpecificInfo.ChannelModInfo.pModifiedChannel = pChannel;
    EventDesc.EventSpecificInfo.ChannelModInfo.pParentChannelSet = pChannelSet;

    CallEventCallbacks(&EventDesc);
  }

  return WAWEHELPER_NOERROR;
}

int  WAWECALL WaWEHelper_ChannelSet_ReadPackedSamples(WaWEChannelSet_p  pChannelSet,
                                                      WaWEPosition_t    StartPos,
                                                      WaWESize_t        ReadSize,
                                                      char             *pchSampleBuffer,
                                                      WaWEFormat_p      pRequestedFormat,
                                                      WaWESize_p        pCouldReadSize)
{
  WaWEChannel_p pChannel;
  int iRequestedBytesPerSample;
  int iChannelNum;
  int iNumOfChannels;
  WaWESample_p  pReadBuf;
  WaWESize_t    CouldRead;
  WaWEPosition_t Pos;
  WaWESample_t  Sample;


  if ((!pChannelSet) || (!pchSampleBuffer) || (!pCouldReadSize) || (!pRequestedFormat))
    return WAWEHELPER_ERROR_INVALIDPARAMETER; // Invalid parameter!

  if (ReadSize==0)
  {
    *pCouldReadSize = 0;
    return WAWEHELPER_NOERROR;
  }

  iRequestedBytesPerSample = (pRequestedFormat->iBits + 7)/8;

  pReadBuf = (WaWESample_p) dbg_malloc_huge(sizeof(WaWESample_t)*ReadSize);
  if (!pReadBuf)
  {
#ifdef DEBUG_BUILD
    printf("[WaWEHelper] : ReadPackedSamples: Out of memory\n"); fflush(stdout);
#endif
    return WAWEHELPER_ERROR_OUTOFMEMORY;
  }

  // Calculate number of channels inside channelset!
  iNumOfChannels = 0;
  pChannel = pChannelSet->pChannelListHead;
  while (pChannel)
  {
    iNumOfChannels++;
    pChannel = pChannel->pNext;
  }
  if (iNumOfChannels==0)
  {
    *pCouldReadSize = 0;
    dbg_free(pReadBuf);
#ifdef DEBUG_BUILD
    printf("[WaWEHelper] : No channels in channel-set!\n"); fflush(stdout);
#endif
    return WAWEHELPER_NOERROR;
  }

  // Now read samples from all channels, convert them, and store in buffer!
  *pCouldReadSize = 0;
  pChannel = pChannelSet->pChannelListHead;
  iChannelNum = 0;
  while (pChannel)
  {
#ifdef DEBUG_BUILD
//    printf("[WaWEHelper] : Read channel %p (chunk ch id %d)\n", pChannel, pChannel->pChunkListHead->iChannelNum);
#endif
    // Read channel samples
    WaWEHelper_Channel_ReadSample(pChannel,
                                  StartPos,
                                  ReadSize,
                                  pReadBuf,
                                  &CouldRead);

    if (CouldRead>(*pCouldReadSize))
      *pCouldReadSize = CouldRead;

    if (CouldRead>0)
    {
      // Sort out samples and convert to target format
      for (Pos=0; Pos<CouldRead; Pos++)
      {
        Sample = pReadBuf[Pos];
        if (!(pRequestedFormat->iIsSigned))
        {
          // Convert to unsigned
          Sample+=0x80000000; // Make the 32bits sample unsigned
        }
        // Convert to requested bit depth
        if (iRequestedBytesPerSample<4)
          Sample>>=(32-pRequestedFormat->iBits);
        // Move sample to target buffer
        memcpy(pchSampleBuffer+
               Pos*iNumOfChannels*iRequestedBytesPerSample+
               iChannelNum*iRequestedBytesPerSample,
               &Sample,
               iRequestedBytesPerSample);
      }
    }

    // Go to next channel!
    pChannel = pChannel->pNext;
    iChannelNum++;
  }

  dbg_free(pReadBuf);

  // Call registered event callbacks
  {
    WaWEEventDesc_t EventDesc;

    EventDesc.iEventType = WAWE_EVENT_POSTCHANNELSETREADPACKEDSAMPLES;
    EventDesc.EventSpecificInfo.ChannelSetReadPackedSamplesInfo.pChannelSet = pChannelSet;
    EventDesc.EventSpecificInfo.ChannelSetReadPackedSamplesInfo.StartPos = StartPos;
    EventDesc.EventSpecificInfo.ChannelSetReadPackedSamplesInfo.Size = *pCouldReadSize;

    CallEventCallbacks(&EventDesc);
  }

  return WAWEHELPER_NOERROR;
}

int  WAWECALL WaWEHelper_ChannelSet_Changed(WaWEChannelSet_p pChannelSet)
{
  if (pChannelSet)
  {
    WaWEEventDesc_t EventDesc;

    // Note that the channel-set have been modified!
    pChannelSet->bModified = TRUE;
    pChannelSet->bRedrawNeeded = TRUE;

    EventDesc.iEventType = WAWE_EVENT_POSTCHANNELSETCHANGE;
    EventDesc.EventSpecificInfo.ChannelSetModInfo.pModifiedChannelSet = pChannelSet;

    CallEventCallbacks(&EventDesc);

    return WAWEHELPER_NOERROR;
  }
  return WAWEHELPER_ERROR_INVALIDPARAMETER;
}

int  WAWECALL WaWEHelper_ChannelSet_StartChSetFormatUsage(WaWEChannelSet_p  pChannelSet)
{
  if (!pChannelSet)
    return WAWEHELPER_ERROR_INVALIDPARAMETER;

  return (DosRequestMutexSem(pChannelSet->hmtxUseChannelSetFormat, SEM_INDEFINITE_WAIT)==NO_ERROR);
}

int  WAWECALL WaWEHelper_ChannelSet_StopChSetFormatUsage(WaWEChannelSet_p  pChannelSet, int iChanged)
{
  if (!pChannelSet)
    return WAWEHELPER_ERROR_INVALIDPARAMETER;

  DosReleaseMutexSem(pChannelSet->hmtxUseChannelSetFormat);

  if (iChanged)
  {
    WaWEEventDesc_t EventDesc;

    pChannelSet->bRedrawNeeded = TRUE;
    pChannelSet->bModified     = TRUE;

    EventDesc.iEventType = WAWE_EVENT_POSTCHANNELSETCHANGE;
    EventDesc.EventSpecificInfo.ChannelSetModInfo.pModifiedChannelSet = pChannelSet;

    CallEventCallbacks(&EventDesc);
  }

  return WAWEHELPER_NOERROR;
}


#ifdef DEBUG_BUILD
/*
static void Print_ChannelSetFormat_Contents(WaWEChannelSetFormat_p pChannelSetFormat)
{
  if (!pChannelSetFormat) return;

  printf("Channel set format contents of (%p):\n", *pChannelSetFormat); fflush(stdout);
  if (!(*pChannelSetFormat))
  {
    printf("[EMPTY]\n"); fflush(stdout);
  } else
  {
    int iFormatSpecSize, i;
    char *pchTemp;
    int iSize;

    pchTemp = *pChannelSetFormat;

    memcpy(&iFormatSpecSize, pchTemp, sizeof(iFormatSpecSize));
    printf("iFormatSpecSize : %d\n", iFormatSpecSize); fflush(stdout);
    i = sizeof(iFormatSpecSize); pchTemp+=sizeof(iFormatSpecSize);
    while (i<iFormatSpecSize)
    {
      printf("@%d : ", i);
      memcpy(&iSize, pchTemp, sizeof(iSize));
      pchTemp+=sizeof(iSize); i+=sizeof(iSize);
      printf("(%d)[%s] = ", iSize, pchTemp); fflush(stdout);
      pchTemp+=iSize; i+=iSize;
      memcpy(&iSize, pchTemp, sizeof(iSize));
      pchTemp+=sizeof(iSize); i+=sizeof(iSize);
      printf("(%d)[%s]\n", iSize, pchTemp); fflush(stdout);
      pchTemp+=iSize; i+=iSize;
    }
  }
}
*/
#endif

int  WAWECALL WaWEHelper_ChannelSetFormat_WriteKey(WaWEChannelSetFormat_p pChannelSetFormat,
                                                   char *pchKeyName,
                                                   char *pchKeyValue)
{
  int iFormatSpecSize;
  char *pchNewPtr;
  int iNewSize;
  int iTemp;

  // First check parameters!
  if ((!pChannelSetFormat) || (!pchKeyName) || (!pchKeyValue))
    return WAWEHELPER_ERROR_INVALIDPARAMETER;

#ifdef DEBUG_BUILD
//  printf("[WaWEHelper_WriteKey] : [%s] = [%s] to %p\n", pchKeyName, pchKeyValue, *pChannelSetFormat);
#endif

  // Ok, add string pair to the end of pchChannelSetFormat stuff
  if (!(*pChannelSetFormat))
  {
#ifdef DEBUG_BUILD
//    printf("[WaWEHelper_WriteKey] : Preparing empty area!\n");
#endif

    iFormatSpecSize = sizeof(iFormatSpecSize);
    // There is no pchChannelSetFormat yet, so prepare one!
    (*pChannelSetFormat) = dbg_malloc(iFormatSpecSize);

    if (!(*pChannelSetFormat))
    {
#ifdef DEBUG_BUILD
//      printf("[WaWEHelper_WriteKey] : Out of memory at malloc!\n");
#endif
      return WAWEHELPER_ERROR_OUTOFMEMORY;
    }
#ifdef DEBUG_BUILD
//    printf("[WaWEHelper_WriteKey] : Result of malloc is %p\n", *pChannelSetFormat);
#endif

    memcpy((*pChannelSetFormat), &iFormatSpecSize, sizeof(iFormatSpecSize));
  }

  // There is pchChannelSetFormat.
  // So reallocate memory,
  // and add key+value pair to the end!
  // Get size of this area first!
  memcpy(&iFormatSpecSize, (*pChannelSetFormat), sizeof(iFormatSpecSize));

#ifdef DEBUG_BUILD
//  printf("[WaWEHelper_WriteKey] : FormatSpecSize is %d\n", iFormatSpecSize);
#endif

  iNewSize =
    iFormatSpecSize +
    sizeof(int) + strlen(pchKeyName)+1 +
    sizeof(int) + strlen(pchKeyValue)+1;

  pchNewPtr = dbg_realloc((*pChannelSetFormat),
                          iNewSize);
  if (!pchNewPtr)
  {
#ifdef DEBUG_BUILD
//    printf("[WaWEHelper_WriteKey] : Out of memory at realloc!\n");
#endif
    return WAWEHELPER_ERROR_OUTOFMEMORY;
  }

  (*pChannelSetFormat) = pchNewPtr;

#ifdef DEBUG_BUILD
//  printf("[WaWEHelper_WriteKey] : Reallocated to new size of %d\n", iNewSize);
#endif


#ifdef DEBUG_BUILD
//  printf("[WaWEHelper_WriteKey] : Store key name\n");
#endif

  // Store size of new key name
  iTemp = strlen(pchKeyName)+1;
  memcpy((*pChannelSetFormat)+iFormatSpecSize, &iTemp, sizeof(int));
  // Store new key name
  memcpy((*pChannelSetFormat)+iFormatSpecSize+sizeof(int), pchKeyName, strlen(pchKeyName)+1);

#ifdef DEBUG_BUILD
//  printf("[WaWEHelper_WriteKey] : Store key value\n");
#endif

  // Store size of new key value
  iTemp = strlen(pchKeyValue)+1;
  memcpy((*pChannelSetFormat)+iFormatSpecSize+sizeof(int)+strlen(pchKeyName)+1, &iTemp, sizeof(int));
  // Store new key value
  memcpy((*pChannelSetFormat)+iFormatSpecSize+sizeof(int)+strlen(pchKeyName)+1+sizeof(int), pchKeyValue, strlen(pchKeyValue)+1);

#ifdef DEBUG_BUILD
//  printf("[WaWEHelper_WriteKey] : Store new formatspecsize\n");
#endif

  // Store new formatspec size
  iFormatSpecSize = iNewSize;
  memcpy((*pChannelSetFormat), &iFormatSpecSize, sizeof(iFormatSpecSize));

#ifdef DEBUG_BUILD
//  Print_ChannelSetFormat_Contents(pChannelSetFormat);
#endif
  // Cool, new key+value pair stored!

  return WAWEHELPER_NOERROR;
}

int  WAWECALL WaWEHelper_ChannelSetFormat_GetKeySize(WaWEChannelSetFormat_p pChannelSetFormat,
                                                     char *pchKeyName,
                                                     int iKeyNum,
                                                     int *piKeyValueSize)
{
  char *pchTemp;
  char *pchString;
  int iCurrPos;
  int iFormatSpecSize;
  int iStrSize;
  int iFoundNum;

  // First check parameters!
  if ((!pChannelSetFormat) || (!pchKeyName) || (iKeyNum<0) || (!piKeyValueSize))
    return WAWEHELPER_ERROR_INVALIDPARAMETER;

  if (!(*pChannelSetFormat))
    return WAWEHELPER_ERROR_KEYNOTFOUND;

  iFoundNum = 0;

  pchTemp = (*pChannelSetFormat);
  iCurrPos = 0;

  memcpy(&iFormatSpecSize, pchTemp, sizeof(iFormatSpecSize));
  pchTemp += sizeof(iFormatSpecSize);
  iCurrPos += sizeof(iFormatSpecSize);

  while (iCurrPos<iFormatSpecSize)
  {
    // Get key size
    memcpy(&iStrSize, pchTemp, sizeof(iStrSize));
    pchTemp += sizeof(iStrSize);
    iCurrPos += sizeof(iStrSize);

    // Get key string pointer
    pchString = pchTemp;
    pchTemp += iStrSize;
    iCurrPos += iStrSize;

    // Check key name
    if (!stricmp(pchKeyName, pchString))
    {
      // Huh, found the required key!
      if (iFoundNum>=iKeyNum)
      {
        // And this is the one we need!
        // Return with the results!
        memcpy(&iStrSize, pchTemp, sizeof(iStrSize));
        *piKeyValueSize = iStrSize;
        return WAWEHELPER_NOERROR;
      } else
      {
        // This is not the one we need!
        iFoundNum++;
      }
    }

    // Skip the value size and value name

    // Get value size
    memcpy(&iStrSize, pchTemp, sizeof(iStrSize));
    pchTemp += sizeof(iStrSize);
    iCurrPos += sizeof(iStrSize);

    // Get value string pointer
    pchString = pchTemp;
    pchTemp += iStrSize;
    iCurrPos += iStrSize;
  }

  return WAWEHELPER_ERROR_KEYNOTFOUND;
}

int  WAWECALL WaWEHelper_ChannelSetFormat_ReadKey(WaWEChannelSetFormat_p pChannelSetFormat,
                                                  char *pchKeyName,
                                                  int iKeyNum,
                                                  char *pchResultValueBuff,
                                                  int iResultValueBuffSize)
{
  char *pchTemp;
  char *pchString;
  int iCurrPos;
  int iFormatSpecSize;
  int iStrSize;
  int iFoundNum;

  // First check parameters!
  if ((!pChannelSetFormat) || (!pchKeyName) || (iKeyNum<0) ||
      (!pchResultValueBuff))
    return WAWEHELPER_ERROR_INVALIDPARAMETER;

  if (!(*pChannelSetFormat))
    return WAWEHELPER_ERROR_KEYNOTFOUND;

  iFoundNum = 0;

  pchTemp = (*pChannelSetFormat);
  iCurrPos = 0;

  memcpy(&iFormatSpecSize, pchTemp, sizeof(iFormatSpecSize));
  pchTemp += sizeof(iFormatSpecSize);
  iCurrPos += sizeof(iFormatSpecSize);

  while (iCurrPos<iFormatSpecSize)
  {
    // Get key size
    memcpy(&iStrSize, pchTemp, sizeof(iStrSize));
    pchTemp += sizeof(iStrSize);
    iCurrPos += sizeof(iStrSize);

    // Get key string pointer
    pchString = pchTemp;
    pchTemp += iStrSize;
    iCurrPos += iStrSize;

#ifdef DEBUG_BUILD
//    printf("[WaWEHelper_ReadKey] : Compare [%s] with [%s]\n", pchKeyName, pchString);
#endif
    // Check key name
    if (!stricmp(pchKeyName, pchString))
    {
      // Huh, found the required key!
      if (iFoundNum>=iKeyNum)
      {
        // And this is the one we need!
#ifdef DEBUG_BUILD
//        printf("[WaWEHelper_ReadKey] : Found it!\n");
#endif

        // Return the key value
        // Get value size
        memcpy(&iStrSize, pchTemp, sizeof(iStrSize));
        pchTemp += sizeof(iStrSize);
        iCurrPos += sizeof(iStrSize);

        // Get key string pointer
        pchString = pchTemp;
        pchTemp += iStrSize;
        iCurrPos += iStrSize;

        if (iResultValueBuffSize<iStrSize)
          return WAWEHELPER_ERROR_BUFFERTOOSMALL;
        memcpy(pchResultValueBuff, pchString, iStrSize);
        return WAWEHELPER_NOERROR;
      } else
      {
        // This is not the one we need!
        iFoundNum++;
#ifdef DEBUG_BUILD
//        printf("[WaWEHelper_ReadKey] : Different number (%d instead of %d)!\n", iFoundNum, iKeyNum);
#endif
      }
    }

    // Skip the value size and value name

    // Get value size
    memcpy(&iStrSize, pchTemp, sizeof(iStrSize));
    pchTemp += sizeof(iStrSize);
    iCurrPos += sizeof(iStrSize);

    // Get value string pointer
    pchString = pchTemp;
    pchTemp += iStrSize;
    iCurrPos += iStrSize;
  }

  return WAWEHELPER_ERROR_KEYNOTFOUND;
}

int  WAWECALL WaWEHelper_ChannelSetFormat_DeleteKey(WaWEChannelSetFormat_p pChannelSetFormat,
                                                    char *pchKeyName,
                                                    int iKeyNum)
{
  char *pchTemp;
  char *pchString;
  int iCurrPos;
  int iFormatSpecSize;
  int iStrSize;
  int iFoundNum;

  // First check parameters!
  if ((!pChannelSetFormat) || (!pchKeyName) || (iKeyNum<0))
    return WAWEHELPER_ERROR_INVALIDPARAMETER;

  if (!(*pChannelSetFormat))
    return WAWEHELPER_ERROR_KEYNOTFOUND;

  iFoundNum = 0;

  pchTemp = (*pChannelSetFormat);
  iCurrPos = 0;

  memcpy(&iFormatSpecSize, pchTemp, sizeof(iFormatSpecSize));
  pchTemp += sizeof(iFormatSpecSize);
  iCurrPos += sizeof(iFormatSpecSize);

  while (iCurrPos<iFormatSpecSize)
  {
    // Get key size
    memcpy(&iStrSize, pchTemp, sizeof(iStrSize));
    pchTemp += sizeof(iStrSize);
    iCurrPos += sizeof(iStrSize);

    // Get key string pointer
    pchString = pchTemp;
    pchTemp += iStrSize;
    iCurrPos += iStrSize;

    // Check key name
    if (!stricmp(pchKeyName, pchString))
    {
      // Huh, found the required key!
      if (iFoundNum>=iKeyNum)
      {
        // And this is the one we need!
        // So, delete this key + value!
        int iNameSize, iValueSize;
        char *pchCopyFrom;
        char *pchCopyTo;
        char *pchNewPtr;

        iNameSize = iStrSize;
        // Get value size
        memcpy(&iValueSize, pchTemp, sizeof(iValueSize));
        iCurrPos += sizeof(iStrSize) + iValueSize;
        pchCopyTo = pchTemp - iNameSize - sizeof(iStrSize);
        pchCopyFrom = pchTemp + sizeof(iStrSize) + iValueSize;

        memcpy(pchCopyTo, pchCopyFrom, iFormatSpecSize - iCurrPos);
        iFormatSpecSize -= iNameSize+iValueSize+2*sizeof(iStrSize);

        // Update new FormatSpecSize value
        memcpy((*pChannelSetFormat), &iFormatSpecSize, sizeof(iFormatSpecSize));

        if (iFormatSpecSize == sizeof(iFormatSpecSize))
        {
          // The ChannelSetFormat stuff has became empty!
          // So, free it!
          dbg_free(*pChannelSetFormat);
          *pChannelSetFormat = NULL;

          return WAWEHELPER_NOERROR;
        } else
        {
          // Reallocate memory array to be smaller
          pchNewPtr = (char *) dbg_realloc((*pChannelSetFormat), iFormatSpecSize);
          if (pchNewPtr)
            *pChannelSetFormat = pchNewPtr;
#ifdef DEBUG_BUILD
          else
            printf("[WaWEHelper_DeleteKey] : Warning, could not shrink memory array!\n");
#endif

          return WAWEHELPER_NOERROR;
        }
      } else
      {
        // This is not the one we need!
        iFoundNum++;
      }
    }

    // Skip the value size and value name

    // Get value size
    memcpy(&iStrSize, pchTemp, sizeof(iStrSize));
    pchTemp += sizeof(iStrSize);
    iCurrPos += sizeof(iStrSize);

    // Get value string pointer
    pchString = pchTemp;
    pchTemp += iStrSize;
    iCurrPos += iStrSize;
  }

  return WAWEHELPER_ERROR_KEYNOTFOUND;
}

int  WAWECALL WaWEHelper_ChannelSetFormat_DeleteKeysStartingWith(WaWEChannelSetFormat_p pChannelSetFormat,
                                                                 char *pchKeyName)
{
  char *pchTemp;
  char *pchString;
  int iCurrPos;
  int iFormatSpecSize;
  int iStrSize;

  // First check parameters!
  if ((!pChannelSetFormat) || (!pchKeyName))
    return WAWEHELPER_ERROR_INVALIDPARAMETER;

  // Shortcut, if there are no keys.
  if (!(*pChannelSetFormat))
    return WAWEHELPER_NOERROR;

  pchTemp = (*pChannelSetFormat);
  iCurrPos = 0;

  memcpy(&iFormatSpecSize, pchTemp, sizeof(iFormatSpecSize));
  pchTemp += sizeof(iFormatSpecSize);
  iCurrPos += sizeof(iFormatSpecSize);

  while (iCurrPos<iFormatSpecSize)
  {
    // Get key size
    memcpy(&iStrSize, pchTemp, sizeof(iStrSize));
    pchTemp += sizeof(iStrSize);
    iCurrPos += sizeof(iStrSize);

    // Get key string pointer
    pchString = pchTemp;
    pchTemp += iStrSize;
    iCurrPos += iStrSize;

    // Check key name
    if (strstartswith(pchString, pchKeyName))
    {
      // Huh, found the required key!
      // So, delete this key + value!
      int iNameSize, iValueSize;
      char *pchCopyFrom;
      char *pchCopyTo;
      char *pchNewPtr;

      iNameSize = iStrSize;
      // Get value size
      memcpy(&iValueSize, pchTemp, sizeof(iValueSize));
      iCurrPos += sizeof(iStrSize) + iValueSize;
      pchCopyTo = pchTemp - iNameSize - sizeof(iStrSize);
      pchCopyFrom = pchTemp + sizeof(iStrSize) + iValueSize;

      memcpy(pchCopyTo, pchCopyFrom, iFormatSpecSize - iCurrPos);
      iFormatSpecSize -= iNameSize+iValueSize+2*sizeof(iStrSize);

      // Update new FormatSpecSize value
      memcpy((*pChannelSetFormat), &iFormatSpecSize, sizeof(iFormatSpecSize));

      if (iFormatSpecSize == sizeof(iFormatSpecSize))
      {
        // The ChannelSetFormat stuff has became empty!
        dbg_free(*pChannelSetFormat);
        *pChannelSetFormat = NULL;

        return WAWEHELPER_NOERROR;
      } else
      {
        // Reallocate memory array to be smaller
        pchNewPtr = (char *) dbg_realloc((*pChannelSetFormat), iFormatSpecSize);
        if (pchNewPtr)
          *pChannelSetFormat = pchNewPtr;
#ifdef DEBUG_BUILD
        else
          printf("[WaWEHelper_DeleteKeysStartingWith] : Warning, could not shrink memory array!\n");
#endif
  
        // Update iCurrPos for new buffer
        iCurrPos -= iNameSize + iValueSize + 2*sizeof(iStrSize);
  
        // Setup new pchTemp pointer
        pchTemp = (*pChannelSetFormat) + iCurrPos;

      }
    } else
    {
      // Skip the value size and value name

      // Get value size
      memcpy(&iStrSize, pchTemp, sizeof(iStrSize));
      pchTemp += sizeof(iStrSize);
      iCurrPos += sizeof(iStrSize);

      // Get value string pointer
      pchString = pchTemp;
      pchTemp += iStrSize;
      iCurrPos += iStrSize;
    }
  }

  return WAWEHELPER_NOERROR;
}

int  WAWECALL WaWEHelper_ChannelSetFormat_DeleteKeysAndSubkeys(WaWEChannelSetFormat_p pChannelSetFormat,
                                                               char *pchKeyName)
{
  char *pchTemp;
  char *pchString;
  int iCurrPos;
  int iFormatSpecSize;
  int iStrSize;
  int iKeyToDelete;

  // First check parameters!
  if ((!pChannelSetFormat) || (!pchKeyName))
    return WAWEHELPER_ERROR_INVALIDPARAMETER;

  // Shortcut, if there are no keys.
  if (!(*pChannelSetFormat))
    return WAWEHELPER_NOERROR;

  pchTemp = (*pChannelSetFormat);
  iCurrPos = 0;

  memcpy(&iFormatSpecSize, pchTemp, sizeof(iFormatSpecSize));
  pchTemp += sizeof(iFormatSpecSize);
  iCurrPos += sizeof(iFormatSpecSize);

  while (iCurrPos<iFormatSpecSize)
  {
    // Get key size
    memcpy(&iStrSize, pchTemp, sizeof(iStrSize));
    pchTemp += sizeof(iStrSize);
    iCurrPos += sizeof(iStrSize);

    // Get key string pointer
    pchString = pchTemp;
    pchTemp += iStrSize;
    iCurrPos += iStrSize;

    // Check if this key has to be deleted!
    iKeyToDelete = !stricmp(pchKeyName, pchString); // Same key name -> Delete
    if (!iKeyToDelete) // if starts with "KeyName\"  -> Delete
    {
      char *pchTemp;
      pchTemp = (char *) dbg_malloc(strlen(pchKeyName)+2);
      if (pchTemp)
      {
        sprintf(pchTemp, "%s\\", pchKeyName);
        iKeyToDelete = strstartswith(pchString, pchTemp);
        dbg_free(pchTemp);
      }
    }
    if (!iKeyToDelete) // if starts with "KeyName/"  -> Delete
    {
      char *pchTemp;
      pchTemp = (char *) dbg_malloc(strlen(pchKeyName)+2);
      if (pchTemp)
      {
        sprintf(pchTemp, "%s/", pchKeyName);
        iKeyToDelete = strstartswith(pchString, pchTemp);
        dbg_free(pchTemp);
      }
    }

    // Check key name
    if (iKeyToDelete)
    {
      // So, delete this key + value!
      int iNameSize, iValueSize;
      char *pchCopyFrom;
      char *pchCopyTo;
      char *pchNewPtr;

      iNameSize = iStrSize;
      // Get value size
      memcpy(&iValueSize, pchTemp, sizeof(iValueSize));
      iCurrPos += sizeof(iStrSize) + iValueSize;
      pchCopyTo = pchTemp - iNameSize - sizeof(iStrSize);
      pchCopyFrom = pchTemp + sizeof(iStrSize) + iValueSize;

      memcpy(pchCopyTo, pchCopyFrom, iFormatSpecSize - iCurrPos);
      iFormatSpecSize -= iNameSize+iValueSize+2*sizeof(iStrSize);

      // Update new FormatSpecSize value
      memcpy((*pChannelSetFormat), &iFormatSpecSize, sizeof(iFormatSpecSize));

      if (iFormatSpecSize == sizeof(iFormatSpecSize))
      {
        // The ChannelSetFormat stuff has became empty!
        dbg_free(*pChannelSetFormat);
        *pChannelSetFormat = NULL;

        return WAWEHELPER_NOERROR;
      } else
      {
        // Reallocate memory array to be smaller
        pchNewPtr = (char *) dbg_realloc((*pChannelSetFormat), iFormatSpecSize);
        if (pchNewPtr)
          *pChannelSetFormat = pchNewPtr;
#ifdef DEBUG_BUILD
        else
          printf("[WaWEHelper_DeleteKeysAndSubkeys] : Warning, could not shrink memory array!\n");
#endif
  
        // Update iCurrPos for new buffer
        iCurrPos -= iNameSize + iValueSize + 2*sizeof(iStrSize);
  
        // Setup new pchTemp pointer
        pchTemp = (*pChannelSetFormat) + iCurrPos;

      }
    } else
    {
      // Skip the value size and value name

      // Get value size
      memcpy(&iStrSize, pchTemp, sizeof(iStrSize));
      pchTemp += sizeof(iStrSize);
      iCurrPos += sizeof(iStrSize);

      // Get value string pointer
      pchString = pchTemp;
      pchTemp += iStrSize;
      iCurrPos += iStrSize;
    }
  }

  return WAWEHELPER_NOERROR;
}

int  WAWECALL WaWEHelper_ChannelSetFormat_DeleteAllKeys(WaWEChannelSetFormat_p pChannelSetFormat)
{
  // First check parameters!
  if (!pChannelSetFormat)
    return WAWEHELPER_ERROR_INVALIDPARAMETER;

  if (*pChannelSetFormat)
  {
    dbg_free(*pChannelSetFormat);
    *pChannelSetFormat = NULL;
  }

  return WAWEHELPER_NOERROR;
}

int  WAWECALL WaWEHelper_ChannelSetFormat_GetNumOfKeys(WaWEChannelSetFormat_p pChannelSetFormat, int *piNumOfKeys)
{
  char *pchTemp;
  int iCurrPos;
  int iFormatSpecSize;
  int iStrSize;

  // First check parameters!
  if ((!pChannelSetFormat) || (!piNumOfKeys))
    return WAWEHELPER_ERROR_INVALIDPARAMETER;

  *piNumOfKeys = 0;

  if (*pChannelSetFormat)
  {
    pchTemp = (*pChannelSetFormat);
    iCurrPos = 0;

    memcpy(&iFormatSpecSize, pchTemp, sizeof(iFormatSpecSize));
    pchTemp += sizeof(iFormatSpecSize);
    iCurrPos += sizeof(iFormatSpecSize);

    while (iCurrPos<iFormatSpecSize)
    {
      // Get key size
      memcpy(&iStrSize, pchTemp, sizeof(iStrSize));
      pchTemp += sizeof(iStrSize);
      iCurrPos += sizeof(iStrSize);

      // Skip the key name
      pchTemp += iStrSize;
      iCurrPos += iStrSize;

      // Skip the value size and value name
      // Get value size
      memcpy(&iStrSize, pchTemp, sizeof(iStrSize));
      pchTemp += sizeof(iStrSize);
      iCurrPos += sizeof(iStrSize);

      pchTemp += iStrSize;
      iCurrPos += iStrSize;

      // Increase number of keys found...
      (*piNumOfKeys)++;
    }
  }

  return WAWEHELPER_NOERROR;
}

int  WAWECALL WaWEHelper_ChannelSetFormat_GetKeySizeByPosition(WaWEChannelSetFormat_p pChannelSetFormat,
                                                               int iKeyPosition,
                                                               int *piKeyNameSize,
                                                               int *piKeyValueSize)
{
  char *pchTemp;
  int iCurrPos;
  int iCurrKeyNum;
  int iFormatSpecSize;
  int iStrSize;

  // First check parameters!
  if ((!pChannelSetFormat) || (iKeyPosition<0) ||
      (!piKeyNameSize) || (!piKeyValueSize))
    return WAWEHELPER_ERROR_INVALIDPARAMETER;

  *piKeyNameSize = 0;
  *piKeyValueSize = 0;
  iCurrKeyNum = 0;

  if (*pChannelSetFormat)
  {
    pchTemp = (*pChannelSetFormat);
    iCurrPos = 0;

    memcpy(&iFormatSpecSize, pchTemp, sizeof(iFormatSpecSize));
    pchTemp += sizeof(iFormatSpecSize);
    iCurrPos += sizeof(iFormatSpecSize);

    while (iCurrPos<iFormatSpecSize)
    {
      // Get key size
      memcpy(&iStrSize, pchTemp, sizeof(iStrSize));
      pchTemp += sizeof(iStrSize);
      iCurrPos += sizeof(iStrSize);

      if (iCurrKeyNum == iKeyPosition)
      {
        // Okay, this is the key we're looking for!
        // Return the key name and value sizes!
        *piKeyNameSize = iStrSize;

        // Skip the key name string
        pchTemp += iStrSize;
        iCurrPos += iStrSize;

        // Get value size
        memcpy(&iStrSize, pchTemp, sizeof(iStrSize));

        *piKeyValueSize = iStrSize;

        return WAWEHELPER_NOERROR;
      } else
      {
        // Skip the key name string
        pchTemp += iStrSize;
        iCurrPos += iStrSize;

        // Get value size
        memcpy(&iStrSize, pchTemp, sizeof(iStrSize));
        pchTemp += sizeof(iStrSize);
        iCurrPos += sizeof(iStrSize);

        // Skip the value string
        pchTemp += iStrSize;
        iCurrPos += iStrSize;

        // Increase number of keys found...
        iCurrKeyNum++;
      }
    }
  }

  return WAWEHELPER_ERROR_KEYNOTFOUND;
}

int  WAWECALL WaWEHelper_ChannelSetFormat_ReadKeyByPosition(WaWEChannelSetFormat_p pChannelSetFormat,
                                                            int iKeyPosition,
                                                            char *pchResultNameBuff,
                                                            int iResultNameBuffSize,
                                                            char *pchResultValueBuff,
                                                            int iResultValueBuffSize)
{
  char *pchTemp;
  int iCurrPos;
  int iCurrKeyNum;
  int iFormatSpecSize;
  int iStrSize;

  // First check parameters!
  if ((!pChannelSetFormat) || (iKeyPosition<0) ||
      (!pchResultNameBuff) || (!pchResultValueBuff) ||
      (iResultNameBuffSize<=0) || (iResultValueBuffSize<=0))
    return WAWEHELPER_ERROR_INVALIDPARAMETER;

  iCurrKeyNum = 0;

  if (*pChannelSetFormat)
  {
    pchTemp = (*pChannelSetFormat);
    iCurrPos = 0;

    memcpy(&iFormatSpecSize, pchTemp, sizeof(iFormatSpecSize));
    pchTemp += sizeof(iFormatSpecSize);
    iCurrPos += sizeof(iFormatSpecSize);

    while (iCurrPos<iFormatSpecSize)
    {
      // Get key size
      memcpy(&iStrSize, pchTemp, sizeof(iStrSize));
      pchTemp += sizeof(iStrSize);
      iCurrPos += sizeof(iStrSize);

      if (iCurrKeyNum == iKeyPosition)
      {
        // Okay, this is the key we're looking for!
        // Return the key name and value strings!
        if (iResultNameBuffSize<iStrSize)
          return WAWEHELPER_ERROR_BUFFERTOOSMALL;

        memcpy(pchResultNameBuff, pchTemp, iStrSize);

        // Skip the key name string
        pchTemp += iStrSize;
        iCurrPos += iStrSize;

        // Get value size
        memcpy(&iStrSize, pchTemp, sizeof(iStrSize));
        pchTemp += sizeof(iStrSize);
        iCurrPos += sizeof(iStrSize);

        if (iResultValueBuffSize<iStrSize)
          return WAWEHELPER_ERROR_BUFFERTOOSMALL;

        memcpy(pchResultValueBuff, pchTemp, iStrSize);

        return WAWEHELPER_NOERROR;
      } else
      {
        // Skip the key name string
        pchTemp += iStrSize;
        iCurrPos += iStrSize;

        // Get value size
        memcpy(&iStrSize, pchTemp, sizeof(iStrSize));
        pchTemp += sizeof(iStrSize);
        iCurrPos += sizeof(iStrSize);

        // Skip the value string
        pchTemp += iStrSize;
        iCurrPos += iStrSize;

        // Increase number of keys found...
        iCurrKeyNum++;
      }
    }
  }

  return WAWEHELPER_ERROR_KEYNOTFOUND;
}

int  WAWECALL WaWEHelper_ChannelSetFormat_DeleteKeyByPosition(WaWEChannelSetFormat_p pChannelSetFormat,
                                                              int iKeyPosition)
{
  char *pchTemp;
  char *pchString;
  int iCurrPos;
  int iFormatSpecSize;
  int iStrSize;
  int iCurrKeyNum;

  // First check parameters!
  if ((!pChannelSetFormat) || (iKeyPosition<0))
    return WAWEHELPER_ERROR_INVALIDPARAMETER;

  if (!(*pChannelSetFormat))
    return WAWEHELPER_ERROR_KEYNOTFOUND;

  iCurrKeyNum = 0;
  pchTemp = (*pChannelSetFormat);
  iCurrPos = 0;

  memcpy(&iFormatSpecSize, pchTemp, sizeof(iFormatSpecSize));
  pchTemp += sizeof(iFormatSpecSize);
  iCurrPos += sizeof(iFormatSpecSize);

  while (iCurrPos<iFormatSpecSize)
  {
    if (iCurrKeyNum!=iKeyPosition)
    {
      // Skip this key name/value combo!
      // Get key name size, and skip key name
      memcpy(&iStrSize, pchTemp, sizeof(iStrSize));
      pchTemp += sizeof(iStrSize);
      iCurrPos += sizeof(iStrSize);
      pchTemp += iStrSize;
      iCurrPos += iStrSize;
      // Get key value size, and skip key value
      memcpy(&iStrSize, pchTemp, sizeof(iStrSize));
      pchTemp += sizeof(iStrSize);
      iCurrPos += sizeof(iStrSize);
      pchTemp += iStrSize;
      iCurrPos += iStrSize;

      // This is not the one we need!
      iCurrKeyNum++;

    } else
    {
      // This is the one we need!
      // So, delete this key + value!
      int iNameSize, iValueSize;
      char *pchCopyFrom;
      char *pchCopyTo;
      char *pchNewPtr;

      // Get key name size
      memcpy(&iNameSize, pchTemp, sizeof(iNameSize));
      pchTemp += sizeof(iNameSize);
      iCurrPos += sizeof(iNameSize);

      // Get key string pointer, and skip key name
      pchString = pchTemp;
      pchTemp += iNameSize;
      iCurrPos += iNameSize;

      // Get value size
      memcpy(&iValueSize, pchTemp, sizeof(iValueSize));
      iCurrPos += sizeof(iStrSize) + iValueSize;
      pchCopyTo = pchTemp - iNameSize - sizeof(iStrSize);
      pchCopyFrom = pchTemp + sizeof(iStrSize) + iValueSize;

      memcpy(pchCopyTo, pchCopyFrom, iFormatSpecSize - iCurrPos);
      iFormatSpecSize -= iNameSize+iValueSize+2*sizeof(iStrSize);

      // Update new FormatSpecSize value
      memcpy((*pChannelSetFormat), &iFormatSpecSize, sizeof(iFormatSpecSize));

      if (iFormatSpecSize == sizeof(iFormatSpecSize))
      {
        // The ChannelSetFormat stuff has became empty!
        // So, free it!
        dbg_free(*pChannelSetFormat);
        *pChannelSetFormat = NULL;

        return WAWEHELPER_NOERROR;
      } else
      {
        // Reallocate memory array to be smaller
        pchNewPtr = (char *) dbg_realloc((*pChannelSetFormat), iFormatSpecSize);
        if (pchNewPtr)
          *pChannelSetFormat = pchNewPtr;
#ifdef DEBUG_BUILD
        else
          printf("[WaWEHelper_DeleteKeyByPosition] : Warning, could not shrink memory array!\n");
#endif

        return WAWEHELPER_NOERROR;
      }
    }
  }

  return WAWEHELPER_ERROR_KEYNOTFOUND;
}

int  WAWECALL WaWEHelper_ChannelSetFormat_ModifyKeyByPosition(WaWEChannelSetFormat_p pChannelSetFormat,
                                                              int iKeyPosition,
                                                              char *pchNewName,
                                                              char *pchNewValue)
{
  char *pchTemp;
  int iCurrPos;
  int iFormatSpecSize;
  int iStrSize;
  int iCurrKeyNum;
  int iNewNameSize, iNewValueSize;

  // First check parameters!
  if ((!pChannelSetFormat) || (iKeyPosition<0) || (!pchNewName) || (!pchNewValue))
    return WAWEHELPER_ERROR_INVALIDPARAMETER;

  if (!(*pChannelSetFormat))
    return WAWEHELPER_ERROR_KEYNOTFOUND;

  iNewNameSize = strlen(pchNewName)+1;
  iNewValueSize = strlen(pchNewValue)+1;

  iCurrKeyNum = 0;
  pchTemp = (*pChannelSetFormat);
  iCurrPos = 0;

  memcpy(&iFormatSpecSize, pchTemp, sizeof(iFormatSpecSize));
  pchTemp += sizeof(iFormatSpecSize);
  iCurrPos += sizeof(iFormatSpecSize);

  while (iCurrPos<iFormatSpecSize)
  {
    if (iCurrKeyNum!=iKeyPosition)
    {
      // Skip this key name/value combo!
      // Get key name size, and skip key name
      memcpy(&iStrSize, pchTemp, sizeof(iStrSize));
      pchTemp += sizeof(iStrSize);
      iCurrPos += sizeof(iStrSize);
      pchTemp += iStrSize;
      iCurrPos += iStrSize;
      // Get key value size, and skip key value
      memcpy(&iStrSize, pchTemp, sizeof(iStrSize));
      pchTemp += sizeof(iStrSize);
      iCurrPos += sizeof(iStrSize);
      pchTemp += iStrSize;
      iCurrPos += iStrSize;

      // This is not the one we need!
      iCurrKeyNum++;

    } else
    {
      // This is the one we need!
      // So, modify this key + value!
      int iNameSize, iValueSize;
      int iNewFormatSpecSize;
      char *pchKeyStartsAt;
      char *pchNewPtr;

      pchKeyStartsAt = pchTemp;
      // Get key name size
      memcpy(&iNameSize, pchTemp, sizeof(iNameSize));
      pchTemp += sizeof(iNameSize);
      iCurrPos += sizeof(iNameSize);

      // Skip key name
      pchTemp += iNameSize;
      iCurrPos += iNameSize;

      // Get value size
      memcpy(&iValueSize, pchTemp, sizeof(iValueSize));
      iCurrPos += sizeof(iStrSize) + iValueSize;
      pchTemp += sizeof(iStrSize) + iValueSize;

      // Calculate new formatspec size!
      iNewFormatSpecSize = iFormatSpecSize;
      iNewFormatSpecSize -= iNameSize+iValueSize+2*sizeof(iStrSize);
      iNewFormatSpecSize += iNewNameSize+iNewValueSize+2*sizeof(iStrSize);

      if (iNewFormatSpecSize == iFormatSpecSize)
      {
        // Ah, coool, the new stuff fits exactly to the place of the old!
        memcpy(pchKeyStartsAt, &iNewNameSize, sizeof(iNewNameSize));
        memcpy(pchKeyStartsAt+sizeof(iNewNameSize), pchNewName, iNewNameSize);
        pchKeyStartsAt+=sizeof(iNewNameSize)+iNewNameSize;
        memcpy(pchKeyStartsAt, &iNewValueSize, sizeof(iNewValueSize));
        memcpy(pchKeyStartsAt+sizeof(iNewValueSize), pchNewValue, iNewValueSize);

        return WAWEHELPER_NOERROR;
      } else
      if (iNewFormatSpecSize > iFormatSpecSize)
      {
        int iOffset, iSizeDiff, i, iToCopy;
        // The new formatspec size is bigger.
        // Try to make space for it!
        iSizeDiff = iNewFormatSpecSize - iFormatSpecSize;

        // Reallocation can change original pointer, so
        // make an offset from pchKeyStartsAt pointer,
        // and we'll make a new pointer from the offset
        // after the reallocation!
        iOffset = pchKeyStartsAt - (*pChannelSetFormat);

        // Reallocate memory array
        pchNewPtr = (char *) dbg_realloc((*pChannelSetFormat), iNewFormatSpecSize);
        if (!pchNewPtr)
          return WAWEHELPER_ERROR_OUTOFMEMORY;

        *pChannelSetFormat = pchNewPtr;
        pchKeyStartsAt = pchNewPtr + iOffset;
        iFormatSpecSize = iNewFormatSpecSize;
        // Update new FormatSpecSize value
        memcpy((*pChannelSetFormat), &iFormatSpecSize, sizeof(iFormatSpecSize));

        // Make space for modified string
        iToCopy = iFormatSpecSize - (iOffset+iNewNameSize+iNewValueSize+2*sizeof(iStrSize));
        pchTemp = (*pChannelSetFormat);
        for (i=0; i<iToCopy; i++)
          pchTemp[iFormatSpecSize-1-i] = pchTemp[iFormatSpecSize-1-i-iSizeDiff];

        // Store the modified string!
        memcpy(pchKeyStartsAt, &iNewNameSize, sizeof(iNewNameSize));
        memcpy(pchKeyStartsAt+sizeof(iNewNameSize), pchNewName, iNewNameSize);
        pchKeyStartsAt+=sizeof(iNewNameSize)+iNewNameSize;
        memcpy(pchKeyStartsAt, &iNewValueSize, sizeof(iNewValueSize));
        memcpy(pchKeyStartsAt+sizeof(iNewValueSize), pchNewValue, iNewValueSize);

        return WAWEHELPER_NOERROR;
      } else
      {
        int iSizeDiff, iOffset, iToCopy;
        int iOldFormatSpecSize;

        // The new formatspec size is smaller.
#ifdef DEBUG_BUILD
//        printf("Make things smaller\n"); fflush(stdout);
#endif
        iSizeDiff = iFormatSpecSize - iNewFormatSpecSize;
        iOldFormatSpecSize = iFormatSpecSize;
        iFormatSpecSize = iNewFormatSpecSize;
        iOffset = pchKeyStartsAt - (*pChannelSetFormat);
        // Update new FormatSpecSize value
        memcpy((*pChannelSetFormat), &iFormatSpecSize, sizeof(iFormatSpecSize));

#ifdef DEBUG_BUILD
//        printf("Store modified data\n"); fflush(stdout);
#endif
        // Store the modified string!
        memcpy(pchKeyStartsAt, &iNewNameSize, sizeof(iNewNameSize));
        memcpy(pchKeyStartsAt+sizeof(iNewNameSize), pchNewName, iNewNameSize);
        pchKeyStartsAt+=sizeof(iNewNameSize)+iNewNameSize;
        memcpy(pchKeyStartsAt, &iNewValueSize, sizeof(iNewValueSize));
        memcpy(pchKeyStartsAt+sizeof(iNewValueSize), pchNewValue, iNewValueSize);
        pchKeyStartsAt+=sizeof(iNewValueSize)+iNewValueSize;

        // Move remaining data
        iToCopy = iOldFormatSpecSize - (iOffset+iValueSize+iNameSize+2*sizeof(iStrSize));
#ifdef DEBUG_BUILD
//        printf("Move remaining data (%d bytes, sizediff is %d)\n", iToCopy, iSizeDiff); fflush(stdout);
#endif
        if (iToCopy>0)
          memcpy(pchKeyStartsAt,
                 pchKeyStartsAt+iSizeDiff,
                 iToCopy);
        // Reallocate memory array to be smaller
        pchNewPtr = (char *) dbg_realloc((*pChannelSetFormat), iFormatSpecSize);
        if (pchNewPtr)
          *pChannelSetFormat = pchNewPtr;
#ifdef DEBUG_BUILD
        else
          printf("[WaWEHelper_DeleteKeyByPosition] : Warning, could not shrink memory array!\n");
#endif

//        Print_ChannelSetFormat_Contents(pChannelSetFormat);
        return WAWEHELPER_NOERROR;
      }
    }
  }

  return WAWEHELPER_ERROR_KEYNOTFOUND;
}

int  WAWECALL WaWEHelper_ChannelSetFormat_MoveKey(WaWEChannelSetFormat_p pChannelSetFormat,
                                                  int iOldKeyPosition,
                                                  int iNewKeyPosition)
{
  char *pchTemp;
  int iCurrPos;
  int iCurrKeyNum;
  int iFormatSpecSize;
  int iStrSize;
  char *pchSource, *pchDestination, *pchDestinationEnd;
  int iSourceSize;

  // First check parameters!
  if ((!pChannelSetFormat) || (iOldKeyPosition<0) || (iNewKeyPosition<0))
    return WAWEHELPER_ERROR_INVALIDPARAMETER;

  // Shortcut
  if (iOldKeyPosition==iNewKeyPosition) return WAWEHELPER_NOERROR;

  iCurrKeyNum = 0;
  pchSource = pchDestination = NULL;

  if (*pChannelSetFormat)
  {
    pchTemp = (*pChannelSetFormat);
    iCurrPos = 0;

    memcpy(&iFormatSpecSize, pchTemp, sizeof(iFormatSpecSize));
    pchTemp += sizeof(iFormatSpecSize);
    iCurrPos += sizeof(iFormatSpecSize);

    while (iCurrPos<iFormatSpecSize)
    {
      // Get key size
      memcpy(&iStrSize, pchTemp, sizeof(iStrSize));

      if (iCurrKeyNum == iNewKeyPosition)
      {
        // Found where we should insert it to!
        pchDestination = pchTemp;
      }

      if (iCurrKeyNum == iOldKeyPosition)
      {
        // Okay, this is one of the keys we're looking for!

        pchSource = pchTemp;
        iSourceSize = sizeof(iStrSize) + iStrSize;

        // Skip key name size
        pchTemp += sizeof(iStrSize);
        iCurrPos += sizeof(iStrSize);

        // Skip the key name string
        pchTemp += iStrSize;
        iCurrPos += iStrSize;

        // Get value size
        memcpy(&iStrSize, pchTemp, sizeof(iStrSize));

        iSourceSize += sizeof(iStrSize) + iStrSize;

        // Skip key value size
        pchTemp += sizeof(iStrSize);
        iCurrPos += sizeof(iStrSize);

        // Skip the key value string
        pchTemp += iStrSize;
        iCurrPos += iStrSize;

      } else
      {
        // Not a key we're looking for.

        // Skip key name size
        pchTemp += sizeof(iStrSize);
        iCurrPos += sizeof(iStrSize);

        // Skip the key name string
        pchTemp += iStrSize;
        iCurrPos += iStrSize;

        // Get value size
        memcpy(&iStrSize, pchTemp, sizeof(iStrSize));
        pchTemp += sizeof(iStrSize);
        iCurrPos += sizeof(iStrSize);

        // Skip the value string
        pchTemp += iStrSize;
        iCurrPos += iStrSize;
      }

      if (iCurrKeyNum == iNewKeyPosition)
      {
        pchDestinationEnd = pchTemp;
      }

      // Increase number of keys found...
      iCurrKeyNum++;
    }

    if ((!pchSource) || (!pchDestination))
    {
#ifdef DEBUG_BUILD
//      printf("[WaWEHelper_ChannelSetFormat_MoveKey] : Key not found!\n");
//      printf("[WaWEHelper_ChannelSetFormat_MoveKey] : iOldKeyPosition = %d\n", iOldKeyPosition);
//      printf("[WaWEHelper_ChannelSetFormat_MoveKey] : iNewKeyPosition = %d\n", iNewKeyPosition);
//      printf("[WaWEHelper_ChannelSetFormat_MoveKey] : pchSource = 0x%p\n", pchSource);
//      printf("[WaWEHelper_ChannelSetFormat_MoveKey] : pchDestination = 0x%p\n", pchDestination);
#endif
      return WAWEHELPER_ERROR_KEYNOTFOUND;
    }

    // Coool! We have the source pointer, the source size, and the destination pointer!
    // Move it then!

    pchTemp = (char *) dbg_malloc(iSourceSize);
    if (!pchTemp)
    {
#ifdef DEBUG_BUILD
      printf("[WaWEHelper_ChannelSetFormat_MoveKey] : Out of memory!\n");
#endif
      return WAWEHELPER_ERROR_OUTOFMEMORY;
    }

    // Save area to be moved
    memcpy(pchTemp, pchSource, iSourceSize);

    // Make space for it!
    if (pchSource > pchDestination)
    {
      int i, iCopySize;

      // Moving an area forward!
      iCopySize = pchSource - pchDestination;
      for (i=0; i<iCopySize; i++)
        *(pchSource + iSourceSize-1-i) = *(pchSource-1-i);
    } else
    {
      int iCopySize;

      // Moving an area backward!
      iCopySize = pchDestinationEnd-(pchSource+iSourceSize);
      memcpy(pchSource, pchSource+iSourceSize, iCopySize);

      pchDestination = pchDestinationEnd - iSourceSize;
    }

    // Copy the saved area to its final position
    memcpy(pchDestination, pchTemp, iSourceSize);

    // Done!
    dbg_free(pchTemp);
    return WAWEHELPER_NOERROR;
  }

  return WAWEHELPER_ERROR_KEYNOTFOUND;
}


// Functions above channel-sets:

int  WAWECALL WaWEHelper_Global_CreateChannelSet(WaWEChannelSet_p *ppChannelSet,
                                                 WaWEFormat_p      pOriginalFormat,
                                                 int               iNumOfChannels,
                                                 char             *pchProposedName,
                                                 char             *pchProposedFileName)
{
  WaWEChannel_p pChannel;
  int iChannelNum;

#ifdef DEBUG_BUILD
  printf("[WaWEHelper] : CreateChannelSet: Enter\n"); fflush(stdout);
#endif

  // Check parameters
  if ((!ppChannelSet) || (!pOriginalFormat) || (iNumOfChannels<0))
    return WAWEHELPER_ERROR_INVALIDPARAMETER;

  // Check "Original Format" parameter for validity:
  // - Frequency must be above 0
  // - BitsPerSample must be 8, 16, 24 or 32
  // - IsSigned must be 0 or 1
  if ((pOriginalFormat->iFrequency<=0) ||
      ((pOriginalFormat->iBits!=8) && (pOriginalFormat->iBits!=16) && (pOriginalFormat->iBits!=24) && (pOriginalFormat->iBits!=32)) ||
      ((pOriginalFormat->iIsSigned!=0) && (pOriginalFormat->iIsSigned!=1))
     )
#ifdef DEBUG_BUILD
  {
    printf("[WaWEHelper] : CreateChannelSet: Invalid parameter(s)!\n"); fflush(stdout);
    printf("               %d %d %d\n", pOriginalFormat->iFrequency, pOriginalFormat->iBits, pOriginalFormat->iIsSigned); fflush(stdout);
#endif
    return WAWEHELPER_ERROR_INVALIDPARAMETER;
#ifdef DEBUG_BUILD
  }
#endif

  // Get the ChannelSetListProtector_Sem!
  if (DosRequestMutexSem(ChannelSetListProtector_Sem, SEM_INDEFINITE_WAIT)!=NO_ERROR)
  {
#ifdef DEBUG_BUILD
    printf("[WaWEHelper] : CreateChannelSet: Could not get mutex semaphore!\n"); fflush(stdout);
#endif
    return WAWEHELPER_ERROR_INTERNALERROR;
  }

  *ppChannelSet = (WaWEChannelSet_p) dbg_malloc(sizeof(WaWEChannelSet_t));
  if (!ppChannelSet)
  {
    DosReleaseMutexSem(ChannelSetListProtector_Sem);
    return WAWEHELPER_ERROR_OUTOFMEMORY;
  }

  // Fill the fields:
  (*ppChannelSet)->usStructureSize = sizeof(WaWEChannelSet_t);
  // OriginalFormat
  memcpy(&((*ppChannelSet)->OriginalFormat), pOriginalFormat, sizeof((*ppChannelSet)->OriginalFormat));
  (*ppChannelSet)->iNumOfChannels = iNumOfChannels;

  // Set the channel-set filename!
  // Use the proposed name, if available, or use empty string!
  if (pchProposedFileName)
    strncpy((*ppChannelSet)->achFileName, pchProposedFileName, WAWE_CHANNELSET_FILENAME_LEN);
  else
    strncpy((*ppChannelSet)->achFileName, "", WAWE_CHANNELSET_FILENAME_LEN);

  // Set the channel-set name!
  // Use the proposed name, if available, or generate one if there is no
  // proposed name.
  if (pchProposedName)
    snprintf((*ppChannelSet)->achName, WAWE_CHANNELSET_NAME_LEN,
             "%s", pchProposedName);
  else
    snprintf((*ppChannelSet)->achName, WAWE_CHANNELSET_NAME_LEN,
             "Channel-set %d", iNextChannelSetID);

  (*ppChannelSet)->iChannelSetID = iNextChannelSetID;
  iNextChannelSetID++;

  (*ppChannelSet)->hwndChannelSet = NULLHANDLE;
  (*ppChannelSet)->iWidth = 0; // Default, set at a different place!
  (*ppChannelSet)->iHeight =
    iChannelSetHeaderHeight+
    iScrollBarHeight +
    iSizerHeight+
    (iNumOfChannels * (iChannelEditAreaHeight + iChannelHeaderHeight));


  if (pLastChannelSet)
  {
    SWP swp;
    WinQueryWindowPos(pLastChannelSet->hwndChannelSet, &swp);
    // If it's not the first one, then put it under the last channel set!
    (*ppChannelSet)->iYPos = swp.y - (*ppChannelSet)->iHeight;
  }
  else
  {
    // If it's the first one, then put it to the top of client area!
    SWP swp;
    WinQueryWindowPos(hwndClient, &swp);
    (*ppChannelSet)->iYPos = swp.cy - (*ppChannelSet)->iHeight;
  }
  (*ppChannelSet)->ZoomCount = 1;
  (*ppChannelSet)->ZoomDenom = 1;
  (*ppChannelSet)->FirstVisiblePosition = 0;
  (*ppChannelSet)->PointerPosition = 0;
  (*ppChannelSet)->bWorkWithChannelSet = 1;
  (*ppChannelSet)->lUsageCounter = 0;
  (*ppChannelSet)->bWindowCreated = 0;
  (*ppChannelSet)->bRedrawNeeded = 0;

  (*ppChannelSet)->bModified = 0;

  (*ppChannelSet)->pOriginalPlugin = NULL;
  (*ppChannelSet)->hOriginalFile   = NULL;

  DosCreateMutexSem(NULL,                                   // Name
                    &((*ppChannelSet)->hmtxUseChannelSetCache),    // Pointer to handle
                    0L,                                     // Non-shared
                    FALSE);                                 // Unowned
  (*ppChannelSet)->ChannelSetCache.bUseCache = FALSE; // No read-cache for empty channel-sets
  (*ppChannelSet)->ChannelSetCache.uiCachePageSize = 0;
  (*ppChannelSet)->ChannelSetCache.uiNumOfCachePages = 0;
  (*ppChannelSet)->ChannelSetCache.FileSize = 0;
  (*ppChannelSet)->ChannelSetCache.CachePages = NULL;

  DosCreateMutexSem(NULL,                                   // Name
                    &((*ppChannelSet)->hmtxUseChannelSetFormat),    // Pointer to handle
                    0L,                                     // Non-shared
                    FALSE);                                 // Unowned

  (*ppChannelSet)->ChannelSetFormat = NULL; // Unknown format

  DosCreateMutexSem(NULL,                                   // Name
                    &((*ppChannelSet)->hmtxUseChannelList),    // Pointer to handle
                    0L,                                     // Non-shared
                    FALSE);                                 // Unowned
  (*ppChannelSet)->pChannelListHead = NULL;

  (*ppChannelSet)->Length = 0;

  (*ppChannelSet)->pNext = NULL;
  (*ppChannelSet)->pPrev = NULL;

  // Link it into the list of ChannelSets!
  if (pLastChannelSet)
  {
    // Found the last one
    pLastChannelSet->pNext = (*ppChannelSet);
    (*ppChannelSet)->pPrev = pLastChannelSet;
  } else
  {
    // No last one! We might be the first element!
    ChannelSetListHead = (*ppChannelSet);
  }
  pLastChannelSet = (*ppChannelSet);

  // Release semaphore until the other thread will work with the channel-sets!
  DosReleaseMutexSem(ChannelSetListProtector_Sem);

  // The creation of the window itself will be done by the
  // PM thread, so do it there!
  if (GetCurrentThreadID()==tidPM)
  {
    // We're the PM thread!
    CreateNewChannelSetWindow(*ppChannelSet);
  } else
  {
    // Send a notification to PM thread!
    WinPostMsg(hwndClient, WM_CREATECSWINDOW, (MPARAM) (*ppChannelSet), (MRESULT) 0);
    // Wait until that thread creates the channel-set window!
    while ((!(*ppChannelSet)->bWindowCreated))
      DosSleep(32);
  }

  // Get back the ChannelSetListProtector_Sem!
  DosRequestMutexSem(ChannelSetListProtector_Sem, SEM_INDEFINITE_WAIT);

  // Create a list of channels inside the channel-set!
  for (iChannelNum=0; iChannelNum<iNumOfChannels; iChannelNum++)
  {
    int rc;

    // Release semaphore until the other thread will work with the channel-sets!
    DosReleaseMutexSem(ChannelSetListProtector_Sem);
    rc = WaWEHelper_ChannelSet_CreateChannel((*ppChannelSet), // Channel-set
                                             WAWECHANNEL_BOTTOM, // Insert after
                                             NULL,
                                             &pChannel);
    DosRequestMutexSem(ChannelSetListProtector_Sem, SEM_INDEFINITE_WAIT);

#ifdef DEBUG_BUILD
    if (rc!=WAWEHELPER_NOERROR)
    {
      printf("CreateChannel rc = %d\n", rc);
    }
#endif
  }

  // The CreateChannel() did change the bModified flag, so re-set it!
  (*ppChannelSet)->bModified = 0;

  // Set this channelset to be the active one, if there is no other
  // active window yet!
  if (pActiveChannelSet == NULL)
    SetActiveChannelSet(*ppChannelSet);

  bChannelSetListChanged = 0;

  // Return the result
  DosReleaseMutexSem(ChannelSetListProtector_Sem);

#ifdef DEBUG_BUILD
  printf("[WaWEHelper] : Redraw changed stuffs\n"); fflush(stdout);
#endif

  // Redraw everything that's changed!
  RedrawChangedChannelsAndChannelSets();

#ifdef DEBUG_BUILD
  printf("[WaWEHelper] : CreateChannelSet: Done (hwnd is 0x%x)\n", (*ppChannelSet)->hwndChannelSet); fflush(stdout);
#endif

  // Call registered event callbacks
  {
    WaWEEventDesc_t EventDesc;

    EventDesc.iEventType = WAWE_EVENT_POSTCHANNELSETCREATE;
    EventDesc.EventSpecificInfo.ChannelSetModInfo.pModifiedChannelSet = *ppChannelSet;

    CallEventCallbacks(&EventDesc);
  }

  return WAWEHELPER_NOERROR;
}

int  WAWECALL WaWEHelper_Global_DeleteChannelSetEx(WaWEChannelSet_p pChannelSet, int bClosePluginFile, int bRedrawNow)
{
  WaWEChannelSet_p temp;
  int rc;

  // Check parameters first!
  if (!pChannelSet) return WAWEHELPER_ERROR_INVALIDPARAMETER;

  // Call registered event callbacks
  {
    WaWEEventDesc_t EventDesc;

    EventDesc.iEventType = WAWE_EVENT_PRECHANNELSETDELETE;
    EventDesc.EventSpecificInfo.ChannelSetModInfo.pModifiedChannelSet = pChannelSet;

    CallEventCallbacks(&EventDesc);
  }

  if (pChannelSet->lUsageCounter>0)
    return WAWEHELPER_ERROR_ELEMENTINUSE;

  // Get the ChannelSetListProtector_Sem!
  if (DosRequestMutexSem(ChannelSetListProtector_Sem, SEM_INDEFINITE_WAIT)!=NO_ERROR)
  {
#ifdef DEBUG_BUILD
    printf("[WaWEHelper] : DeleteChannelSet: Could not get mutex semaphore!\n"); fflush(stdout);
#endif
    return WAWEHELPER_ERROR_INTERNALERROR;
  }

#ifdef DEBUG_BUILD
//  printf("  Check if it's a real channel-set!\n"); fflush(stdout);
#endif
  // Check if it's a real channel set!
  // Find it in the list of channel-sets!
  temp = ChannelSetListHead;
  while ((temp) && (temp!=pChannelSet))
    temp=temp->pNext;

  if (!temp)
  {
    // Could not find channel-set in list of channel-sets!
    DosReleaseMutexSem(ChannelSetListProtector_Sem);
#ifdef DEBUG_BUILD
    printf("[WaWEHelper_Global_DeleteChannelSet: Could not find channel-set!\n"); fflush(stdout);
#endif
    return WAWEHELPER_ERROR_INVALIDPARAMETER;
  } else
  {
    // Found in the list! Cool!
    // So, delete it!

#ifdef DEBUG_BUILD
//    printf("  Free all channels in set!\n"); fflush(stdout);
#endif

    // free all channels in this channel-set!
    while (pChannelSet->pChannelListHead)
    {
      DosReleaseMutexSem(ChannelSetListProtector_Sem);
      rc = WaWEHelper_ChannelSet_DeleteChannel(pChannelSet->pChannelListHead);
      DosRequestMutexSem(ChannelSetListProtector_Sem, SEM_INDEFINITE_WAIT);
      if (rc!=WAWEHELPER_NOERROR)
      {
#ifdef DEBUG_BUILD
        printf("[WaWEHelper_Global_DeleteChannelSet] : Error deleting channel, rc=%d\n", rc); fflush(stdout);
#endif
      }
    }

#ifdef DEBUG_BUILD
    printf("  Unlink from list!\n"); fflush(stdout);
#endif

    // First, unlink from list!
    if (pChannelSet->pPrev)
    {
      // It has a previous
      temp = pChannelSet->pPrev;
      temp->pNext = pChannelSet->pNext;
    } else
    {
      // It doesn't have a prev., so modify head!
      ChannelSetListHead = pChannelSet->pNext;
    }
    // Take care of last channel set
    if (pLastChannelSet == pChannelSet)
      pLastChannelSet = pChannelSet->pPrev;

    if (pChannelSet->pNext)
    {
      temp = pChannelSet->pNext;
      temp->pPrev = pChannelSet->pPrev;
    }
#ifdef DEBUG_BUILD
    printf("  Destroy channel-set window!\n"); fflush(stdout);
#endif
    // Ok, destroy the channel set window!
    if (pActiveChannelSet == pChannelSet)
    {
      // Ups, this is the active channel-set!
      // Set another channelset active!
      if (pChannelSet->pPrev)
      {
        DosRequestMutexSem(ChannelSetListProtector_Sem, SEM_INDEFINITE_WAIT);
        SetActiveChannelSet(pChannelSet->pPrev);
        DosReleaseMutexSem(ChannelSetListProtector_Sem);
      }
      else
      if (pChannelSet->pNext)
      {
        DosRequestMutexSem(ChannelSetListProtector_Sem, SEM_INDEFINITE_WAIT);
        SetActiveChannelSet(pChannelSet->pNext);
        DosReleaseMutexSem(ChannelSetListProtector_Sem);
      }
      else
      {
        DosRequestMutexSem(ChannelSetListProtector_Sem, SEM_INDEFINITE_WAIT);
        SetActiveChannelSet(NULL);
        DosReleaseMutexSem(ChannelSetListProtector_Sem);
      }
    }
    if (pChannelSet->hwndChannelSet)
      PMThread_WinDestroyWindow(pChannelSet->hwndChannelSet);

    // Now close file handle of import plugin, if we had one!
    if ((bClosePluginFile) && (pChannelSet->pOriginalPlugin) && (pChannelSet->hOriginalFile))
    {
#ifdef DEBUG_BUILD
      printf("  Close file handle of import plugin!\n"); fflush(stdout);
#endif
      WaWEHelper_ChannelSetFormat_DeleteAllKeys(&(pChannelSet->hOriginalFile->ChannelSetFormat));
      pChannelSet->pOriginalPlugin->TypeSpecificInfo.ImportPluginInfo.fnClose(pChannelSet->hOriginalFile);
    }

#ifdef DEBUG_BUILD
      printf("  Free FormatSpec area!\n"); fflush(stdout);
#endif

    // Free FormatSpec area if there is any
    if (DosRequestMutexSem(pChannelSet->hmtxUseChannelSetFormat, SEM_INDEFINITE_WAIT) == NO_ERROR)
    {
      if (pChannelSet->ChannelSetFormat)
      {
        WaWEHelper_ChannelSetFormat_DeleteAllKeys(&(pChannelSet->ChannelSetFormat));
        pChannelSet->ChannelSetFormat = NULL;
      }
      DosReleaseMutexSem(pChannelSet->hmtxUseChannelSetFormat);
    }
    DosCloseMutexSem(pChannelSet->hmtxUseChannelSetFormat);

    // Free channel-set read-cache, if there is any
    if ((pChannelSet->ChannelSetCache.bUseCache) &&
        (pChannelSet->ChannelSetCache.CachePages))
    {
      WaWEChannelSetCachePage_p pCachePages;
      unsigned int uiTemp;

#ifdef DEBUG_BUILD
      printf("  Free channel-set cache...\n"); fflush(stdout);
#endif

      // Free the cache pages first
      pCachePages = pChannelSet->ChannelSetCache.CachePages;
  
      for (uiTemp=0; uiTemp<pChannelSet->ChannelSetCache.uiNumOfCachePages; uiTemp++)
      {
        if (pCachePages[uiTemp].puchCachedData)
        {
#ifdef DEBUG_BUILD
//          printf("    Freeing %d page (%d)\n", uiTemp, (int) (pCachePages[uiTemp].PageNumber));
#endif
          dbg_free(pCachePages[uiTemp].puchCachedData);
          pCachePages[uiTemp].puchCachedData = NULL;
        }
        pCachePages[uiTemp].uiNumOfBytesOnPage = 0;
        pCachePages[uiTemp].PageNumber = -1;
      }
      // Then free the cache descriptor
      dbg_free(pChannelSet->ChannelSetCache.CachePages);
      pChannelSet->ChannelSetCache.CachePages = NULL;
    }
    DosCloseMutexSem(pChannelSet->hmtxUseChannelSetCache);

    // Release other resources
    DosCloseMutexSem(pChannelSet->hmtxUseChannelList);

    // All done, now dbg_free this structure!
    dbg_free(pChannelSet);
  }

  bChannelSetListChanged = 1;

  DosReleaseMutexSem(ChannelSetListProtector_Sem);

  if (bRedrawNow)
  {
    // Redraw everything that's changed!
    RedrawChangedChannelsAndChannelSets();
  }

  // Call registered event callbacks
  {
    WaWEEventDesc_t EventDesc;

    EventDesc.iEventType = WAWE_EVENT_POSTCHANNELSETDELETE;
    EventDesc.EventSpecificInfo.ChannelSetModInfo.pModifiedChannelSet = pChannelSet;

    CallEventCallbacks(&EventDesc);
  }

  return WAWEHELPER_NOERROR;
}

int  WAWECALL WaWEHelper_Global_DeleteChannelSet(WaWEChannelSet_p pChannelSet)
{
  return WaWEHelper_Global_DeleteChannelSetEx(pChannelSet, TRUE, TRUE);
}

int  WAWECALL WaWEHelper_Global_MoveChannelSet(WaWEChannelSet_p pChannelSet, WaWEChannelSet_p pInsertAfter, int bRedrawNow)
{
  WaWEChannelSet_p pPrevChannelSet, pNextChannelSet;

  // Check parameters first!
  if ((!pChannelSet) || (!pInsertAfter)) return WAWEHELPER_ERROR_INVALIDPARAMETER;

  // Get the ChannelSetListProtector_Sem!
  if (DosRequestMutexSem(ChannelSetListProtector_Sem, SEM_INDEFINITE_WAIT)!=NO_ERROR)
  {
#ifdef DEBUG_BUILD
    printf("[WaWEHelper] : MoveChannelSet: Could not get mutex semaphore!\n"); fflush(stdout);
#endif
    return WAWEHELPER_ERROR_INTERNALERROR;
  }

  // Unlink from list

  pPrevChannelSet = pChannelSet->pPrev;
  pNextChannelSet = pChannelSet->pNext;

  if (pPrevChannelSet)
    pPrevChannelSet->pNext = pChannelSet->pNext;
  else
    ChannelSetListHead = pChannelSet->pNext;

  if (pNextChannelSet)
    pNextChannelSet->pPrev = pChannelSet->pPrev;

  // Link to new position in list!
  if ((pInsertAfter!=WAWECHANNELSET_TOP) && (pInsertAfter!=WAWECHANNELSET_BOTTOM))
  {
    // Insert to middle
    pChannelSet->pPrev = pInsertAfter;
    pNextChannelSet = pChannelSet->pNext = pInsertAfter->pNext;

    pInsertAfter->pNext = pChannelSet;
    pNextChannelSet->pPrev = pChannelSet;
  } else
  if ((pInsertAfter==WAWECHANNELSET_TOP) || (!ChannelSetListHead))
  {
    // Insert to beginning of list
    pChannelSet->pNext = ChannelSetListHead;
    pChannelSet->pPrev = NULL;
    if (ChannelSetListHead)
      ChannelSetListHead->pPrev = pChannelSet;
    ChannelSetListHead = pChannelSet;
  } else
//  if (pInsertAfter==WAWECHANNELSET_BOTTOM)
  {
    // Insert to end of list
    WaWEChannelSet_p pLast;

    pLast = ChannelSetListHead;
    while ((pLast) && (pLast->pNext)) pLast = pLast->pNext;

    if (!pLast)
    {
      // Insert to beginning of list
      pChannelSet->pNext = ChannelSetListHead;
      pChannelSet->pPrev = NULL;
      if (ChannelSetListHead)
        ChannelSetListHead->pPrev = pChannelSet;
      ChannelSetListHead = pChannelSet;
    } else
    {
      // Insert after pLast
      pChannelSet->pNext = NULL;
      pChannelSet->pPrev = pLast;
      pLast->pNext = pChannelSet;
    }
  }

  bChannelSetListChanged = 1;

  DosReleaseMutexSem(ChannelSetListProtector_Sem);

  if (bRedrawNow)
  {
    // Redraw everything that's changed!
    RedrawChangedChannelsAndChannelSets();
  }

  // Call registered event callbacks
  {
    WaWEEventDesc_t EventDesc;

    EventDesc.iEventType = WAWE_EVENT_POSTCHANNELSETCHANGE;
    EventDesc.EventSpecificInfo.ChannelSetModInfo.pModifiedChannelSet = pChannelSet;

    CallEventCallbacks(&EventDesc);
  }
  return WAWEHELPER_NOERROR;
}

int  WAWECALL WaWEHelper_Global_SetEventCallback(pfn_WaWEP_EventCallback pfnEventCallback, int iEventType)
{
  if (SetEventCallback(iEventType, pfnEventCallback))
    return WAWEHELPER_NOERROR;
  else
    return WAWEHELPER_ERROR_INTERNALERROR;
}

int  WAWECALL WaWEHelper_Global_RedrawChanges(void)
{
  // Redraw everything that's changed!
  if (!RedrawChangedChannelsAndChannelSets())
    return WAWEHELPER_ERROR_INTERNALERROR;

  return WAWEHELPER_NOERROR;
}

void FillPluginHelpersStructure(WaWEPluginHelpers_p pPluginHelpers)
{
  if (pPluginHelpers)
  {
    pPluginHelpers->fn_WaWEHelper_Channel_ReadSample = WaWEHelper_Channel_ReadSample;
    pPluginHelpers->fn_WaWEHelper_Channel_WriteSample = WaWEHelper_Channel_WriteSample;
    pPluginHelpers->fn_WaWEHelper_Channel_InsertSample = WaWEHelper_Channel_InsertSample;
    pPluginHelpers->fn_WaWEHelper_Channel_DeleteSample = WaWEHelper_Channel_DeleteSample;
    pPluginHelpers->fn_WaWEHelper_Channel_SetCurrentPosition = WaWEHelper_Channel_SetCurrentPosition;
    pPluginHelpers->fn_WaWEHelper_Channel_Changed = WaWEHelper_Channel_Changed;

    pPluginHelpers->fn_WaWEHelper_ChannelSet_CreateChannel = WaWEHelper_ChannelSet_CreateChannel;
    pPluginHelpers->fn_WaWEHelper_ChannelSet_DeleteChannel = WaWEHelper_ChannelSet_DeleteChannel;
    pPluginHelpers->fn_WaWEHelper_ChannelSet_MoveChannel = WaWEHelper_ChannelSet_MoveChannel;
    pPluginHelpers->fn_WaWEHelper_ChannelSet_ReadPackedSamples = WaWEHelper_ChannelSet_ReadPackedSamples;
    pPluginHelpers->fn_WaWEHelper_ChannelSet_Changed = WaWEHelper_ChannelSet_Changed;
    pPluginHelpers->fn_WaWEHelper_ChannelSet_StartChSetFormatUsage = WaWEHelper_ChannelSet_StartChSetFormatUsage;
    pPluginHelpers->fn_WaWEHelper_ChannelSet_StopChSetFormatUsage = WaWEHelper_ChannelSet_StopChSetFormatUsage;

    pPluginHelpers->fn_WaWEHelper_ChannelSetFormat_WriteKey = WaWEHelper_ChannelSetFormat_WriteKey;
    pPluginHelpers->fn_WaWEHelper_ChannelSetFormat_GetKeySize = WaWEHelper_ChannelSetFormat_GetKeySize;
    pPluginHelpers->fn_WaWEHelper_ChannelSetFormat_ReadKey = WaWEHelper_ChannelSetFormat_ReadKey;
    pPluginHelpers->fn_WaWEHelper_ChannelSetFormat_DeleteKey = WaWEHelper_ChannelSetFormat_DeleteKey;
    pPluginHelpers->fn_WaWEHelper_ChannelSetFormat_DeleteKeysStartingWith = WaWEHelper_ChannelSetFormat_DeleteKeysStartingWith;
    pPluginHelpers->fn_WaWEHelper_ChannelSetFormat_DeleteKeysAndSubkeys = WaWEHelper_ChannelSetFormat_DeleteKeysAndSubkeys;
    pPluginHelpers->fn_WaWEHelper_ChannelSetFormat_DeleteAllKeys = WaWEHelper_ChannelSetFormat_DeleteAllKeys;
    pPluginHelpers->fn_WaWEHelper_ChannelSetFormat_GetNumOfKeys = WaWEHelper_ChannelSetFormat_GetNumOfKeys;
    pPluginHelpers->fn_WaWEHelper_ChannelSetFormat_GetKeySizeByPosition = WaWEHelper_ChannelSetFormat_GetKeySizeByPosition;
    pPluginHelpers->fn_WaWEHelper_ChannelSetFormat_ReadKeyByPosition = WaWEHelper_ChannelSetFormat_ReadKeyByPosition;
    pPluginHelpers->fn_WaWEHelper_ChannelSetFormat_DeleteKeyByPosition = WaWEHelper_ChannelSetFormat_DeleteKeyByPosition;
    pPluginHelpers->fn_WaWEHelper_ChannelSetFormat_ModifyKeyByPosition = WaWEHelper_ChannelSetFormat_ModifyKeyByPosition;
    pPluginHelpers->fn_WaWEHelper_ChannelSetFormat_MoveKey = WaWEHelper_ChannelSetFormat_MoveKey;

    pPluginHelpers->fn_WaWEHelper_Global_CreateChannelSet = WaWEHelper_Global_CreateChannelSet;
    pPluginHelpers->fn_WaWEHelper_Global_DeleteChannelSet = WaWEHelper_Global_DeleteChannelSet;
    pPluginHelpers->fn_WaWEHelper_Global_DeleteChannelSetEx = WaWEHelper_Global_DeleteChannelSetEx;
    pPluginHelpers->fn_WaWEHelper_Global_MoveChannelSet = WaWEHelper_Global_MoveChannelSet;
    pPluginHelpers->fn_WaWEHelper_Global_SetEventCallback = WaWEHelper_Global_SetEventCallback;
    pPluginHelpers->fn_WaWEHelper_Global_RedrawChanges = WaWEHelper_Global_RedrawChanges;
  }
}

