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
 * Playback processor plugin                                                 *
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <process.h>
#include <string.h>
#include <limits.h>

#define INCL_TYPES
#define INCL_WIN
#define INCL_GPI
#define INCL_DOSRESOURCES
#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>

#include "WaWECommon.h"         // Common defines and structures
#include "PPlayConfig.h"        // Functions about configuration of plugin
#include "PPlayPlayback.h"      // Functions about playing back samples
#include "PPlay-Resources.h"

typedef struct _PlaybackDesc_struct
{
  int bOnlySelectedArea; // TRUE, if only the selected area(s) of the channels have
                         // to be played back.
                         // FALSE, if the whole channel(set) has to be played back!

  int               iNumOfChannels; // Number of channel-pointers in pChannels
  WaWEChannel_p    *pChannels;      // Array of channel-pointers to be played back

  WaWEPosition_t   *pChannelPos;    // Used in bSlowMachineMode (see configuration)

} PlaybackDesc_t, *PlaybackDesc_p;

// Global variables
// (Per process, because the DLL is initinstance/terminstance!)
HMODULE hmodOurDLLHandle;
WaWEPluginHelpers_t PluginHelpers;

char *pchPluginName = "~Playback plugin";
char *pchAboutTitle = "About Playback plugin";
char *pchAboutMessage =
  "Playback plugin v1.0 for WaWE\n"
  "Written by Doodle\n"
  "\n"
  "Currently supported outputs:\n"
  "    - DART\n"
  "\n"
  "Compilation date: "__DATE__" "__TIME__;



// Helper functions, called from inside:

void ErrorMsg(char *Msg)
{
#ifdef DEBUG_BUILD
  printf("ErrorMsg: %s\n",Msg); fflush(stdout);
#endif
  WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                Msg, "Playback plugin error", 0, MB_OK | MB_MOVEABLE | MB_ICONEXCLAMATION);
}

void WarningMsg(char *Msg)
{
#ifdef DEBUG_BUILD
  printf("WarningMsg: %s\n",Msg); fflush(stdout);
#endif
  WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                Msg, "Playback plugin warning", 0, MB_OK | MB_MOVEABLE | MB_ICONEXCLAMATION);
}

// Functions called from outside:


static int NumOfChannelsWithSelectedAreasInChannelSet(WaWEState_p pWaWEState, WaWEChannelSet_p pChannelSet)
{
  int iResult = 0;
  WaWEChannel_p pChannel;

  if ((!pWaWEState) || (!pChannelSet)) return 0;

  if (DosRequestMutexSem(pChannelSet->hmtxUseChannelList, SEM_INDEFINITE_WAIT)!=NO_ERROR)
    return 0;

  pChannel = pChannelSet->pChannelListHead;
  while (pChannel)
  {
    if (pChannel->pLastValidSelectedRegionListHead)
      iResult++;
    pChannel = pChannel->pNext;
  }

  DosReleaseMutexSem(pChannelSet->hmtxUseChannelList);

  return iResult;
}


#define ALLOCATE_MEMORY_FOR_NEW_MENUENTRY()                                                                                                   \
          (*piNumOfPopupMenuEntries)++;                                                                                                   \
          if ((*piNumOfPopupMenuEntries)==1)                                                                                              \
          {                                                                                                                               \
            /* First entry, so allocate memory */                                                                                         \
            pEntries = (WaWEPopupMenuEntries_p) malloc((*piNumOfPopupMenuEntries) * sizeof(WaWEPopupMenuEntries_t));                      \
            if (!pEntries)                                                                                                                \
            {                                                                                                                             \
              *ppPopupMenuEntries = pEntries;                                                                                             \
              *piNumOfPopupMenuEntries = 0;                                                                                               \
              return 0;                                                                                                                   \
            }                                                                                                                             \
            *ppPopupMenuEntries = pEntries;                                                                                               \
          } else                                                                                                                          \
          {                                                                                                                               \
            /* Not the first entry, so reallocate memory */                                                                               \
            pEntries = (WaWEPopupMenuEntries_p) realloc(*ppPopupMenuEntries, (*piNumOfPopupMenuEntries) * sizeof(WaWEPopupMenuEntries_t));\
            if (!pEntries)                                                                                                                \
            {                                                                                                                             \
              free(*ppPopupMenuEntries); *ppPopupMenuEntries = NULL;                                                                      \
              *piNumOfPopupMenuEntries = 0;                                                                                               \
              return 0;                                                                                                                   \
            }                                                                                                                             \
            *ppPopupMenuEntries = pEntries;                                                                                               \
                                                                                                                                          \
          }


