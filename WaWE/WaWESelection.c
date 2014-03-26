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
 * Selected region list handling routines                                    *
 *****************************************************************************/

#include "WaWE.h"

void CreateRequiredSelectedRegionList(WaWEChannel_p pChannel,
                                      WaWEPosition_t BasePosition,
                                      WaWEPosition_t TargetPosition,
                                      int iSelectionMode)
{
  WaWESelectedRegion_p pTemp, pNew, pPrev, pNext;
  WaWEPosition_t IntersectStart, IntersectEnd, EmptyStart, EmptyEnd, LastRegionEnd;

  // Make sure we'll have a positive region
  if (BasePosition>TargetPosition)
  {
    WaWEPosition_t Temp;
    Temp = BasePosition;
    BasePosition = TargetPosition;
    TargetPosition = Temp;
  }
  if (!pChannel) return;

  // Our job is to use create a new RequiredShownSelectedRegionList
  // from the LastValidSelectedRegionList plus on the newly passed
  // regin (Base->Target), based on iSelectionMode!

  if (DosRequestMutexSem(pChannel->hmtxUseRequiredShownSelectedRegionList, SEM_INDEFINITE_WAIT) == NO_ERROR)
  {
    if (DosRequestMutexSem(pChannel->hmtxUseLastValidSelectedRegionList, SEM_INDEFINITE_WAIT) == NO_ERROR)
    {
      // Ok, we can work on the lists now.
      // First, destroy the old RequiredShown...List, if there is any, because
      // we'll create a new one!
      while (pChannel->pRequiredShownSelectedRegionListHead)
      {
        pTemp = pChannel->pRequiredShownSelectedRegionListHead;
        dbg_free(pTemp);
        pChannel->pRequiredShownSelectedRegionListHead =
          pChannel->pRequiredShownSelectedRegionListHead->pNext;
      }

      // Now create the new RequiredShown list.
      if (iSelectionMode == WAWE_SELECTIONMODE_REPLACE)
      {
        // If the SelectionMode is REPLACE, then create a new list with only this
        // region inside!
        pNew = (WaWESelectedRegion_p) dbg_malloc(sizeof(WaWESelectedRegion_t));
        if (pNew)
        {
          pNew->Start = BasePosition;
          pNew->End = TargetPosition;
          pNew->pNext = NULL;
          pNew->pPrev = NULL;
          pChannel->pRequiredShownSelectedRegionListHead = pNew;
        }
      } else
      {
        // The selection mode is NOT REPLACE, so we need the original list
        // as a base.
        // So copy the LastValid...List to the RequiredShown...List
        pTemp = pChannel->pLastValidSelectedRegionListHead;
        pPrev = NULL;
        while (pTemp)
        {
          pNew = (WaWESelectedRegion_p) dbg_malloc(sizeof(WaWESelectedRegion_t));
          if (pNew)
          {
            pNew->Start = pTemp->Start;
            pNew->End = pTemp->End;
            pNew->pNext = NULL;
            pNew->pPrev = pPrev;
  
            if (pPrev)
              pPrev->pNext = pNew;
            else
              pChannel->pRequiredShownSelectedRegionListHead = pNew;
  
            pPrev = pNew;
	  }
          pTemp = pTemp->pNext;
        }
        // Now modify this list with the new region
        if (!(pChannel->pRequiredShownSelectedRegionListHead))
        {
          // If our list is empty
          if ((iSelectionMode == WAWE_SELECTIONMODE_ADD) ||
              (iSelectionMode == WAWE_SELECTIONMODE_XOR))
          {
            // then create a new list with only this region!
            pNew = (WaWESelectedRegion_p) dbg_malloc(sizeof(WaWESelectedRegion_t));
            if (pNew)
            {
              pNew->Start = BasePosition;
              pNew->End = TargetPosition;
              pNew->pNext = NULL;
              pNew->pPrev = NULL;
              pChannel->pRequiredShownSelectedRegionListHead = pNew;
            }
          }
        } else
        {
          // Our list is not empty, so we have to modify a non-empty
          // region list, based on iSelectionMode and the region.
#ifdef DEBUG_BUILD
//          printf("Current node list (Should be lastvalid!):\n");
//          pTemp = pChannel->pRequiredShownSelectedRegionListHead;
//          while (pTemp)
//          {
//            printf("NODE %d->%d\n", (int) pTemp->Start, (int) pTemp->End);
//            pTemp = pTemp->pNext;
//          }
//          printf("Modifying...\n");
#endif
          switch (iSelectionMode)
	  {
	    case WAWE_SELECTIONMODE_XOR:  // ------- XOR a region to selected region list ------
              pTemp = pChannel->pRequiredShownSelectedRegionListHead;
              LastRegionEnd = 0;
	      while (pTemp)
              {
                // First create a new selected region from the space
                // between the selected regions
                pPrev = pTemp->pPrev;
                if (pPrev)
                  EmptyStart = LastRegionEnd+1;
                else
                  EmptyStart = 0;
                EmptyEnd = pTemp->Start-1;

                LastRegionEnd = pTemp->End;

                if (CalculateRegionIntersect(EmptyStart, EmptyEnd,
                                             BasePosition, TargetPosition,
                                             &IntersectStart, &IntersectEnd))
                {
                  // We have to create a new region
//                  printf("Empty Space Intersect of %d -> %d and %d -> %d is %d -> %d\n",
//                         (int) EmptyStart, (int) EmptyEnd,
//                         (int) BasePosition, (int) TargetPosition,
//                         (int) IntersectStart, (int) IntersectEnd); fflush(stdout);

                  pNew = (WaWESelectedRegion_p) dbg_malloc(sizeof(WaWESelectedRegion_t));
                  if (pNew)
                  {
                    pNew->Start = IntersectStart;
                    pNew->End = IntersectEnd;
                    pNew->pNext = pTemp;
                    pNew->pPrev = pPrev;

                    if (pPrev)
                      pPrev->pNext = pNew;
                    else
                      pChannel->pRequiredShownSelectedRegionListHead = pNew;

                    pTemp->pPrev = pNew;
                  }
                }

                // Then cut stuff from this region
                if (CalculateRegionIntersect(pTemp->Start, pTemp->End,
                                             BasePosition, TargetPosition,
                                             &IntersectStart, &IntersectEnd))
                {
//                  printf("Deleting from region\n");
                  // We have to delete some from this region!
                  if ((IntersectStart == pTemp->Start) &&
                      (IntersectEnd == pTemp->End))
                  {
//                    printf("  deleting All\n");
                    // All the region should be killed!
                    pPrev = pTemp->pPrev;
                    pNext = pTemp->pNext;
                    if (pPrev)
                      pPrev->pNext = pNext;
                    else
                      pChannel->pRequiredShownSelectedRegionListHead = pNext;
                    if (pNext)
                      pNext->pPrev = pPrev;
                    dbg_free(pTemp);
                    // Go for the next selected region
                    pTemp = pNext;
                  } else
                  if ((IntersectStart == pTemp->Start) &&
                      (IntersectEnd < pTemp->End))
                  {
//                    printf("  deleting left side (modifying)\n");
                    // The region should be modified!
                    pTemp->Start = IntersectEnd+1;
                    // Go for the next selected region
                    pTemp = pTemp->pNext;
                  } else
                  if ((IntersectStart > pTemp->Start) &&
                      (IntersectEnd == pTemp->End))
                  {
//                    printf("  deleting right side (modifying)\n");
                    // The region should be modified!
                    pTemp->End = IntersectStart-1;
                    // Go for the next selected region
                    pTemp = pTemp->pNext;
                  } else
                  if ((IntersectStart > pTemp->Start) &&
                      (IntersectEnd < pTemp->End))
                  {
//                    printf("  deleting from middle (cutting)\n");
                    // The region should be cut by two
                    pNew = (WaWESelectedRegion_p) dbg_malloc(sizeof(WaWESelectedRegion_t));
		    if (pNew)
		    {
		      pNew->Start = IntersectEnd+1;
		      pNew->End = pTemp->End;
		      pNew->pNext = pTemp->pNext;
		      pNew->pPrev = pTemp;

                      pNext = pTemp->pNext;
		      if (pNext)
                        pNext->pPrev = pNew;

                      pTemp->pNext = pNew;

                      pTemp->End = IntersectStart-1;
		    }
                    // Go for the next selected region
                    pTemp = pTemp->pNext;
                  } else
                  {
                    // Hm, this cannot be. :) But who knows...
#ifdef DEBUG_BUILD
                    printf("[CreateRequiredSelectedRegionList] : Warning! Invalid branch!\n"); fflush(stdout);
#endif
                    // Go for the next selected region
                    pTemp = pTemp->pNext;
                  }
                } else
                {
                  // Go for the next selected region
                  pTemp = pTemp->pNext;
                }
              }
              // Now create a selected region after the last valid regions, if we have to!
              pTemp = pChannel->pRequiredShownSelectedRegionListHead;
              while ((pTemp) && (pTemp->pNext)) pTemp = pTemp->pNext;
              if (pTemp)
              {
                EmptyStart = LastRegionEnd+1;
                EmptyEnd = TargetPosition;

                if (CalculateRegionIntersect(EmptyStart, EmptyEnd,
                                             BasePosition, TargetPosition,
                                             &IntersectStart, &IntersectEnd))
                {
                  // We have to create a new region
//                  printf("Last empty space Intersect of %d -> %d and %d -> %d is %d -> %d\n",
//                         (int) EmptyStart, (int) EmptyEnd,
//                         (int) BasePosition, (int) TargetPosition,
//                         (int) IntersectStart, (int) IntersectEnd); fflush(stdout);
                  pNew = (WaWESelectedRegion_p) dbg_malloc(sizeof(WaWESelectedRegion_t));
                  if (pNew)
                  {
                    pNew->Start = IntersectStart;
                    pNew->End = IntersectEnd;
                    pNew->pNext = NULL;
                    pNew->pPrev = pTemp;
  
                    pTemp->pNext = pNew;
                  }
                }
              }
              break;
            case WAWE_SELECTIONMODE_SUB:  // ------- SUB a region to selected region list ------
              pTemp = pChannel->pRequiredShownSelectedRegionListHead;
	      while (pTemp)
              {
                if (CalculateRegionIntersect(pTemp->Start, pTemp->End,
                                             BasePosition, TargetPosition,
                                             &IntersectStart, &IntersectEnd))
                {
                  // We have to delete some from this region!
                  if ((IntersectStart == pTemp->Start) &&
                      (IntersectEnd == pTemp->End))
                  {
                    // All the region should be killed!
                    pPrev = pTemp->pPrev;
                    pNext = pTemp->pNext;
                    if (pPrev)
                      pPrev->pNext = pNext;
                    else
                      pChannel->pRequiredShownSelectedRegionListHead = pNext;
                    if (pNext)
                      pNext->pPrev = pPrev;
                    dbg_free(pTemp);
                    // Go for the next selected region
                    pTemp = pNext;
                  } else
                  if ((IntersectStart == pTemp->Start) &&
                      (IntersectEnd < pTemp->End))
                  {
                    // The region should be modified!
                    pTemp->Start = IntersectEnd+1;
                    // Go for the next selected region
                    pTemp = pTemp->pNext;
                  } else
                  if ((IntersectStart > pTemp->Start) &&
                      (IntersectEnd == pTemp->End))
                  {
                    // The region should be modified!
                    pTemp->End = IntersectStart-1;
                    // Go for the next selected region
                    pTemp = pTemp->pNext;
                  } else
                  if ((IntersectStart > pTemp->Start) &&
                      (IntersectEnd < pTemp->End))
                  {
                    // The region should be cut by two
                    pNew = (WaWESelectedRegion_p) dbg_malloc(sizeof(WaWESelectedRegion_t));
		    if (pNew)
		    {
		      pNew->Start = IntersectEnd+1;
		      pNew->End = pTemp->End;
		      pNew->pNext = pTemp->pNext;
		      pNew->pPrev = pTemp;

                      pNext = pTemp->pNext;
		      if (pNext)
                        pNext->pPrev = pNew;

                      pTemp->pNext = pNew;

                      pTemp->End = IntersectStart-1;
		    }
                    // Go for the next selected region
                    pTemp = pTemp->pNext;
                  } else
                  {
                    // Hm, this cannot be. :) But who knows...
#ifdef DEBUG_BUILD
                    printf("[CreateRequiredSelectedRegionList] : Warning! Invalid branch!\n"); fflush(stdout);
#endif
                    // Go for the next selected region
                    pTemp = pTemp->pNext;
                  }
                } else
                {
                  // Go for the next selected region
                  pTemp = pTemp->pNext;
                }
	      }
              break;
	    case WAWE_SELECTIONMODE_ADD:  // ------- ADD a region to selected region list ------
            default:
              pTemp = pChannel->pRequiredShownSelectedRegionListHead;
	      while (pTemp)
	      {
		if (pTemp->Start>BasePosition)
		{
                  pPrev = pTemp->pPrev;
		  if ((!pPrev) || (pPrev->End<BasePosition))
		  {
		    // We have to insert it here.
//		    printf("Inserting here\n"); fflush(stdout);
		    pNew = (WaWESelectedRegion_p) dbg_malloc(sizeof(WaWESelectedRegion_t));
		    if (pNew)
		    {
		      pNew->Start = BasePosition;
		      pNew->End = TargetPosition;

		      pPrev = pTemp->pPrev;

		      pNew->pNext = pTemp;
		      pNew->pPrev = pPrev;

		      if (pPrev)
			pPrev->pNext = pNew;
		      else
			pChannel->pRequiredShownSelectedRegionListHead = pNew;

		      pTemp->pPrev = pNew;
		      pTemp = pTemp->pPrev;
		    }
	          }
                } else
                if (pTemp->End>=BasePosition)
		{
//		  printf("Modifying here\n");
                  // Hm, the selected region is also in this region
                  if (pTemp->End<TargetPosition)
                    pTemp->End = TargetPosition;
		} else
		{
		  // Make sure we can insert to the end too:
		  if (!(pTemp->pNext))
		  {
//		    printf("Inserting to the end\n"); fflush(stdout);
		    pNew = (WaWESelectedRegion_p) dbg_malloc(sizeof(WaWESelectedRegion_t));
		    if (pNew)
		    {
		      pNew->Start = BasePosition;
		      pNew->End = TargetPosition;
		      pNew->pNext = NULL;
		      pNew->pPrev = pTemp;
		      pTemp->pNext = pNew;
		    }
		  }
		}
		// Now check if we can merge two neightbourours regions
		pPrev = pTemp->pPrev;
		if (pPrev)
		{
		  if (pPrev->End >= pTemp->Start)
		  {
                    // Modifying previous node, and deleting current node
//		    printf("Merging two\n");
//		    printf("%d->%d and %d->%d\n",
//			   (int) pPrev->Start, (int) pPrev->End,
//			   (int) pTemp->Start, (int) pTemp->End);

		    pNext = pTemp->pNext;

		    pPrev->pNext = pNext;
                    if (pNext)
		      pNext->pPrev = pPrev;

		    if (pTemp->End>pPrev->End)
		      pPrev->End = pTemp->End;

		    dbg_free(pTemp);
                    pTemp = pPrev;
		  }
		}
                // Go for the next selected region
                pTemp = pTemp->pNext;
	      }
	      break;
          }
        }
      }
#ifdef DEBUG_BUILD
//      printf("New node list:\n");
//      pTemp = pChannel->pRequiredShownSelectedRegionListHead;
//      while (pTemp)
//      {
//        printf("NODE %d->%d\n", (int) pTemp->Start, (int) pTemp->End);
//        pTemp = pTemp->pNext;
//      }
//      printf("That's it\n");
#endif

      DosReleaseMutexSem(pChannel->hmtxUseLastValidSelectedRegionList);
    }
    DosReleaseMutexSem(pChannel->hmtxUseRequiredShownSelectedRegionList);
  }
}

