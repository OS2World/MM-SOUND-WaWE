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
 * WaWEMemory : Debug version of memory handling functions to be able to     *
 *              detect memory leaks                                          *
 *                                                                           *
 * v1.0 - Initial version                                                    *
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>  // sprintf and file usage
#include <malloc.h>
#include <string.h> // memset()
#include "WaWEMemory.h"

// Then the needed OS/2 API stuffs
#define INCL_TYPES
#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>

// Define the following to have area before and after the allocated memory!
//#define OVERWRITE_DETECTION_AREA         8192
//#define OVERWRITE_DETECTION_AREA_DATA    0xDA

typedef struct MemList_t_struct {
#ifdef OVERWRITE_DETECTION_AREA
  char *pRealPtr;
#endif
  void *pPtr;
  unsigned int uiSize;

  int iLineNum;
  char *pchSrcFile;

  void *pNext;
} MemList_t, *MemList_p;

static MemList_p pMemListHead = NULL;
long dbg_max_allocated_memory = 0;
long dbg_allocated_memory = 0;
HMTX MemoryUsageDebug_Sem = -1;

#ifdef OVERWRITE_DETECTION_AREA
static void dbg_check_for_overwrite(MemList_p pTemp)
{
  int i;
  int iOverWriteStart, iOverWriteSize;
  char *pchMem;

  // Check overwrite before mem array
  iOverWriteSize = 0;
  iOverWriteStart = 0;
  pchMem = pTemp->pRealPtr;
  for (i=0; i<OVERWRITE_DETECTION_AREA; i++)
  {
    if (pchMem[i]!=OVERWRITE_DETECTION_AREA_DATA)
    {
      if (iOverWriteSize==0)
        iOverWriteStart = i;
      iOverWriteSize++;
    }
  }
  if (iOverWriteSize)
  {
    printf("[dbg_check_for_overwrite] : Memory overwrite detected!\n");
    printf("  %d @%p  (%s line %d)\n", pTemp->uiSize, pTemp->pPtr, pTemp->pchSrcFile, pTemp->iLineNum);
    printf("  Overwrite before memory: %d bytes", iOverWriteSize);
    for (i=0; i<25; i++)
      printf("%02x ", pchMem[i+iOverWriteStart]);
    printf("\n");
  }

  // Check overwrite after mem array
  iOverWriteSize = 0;
  iOverWriteStart = 0;
  pchMem = ((char *) (pTemp->pPtr)) + pTemp->uiSize;
  for (i=0; i<OVERWRITE_DETECTION_AREA; i++)
  {
    if (pchMem[i]!=OVERWRITE_DETECTION_AREA_DATA)
    {
      if (iOverWriteSize==0)
        iOverWriteStart = i;
      iOverWriteSize++;
    }
  }
  if (iOverWriteSize)
  {
    printf("[dbg_check_for_overwrite] : Memory overwrite detected!\n");
    printf("  %d @%p  (%s line %d)\n", pTemp->uiSize, pTemp->pPtr, pTemp->pchSrcFile, pTemp->iLineNum);
    printf("  Overwrite after memory: %d bytes", iOverWriteSize);
    for (i=0; i<25; i++)
      printf("%02x ", pchMem[i+iOverWriteStart]);
    printf("\n");
  }
}

static void dbg_check_for_overwrite_all(void)
{
  MemList_p pTemp;

  pTemp=pMemListHead;
  while (pTemp)
  {
    dbg_check_for_overwrite(pTemp);
    pTemp = pTemp->pNext;
  }
}
#endif