int  WAWECALL plugin_GetPopupMenuEntries(int iPopupMenuID,
                                         WaWEState_p pWaWEState,
                                         int *piNumOfPopupMenuEntries,
                                         WaWEPopupMenuEntries_p *ppPopupMenuEntries)
{
#ifdef DEBUG_BUILD
//  printf("[GetPopupMenuEntries] : PPlay : Enter, id = %d\n", iPopupMenuID); fflush(stdout);
#endif
  // Check parameters!
  if ((!pWaWEState) || (!piNumOfPopupMenuEntries) ||
      (!ppPopupMenuEntries))
  {
#ifdef DEBUG_BUILD
    printf("[GetPopupMenuEntries] : PPlay : Wrong parameter(s)!\n"); fflush(stdout);
#endif
    return 0;
  }

  // We cannot do anything if there is no current channel or channelset!
  switch (iPopupMenuID)
  {
    case WAWE_POPUPMENU_ID_ROOT:
      {
        // Ok, WaWE asks what menu items we want into first popup menu!
        if (bIsPlaying)
        {
          (*piNumOfPopupMenuEntries) = 1;
  
          *ppPopupMenuEntries = (WaWEPopupMenuEntries_p) malloc((*piNumOfPopupMenuEntries) * sizeof(WaWEPopupMenuEntries_t));
          if (!(*ppPopupMenuEntries))
          {
#ifdef DEBUG_BUILD
            printf("[GetPopupMenuEntries] : Warning, out of memory!\n"); fflush(stdout);
#endif
            return 0;
          }
          // We're currently playing back something, so we can offer the user the stop of the playback.
          (*ppPopupMenuEntries)[0].pchEntryText = "~Stop playback";
          (*ppPopupMenuEntries)[0].iEntryID     = POPUPMENU_ID_STOP_PLAYBACK;
          (*ppPopupMenuEntries)[0].iEntryStyle  = WAWE_POPUPMENU_STYLE_MENUITEM;
        } else
        {
          // We're currently not playing back anything, so we can offer the user
          // to play back something, if it has clicked on a channel or channel-set!
          if ((pWaWEState->pCurrentChannel) ||
              (pWaWEState->pCurrentChannelSet))
          {
            (*piNumOfPopupMenuEntries) = 1;
  
            *ppPopupMenuEntries = (WaWEPopupMenuEntries_p) malloc((*piNumOfPopupMenuEntries) * sizeof(WaWEPopupMenuEntries_t));
            if (!(*ppPopupMenuEntries))
            {
#ifdef DEBUG_BUILD
              printf("[GetPopupMenuEntries] : Warning, out of memory!\n"); fflush(stdout);
#endif
              return 0;
            }

            (*ppPopupMenuEntries)[0].pchEntryText = "~Play";
            (*ppPopupMenuEntries)[0].iEntryID     = POPUPMENU_ID_PLAY;
            (*ppPopupMenuEntries)[0].iEntryStyle  = WAWE_POPUPMENU_STYLE_SUBMENU;
          }
        }
        break;
      }
    case POPUPMENU_ID_PLAY:
      {
        WaWEPopupMenuEntries_p pEntries;

        // Ok, WaWE asks what menu items we want into our 'Play' menu!
        // Let's see!

        *piNumOfPopupMenuEntries = 0;

        if (pWaWEState->pCurrentChannelSet)
        {
          ALLOCATE_MEMORY_FOR_NEW_MENUENTRY();
          // Ok, memory is available, now fill entries!
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].pchEntryText = "Channel-~Set";
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryID     = POPUPMENU_ID_PLAY_CHANNELSET;
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryStyle  = WAWE_POPUPMENU_STYLE_SUBMENU;
        }
        if (pWaWEState->pCurrentChannel)
        {
          ALLOCATE_MEMORY_FOR_NEW_MENUENTRY();
          // Ok, memory is available, now fill entries!
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].pchEntryText = "~Channel";
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryID     = POPUPMENU_ID_PLAY_CHANNEL;
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryStyle  = WAWE_POPUPMENU_STYLE_SUBMENU;
        }
        break;
      }
    case POPUPMENU_ID_PLAY_CHANNELSET:
      {
        WaWEPopupMenuEntries_p pEntries;

        // Ok, WaWE asks what menu items we want into our 'Play->Channel-Set' menu!
        // Let's see!

        *piNumOfPopupMenuEntries = 0;


        // If there is a current channelset, and there is a selected area in there, then we
        // offer a menu item of playback of selected area from current channel
        if ((pWaWEState->pCurrentChannelSet) && (NumOfChannelsWithSelectedAreasInChannelSet(pWaWEState, pWaWEState->pCurrentChannelSet)>0))
        {
          ALLOCATE_MEMORY_FOR_NEW_MENUENTRY();
          // Ok, memory is available, now fill entries!
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].pchEntryText = "~Selected area";
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryID     = POPUPMENU_ID_PLAY_CHANNELSET_SELECTEDDATA;
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryStyle  = WAWE_POPUPMENU_STYLE_MENUITEM;
        }
        // If there is a current channelset, then we
	// offer a menu item of playback of the whole current channelset
        // and a menu item of playback from the current position
        if (pWaWEState->pCurrentChannelSet)
        {
          ALLOCATE_MEMORY_FOR_NEW_MENUENTRY();
          // Ok, memory is available, now fill entries!
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].pchEntryText = "~All";
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryID     = POPUPMENU_ID_PLAY_CHANNELSET_ALLDATA;
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryStyle  = WAWE_POPUPMENU_STYLE_MENUITEM;

	  ALLOCATE_MEMORY_FOR_NEW_MENUENTRY();
          // Ok, memory is available, now fill entries!
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].pchEntryText = "~From current position";
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryID     = POPUPMENU_ID_PLAY_CHANNELSET_FROMCURRENTPOS;
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryStyle  = WAWE_POPUPMENU_STYLE_MENUITEM;
	}
	break;
      }
    case POPUPMENU_ID_PLAY_CHANNEL:
      {
        WaWEPopupMenuEntries_p pEntries;

        // Ok, WaWE asks what menu items we want into our 'Play->Channel' menu!
        // Let's see!

        *piNumOfPopupMenuEntries = 0;


	// If there is a current channel, and there is a selected area in there, then we
        // offer a menu item of playback of selected area from current channel
        if ((pWaWEState->pCurrentChannel) && (pWaWEState->pCurrentChannel->pLastValidSelectedRegionListHead))
        {
          ALLOCATE_MEMORY_FOR_NEW_MENUENTRY();
          // Ok, memory is available, now fill entries!
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].pchEntryText = "~Selected area";
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryID     = POPUPMENU_ID_PLAY_CHANNEL_SELECTEDDATA;
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryStyle  = WAWE_POPUPMENU_STYLE_MENUITEM;
        }

        // If there is a current channel, then we
	// offer a menu item of playback of the whole current channel
        // and a menu item of playback from the current position
        if (pWaWEState->pCurrentChannel)
        {
          ALLOCATE_MEMORY_FOR_NEW_MENUENTRY();
          // Ok, memory is available, now fill entries!
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].pchEntryText = "~All";
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryID     = POPUPMENU_ID_PLAY_CHANNEL_ALLDATA;
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryStyle  = WAWE_POPUPMENU_STYLE_MENUITEM;

          ALLOCATE_MEMORY_FOR_NEW_MENUENTRY();
          // Ok, memory is available, now fill entries!
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].pchEntryText = "~From current position";
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryID     = POPUPMENU_ID_PLAY_CHANNEL_FROMCURRENTPOS;
          (*ppPopupMenuEntries)[(*piNumOfPopupMenuEntries)-1].iEntryStyle  = WAWE_POPUPMENU_STYLE_MENUITEM;
	}
        break;
      }
    default:
#ifdef DEBUG_BUILD
      printf("[GetPopupMenuEntries] : Invalid Menu ID!\n"); fflush(stdout);
#endif
      return 0;
  }
#ifdef DEBUG_BUILD
//  printf("[GetPopupMenuEntries] : PPlay : Returning 1\n"); fflush(stdout);
#endif

  return 1;
}

void WAWECALL plugin_FreePopupMenuEntries(int iNumOfPopupMenuEntries,
                                          WaWEPopupMenuEntries_p pPopupMenuEntries)
{
#ifdef DEBUG_BUILD
//  printf("[FreePopupMenuEntries] : Enter\n"); fflush(stdout);
#endif
  if (pPopupMenuEntries)
  {
    // No need to free stuffs inside the descriptors, because all the
    // strings in there were constants.
    free(pPopupMenuEntries);
  }
  return;
}

static int Stop(WaWEState_p pWaWEState, HWND hwndOwner)
{
  if (!StopPlayback())
    WarningMsg("Could not stop playback!");

  return 1;
}

static void *fnOpenStream(void *pUserParm)
{
  // This should open the stream to be played back (the pUserParm should
  // help to identify what stream we're talking about), and return a handle to
  // it.
  // As the pUserParm describes it very well, we simply return that. :)

  return pUserParm;
}