// Copies the Required RegionList to LastValid
void MakeRequiredRegionListLastValid(WaWEChannel_p pChannel)
{
  WaWESelectedRegion_p pTemp, pNew, pPrev;

  if (DosRequestMutexSem(pChannel->hmtxUseRequiredShownSelectedRegionList, SEM_INDEFINITE_WAIT) == NO_ERROR)
  {
    if (DosRequestMutexSem(pChannel->hmtxUseLastValidSelectedRegionList, SEM_INDEFINITE_WAIT) == NO_ERROR)
    {
      // Ok, we can work on the lists now.
      // First, destroy the old LastValid...List, if ther is any, because
      // we'll create a new one!
      while (pChannel->pLastValidSelectedRegionListHead)
      {
	pTemp = pChannel->pLastValidSelectedRegionListHead;
	dbg_free(pTemp);
	pChannel->pLastValidSelectedRegionListHead =
	  pChannel->pLastValidSelectedRegionListHead->pNext;
      }
  
      // Then copy the RequiredShown...List to the LastValid...List
//      printf("Copying RequiredShown to lastvalid:\n");
      pPrev = NULL;
      pTemp = pChannel->pRequiredShownSelectedRegionListHead;
      while (pTemp)
      {
	pNew = (WaWESelectedRegion_p) dbg_malloc(sizeof(WaWESelectedRegion_t));
	if (pNew)
	{
	  pNew->pPrev = pPrev;
	  pNew->pNext = NULL;
	  pNew->Start = pTemp->Start;
	  pNew->End = pTemp->End;
  
	  if (!pPrev)
	    pChannel->pLastValidSelectedRegionListHead = pNew;
	  else
	    pPrev->pNext = pNew;
  
	  pPrev = pNew;
	}
	pTemp = pTemp->pNext;
      }
      DosReleaseMutexSem(pChannel->hmtxUseLastValidSelectedRegionList);
    }
    DosReleaseMutexSem(pChannel->hmtxUseRequiredShownSelectedRegionList);
  }
}

