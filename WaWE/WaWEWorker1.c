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
 * Worker-1 thread                                                           *
 *****************************************************************************/

#include "WaWE.h"

TID tidWorker1;
HQUEUE hqWorker1MessageQueue;
int iWorker1State = WORKER1STATE_IDLE;

#define SELECTED_REGION_COLOR  (0x00b0b050)

#define DRAWCURRENTPOSITIONMARKER(hps, xpos, height)                  \
  {                                                                   \
    POINTL ptl;                                                       \
    GpiSetColor(hps, RGB_YELLOW);                                     \
    ptl.x = xpos;                                                     \
    ptl.y = 0;                                                        \
    GpiMove(hps, &ptl);                                               \
    ptl.x = xpos;                                                     \
    ptl.y = height;                                                   \
    GpiLine(hps, &ptl);                                               \
  }

#define DRAWSELECTEDREGION(hps, ImageBufferSize, BufferFirstSamplePos, FirstPos, LastPos, ClipXStart, ClipXEnd) \
  if (DosRequestMutexSem(pChannel->hmtxUseShownSelectedRegionList, SEM_INDEFINITE_WAIT)==NO_ERROR)     \
  {                                                                                               \
    WaWESelectedRegion_p pShown;                                                                  \
    WaWEPosition_t IntersectStart, IntersectEnd;                                                  \
                                                                                                  \
    GpiSetMix(hps, FM_XOR);                                                                       \
    GpiSetColor(hps, SELECTED_REGION_COLOR);                                                      \
    /* Check it region by region, and if it's in our area */                                      \
    /* then XOR that part!                                */                                      \
    pShown = pChannel->pShownSelectedRegionListHead;                                              \
    while (pShown)                                                                                \
    {                                                                                             \
      /* Check region if has common part with what is on the screen */                            \
      if (CalculateRegionIntersect(pShown->Start, pShown->End,                                    \
                                   FirstPos, LastPos,                                             \
                                   &IntersectStart, &IntersectEnd))                               \
      {                                                                                           \
        POINTL ptl1, ptl2;                                                                        \
        /* It has common part, so XOR! */                                                         \
                                                                                                  \
        IntersectEnd++;                                                                           \
                                                                                                  \
        ptl1.y = 0;                                                                               \
        ptl1.x = (IntersectStart - BufferFirstSamplePos) *                                        \
          pChannel->pParentChannelSet->ZoomCount / pChannel->pParentChannelSet->ZoomDenom ;       \
        if (ptl1.x<ClipXStart) ptl1.x = ClipXStart;                                               \
        GpiSetCurrentPosition(hps, &ptl1);                                                        \
        ptl2.y = ImageBufferSize.cy-1;                                                            \
        ptl2.x = (IntersectEnd - BufferFirstSamplePos) *                                          \
          pChannel->pParentChannelSet->ZoomCount / pChannel->pParentChannelSet->ZoomDenom - 1;    \
        if (ptl2.x<ptl1.x) ptl2.x = ptl1.x;                                                       \
        if (ptl2.x>ClipXEnd) ptl2.x = ClipXEnd;                                                   \
        GpiBox(hps,                                                                               \
               DRO_FILL,                                                                          \
               &ptl2,                                                                             \
               0, 0);                                                                             \
      }                                                                                           \
      pShown = pShown->pNext;                                                                     \
    }                                                                                             \
    /* Set back old mixmode */                                                                    \
    GpiSetMix(hps, FM_DEFAULT);                                                                   \
                                                                                                  \
    DosReleaseMutexSem(pChannel->hmtxUseShownSelectedRegionList);                                 \
  }


__int64 GCD(__int64 llValue1, __int64 llValue2)
{
  __int64 llTemp;

  // Make sure we won't get zero
  if ((!llValue1) || (!llValue2)) return 1;

  while (llValue1 % llValue2)
  {
    llTemp = llValue1;
    llValue1 = llValue2;
    llValue2 = llTemp % llValue2;
  }

  return llValue2;
}

int CalculateRegionIntersect(WaWEPosition_t Start1, WaWEPosition_t End1,
                             WaWEPosition_t Start2, WaWEPosition_t End2,
                             WaWEPosition_p pIntersectStart, WaWEPosition_p pIntersectEnd)
{
  // This function calculates the intersect of the two regions.
  // Returns non-zero if they intersect, or 0 if there is no common part

  if ((Start1>End2) ||
      (End1<Start2))
    // Ups, we cannot reuse anything...
    return 0;

  // Seems like we can reuse something!
  if (Start1>Start2)
    *pIntersectStart = Start1;
  else
    *pIntersectStart = Start2;
  if (End1<End2)
    *pIntersectEnd = End1;
  else
    *pIntersectEnd = End2;

  return 1;
}