static WaWESize_t fnReadStream(void *hStream,
                               int   iChannel,
                               WaWESize_t ulNumOfSamples,
                               WaWESample_p pSampleBuffer)
{
  PlaybackDesc_p pPlaybackDesc;
  WaWEChannel_p pChannel;
  WaWESize_t ToRead, CouldRead;
  WaWEPosition_t CurrPos;
  WaWESize_t Len;

  if (!hStream) return 0;
  pPlaybackDesc = hStream;

  if (iChannel>=pPlaybackDesc->iNumOfChannels) return 0;

  pChannel = pPlaybackDesc->pChannels[iChannel];

  ToRead = ulNumOfSamples;
  Len = pChannel->Length;
  CouldRead = 0;

  if (PluginConfiguration.bSlowMachineMode)
    CurrPos = pPlaybackDesc->pChannelPos[iChannel];
  else
  {
    CurrPos = pChannel->RequiredCurrentPosition;
  }

  if ((pPlaybackDesc->bOnlySelectedArea) &&
      (DosRequestMutexSem(pChannel->hmtxUseShownSelectedRegionList, SEM_INDEFINITE_WAIT)==NO_ERROR))
  {
    // We have to play the selected area of the channel, so
    // we have to go through the selected area of channel!

    // Go through the selected region areas, and find the first
    // one which contains the current position.
    // If we find that, start to read ToRead bytes in selected areas!

    WaWESelectedRegion_p pRegion;
    WaWESize_t ToReadFromRegion, CouldReadFromRegion;
    WaWEPosition_t ReadFrom, LastPositionReached;

    pRegion = pChannel->pShownSelectedRegionListHead;
    while ((pRegion) &&
	   ((pRegion->Start>CurrPos) || (pRegion->End<CurrPos))
	  )
      pRegion = pRegion->pNext;

    LastPositionReached = CurrPos;
    while ((pRegion) && (ToRead>0))
    {
      if ((pRegion->Start<=CurrPos) && (pRegion->End>=CurrPos))
      {
        // We should read only part of this region
	ToReadFromRegion = pRegion->End - CurrPos + 1;
        ReadFrom = CurrPos;
      } else
      {
	// We should read almost all of this region
	ToReadFromRegion = pRegion->End - pRegion->Start + 1;
        ReadFrom = pRegion->Start;
      }

      if (ToReadFromRegion>ToRead)
      {
	ToReadFromRegion = ToRead;
      }

      PluginHelpers.fn_WaWEHelper_Channel_ReadSample(pChannel,
						     ReadFrom,
						     ToReadFromRegion,
						     pSampleBuffer,
						     &CouldReadFromRegion);

      LastPositionReached = ReadFrom + CouldReadFromRegion;

      if (!CouldReadFromRegion) // Error reading!
        break;

      pSampleBuffer += CouldReadFromRegion;
      CouldRead += CouldReadFromRegion;
      ToRead -= CouldReadFromRegion;

      pRegion = pRegion->pNext;
    }

    pPlaybackDesc->pChannelPos[iChannel] = LastPositionReached;
    PluginHelpers.fn_WaWEHelper_Channel_SetCurrentPosition(pChannel,
							   LastPositionReached);

    DosReleaseMutexSem(pChannel->hmtxUseShownSelectedRegionList);
  } else
  {
    // We have to play the whole channel, so simply read until we
    // find end of channel!

    if ((Len - CurrPos) < ToRead)
      ToRead = Len - CurrPos;
  
    if (ToRead>0)
    {
      PluginHelpers.fn_WaWEHelper_Channel_ReadSample(pChannel,
						     CurrPos,
						     ToRead,
						     pSampleBuffer,
						     &CouldRead);
  
      CurrPos += CouldRead;
  
      pPlaybackDesc->pChannelPos[iChannel] = CurrPos;
      PluginHelpers.fn_WaWEHelper_Channel_SetCurrentPosition(pChannel,
							     CurrPos);
  
#ifdef DEBUG_BUILD
      //printf("[fnReadStream] : Ch %d (0x%p), Pos is %d\n", iChannel, pChannel, (int) CurrPos);
#endif
    }
  }
  return CouldRead;
}

static void fnCloseStream(void *hStream)
{
  WaWEChannelSet_p pChannelSet;
  PlaybackDesc_p pPlaybackDesc;

  // This has to do the cleanup after finishing the playback.
  if (!hStream) return;

#ifdef DEBUG_BUILD
//  printf("[fnCloseStream] : Enter\n");
#endif

  pPlaybackDesc = hStream;

#ifdef DEBUG_BUILD
//  printf("[fnCloseStream] : @1\n");
#endif
  pChannelSet = pPlaybackDesc->pChannels[0]->pParentChannelSet;
#ifdef DEBUG_BUILD
//  printf("[fnCloseStream] : @2\n");
#endif

  pChannelSet->lUsageCounter--; // Not using the channel-set anymore!

#ifdef DEBUG_BUILD
//  printf("[fnCloseStream] : @3\n");
#endif

  free(pPlaybackDesc->pChannelPos);
#ifdef DEBUG_BUILD
//  printf("[fnCloseStream] : @4\n");
#endif
  free(pPlaybackDesc->pChannels);
#ifdef DEBUG_BUILD
//  printf("[fnCloseStream] : @5\n");
#endif

  free(pPlaybackDesc);
#ifdef DEBUG_BUILD
//  printf("[fnCloseStream] : Leave\n");
#endif
}