void *dbg_malloc_core( unsigned int numbytes, char *pchSrcFile, int iLineNum )
{
  MemList_p pTemp;

  if (DosRequestMutexSem(MemoryUsageDebug_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {

    pTemp = (MemList_p) malloc(sizeof(MemList_t));
    if (pTemp)
    {
#ifdef OVERWRITE_DETECTION_AREA
      pTemp->pPtr = malloc(numbytes + 2*OVERWRITE_DETECTION_AREA);

#else
      pTemp->pPtr = malloc(numbytes);
#endif
      if (pTemp->pPtr)
      {
#ifdef OVERWRITE_DETECTION_AREA
        memset(pTemp->pPtr, OVERWRITE_DETECTION_AREA_DATA, OVERWRITE_DETECTION_AREA);
        pTemp->pRealPtr = pTemp->pPtr;
        pTemp->pPtr = ((char *) (pTemp->pPtr)) + OVERWRITE_DETECTION_AREA;
        memset(((char *)(pTemp->pPtr))+numbytes, OVERWRITE_DETECTION_AREA_DATA, OVERWRITE_DETECTION_AREA);
#endif
        pTemp->uiSize = numbytes;
        pTemp->pNext = pMemListHead;
        pTemp->pchSrcFile = pchSrcFile;
        pTemp->iLineNum = iLineNum;

        pMemListHead = pTemp;

        dbg_allocated_memory += numbytes;
        if (dbg_allocated_memory>dbg_max_allocated_memory)
          dbg_max_allocated_memory = dbg_allocated_memory;

#ifdef OVERWRITE_DETECTION_AREA
        dbg_check_for_overwrite_all();
#endif

        DosReleaseMutexSem(MemoryUsageDebug_Sem);
        return pTemp->pPtr;
      } else
      {
#ifdef DEBUG_BUILD
        printf("[dbg_malloc] : Out of memory (Inside)!\n");
        printf("               Caller: %s line %d\n", pchSrcFile, iLineNum);
#endif
        free(pTemp);
        DosReleaseMutexSem(MemoryUsageDebug_Sem);
        return NULL;
      }
    } else
    {
#ifdef DEBUG_BUILD
      printf("[dbg_malloc] : Out of memory!\n");
      printf("               Caller: %s line %d\n", pchSrcFile, iLineNum);
#endif
      DosReleaseMutexSem(MemoryUsageDebug_Sem);
      return NULL;
    }
  } else  
  {
#ifdef DEBUG_BUILD
    printf("[dbg_malloc] : Could not get semaphore!\n");
#endif
    return malloc(numbytes);
  }
}

void dbg_free_core( void *ptr, char *pchSrcFile, int iLineNum )
{
  MemList_p pTemp, pPrev;

  if (DosRequestMutexSem(MemoryUsageDebug_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    pPrev = NULL;
    pTemp = pMemListHead;
    while ((pTemp) && ((pTemp->pPtr) != ptr)) 
    {
      pPrev = pTemp;
      pTemp = pTemp->pNext;
    }

    if (!pTemp)
    {
#ifdef DEBUG_BUILD
      printf("[dbg_free] : Warning, memory block cannot be found in list (%p)!\n", ptr);
      printf("             Caller: %s line %d\n", pchSrcFile, iLineNum);
#endif
      free(ptr);
    } else
    {
#ifdef OVERWRITE_DETECTION_AREA
      dbg_check_for_overwrite_all();
      free(pTemp->pRealPtr);
#else
      free(ptr);
#endif
      dbg_allocated_memory-=pTemp->uiSize;

      if (pPrev)
      {
        pPrev->pNext = pTemp->pNext;
      } else
      {
        pMemListHead = pTemp->pNext;
      }
      free(pTemp);
    }
    DosReleaseMutexSem(MemoryUsageDebug_Sem);
  } else
  {
#ifdef DEBUG_BUILD
    printf("[dbg_free] : Could not get semaphore!\n");
#endif
    free(ptr);
  }
}

void *dbg_realloc_core( void *old_blk, size_t size, char *pchSrcFile, int iLineNum )
{
  MemList_p pTemp;
  void *newptr;

  if (old_blk==NULL)
  {
    // By definition
    return dbg_malloc_core(size, "Unknown", 0);
  }

  if (size==0)
  {
    // By definition
    dbg_free(old_blk);
    return NULL;
  }

  if (DosRequestMutexSem(MemoryUsageDebug_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    // Find the pointer in our list
    pTemp = pMemListHead;
    while ((pTemp) && ((pTemp->pPtr) != old_blk)) 
      pTemp = pTemp->pNext;

#ifdef OVERWRITE_DETECTION_AREA
    if (!pTemp)
    {
#ifdef DEBUG_BUILD
      printf("[dbg_realloc] : Warning, memory block cannot be found in list (%p)!\n", old_blk);
      printf("                Reallocation failed, returning NULL.\n");
      printf("                Caller: %s line %d\n", pchSrcFile, iLineNum);
#endif
      return NULL;
    }

    newptr = realloc(pTemp->pRealPtr, size+2*OVERWRITE_DETECTION_AREA);
    if (newptr!=NULL)
    {
      // If realloc successful
      dbg_allocated_memory-=pTemp->uiSize;
      dbg_allocated_memory+=size;

      if (dbg_allocated_memory>dbg_max_allocated_memory)
        dbg_max_allocated_memory = dbg_allocated_memory;

      pTemp->uiSize=size;
      pTemp->pRealPtr = newptr;
      pTemp->pPtr = pTemp->pRealPtr + OVERWRITE_DETECTION_AREA;
      memset(((char *)(pTemp->pPtr))+size, OVERWRITE_DETECTION_AREA_DATA, OVERWRITE_DETECTION_AREA);
      pTemp->iLineNum = iLineNum;
      pTemp->pchSrcFile = pchSrcFile;

      dbg_check_for_overwrite_all();

      DosReleaseMutexSem(MemoryUsageDebug_Sem);

      return pTemp->pPtr;
    }

#else
    newptr = realloc(old_blk, size);

    if (newptr!=NULL)
    {
      // If realloc successful
      if (!pTemp)
      {
#ifdef DEBUG_BUILD
        printf("[dbg_realloc] : Warning, memory block cannot be found in list (%p)!\n", old_blk);
        printf("                Caller: %s line %d\n", pchSrcFile, iLineNum);
#endif
        // Add it to list
        pTemp = (MemList_p) malloc(sizeof(MemList_t));
        if (pTemp)
        {
          pTemp->pPtr = newptr;
          pTemp->uiSize = size;
          pTemp->pNext = pMemListHead;
          pTemp->pchSrcFile = "Unknown";
          pTemp->iLineNum = 0;
          pMemListHead = pTemp;
    
          dbg_allocated_memory += size;
          if (dbg_allocated_memory>dbg_max_allocated_memory)
            dbg_max_allocated_memory = dbg_allocated_memory;
          DosReleaseMutexSem(MemoryUsageDebug_Sem);
    
          return newptr;
        } else
        {
#ifdef DEBUG_BUILD
          printf("[dbg_realloc] : Out of memory!\n");
#endif
          DosReleaseMutexSem(MemoryUsageDebug_Sem);
    
          return NULL;
        }
      } else
      {
        dbg_allocated_memory-=pTemp->uiSize;
        dbg_allocated_memory+=size;

        if (dbg_allocated_memory>dbg_max_allocated_memory)
          dbg_max_allocated_memory = dbg_allocated_memory;

        pTemp->uiSize=size;
        pTemp->pPtr = newptr;
        pTemp->iLineNum = iLineNum;
        pTemp->pchSrcFile = pchSrcFile;

        DosReleaseMutexSem(MemoryUsageDebug_Sem);

        return newptr;
      }
    }
#endif
    DosReleaseMutexSem(MemoryUsageDebug_Sem);
  } else
  {
#ifdef DEBUG_BUILD
    printf("[dbg_realloc] : Could not get semaphore!\n");
#endif
    newptr = NULL;
  }
  return newptr;
}

void dbg_ReportMemoryLeak()
{
  MemList_p pTemp;
  long lSum = 0;
  FILE *hFile;

  if (DosRequestMutexSem(MemoryUsageDebug_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    pTemp = pMemListHead;
    if ((pTemp) || (dbg_allocated_memory))
    {
#ifdef DEBUG_BUILD
      printf("Some memory is still allocated!\n");
      printf("Leaked memory: %d Bytes = %d KBytes = %d MBytes\n",
             dbg_allocated_memory,
             dbg_allocated_memory/1024,
             dbg_allocated_memory/(1024*1024)
  	  );
      hFile = stdout; // Report detailed info to standard output
#else
      // Not debug build, so cannot show debug messages. Make audible alarm instead!
      DosBeep(440,50);
      DosSleep(50);
      DosBeep(440,50);
      DosSleep(50);
      DosBeep(440,50);
      hFile = fopen("MemLeak.log", "a+t"); // Detailed info will be put into this file!
#endif

      fprintf(hFile, " --- dbg_ReportMemoryLeak -------------\n");
      while (pTemp)
      {
        fprintf(hFile, "%d @%p  (%s line %d)\n", pTemp->uiSize, pTemp->pPtr, pTemp->pchSrcFile, pTemp->iLineNum);
        lSum+=pTemp->uiSize;
  
        if (pTemp==(pTemp->pNext)) 
        {
          fprintf(hFile, "[dbg_ReportMemoryLeak] : Internal error: LOOOOOOOOOoooooooooop!\n");
          break;
        }
        pTemp = pTemp->pNext;
      }
      fprintf(hFile, " --- SUM: %ld bytes = %ld KBytes ---\n", lSum, lSum/1024);
      fprintf(hFile, " --- Accounted SUM: %ld bytes = %ld KBytes ---\n",
             dbg_allocated_memory,
             dbg_allocated_memory/1024);
      
      if ((hFile) && (hFile!=stdout))
        fclose(hFile);
    }
    DosReleaseMutexSem(MemoryUsageDebug_Sem);
  }
}

int Initialize_MemoryUsageDebug()
{
  int rc;

  if (MemoryUsageDebug_Sem != -1) return TRUE;

  pMemListHead = NULL;
  dbg_allocated_memory = 0;
  dbg_max_allocated_memory = 0;

  rc = DosCreateMutexSem(NULL, &MemoryUsageDebug_Sem, 0, FALSE);
  return (rc == NO_ERROR);
}

void Uninitialize_MemoryUsageDebug()
{
  MemList_p pTemp, pOld;

  // Free remaining memory
  if (DosRequestMutexSem(MemoryUsageDebug_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    pTemp=pMemListHead;
    while (pTemp)
    {
      pOld = pTemp;
      pTemp = pTemp->pNext;
#ifdef OVERWRITE_DETECTION_AREA
      free(pOld->pRealPtr);
#else
      free(pOld->pPtr);
#endif
      free(pOld);
    }
    pMemListHead = NULL;
    DosReleaseMutexSem(MemoryUsageDebug_Sem);
  }
  // And close semaphore
  DosCloseMutexSem(MemoryUsageDebug_Sem); MemoryUsageDebug_Sem = -1;
}