void RedrawChannel(WaWEChannel_p pChannel, int bDontOptimize)
{
  SIZEL CurrentImageBufferSize;
  SIZEL RequiredImageBufferSize;
  WaWEPosition_t RequiredFirstSamplePos;
  WaWEPosition_t RequiredLastSamplePos;
  int            iRequiredWidth;
  int            iRequiredHeight;
  HDC hdc;
  HAB hab;
  HBITMAP hbmp;
  BITMAPINFOHEADER2 bmphdr;
  HPS hps;
  WaWEEdP_Draw_Desc_t DrawDesc;
  long lCurrentPosition;

  if (!pChannel) return;

#ifdef DEBUG_BUILD
//  printf("[Worker-1] : Redraw channel %d (DontOptimize=%d)\n", pChannel->iChannelID, bDontOptimize);
#endif

  // Notify others that this channel-set is being used!
  pChannel->pParentChannelSet->lUsageCounter++;

  if (pChannel->pEditorPlugin)
  {
    if (DosRequestMutexSem(pChannel->hmtxUseImageBuffer, 1000)==NO_ERROR)
    {
      // Get how big the current image is!
      if (pChannel->hpsCurrentImageBuffer)
        GpiQueryPS(pChannel->hpsCurrentImageBuffer, &CurrentImageBufferSize);
      else
      {
        CurrentImageBufferSize.cx = 0;
        CurrentImageBufferSize.cy = 0;
      }
      // Get the draw parameters!
      if (DosRequestMutexSem(pChannel->hmtxUseRequiredFields, 1000)==NO_ERROR)
      {
        RequiredFirstSamplePos = pChannel->RequiredFirstSamplePos;
        RequiredLastSamplePos = pChannel->RequiredLastSamplePos;
        iRequiredWidth = pChannel->RequiredWidth;
        iRequiredHeight = pChannel->RequiredHeight;
        DosReleaseMutexSem(pChannel->hmtxUseRequiredFields);
      } else
      {
        // Could not grab semaphore
        RequiredFirstSamplePos = pChannel->FirstSamplePos;
        RequiredLastSamplePos = pChannel->LastSamplePos;
        iRequiredWidth = CurrentImageBufferSize.cx;
        iRequiredHeight = CurrentImageBufferSize.cy;
      }

      // Only redraw image if something has been changed from current setting!
      if (bDontOptimize ||
          (CurrentImageBufferSize.cx!=iRequiredWidth) ||
          (CurrentImageBufferSize.cy!=iRequiredHeight) ||
          (pChannel->FirstSamplePos!=RequiredFirstSamplePos) ||
          (pChannel->LastSamplePos!=RequiredLastSamplePos))
      {
        // Now redraw image into a new PS.

        // Tell the main window that we'll work...
        iWorker1State = WORKER1STATE_REDRAWING;
        WinPostMsg(hwndClient, WM_UPDATEWINDOWTITLE, NULL, NULL);

        // After redrawing, check if it's still the same size and other parameters
        // as the image buffer we had to create, and if it is, replace the old one
        // with the new one!

        // First:
        // Calculate position of CurrentPosition marker in pixels!
        if ((pChannel->CurrentPosition>=RequiredFirstSamplePos) &&
            (pChannel->CurrentPosition<=RequiredLastSamplePos))
        {
          lCurrentPosition =
            (pChannel->CurrentPosition - RequiredFirstSamplePos) *
            (pChannel->pParentChannelSet->ZoomCount) / (pChannel->pParentChannelSet->ZoomDenom);
        } else
          lCurrentPosition = -1;

        // Create DC and PS for this area
        hab = WinQueryAnchorBlock(pChannel->hwndEditAreaWindow);
        hdc = DevOpenDC(hab, OD_MEMORY, "*", 0L, NULL, NULLHANDLE);
        if (hdc)
        {
          RequiredImageBufferSize.cx = iRequiredWidth;
          RequiredImageBufferSize.cy = iRequiredHeight;
          hps = GpiCreatePS(hab, hdc, &RequiredImageBufferSize, PU_PELS | GPIT_NORMAL | GPIA_ASSOC);
          // Ok, we have a PS. Now create a bitmap, and associate with it!
          memset(&bmphdr, 0, sizeof(bmphdr));
          bmphdr.cbFix = sizeof(bmphdr);
          bmphdr.cx = RequiredImageBufferSize.cx;
          bmphdr.cy = RequiredImageBufferSize.cy;
          bmphdr.cPlanes = 1;
          bmphdr.cBitCount = 24;
          hbmp = GpiCreateBitmap(hps,
                                 &bmphdr,
                                 0,
                                 NULL,
                                 NULL);
          if (hbmp) GpiSetBitmap(hps, hbmp);
          // Set RGB mode in PS!
          GpiCreateLogColorTable(hps, 0, LCOLF_RGB, 0, 0, NULL);
  
          // Ok, we have a new PS, in which the plugin can draw the stuff.
          // For optimization, check if we have to redraw the whole stuff, or
          // we can reuse some parts of the old image!
          // Note, that if the zoom is not 1/1, we have to redraw the whole area,
          // because we don't know how pixel/sample assigments have changed,
          // so we cannot reuse parts of the old image!
          if ((!bDontOptimize) &&
              (CurrentImageBufferSize.cy==RequiredImageBufferSize.cy) &&
              (pChannel->pParentChannelSet->ZoomCount == pChannel->pParentChannelSet->ZoomDenom)
             )
          {
            // Aaaah, fine, we have to draw an image with the same height as the old,
            // with zoom of 1/1, so
            // we have the hope that we don't have to redraw everything!
            int iCurrentPixelWidth, iRequiredPixelWidth;
            WaWEPosition_t CurrentSampleWidth, RequiredSampleWidth;
            __int64 i64GCD;
  
            int iReusableFirstSrcX, iReusableLastSrcX;
            int iReusableFirstDstX, iReusableLastDstX;
            WaWEPosition_t ReusableFirstPos, ReusableLastPos;
  
            // Calculate how many parts can be reused of the old buffer, and
            // where, then copy that part to the new HPS
  
            // If the granulity (PixelWidth/SampleWidth) of the old and new drawing
            // is the same, then we can reuse some parts of the old image.
            // Otherwise, we have to redraw it all.
  
            // Let's see the Pixel/Sample for the current and for the Required buffers!
            iCurrentPixelWidth = CurrentImageBufferSize.cx;
            iRequiredPixelWidth = RequiredImageBufferSize.cx;
            CurrentSampleWidth = pChannel->LastSamplePos - pChannel->FirstSamplePos + 1;
            RequiredSampleWidth = RequiredLastSamplePos - RequiredFirstSamplePos + 1;
  
            // Simplify them, to be comparable!
            // To do this, we calculate the greatest common divider for them, and
            // divide them with it!
  
            i64GCD = GCD(iCurrentPixelWidth, CurrentSampleWidth);
            iCurrentPixelWidth /= i64GCD;
            CurrentSampleWidth /= i64GCD;
  
            i64GCD = GCD(iRequiredPixelWidth, RequiredSampleWidth);
            iRequiredPixelWidth /= i64GCD;
            RequiredSampleWidth /= i64GCD;
  
            // Now they are comparable.
            if ((iCurrentPixelWidth == iRequiredPixelWidth) &&
                (CurrentSampleWidth == RequiredSampleWidth))
            {
              // Good, the ratio is the same! We can reuse some of the old graphics!
#ifdef DEBUG_BUILD
//              printf("[Worker-1] : Same ratio! Reusing parts, if possible!\n");
//              printf("  FSP: %d\n", pChannel->FirstSamplePos);
//              printf("  LSP: %d\n", pChannel->LastSamplePos);
//              printf(" RFSP: %d\n", RequiredFirstSamplePos);
//              printf(" RLSP: %d\n", RequiredLastSamplePos);
#endif
              if (!CalculateRegionIntersect(pChannel->FirstSamplePos, pChannel->LastSamplePos,
                                            RequiredFirstSamplePos, RequiredLastSamplePos,
                                            &ReusableFirstPos, &ReusableLastPos))
              {
                // Ups, we cannot reuse anything...
                iReusableLastSrcX = iReusableFirstSrcX = iReusableLastDstX = iReusableFirstDstX = -1;
                ReusableFirstPos = ReusableLastPos = -1;
              } else
              {
                // Avoid dividing by zero!
                if (!(pChannel->LastSamplePos - pChannel->FirstSamplePos + 1))
                {
                  iReusableLastSrcX = iReusableFirstSrcX = iReusableLastDstX = iReusableFirstDstX = -1;
                  ReusableFirstPos = ReusableLastPos = -1;
                }
                else
                {
                  iReusableFirstSrcX = CurrentImageBufferSize.cx * (ReusableFirstPos - pChannel->FirstSamplePos) / (pChannel->LastSamplePos - pChannel->FirstSamplePos + 1);
                  iReusableLastSrcX = CurrentImageBufferSize.cx * (ReusableLastPos - pChannel->FirstSamplePos) / (pChannel->LastSamplePos - pChannel->FirstSamplePos + 1);
                }
                // Avoid dividing by zero!
                if (!(RequiredLastSamplePos - RequiredFirstSamplePos + 1))
                {
                  iReusableLastSrcX = iReusableFirstSrcX = iReusableLastDstX = iReusableFirstDstX = -1;
                  ReusableFirstPos = ReusableLastPos = -1;
                } else
                {
                  iReusableFirstDstX = RequiredImageBufferSize.cx * (ReusableFirstPos - RequiredFirstSamplePos) / (RequiredLastSamplePos - RequiredFirstSamplePos + 1);
                  iReusableLastDstX = RequiredImageBufferSize.cx * (ReusableLastPos - RequiredFirstSamplePos) / (RequiredLastSamplePos - RequiredFirstSamplePos + 1);
                }
              }
            } else
            {
              // Bad, different ratio. Cannot reuse a bit. :(
              iReusableLastSrcX = iReusableFirstSrcX = iReusableLastDstX = iReusableFirstDstX = -1;
              ReusableFirstPos = ReusableLastPos = -1;
            }
  
            // Copy reusable parts from old bitmap to new bitmap!
            if ((iReusableFirstSrcX>=0) &&
                (iReusableLastSrcX>=0) &&
                (iReusableFirstDstX>=0) &&
                (iReusableLastDstX>=0))
            {
              POINTL Points[3];
  
#ifdef DEBUG_BUILD
//              printf("[Worker-1] : Reusing part: %d to %d -> %d to %d (sample %d to %d)\n",
//                     (int) iReusableFirstSrcX,
//                     (int) iReusableLastSrcX,
//                     (int) iReusableFirstDstX,
//                     (int) iReusableLastDstX,
//                     (int) ReusableFirstPos,
//                     (int) ReusableLastPos);
#endif

              // Tx then Sx
              Points[0].x = iReusableFirstDstX;
              Points[0].y = 0;
              Points[1].x = iReusableLastDstX+1; // Upper right corner is non-inclusive in GpiBitBlt!!
              Points[1].y = CurrentImageBufferSize.cy;

              Points[2].x = iReusableFirstSrcX;
              Points[2].y = 0;
//              Points[3].x = iReusableLastSrcX+1; // Upper right corner is non-inclusive in GpiBitBlt!!
//              Points[3].y = CurrentImageBufferSize.cy;

              GpiBitBlt(hps, pChannel->hpsCurrentImageBuffer,
                        3, &(Points[0]),
                        ROP_SRCCOPY,
                        BBO_IGNORE);
            }
  
            // Destroy old Bitmap, PS and DC, if there is!
            if (pChannel->hpsCurrentImageBuffer)
            {
              hdc = GpiQueryDevice(pChannel->hpsCurrentImageBuffer);
              hbmp = GpiSetBitmap(pChannel->hpsCurrentImageBuffer, NULLHANDLE);
              if (hbmp)
                GpiDeleteBitmap(hbmp);
              GpiDestroyPS(pChannel->hpsCurrentImageBuffer);
              DevCloseDC(hdc);
              pChannel->hpsCurrentImageBuffer = NULLHANDLE;
            }
            // Don't block access to current image buffer while drawing new!
            DosReleaseMutexSem(pChannel->hmtxUseImageBuffer);
  
            // Redraw the needed part of the new buffer
            if ((iReusableFirstSrcX>=0) &&
                (iReusableLastSrcX>=0) &&
                (iReusableFirstDstX>=0) &&
                (iReusableLastDstX>=0))
            {
              // There was a reused part, so redraw only the parts around that!
              if (iReusableFirstDstX>0)
              {
                DrawDesc.hpsImageBuffer = hps;
                DrawDesc.iDrawAreaLeftX = 0;
                DrawDesc.iDrawAreaRightX = iReusableFirstDstX-1;
                DrawDesc.pChannelDesc = pChannel;
                DrawDesc.FirstSamplePos = RequiredFirstSamplePos;
                DrawDesc.LastSamplePos = RequiredLastSamplePos;
#ifdef DEBUG_BUILD
//                printf("[Worker-1] : DrawDesc@1 is:\n");
//                printf("   FirstSample=%d  LastSample=%d\n", (int) (DrawDesc.FirstSamplePos),(int) (DrawDesc.LastSamplePos));
//                printf("   iDrawAreaLeftX=%d  DrawAreaRightX=%d\n", DrawDesc.iDrawAreaLeftX, DrawDesc.iDrawAreaRightX);
#endif
                pChannel->pEditorPlugin->TypeSpecificInfo.EditorPluginInfo.fnDraw(&DrawDesc);

#ifdef DEBUG_BUILD
//                printf("[Worker-1] : fnDraw returned.\n"); fflush(stdout);
#endif

		// Check if this place is in a selected region, then
                // change it to be selected!
                DRAWSELECTEDREGION(hps, RequiredImageBufferSize, RequiredFirstSamplePos,
                                   RequiredFirstSamplePos, ReusableFirstPos-1,
                                   0, iReusableFirstDstX-1);

		// If the current position marker is in this area, then draw it!
                if ((lCurrentPosition>=0) && (lCurrentPosition<=iReusableFirstDstX-1))
                  DRAWCURRENTPOSITIONMARKER(hps, lCurrentPosition, RequiredImageBufferSize.cy);
#ifdef DEBUG_BUILD
//                printf("[Worker-1] : DrawPosMarker done\n"); fflush(stdout);
#endif

              }
              if (iReusableLastDstX<RequiredImageBufferSize.cx-1)
              {
                DrawDesc.hpsImageBuffer = hps;
                DrawDesc.iDrawAreaLeftX = iReusableLastDstX+1;
                DrawDesc.iDrawAreaRightX = RequiredImageBufferSize.cx-1;
                DrawDesc.pChannelDesc = pChannel;
                DrawDesc.FirstSamplePos = RequiredFirstSamplePos;
                DrawDesc.LastSamplePos = RequiredLastSamplePos;
#ifdef DEBUG_BUILD
//                printf("[Worker-1] : DrawDesc@2 is:\n");
//                printf("   FirstSample=%d  LastSample=%d\n", (int) (DrawDesc.FirstSamplePos),(int) (DrawDesc.LastSamplePos));
//                printf("   iDrawAreaLeftX=%d  DrawAreaRightX=%d\n", DrawDesc.iDrawAreaLeftX, DrawDesc.iDrawAreaRightX);
#endif
                pChannel->pEditorPlugin->TypeSpecificInfo.EditorPluginInfo.fnDraw(&DrawDesc);

#ifdef DEBUG_BUILD
//                printf("[Worker-1] : fnDraw returned.\n"); fflush(stdout);
#endif

		// Check if this place is in a selected region, then
                // change it to be selected!
                DRAWSELECTEDREGION(hps, RequiredImageBufferSize, RequiredFirstSamplePos,
                                   ReusableLastPos+1, RequiredLastSamplePos,
                                   iReusableLastDstX+1, RequiredImageBufferSize.cx-1);

		// If the current position marker is in this area, then draw it!
                if ((lCurrentPosition>=iReusableLastDstX+1) &&
                    (lCurrentPosition<=RequiredImageBufferSize.cx-1))
                  DRAWCURRENTPOSITIONMARKER(hps, lCurrentPosition, RequiredImageBufferSize.cy);
#ifdef DEBUG_BUILD
//                printf("[Worker-1] : DrawPosMarker done\n"); fflush(stdout);
#endif
              }
            } else
            {
              // There was no reused part, so redraw everything!
              DrawDesc.hpsImageBuffer = hps;
              DrawDesc.iDrawAreaLeftX = 0;
              DrawDesc.iDrawAreaRightX = RequiredImageBufferSize.cx-1;
              DrawDesc.pChannelDesc = pChannel;
              DrawDesc.FirstSamplePos = RequiredFirstSamplePos;
              DrawDesc.LastSamplePos = RequiredLastSamplePos;
#ifdef DEBUG_BUILD
//              printf("[Worker-1] : DrawDesc@3 is:\n");
//              printf("   FirstSample=%d  LastSample=%d\n", (int) (DrawDesc.FirstSamplePos),(int) (DrawDesc.LastSamplePos));
//              printf("   iDrawAreaLeftX=%d  DrawAreaRightX=%d\n", DrawDesc.iDrawAreaLeftX, DrawDesc.iDrawAreaRightX);
#endif
              pChannel->pEditorPlugin->TypeSpecificInfo.EditorPluginInfo.fnDraw(&DrawDesc);

#ifdef DEBUG_BUILD
//                printf("[Worker-1] : fnDraw returned.\n"); fflush(stdout);
#endif

              // Check if this place is in a selected region, then
              // change it to be selected!
              DRAWSELECTEDREGION(hps, RequiredImageBufferSize, RequiredFirstSamplePos,
                                 RequiredFirstSamplePos, RequiredLastSamplePos,
                                 0, RequiredImageBufferSize.cx-1);

	      // If the current position marker is in this area, then draw it!
              if ((lCurrentPosition>=0) &&
                  (lCurrentPosition<=RequiredImageBufferSize.cx-1))
                DRAWCURRENTPOSITIONMARKER(hps, lCurrentPosition, RequiredImageBufferSize.cy);
#ifdef DEBUG_BUILD
//                printf("[Worker-1] : DrawPosMarker done\n"); fflush(stdout);
#endif
            }
          } else
          {
            // Damned! Different height! We have to redraw the whole area!
#ifdef DEBUG_BUILD
//            printf("[Worker-1] : Different height! Redrawing all!\n");
#endif
  
            // Destroy old Bitmap, PS and DC, if there is!
            if (pChannel->hpsCurrentImageBuffer)
            {
              hdc = GpiQueryDevice(pChannel->hpsCurrentImageBuffer);
              hbmp = GpiSetBitmap(pChannel->hpsCurrentImageBuffer, NULLHANDLE);
              if (hbmp)
                GpiDeleteBitmap(hbmp);
              GpiDestroyPS(pChannel->hpsCurrentImageBuffer);
              DevCloseDC(hdc);
              pChannel->hpsCurrentImageBuffer = NULLHANDLE;
            }
            // Don't block access to current image buffer while drawing new!
            DosReleaseMutexSem(pChannel->hmtxUseImageBuffer);
  
            // Now call the plugin to do redraw the whole stuff!
            DrawDesc.hpsImageBuffer = hps;
            DrawDesc.iDrawAreaLeftX = 0;
            DrawDesc.iDrawAreaRightX = RequiredImageBufferSize.cx-1;
            DrawDesc.pChannelDesc = pChannel;
            DrawDesc.FirstSamplePos = RequiredFirstSamplePos;
            DrawDesc.LastSamplePos = RequiredLastSamplePos;
#ifdef DEBUG_BUILD
//            printf("[Worker-1] : DrawDesc@4 is:\n");
//            printf("   FirstSample=%d  LastSample=%d\n", (int) (DrawDesc.FirstSamplePos),(int) (DrawDesc.LastSamplePos));
//            printf("   iDrawAreaLeftX=%d  DrawAreaRightX=%d\n", DrawDesc.iDrawAreaLeftX, DrawDesc.iDrawAreaRightX);
#endif
            pChannel->pEditorPlugin->TypeSpecificInfo.EditorPluginInfo.fnDraw(&DrawDesc);

#ifdef DEBUG_BUILD
//            printf("[Worker-1] : fnDraw returned.\n"); fflush(stdout);
#endif

            // Check if this place is in a selected region, then
            // change it to be selected!
            DRAWSELECTEDREGION(hps, RequiredImageBufferSize, RequiredFirstSamplePos,
                               RequiredFirstSamplePos, RequiredLastSamplePos,
                               0, RequiredImageBufferSize.cx-1);

	    // If the current position marker is in this area, then draw it!
            if ((lCurrentPosition>=0) &&
                (lCurrentPosition<=RequiredImageBufferSize.cx-1))
              DRAWCURRENTPOSITIONMARKER(hps, lCurrentPosition, RequiredImageBufferSize.cy);
#ifdef DEBUG_BUILD
//            printf("[Worker-1] : DrawPosMarker done\n"); fflush(stdout);
#endif

          }
  
          // Now that the plugin has redrawn the stuff, it's time to set it current!
          if (DosRequestMutexSem(pChannel->hmtxUseImageBuffer, 1000)==NO_ERROR)
          {
            // In theory, only this thread creates image buffers, so
            // it cannot happen that a pChannel->hpsCurrentImageBuffer is not NULL here,
            // so noone will create it, while we're drawing the new. But to make sure,
            // we check it, and delete the old one, if it exists!
            if (pChannel->hpsCurrentImageBuffer)
            {
              hdc = GpiQueryDevice(pChannel->hpsCurrentImageBuffer);
              hbmp = GpiSetBitmap(pChannel->hpsCurrentImageBuffer, NULLHANDLE);
              if (hbmp)
                GpiDeleteBitmap(hbmp);
              GpiDestroyPS(pChannel->hpsCurrentImageBuffer);
              DevCloseDC(hdc);
              pChannel->hpsCurrentImageBuffer = NULLHANDLE;
            }
            // And set new image, and its parameters!
            pChannel->hpsCurrentImageBuffer = hps;
            pChannel->FirstSamplePos = RequiredFirstSamplePos;
            pChannel->LastSamplePos = RequiredLastSamplePos;
            // Also tell the window to repaint itself!
            WinInvalidateRect(pChannel->hwndEditAreaWindow, NULL, FALSE);
            DosReleaseMutexSem(pChannel->hmtxUseImageBuffer);
          }
        }
        else
        {
#ifdef DEBUG_BUILD
          printf("[Worker-1] : RedrawChannel : Could not create HDC!\n");
#endif
          DosReleaseMutexSem(pChannel->hmtxUseImageBuffer);
        }

        // Tell the main window that we've finished working...
        iWorker1State = WORKER1STATE_IDLE;
        WinPostMsg(hwndClient, WM_UPDATEWINDOWTITLE, NULL, NULL);

      } else
      {
#ifdef DEBUG_BUILD
//        printf("[Worker-1] : RedrawChannel : No work is needed!\n");
#endif
        DosReleaseMutexSem(pChannel->hmtxUseImageBuffer);
      }
    }
  }

  // Notify others that this channel-set is not used anymore (by us)!
  pChannel->pParentChannelSet->lUsageCounter--;
#ifdef DEBUG_BUILD
//  printf("[Worker-1] : Leaving redraw channel %d\n", pChannel->iChannelID);
#endif

}