static int Play_WholeChannelSet(WaWEState_p pWaWEState, HWND hwndOwner, int bPlayFromBeginning)
{
  PlaybackDesc_p pPlaybackDesc;
  WaWEChannel_p pChannel;
  int i;

  if (!pWaWEState) return 0;
  if (!pWaWEState->pCurrentChannelSet) return 0;

  if (bIsPlaying)
  {
    WarningMsg("Already playing, please stop the playback first!");
    return 0;
  }

  pPlaybackDesc = (PlaybackDesc_p) malloc(sizeof(PlaybackDesc_t));
  if (!pPlaybackDesc)
  {
    ErrorMsg("Out of memory!\nOperation failed!");
    return 0;
  }

  if (DosRequestMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList, SEM_INDEFINITE_WAIT)!=NO_ERROR)
  {
    free(pPlaybackDesc);
    ErrorMsg("Internal error!\nOperation failed!");
    return 0;
  }

  // Note that we'll use this channelset, so nobody should delete or modify it!
  pWaWEState->pCurrentChannelSet->lUsageCounter++;

  // Store that we'll use the whole channel(set), not only the selected areas!
  pPlaybackDesc->bOnlySelectedArea = FALSE;

  // Get the number of channels inside
  pPlaybackDesc->iNumOfChannels = 0;

  pChannel = pWaWEState->pCurrentChannelSet->pChannelListHead;
  while (pChannel)
  {
    pPlaybackDesc->iNumOfChannels++;
    pChannel = pChannel->pNext;
  }

  // Check for empty channel-set!
  if (pPlaybackDesc->iNumOfChannels==0)
  {
    DosReleaseMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList);
    free(pPlaybackDesc);
    pWaWEState->pCurrentChannelSet->lUsageCounter--; // Not using it anymore!
    WarningMsg("Empty channel-sets cannot be played back!");
    return 0;
  }

  // Now collect the channel-set channels to be played back!
  pPlaybackDesc->pChannels = (WaWEChannel_p *) malloc(sizeof(WaWEChannel_p) * pPlaybackDesc->iNumOfChannels);

  if (!pPlaybackDesc->pChannels)
  {
    DosReleaseMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList);
    // Could not allocate memory for channel array!
    free(pPlaybackDesc);
    pWaWEState->pCurrentChannelSet->lUsageCounter--; // Not using it anymore!
    ErrorMsg("Out of memory!\nOperation failed!");
    return 0;
  }

  pPlaybackDesc->pChannelPos = (WaWEPosition_p) malloc(sizeof(WaWEPosition_t) * pPlaybackDesc->iNumOfChannels);

  if (!pPlaybackDesc->pChannelPos)
  {
    DosReleaseMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList);
    // Could not allocate memory for channel position array!
    free(pPlaybackDesc->pChannels);
    free(pPlaybackDesc);
    pWaWEState->pCurrentChannelSet->lUsageCounter--; // Not using it anymore!
    ErrorMsg("Out of memory!\nOperation failed!");
    return 0;
  }

  // Fill the channel array with channel pointers,
  // and set the current position of the channels to
  // the beginning, because we'll play back the whole channel!
  // (Or get current position, if we want to play back from curr pos)
  pChannel = pWaWEState->pCurrentChannelSet->pChannelListHead;
  i = 0;
  while (pChannel)
  {
    pPlaybackDesc->pChannels[i] = pChannel;

    if (bPlayFromBeginning)
    {
      pPlaybackDesc->pChannelPos[i] = 0;
      PluginHelpers.fn_WaWEHelper_Channel_SetCurrentPosition(pChannel,
							     0);
    } else
    {
      pPlaybackDesc->pChannelPos[i] = pChannel->RequiredCurrentPosition;
    }

    i++;
    pChannel = pChannel->pNext;
  }

  DosReleaseMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList);

  // Descriptor is prepared, now try to start the playback!
  if (!StartPlayback(pPlaybackDesc,
                     fnOpenStream,
                     fnReadStream,
                     fnCloseStream,
                     pWaWEState->pCurrentChannelSet->OriginalFormat.iFrequency,
                     pPlaybackDesc->iNumOfChannels))
  {
    // Could not start playback!
    free(pPlaybackDesc->pChannelPos);
    free(pPlaybackDesc->pChannels);
    free(pPlaybackDesc);
    pWaWEState->pCurrentChannelSet->lUsageCounter--; // Not using it anymore!
    ErrorMsg("Could not start playback!\n"
             "\n"
             "Make sure that the plugin is correctly configured, and\n"
             "the selected driver supports the playback of this number\n"
             "of channels!");
    return 0;
  }

  return 1;
}

static int Play_WholeChannel(WaWEState_p pWaWEState, HWND hwndOwner, int bPlayFromBeginning)
{
  PlaybackDesc_p pPlaybackDesc;
  WaWEChannelSet_p pChannelSet;

  if (!pWaWEState) return 0;
  if (!pWaWEState->pCurrentChannel) return 0;

  pChannelSet = pWaWEState->pCurrentChannel->pParentChannelSet;

  if (bIsPlaying)
  {
    WarningMsg("Already playing, please stop the playback first!");
    return 0;
  }

  pPlaybackDesc = (PlaybackDesc_p) malloc(sizeof(PlaybackDesc_t));
  if (!pPlaybackDesc)
  {
    ErrorMsg("Out of memory!\nOperation failed!");
    return 0;
  }

  // Note that we'll use this channelset, so nobody should delete or modify it!
  pChannelSet->lUsageCounter++;

  // Store that we'll use the whole channel(set), not only the selected areas!
  pPlaybackDesc->bOnlySelectedArea = FALSE;

  // Get the number of channels inside
  pPlaybackDesc->iNumOfChannels = 1;

  // Now collect the channels to be played back!
  pPlaybackDesc->pChannels = (WaWEChannel_p *) malloc(sizeof(WaWEChannel_p) * pPlaybackDesc->iNumOfChannels);

  if (!pPlaybackDesc->pChannels)
  {
    // Could not allocate memory for channel array!
    free(pPlaybackDesc);
    pWaWEState->pCurrentChannelSet->lUsageCounter--; // Not using it anymore!
    ErrorMsg("Out of memory!\nOperation failed!");
    return 0;
  }

  pPlaybackDesc->pChannelPos = (WaWEPosition_p) malloc(sizeof(WaWEPosition_t) * pPlaybackDesc->iNumOfChannels);

  if (!pPlaybackDesc->pChannelPos)
  {
    // Could not allocate memory for channel position array!
    free(pPlaybackDesc->pChannels);
    free(pPlaybackDesc);
    pWaWEState->pCurrentChannelSet->lUsageCounter--; // Not using it anymore!
    ErrorMsg("Out of memory!\nOperation failed!");
    return 0;
  }

  // Fill the channel array with channel pointers,
  // and set the current position of the channels to
  // the beginning, because we'll play back the whole channel!
  // (Or get current position, if we want to play back from curr pos)
  pPlaybackDesc->pChannels[0] = pWaWEState->pCurrentChannel;

  if (bPlayFromBeginning)
  {
    pPlaybackDesc->pChannelPos[0] = 0;
    PluginHelpers.fn_WaWEHelper_Channel_SetCurrentPosition(pWaWEState->pCurrentChannel,
							   0);
  } else
  {
    pPlaybackDesc->pChannelPos[0] = pWaWEState->pCurrentChannel->RequiredCurrentPosition;
  }

  // Descriptor is prepared, now try to start the playback!
  if (!StartPlayback(pPlaybackDesc,
                     fnOpenStream,
                     fnReadStream,
                     fnCloseStream,
                     pChannelSet->OriginalFormat.iFrequency,
                     pPlaybackDesc->iNumOfChannels))
  {
    // Could not start playback!
    free(pPlaybackDesc->pChannelPos);
    free(pPlaybackDesc->pChannels);
    free(pPlaybackDesc);
    pWaWEState->pCurrentChannelSet->lUsageCounter--; // Not using it anymore!
    ErrorMsg("Could not start playback!\n"
             "\n"
             "Make sure that the plugin is correctly configured, and\n"
             "the selected driver supports the playback of this number\n"
             "of channels!");
    return 0;
  }

  return 1;
}

static int Play_SelectedChannelSetData(WaWEState_p pWaWEState, HWND hwndOwner)
{
  PlaybackDesc_p pPlaybackDesc;
  WaWEChannel_p pChannel;
  int i;

  if (!pWaWEState) return 0;
  if (!pWaWEState->pCurrentChannelSet) return 0;
  

  if (bIsPlaying)
  {
    WarningMsg("Already playing, please stop the playback first!");
    return 0;
  }

  pPlaybackDesc = (PlaybackDesc_p) malloc(sizeof(PlaybackDesc_t));
  if (!pPlaybackDesc)
  {
    ErrorMsg("Out of memory!\nOperation failed!");
    return 0;
  }

  if (DosRequestMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList, SEM_INDEFINITE_WAIT)!=NO_ERROR)
  {
    free(pPlaybackDesc);
    ErrorMsg("Internal error!\nOperation failed!");
    return 0;
  }


  // Note that we'll use this channelset, so nobody should delete or modify it!
  pWaWEState->pCurrentChannelSet->lUsageCounter++;

  // Store that we'll use only the selected areas!
  pPlaybackDesc->bOnlySelectedArea = TRUE;

  // Get the number of channels inside
  pPlaybackDesc->iNumOfChannels = 0;

  pChannel = pWaWEState->pCurrentChannelSet->pChannelListHead;
  while (pChannel)
  {
    pPlaybackDesc->iNumOfChannels++;
    pChannel = pChannel->pNext;
  }

  // Check for empty channel-set!
  if (pPlaybackDesc->iNumOfChannels==0)
  {
    DosReleaseMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList);
    free(pPlaybackDesc);
    pWaWEState->pCurrentChannelSet->lUsageCounter--; // Not using it anymore!
    WarningMsg("Empty channel-sets cannot be played back!");
    return 0;
  }

  // Now collect the channel-set channels to be played back!
  pPlaybackDesc->pChannels = (WaWEChannel_p *) malloc(sizeof(WaWEChannel_p) * pPlaybackDesc->iNumOfChannels);

  if (!pPlaybackDesc->pChannels)
  {
    // Could not allocate memory for channel array!
    DosReleaseMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList);
    free(pPlaybackDesc);
    pWaWEState->pCurrentChannelSet->lUsageCounter--; // Not using it anymore!
    ErrorMsg("Out of memory!\nOperation failed!");
    return 0;
  }

  pPlaybackDesc->pChannelPos = (WaWEPosition_p) malloc(sizeof(WaWEPosition_t) * pPlaybackDesc->iNumOfChannels);

  if (!pPlaybackDesc->pChannelPos)
  {
    // Could not allocate memory for channel position array!
    DosReleaseMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList);
    free(pPlaybackDesc->pChannels);
    free(pPlaybackDesc);
    pWaWEState->pCurrentChannelSet->lUsageCounter--; // Not using it anymore!
    ErrorMsg("Out of memory!\nOperation failed!");
    return 0;
  }

  // Fill the channel array with channel pointers,
  // and set the current position of the channels to
  // the beginning of their selected regions.
  pChannel = pWaWEState->pCurrentChannelSet->pChannelListHead;
  i = 0;
  while (pChannel)
  {
    pPlaybackDesc->pChannels[i] = pChannel;

    pPlaybackDesc->pChannelPos[i] = 0;
    if (DosRequestMutexSem(pChannel->hmtxUseShownSelectedRegionList, SEM_INDEFINITE_WAIT) == NO_ERROR)
    {
      if (pChannel->pShownSelectedRegionListHead)
      {
	pPlaybackDesc->pChannelPos[i] =
	  pChannel->pShownSelectedRegionListHead->Start;
      }
      DosReleaseMutexSem(pChannel->hmtxUseShownSelectedRegionList);
    }
    PluginHelpers.fn_WaWEHelper_Channel_SetCurrentPosition(pChannel,
							   pPlaybackDesc->pChannelPos[i]);

    i++;
    pChannel = pChannel->pNext;
  }

  DosReleaseMutexSem(pWaWEState->pCurrentChannelSet->hmtxUseChannelList);

  // Descriptor is prepared, now try to start the playback!
  if (!StartPlayback(pPlaybackDesc,
                     fnOpenStream,
                     fnReadStream,
                     fnCloseStream,
                     pWaWEState->pCurrentChannelSet->OriginalFormat.iFrequency,
                     pPlaybackDesc->iNumOfChannels))
  {
    // Could not start playback!
    free(pPlaybackDesc->pChannelPos);
    free(pPlaybackDesc->pChannels);
    free(pPlaybackDesc);
    pWaWEState->pCurrentChannelSet->lUsageCounter--; // Not using it anymore!
    ErrorMsg("Could not start playback!\n"
             "\n"
             "Make sure that the plugin is correctly configured, and\n"
             "the selected driver supports the playback of this number\n"
             "of channels!");
    return 0;
  }

  return 1;
}