void SetChannelCurrentPosition(WaWEChannel_p pChannel)
{
  SIZEL CurrentImageBufferSize;
  WaWEEdP_Draw_Desc_t DrawDesc;
  RECTL rcl;
  WaWEPosition_t RequiredCurrentPosition;
  long lCurrentPosition;

  if (!pChannel) return;

#ifdef DEBUG_BUILD
//  printf("[Worker-1] : SetChannelCurrentPosition for ch%d!\n", pChannel->iChannelID);
#endif

  // Let's see if we have to do something or not!
  if (DosRequestMutexSem(pChannel->hmtxUseRequiredFields, 1000)==NO_ERROR)
  {
    RequiredCurrentPosition = pChannel->RequiredCurrentPosition;
    DosReleaseMutexSem(pChannel->hmtxUseRequiredFields);
  } else
    RequiredCurrentPosition = pChannel->CurrentPosition;

#ifdef DEBUG_BUILD
//  printf("[Worker-1] : SetChannelCurrentPosition() : Curr: %I64d  Req: %I64d\n",
//         pChannel->CurrentPosition,
//         RequiredCurrentPosition);
#endif

  if (RequiredCurrentPosition == pChannel->CurrentPosition) return;

  // Notify others that this channel-set is being used!
  pChannel->pParentChannelSet->lUsageCounter++;

  if (DosRequestMutexSem(pChannel->hmtxUseImageBuffer, 1000)==NO_ERROR)
  {
    // Get how big the current image is!
    if (pChannel->hpsCurrentImageBuffer)
    {
      GpiQueryPS(pChannel->hpsCurrentImageBuffer, &CurrentImageBufferSize);

      if ((pChannel->CurrentPosition>=pChannel->FirstSamplePos) &&
          (pChannel->CurrentPosition<=pChannel->LastSamplePos))
      {
        // Redraw the window under the old current-position!
        lCurrentPosition =
          (pChannel->CurrentPosition - pChannel->FirstSamplePos) *
          (pChannel->pParentChannelSet->ZoomCount) / (pChannel->pParentChannelSet->ZoomDenom);

#ifdef DEBUG_BUILD
//        printf("[Worker-1] : SetChannelCurrentPosition() : Erase from %d\n",
//               lCurrentPosition);
#endif

        DrawDesc.hpsImageBuffer = pChannel->hpsCurrentImageBuffer;
        DrawDesc.iDrawAreaLeftX = lCurrentPosition;
        DrawDesc.iDrawAreaRightX = lCurrentPosition;
        DrawDesc.pChannelDesc = pChannel;
        DrawDesc.FirstSamplePos = pChannel->FirstSamplePos;
        DrawDesc.LastSamplePos = pChannel->LastSamplePos;
        if (pChannel->pEditorPlugin)
	  pChannel->pEditorPlugin->TypeSpecificInfo.EditorPluginInfo.fnDraw(&DrawDesc);


	// Check if this place is a selected region, then
        // change it to be selected!
        DRAWSELECTEDREGION(pChannel->hpsCurrentImageBuffer, CurrentImageBufferSize,
                           pChannel->FirstSamplePos,
                           pChannel->CurrentPosition, pChannel->CurrentPosition,
                           lCurrentPosition, lCurrentPosition);

        // Invalidate the window where we've changed it
        // plus one pixel to the left and one to the right.
        rcl.xLeft = lCurrentPosition     -1;
        rcl.xRight = lCurrentPosition+1  +1;
        rcl.yBottom = 0;
        rcl.yTop = CurrentImageBufferSize.cy;
        WinInvalidateRect(pChannel->hwndEditAreaWindow, &rcl, FALSE);
      }

      // Now draw the new position marker!
      pChannel->CurrentPosition = RequiredCurrentPosition;

      if ((pChannel->CurrentPosition>=pChannel->FirstSamplePos) &&
          (pChannel->CurrentPosition<=pChannel->LastSamplePos))
      {
        lCurrentPosition =
          (pChannel->CurrentPosition - pChannel->FirstSamplePos) *
          (pChannel->pParentChannelSet->ZoomCount) / (pChannel->pParentChannelSet->ZoomDenom);

#ifdef DEBUG_BUILD
//        printf("[Worker-1] : SetChannelCurrentPosition() : Redraw at %d\n",
//               lCurrentPosition);
#endif

        DRAWCURRENTPOSITIONMARKER(pChannel->hpsCurrentImageBuffer, lCurrentPosition, CurrentImageBufferSize.cy);

        // Invalidate the window where we've changed it,
        // plus one pixel to the left and one to the right.
        rcl.xLeft = lCurrentPosition    -1;
        rcl.xRight = lCurrentPosition+1 +1;
        rcl.yBottom = 0;
        rcl.yTop = CurrentImageBufferSize.cy;
        WinInvalidateRect(pChannel->hwndEditAreaWindow, &rcl, FALSE);
      }
    }
    DosReleaseMutexSem(pChannel->hmtxUseImageBuffer);
  }

  // And update the string there!
  UpdateChannelPositionString(pChannel);

  // Notify others that this channel-set is not used anymore (by us)!
  pChannel->pParentChannelSet->lUsageCounter--;
}