static int Play_SelectedChannelData(WaWEState_p pWaWEState, HWND hwndOwner)
{
  PlaybackDesc_p pPlaybackDesc;
  WaWEChannelSet_p pChannelSet;

  if (!pWaWEState) return 0;
  if (!pWaWEState->pCurrentChannel) return 0;
  if (pWaWEState->pCurrentChannel->ShownSelectedLength<=0) return 0;

  pChannelSet = pWaWEState->pCurrentChannel->pParentChannelSet;

  if (bIsPlaying)
  {
    WarningMsg("Already playing, please stop the playback first!");
    return 0;
  }

  pPlaybackDesc = (PlaybackDesc_p) malloc(sizeof(PlaybackDesc_t));
  if (!pPlaybackDesc)
  {
    ErrorMsg("Out of memory!\nOperation failed!");
    return 0;
  }

  // Note that we'll use this channelset, so nobody should delete or modify it!
  pChannelSet->lUsageCounter++;

  // Store that we'll use only the selected areas!
  pPlaybackDesc->bOnlySelectedArea = TRUE;

  // Get the number of channels inside
  pPlaybackDesc->iNumOfChannels = 1;

  // Now collect the channels to be played back!
  pPlaybackDesc->pChannels = (WaWEChannel_p *) malloc(sizeof(WaWEChannel_p) * pPlaybackDesc->iNumOfChannels);

  if (!pPlaybackDesc->pChannels)
  {
    // Could not allocate memory for channel array!
    free(pPlaybackDesc);
    pWaWEState->pCurrentChannelSet->lUsageCounter--; // Not using it anymore!
    ErrorMsg("Out of memory!\nOperation failed!");
    return 0;
  }

  pPlaybackDesc->pChannelPos = (WaWEPosition_p) malloc(sizeof(WaWEPosition_t) * pPlaybackDesc->iNumOfChannels);

  if (!pPlaybackDesc->pChannelPos)
  {
    // Could not allocate memory for channel position array!
    free(pPlaybackDesc->pChannels);
    free(pPlaybackDesc);
    pWaWEState->pCurrentChannelSet->lUsageCounter--; // Not using it anymore!
    ErrorMsg("Out of memory!\nOperation failed!");
    return 0;
  }

  // Fill the channel array with channel pointers,
  // and set the current position of the channels to
  // the beginning of selected region!
  pPlaybackDesc->pChannels[0] = pWaWEState->pCurrentChannel;
  pPlaybackDesc->pChannelPos[0] = 0;
  if (DosRequestMutexSem(pWaWEState->pCurrentChannel->hmtxUseShownSelectedRegionList, SEM_INDEFINITE_WAIT) == NO_ERROR)
  {
    if (pWaWEState->pCurrentChannel->pShownSelectedRegionListHead)
    {
      pPlaybackDesc->pChannelPos[0] =
	pWaWEState->pCurrentChannel->pShownSelectedRegionListHead->Start;
    }
    DosReleaseMutexSem(pWaWEState->pCurrentChannel->hmtxUseShownSelectedRegionList);
  }
  PluginHelpers.fn_WaWEHelper_Channel_SetCurrentPosition(pWaWEState->pCurrentChannel,
							 pPlaybackDesc->pChannelPos[0]);


  // Descriptor is prepared, now try to start the playback!
  if (!StartPlayback(pPlaybackDesc,
                     fnOpenStream,
                     fnReadStream,
                     fnCloseStream,
                     pChannelSet->OriginalFormat.iFrequency,
                     pPlaybackDesc->iNumOfChannels))
  {
    // Could not start playback!
    free(pPlaybackDesc->pChannelPos);
    free(pPlaybackDesc->pChannels);
    free(pPlaybackDesc);
    pWaWEState->pCurrentChannelSet->lUsageCounter--; // Not using it anymore!
    ErrorMsg("Could not start playback!\n"
             "\n"
             "Make sure that the plugin is correctly configured, and\n"
             "the selected driver supports the playback of this number\n"
             "of channels!");
    return 0;
  }

  return 1;
}

int  WAWECALL plugin_DoPopupMenuAction(int iPopupMenuID,
                                       WaWEState_p pWaWEState,
                                       HWND hwndOwner)
{
#ifdef DEBUG_BUILD
//  printf("[DoPopupMenuAction] : Enter, id = %d\n", iPopupMenuID); fflush(stdout);
#endif
  // Check parameters
  if (!pWaWEState) return 0;

  switch (iPopupMenuID)
  {
    case POPUPMENU_ID_PLAY_CHANNEL_SELECTEDDATA:
      return Play_SelectedChannelData(pWaWEState, hwndOwner);
    case POPUPMENU_ID_PLAY_CHANNELSET_SELECTEDDATA:
      return Play_SelectedChannelSetData(pWaWEState, hwndOwner);
    case POPUPMENU_ID_PLAY_CHANNEL_ALLDATA:
      return Play_WholeChannel(pWaWEState, hwndOwner, TRUE);
    case POPUPMENU_ID_PLAY_CHANNELSET_ALLDATA:
      return Play_WholeChannelSet(pWaWEState, hwndOwner, TRUE);
    case POPUPMENU_ID_PLAY_CHANNEL_FROMCURRENTPOS:
      return Play_WholeChannel(pWaWEState, hwndOwner, FALSE);
    case POPUPMENU_ID_PLAY_CHANNELSET_FROMCURRENTPOS:
      return Play_WholeChannelSet(pWaWEState, hwndOwner, FALSE);

    case POPUPMENU_ID_STOP_PLAYBACK:
      return Stop(pWaWEState, hwndOwner);
    default:
      return 0;
  }
}

void WAWECALL plugin_About(HWND hwndOwner)
{
  WinMessageBox(HWND_DESKTOP, hwndOwner,
                pchAboutMessage, pchAboutTitle, 0, MB_OK | MB_MOVEABLE | MB_INFORMATION);
}

void WAWECALL plugin_Configure(HWND hwndOwner)
{
  DoConfigureWindow(hwndOwner, hmodOurDLLHandle);
}


int WAWECALL plugin_Initialize(HWND hwndOwner,
                               WaWEPluginHelpers_p pWaWEPluginHelpers)
{
  if (!pWaWEPluginHelpers) return 0;

  // Save the helper functions for later usage!
  memcpy(&PluginHelpers, pWaWEPluginHelpers, sizeof(PluginHelpers));

  // Load configuration, or set default one!
  LoadPluginConfig();

  // Return success!
  return 1;
}

void WAWECALL plugin_PrepareForShutdown(void)
{
  // Make sure we stop the playback and uninitialize the audio drivers.
  StopPlayback();
}

void WAWECALL plugin_Uninitialize(HWND hwndOwner)
{
  // Make sure we stop the playback and uninitialize the audio drivers.
  StopPlayback();
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

  // Almost maximum importance, so it'll be at the top of the popup menu (After properties)
  pPluginDesc->iPluginImportance = WAWE_PLUGIN_IMPORTANCE_MAX-1;

  pPluginDesc->iPluginType = WAWE_PLUGIN_TYPE_PROCESSOR;

  // Plugin functions:
  pPluginDesc->fnAboutPlugin = plugin_About;
  pPluginDesc->fnConfigurePlugin = plugin_Configure;
  pPluginDesc->fnInitializePlugin = plugin_Initialize;
  pPluginDesc->fnPrepareForShutdown = plugin_PrepareForShutdown;
  pPluginDesc->fnUninitializePlugin = plugin_Uninitialize;

  pPluginDesc->TypeSpecificInfo.ProcessorPluginInfo.fnGetPopupMenuEntries =
    plugin_GetPopupMenuEntries;
  pPluginDesc->TypeSpecificInfo.ProcessorPluginInfo.fnFreePopupMenuEntries =
    plugin_FreePopupMenuEntries;
  pPluginDesc->TypeSpecificInfo.ProcessorPluginInfo.fnDoPopupMenuAction =
    plugin_DoPopupMenuAction;

  // Save module handle for later use
  // (We might need it when we want to load some resources from DLL)
  hmodOurDLLHandle = pPluginDesc->hmodDLL;

  // Return success!
  return 1;
}