// UpdateShownSelection is called when there is a change in selection,
// so the ShownSelectedRegionList should be changed to RequiredSelectedRegionList
void UpdateShownSelection(WaWEChannel_p pChannel)
{
  WaWEChannelSet_p pChannelSet;
  int iDifferent;
  int iSelectedRegions;
  WaWESize_t SelectedSize;
  SIZEL CurrentImageBufferSize;
  WaWEPosition_t FirstSamplePos, LastSamplePos, CurrentPosition;
  WaWESize_t ZoomCount, ZoomDenom;

  if (!pChannel) return;
#ifdef DEBUG_BUILD
//  printf("[Worker-1] : Updating shown selection for channel %d\n", pChannel->iChannelID);
#endif

  // Notify others that this channel-set is being used!
  pChannel->pParentChannelSet->lUsageCounter++;

  pChannelSet = pChannel->pParentChannelSet;

  // Let's see if the two selected region list is different or not!
  iDifferent = 0;

  // Get mutex of RequiredShown and Shown
  if (DosRequestMutexSem(pChannel->hmtxUseRequiredShownSelectedRegionList, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    if (DosRequestMutexSem(pChannel->hmtxUseShownSelectedRegionList, SEM_INDEFINITE_WAIT)==NO_ERROR)
    {
      // Check if RequiredShown is different from Shown region list
      // We assume that the region lists are already optimized,
      // so we only have to check if it has the same number of
      // regions, and if the regions are the same.
      WaWESelectedRegion_p pShown, pRequired, pPrev;
      long lCurrentPosition;

      // Get status of channel/channelset!
      CurrentPosition = pChannel->CurrentPosition;
      FirstSamplePos = pChannel->FirstSamplePos;
      LastSamplePos = pChannel->LastSamplePos;
      ZoomCount = pChannel->pParentChannelSet->ZoomCount;
      ZoomDenom = pChannel->pParentChannelSet->ZoomDenom;

      lCurrentPosition =
        (CurrentPosition - FirstSamplePos) *
        ZoomCount / ZoomDenom;

      pShown = pChannel->pShownSelectedRegionListHead;
      pRequired = pChannel->pRequiredShownSelectedRegionListHead;

      while ((pShown) && (pRequired))
      {
        // Break the loop if we find a different one
        if ((pShown->Start!=pRequired->Start) ||
            (pShown->End!=pRequired->End)) break;

        pShown = pShown->pNext;
        pRequired = pRequired->pNext;
      }
      if ((pShown) || (pRequired)) iDifferent = 1;

      // If they are different, then we have to redraw the new
      // selection. We do it by redrawing the ShownSelection,
      // which will result in deleting it (because of XOR drawing method)
      if (iDifferent)
      {
        if (DosRequestMutexSem(pChannel->hmtxUseImageBuffer, SEM_INDEFINITE_WAIT) == NO_ERROR)
        {
          // Only work on image buffer if we have one. :)
          if (pChannel->hpsCurrentImageBuffer)
          {
            GpiQueryPS(pChannel->hpsCurrentImageBuffer, &CurrentImageBufferSize);

            // Delete old selection from screen
            DRAWSELECTEDREGION(pChannel->hpsCurrentImageBuffer, CurrentImageBufferSize,
                               FirstSamplePos,
                               FirstSamplePos, LastSamplePos,
                               0, CurrentImageBufferSize.cx-1);

            // Erase Shown list
            while (pChannel->pShownSelectedRegionListHead)
            {
              pShown = pChannel->pShownSelectedRegionListHead;
      
              pChannel->pShownSelectedRegionListHead =
                pChannel->pShownSelectedRegionListHead->pNext;
      
              dbg_free(pShown);
            }
      
            // Copy the required list to the Shown list!
            pRequired = pChannel->pRequiredShownSelectedRegionListHead;
            pPrev = NULL;
            while (pRequired)
            {
              pShown = (WaWESelectedRegion_p) dbg_malloc(sizeof(WaWESelectedRegion_t));
              if (pShown)
              {
                pShown->Start = pRequired->Start;
                pShown->End = pRequired->End;
                pShown->pNext = NULL;
                pShown->pPrev = pPrev;
                if (pPrev)
                  pPrev->pNext = pShown;
                else
                  pChannel->pShownSelectedRegionListHead = pShown;

                pPrev = pShown;
              }
              pRequired = pRequired->pNext;
            }

            // Draw new selection to screen
            DRAWSELECTEDREGION(pChannel->hpsCurrentImageBuffer, CurrentImageBufferSize,
                               FirstSamplePos,
                               FirstSamplePos, LastSamplePos,
                               0, CurrentImageBufferSize.cx-1);
            // Restore current position marker!
            DRAWCURRENTPOSITIONMARKER(pChannel->hpsCurrentImageBuffer, lCurrentPosition, CurrentImageBufferSize.cy);
          }
          DosReleaseMutexSem(pChannel->hmtxUseImageBuffer);
        }
        // Create new statistics of selected msecs and chunks
        iSelectedRegions = 0;
        SelectedSize = 0;
        pShown = pChannel->pShownSelectedRegionListHead;
        while (pShown)
        {
          iSelectedRegions++;
          SelectedSize+=(pShown->End)-(pShown->Start)+1;
          pShown = pShown->pNext;
        }
        pChannel->ShownSelectedLength = SelectedSize;
        pChannel->iShownSelectedPieces = iSelectedRegions;
      }
      // Release mutex of Shown
      DosReleaseMutexSem(pChannel->hmtxUseShownSelectedRegionList);
    }
    // Release mutex of RequiredShown
    DosReleaseMutexSem(pChannel->hmtxUseRequiredShownSelectedRegionList);
  }

  if (iDifferent)
  {
    // Make sure our changes will be shown
    WinInvalidateRegion(pChannel->hwndEditAreaWindow, NULLHANDLE, FALSE);
    // And update the strings too
    UpdateChannelSelectionString(pChannel);
  }
  // Notify others that this channel-set is not used anymore (by us)!
  pChannel->pParentChannelSet->lUsageCounter--;
}

void UpdatePositionAndSizeStrings()
{
  WaWEChannelSet_p pTempChannelSet;
  WaWEChannel_p pTempChannel;

#ifdef DEBUG_BUILD
  printf("[Worker-1] : UpdatePositionAndSizeStrings() Enter\n");
#endif
  // Get the ChannelSetListProtector_Sem!
  if (DosRequestMutexSem(ChannelSetListProtector_Sem, SEM_INDEFINITE_WAIT)!=NO_ERROR)
  {
#ifdef DEBUG_BUILD
    printf("[Worker-1] : UpdatePositionAndSizeStrings() : Could not get mutex semaphore!\n"); fflush(stdout);
#endif
    ErrorMsg("Internal error [Worker-1, UpdatePositionAndSizeStrings(), @1]");
    return;
  }

  pTempChannelSet = ChannelSetListHead;
  while (pTempChannelSet)
  {
    UpdateChannelSetLengthString(pTempChannelSet);
    UpdateChannelSetPointerPositionString(pTempChannelSet);

    pTempChannel = pTempChannelSet->pChannelListHead;
    while (pTempChannel)
    {
      UpdateChannelPositionString(pTempChannel);
      UpdateChannelSelectionString(pTempChannel);
      pTempChannel = pTempChannel->pNext;
    }

    pTempChannelSet = pTempChannelSet->pNext;
  }
  DosReleaseMutexSem(ChannelSetListProtector_Sem);

#ifdef DEBUG_BUILD
  printf("[Worker-1] : UpdatePositionAndSizeStrings() Leave\n");
#endif
}

void Worker1ThreadFunc(void *pParm)
{
  HAB hab;
  HMQ hmq;
  int rc;
  REQUESTDATA Request;
  int   iEventCode;
  ULONG ulEventParmSize;
  void *pEventParm;
  BYTE  ElemPrty;

  // Setup this thread for using PM (sending messages etc...)
  hab = WinInitialize(0);
  hmq = WinCreateMsgQueue(hab, 0);

  do{
    iWorker1State = WORKER1STATE_IDLE;
    iEventCode = WORKER1_MSG_SHUTDOWN; // Set as default!
    rc = DosReadQueue(hqWorker1MessageQueue,
                      &Request,
                      &ulEventParmSize,
                      &pEventParm,
                      0,
                      DCWW_WAIT,
                      &ElemPrty,
                      0);
    if (rc == NO_ERROR)
    {
      iEventCode = Request.ulData;
      // Processing of event codes comes here
      // These should free memory pointers passed *inside* pEventParm!
      switch(iEventCode)
      {
        case WORKER1_MSG_SHUTDOWN:
            {
#ifdef DEBUG_BUILD
              printf("[Worker-1] : Shutting down\n");
#endif
              // We don't have to do anything here.
              break;
            }
	case WORKER1_MSG_CHANNELDRAW:
	  {
            WaWEWorker1Msg_ChannelDraw_p pChannelDrawMsgParm = pEventParm;
            if (pChannelDrawMsgParm)
            {
              RedrawChannel(pChannelDrawMsgParm->pChannel, pChannelDrawMsgParm->bDontOptimize);
              // Then free the message!
              dbg_free(pChannelDrawMsgParm); pEventParm = NULL;
            }
            break;
          }
        case WORKER1_MSG_SETCHANNELCURRENTPOSITION:
          {
            WaWEWorker1Msg_SetChannelCurrentPosition_p pSCCPMsgParm = pEventParm;
            if (pSCCPMsgParm)
            {
//              iWorker1State = WORKER1STATE_WORKINGOTHER;
//              WinPostMsg(hwndClient, WM_UPDATEWINDOWTITLE, NULL, NULL);

              SetChannelCurrentPosition(pSCCPMsgParm->pChannel);

//              iWorker1State = WORKER1STATE_IDLE;
//              WinPostMsg(hwndClient, WM_UPDATEWINDOWTITLE, NULL, NULL);

              dbg_free(pSCCPMsgParm); pEventParm = NULL;
            }
            break;
	  }
	case WORKER1_MSG_UPDATESHOWNSELECTION:
	  {
            WaWEWorker1Msg_UpdateShownSelection_p pUSSMsgParm = pEventParm;
            if (pUSSMsgParm)
            {
//              iWorker1State = WORKER1STATE_WORKINGOTHER;
//              WinPostMsg(hwndClient, WM_UPDATEWINDOWTITLE, NULL, NULL);
              UpdateShownSelection(pUSSMsgParm->pChannel);
//              iWorker1State = WORKER1STATE_IDLE;
//              WinPostMsg(hwndClient, WM_UPDATEWINDOWTITLE, NULL, NULL);
              dbg_free(pUSSMsgParm); pEventParm = NULL;
	    }
            break;
          }
        case WORKER1_MSG_UPDATEPOSITIONANDSIZESTRINGS:
          UpdatePositionAndSizeStrings();
          break;
        default:
#ifdef DEBUG_BUILD
	  printf("[Worker-1] : Unknown message: %d\n", iEventCode);
#endif
	  break;
      }

      // End of message processing
      // Free event parameter, if it's not free'd yet!
      if (pEventParm)
        dbg_free(pEventParm);
    }
#ifdef DEBUG_BUILD
    else
      printf("[Worker-1] : DosReadQueue error, rc=%d\n", rc);
#endif
  } while ((rc==NO_ERROR) && (iEventCode!=WORKER1_MSG_SHUTDOWN));

  // Uninitialize PM
  iWorker1State = WORKER1STATE_SHUTTEDDOWN;
  WinDestroyMsgQueue(hmq);
  WinTerminate(hab);
  _endthread();
}

int Initialize_Worker1()
{
  char achQueueName[100];
  int rc;
  PPIB pib;
  PTIB tib;

  iWorker1State = WORKER1STATE_IDLE;
  DosGetInfoBlocks(&tib, &pib);
  sprintf(achQueueName, "\\QUEUES\\WaWE\PID%d\\Worker1MsgQ", pib->pib_ulpid);

#ifdef DEBUG_BUILD
//  printf("Creating mq for worker-1 thread: %s\n", achQueueName);
#endif

  rc = DosCreateQueue(&hqWorker1MessageQueue,
                      QUE_FIFO | QUE_CONVERT_ADDRESS,
                      achQueueName);
  if (rc!=NO_ERROR)
  {
#ifdef DEBUG_BUILD
    printf("Error: Could not create mq for worker-1 thread!\n");
#endif
    return 0;
  }

  // Start worker thread!
  tidWorker1 = _beginthread(Worker1ThreadFunc, 0, 1024*1024, NULL);
  // Set priority of Worker thread to low, so it won't hang PM updating...
  DosSetPriority(PRTYS_THREAD, PRTYC_IDLETIME, +30, tidWorker1);

#ifdef DEBUG_BUILD
  printf("Worker-1 thread created, tid=%d\n", tidWorker1);
#endif

  return 1;
}

int AddNewWorker1Msg(int iEventCode, int iEventParmSize, void *pEventParm)
{
  int rc;
#ifdef DEBUG_BUILD
//  printf("[AddNewWorker1Msg] : EventCode = %d\n", iEventCode);
#endif

  rc = DosWriteQueue(hqWorker1MessageQueue,
                     iEventCode,
                     iEventParmSize,
                     pEventParm,
                     0L);
  return (rc == NO_ERROR);
}

void Uninitialize_Worker1()
{
#ifdef DEBUG_BUILD
  if ((iWorker1State!=WORKER1STATE_SHUTTEDDOWN) &&
      (iWorker1State!=WORKER1STATE_IDLE))
  {
    printf("[Uninitialize_Worker1] : The worker-1 thread is busy, it might take a while for it to finish its job and get the Shutdown request!\n");
  }
#endif

  // Add new message of shutting down
  // Make sure it gets it!
  while (!AddNewWorker1Msg(WORKER1_MSG_SHUTDOWN, 0, NULL)) DosSleep(64);
  // Wait for it to die
  DosWaitThread(&tidWorker1, DCWW_WAIT);
  // Then close queue
  DosCloseQueue(hqWorker1MessageQueue);
}
