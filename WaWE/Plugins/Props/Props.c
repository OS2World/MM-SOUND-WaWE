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
 * Channel-Set Properties plugin                                             *
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <process.h>
#include <string.h>
#include <limits.h>

#define INCL_TYPES
#define INCL_WIN
#define INCL_DOSRESOURCES
#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>

#include "WaWECommon.h"         // Common defines and structures
#include "PropsWindowHandling.h" // Functions about handling Properties windows

// Global variables
// (Per process, because the DLL is initinstance/terminstance!)
HMODULE hmodOurDLLHandle;
WaWEPluginHelpers_t PluginHelpers;

#define PLUGIN_NAME "Channel-Set Properties plugin"
char *pchPluginName = PLUGIN_NAME;
char *pchAboutTitle = "About "PLUGIN_NAME;
char *pchAboutMessage =
  "Channel-Set Properties plugin v1.0 for WaWE\n"
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
                Msg, PLUGIN_NAME" error", 0, MB_OK | MB_MOVEABLE | MB_ICONEXCLAMATION);
}

void WarningMsg(char *Msg)
{
#ifdef DEBUG_BUILD
  printf("WarningMsg: %s\n",Msg); fflush(stdout);
#endif
  WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                Msg, PLUGIN_NAME" warning", 0, MB_OK | MB_MOVEABLE | MB_ICONEXCLAMATION);
}

// Functions called from outside:

// Define menu IDs. Note, that the ID of 0 is reserved for WAWE_POPUPMENU_ID_ROOT!
#define POPUPMENU_ID_PROPERTIES                       1


int  WAWECALL plugin_GetPopupMenuEntries(int iPopupMenuID,
                                         WaWEState_p pWaWEState,
                                         int *piNumOfPopupMenuEntries,
                                         WaWEPopupMenuEntries_p *ppPopupMenuEntries)
{
#ifdef DEBUG_BUILD
//  printf("[GetPopupMenuEntries] : Props: Enter, id = %d\n", iPopupMenuID); fflush(stdout);
#endif
  // Check parameters!
  if ((!pWaWEState) || (!piNumOfPopupMenuEntries) ||
      (!ppPopupMenuEntries))
  {
#ifdef DEBUG_BUILD
    printf("[GetPopupMenuEntries] : Props: Invalid params\n"); fflush(stdout);
#endif
    return 0;
  }

  // We cannot do anything if there is no current channelset!
  if (!(pWaWEState->pCurrentChannelSet))
  {
#ifdef DEBUG_BUILD
//    printf("[GetPopupMenuEntries] : Props: No current chset"); fflush(stdout);
#endif
    return 0;
  }

  switch (iPopupMenuID)
  {
    case WAWE_POPUPMENU_ID_ROOT:
      {
        // Ok, WaWE asks what menu items we want into first popup menu!
        (*piNumOfPopupMenuEntries) = 1;
  
        *ppPopupMenuEntries = (WaWEPopupMenuEntries_p) malloc((*piNumOfPopupMenuEntries) * sizeof(WaWEPopupMenuEntries_t));
        if (!(*ppPopupMenuEntries))
        {
#ifdef DEBUG_BUILD
          printf("[GetPopupMenuEntries] : Warning, out of memory!\n"); fflush(stdout);
#endif
          return 0;
        }
        // The only-one popupmenu entry is the Properties!
        (*ppPopupMenuEntries)[0].pchEntryText = "Pr~operties";
        (*ppPopupMenuEntries)[0].iEntryID     = POPUPMENU_ID_PROPERTIES;
        (*ppPopupMenuEntries)[0].iEntryStyle  = WAWE_POPUPMENU_STYLE_MENUITEM;
        
        break;
      }
    default:
#ifdef DEBUG_BUILD
      printf("[GetPopupMenuEntries] : Invalid Menu ID!\n"); fflush(stdout);
#endif
      return 0;
  }
#ifdef DEBUG_BUILD
//  printf("[GetPopupMenuEntries] : Props: Returning 1\n"); fflush(stdout);
#endif

  return 1;
}

int ShowPropertiesForChannelSet(WaWEChannelSet_p pChannelSet, HWND hwndOwner)
{
  PropertiesWindowList_p pTemp;
  int iResult;

  // First of all, check if we already have a properties window of this channel-set!
  // Show that window if we have it, or create a new one if we dont!

  if (DosRequestMutexSem(hmtxUseWindowList, SEM_INDEFINITE_WAIT)!=NO_ERROR)
  {
#ifdef DEBUG_BUILD
    printf("[ShowPropertiesForChannelSet] : Could not get mutex semaphore!\n"); fflush(stdout);
#endif
    return 0;
  }

  iResult = 1;

  pTemp = pWindowListHead;
  while ((pTemp) && (pTemp->pChannelSet!=pChannelSet)) pTemp=pTemp->pNext;

  if (pTemp)
  {
    // We already have a properties window for this channel-set, so show it!

    // Activate and make it visible
    WinSetWindowPos(pTemp->hwndPropertiesWindow, HWND_TOP, 0, 0, 0, 0,
		    SWP_SHOW | SWP_ACTIVATE | SWP_ZORDER);
    WinSetFocus(HWND_DESKTOP, pTemp->hwndPropertiesWindow);
  } else
  {
    // We don't have a properties window for this channel-set yet, so
    // create a new properties window!
    pTemp = (PropertiesWindowList_p) malloc(sizeof(PropertiesWindowList_t));
    if (pTemp)
    {
      if (!CreateNewPropertiesWindow(pTemp,
                                     hmodOurDLLHandle,
                                     pChannelSet,
                                     hwndOwner))
      {
#ifdef DEBUG_BUILD
        printf("[ShowPropertiesForChannelSet] : Could not create properties window!\n"); fflush(stdout);
#endif
        free(pTemp); pTemp = NULL;
        iResult = 0;
      } else
      {
        // Fine, properties window has been created, so
        // link this entry to the list of windows!
        pTemp->pNext = pWindowListHead;
        pWindowListHead = pTemp;
      }
    } else
    {
#ifdef DEBUG_BUILD
      printf("[ShowPropertiesForChannelSet] : Out of memory!\n"); fflush(stdout);
#endif
      iResult = 0;
    }
  }

  DosReleaseMutexSem(hmtxUseWindowList);

  return iResult;
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
    // strings in there were constants, we've not allocated any of them...
    free(pPopupMenuEntries);
  }
  return;
}

int  WAWECALL plugin_DoPopupMenuAction(int iPopupMenuID,
                                       WaWEState_p pWaWEState,
                                       HWND hwndOwner)
{
#ifdef DEBUG_BUILD
//  printf("[DoPopupMenuAction] : Props, Enter, id = %d\n", iPopupMenuID); fflush(stdout);
#endif
  // Check parameters
  if (!pWaWEState) return 0;

  if (iPopupMenuID == POPUPMENU_ID_PROPERTIES)
  {
    // Properties!
    if (!(pWaWEState->pCurrentChannelSet))
    {
#ifdef DEBUG_BUILD
      printf("[DoPopupMenuAction] : No current channel-set!\n"); fflush(stdout);
#endif
      return 0;
    }

    // Cool, there is a current channel-set, we have to show the properties window for it!
    return ShowPropertiesForChannelSet(pWaWEState->pCurrentChannelSet, hwndOwner);
  }
  else
  {
#ifdef DEBUG_BUILD
    printf("[DoPopupMenuAction] : Invalid Menu ID! (%d)\n", iPopupMenuID); fflush(stdout);
#endif
    return 0;
  }
}

void WAWECALL plugin_About(HWND hwndOwner)
{
  WinMessageBox(HWND_DESKTOP, hwndOwner,
                pchAboutMessage, pchAboutTitle, 0, MB_OK | MB_MOVEABLE | MB_INFORMATION);
}

void WAWECALL plugin_EventCallback(WaWEEventDesc_p pEventDescription)
{
  PropertiesWindowList_p pTemp;
  WaWEChannelSet_p pChannelSet;
  WaWEChannel_p pChannel;

  // This is the function that will be called at the events we've registered for.
  // The rules for event-callbacks are:
  // - Finish the function as quick as possible. Postpone every time-hungry
  //   work to a worker thread, don't do long jobs in this function!
  // - Never ever do anything in this function that would trigger another event!
  //   This would lead to a deadlock in WaWE!
  //   It's because WaWE gets a mutex semaphore to protect the list of registered event handlers.
  //   It goes through the list and calls the event handlers. If an event handler at this time
  //   triggers another event, it will try to get that mutex again, ang get a deadlock.

  if (pEventDescription)
  {
#ifdef DEBUG_BUILD
//    printf("[plugin_EventCallback] : Event is 0x%04x\n", pEventDescription->iEventType); fflush(stdout);
#endif
    switch (pEventDescription->iEventType)
    {
      case WAWE_EVENT_POSTSAMPLEWRITE:
      case WAWE_EVENT_POSTSAMPLEINSERT:
      case WAWE_EVENT_POSTSAMPLEDELETE:
	// These events can change the length of a channel, and this can result in the change
	// of the length of a channel-set. So, update the Channels and the General pages!

	// First find the properties window descriptor of this channel!
	pChannel = pEventDescription->EventSpecificInfo.SampleModInfo.pModifiedChannel;
	if (pChannel)
	  pChannelSet = pChannel->pParentChannelSet;
	else
          pChannelSet = NULL;

	if (DosRequestMutexSem(hmtxUseWindowList, SEM_INDEFINITE_WAIT)==NO_ERROR)
	{
	  pTemp = pWindowListHead;
	  while ((pTemp) && (pTemp->pChannelSet!=pChannelSet)) pTemp=pTemp->pNext;
	  if ((pTemp) && (pTemp->iWindowCreated==1) && (pTemp->bConvertInProgress==0))
	  {
	    // Fine, found the properties window of this channel-set!
            // Now post (!) messages to it to update some of its pages!
            // (We don't do it if we're just converting it, because the convert function
            // generates a lot of write and insert stuff, which would result in updating
            // the window every time we write a sample. That slows down the convertation
            // process a lot!)
	    WinPostMsg(pTemp->hwndGeneralNBP, WM_UPDATECONTROLVALUES, (MPARAM) 0, (MPARAM) 0);
            WinPostMsg(pTemp->hwndChannelsNBP, WM_UPDATECONTROLVALUES, (MPARAM) 0, (MPARAM) 0);
	  }
	  DosReleaseMutexSem(hmtxUseWindowList);
	}
	break;

      case WAWE_EVENT_POSTCHANNELDELETE:
      case WAWE_EVENT_POSTCHANNELCREATE:
	// The number of channels have been changed!

	// First find the properties window descriptor of this channel!
	pChannelSet = pEventDescription->EventSpecificInfo.ChannelModInfo.pParentChannelSet;

	if (DosRequestMutexSem(hmtxUseWindowList, SEM_INDEFINITE_WAIT)==NO_ERROR)
	{
	  pTemp = pWindowListHead;
	  while ((pTemp) && (pTemp->pChannelSet!=pChannelSet)) pTemp=pTemp->pNext;
	  if ((pTemp) && (pTemp->iWindowCreated==1))
	  {
	    // Fine, found the properties window of this channel-set!
	    // Now post (!) messages to it to update some of its pages!
	    WinPostMsg(pTemp->hwndGeneralNBP, WM_UPDATECONTROLVALUES, (MPARAM) 0, (MPARAM) 0);
            WinPostMsg(pTemp->hwndFormatCommonNBP, WM_UPDATECONTROLVALUES, (MPARAM) 0, (MPARAM) 0);
            WinPostMsg(pTemp->hwndChannelsNBP, WM_UPDATECONTROLVALUES, (MPARAM) 0, (MPARAM) 0);
	  }
	  DosReleaseMutexSem(hmtxUseWindowList);
	}
	break;

      case WAWE_EVENT_POSTCHANNELCHANGE:
        // Something about a channel have been changed. It can be anything in the channel structure!

	// First find the properties window descriptor of this channel!
        pChannelSet = pEventDescription->EventSpecificInfo.ChannelModInfo.pParentChannelSet;

	if (DosRequestMutexSem(hmtxUseWindowList, SEM_INDEFINITE_WAIT)==NO_ERROR)
        {
	  pTemp = pWindowListHead;
          while ((pTemp) && (pTemp->pChannelSet!=pChannelSet)) pTemp=pTemp->pNext;
	  if ((pTemp) && (pTemp->iWindowCreated==1))
          {
	    // Fine, found the properties window of this channel-set!
	    // Now post (!) messages to it to update some of its pages!
	    WinPostMsg(pTemp->hwndGeneralNBP, WM_UPDATECONTROLVALUES, (MPARAM) 0, (MPARAM) 0);
	    WinPostMsg(pTemp->hwndFormatCommonNBP, WM_UPDATECONTROLVALUES, (MPARAM) 0, (MPARAM) 0);
            WinPostMsg(pTemp->hwndFormatSpecificNBP, WM_UPDATECONTROLVALUES, (MPARAM) 0, (MPARAM) 0);
            WinPostMsg(pTemp->hwndChannelsNBP, WM_UPDATECONTROLVALUES, (MPARAM) 0, (MPARAM) 0);
          }
          DosReleaseMutexSem(hmtxUseWindowList);
        }
	break;

      case WAWE_EVENT_PRECHANNELSETDELETE:
	// A channel-set is about to be deleted. Destroy the properties window first!

        // First find the properties window descriptor of this channel-set!
	pChannelSet = pEventDescription->EventSpecificInfo.ChannelSetModInfo.pModifiedChannelSet;

	if (DosRequestMutexSem(hmtxUseWindowList, SEM_INDEFINITE_WAIT)==NO_ERROR)
	{
	  pTemp = pWindowListHead;
	  while ((pTemp) && (pTemp->pChannelSet!=pChannelSet)) pTemp=pTemp->pNext;
          DosReleaseMutexSem(hmtxUseWindowList);
	  if ((pTemp) && (pTemp->iWindowCreated==1))
	  {
	    // Fine, found the properties window of this channel-set!
	    // Now destroy the window!
	    DestroyPropertiesWindow(pTemp);
	  }
	}
	break;

      case WAWE_EVENT_POSTCHANNELSETCHANGE:
	// Something in the channel-set has been changed (like format-specific stuffs, format or anything).

        // First find the properties window descriptor of this channel-set!
	pChannelSet = pEventDescription->EventSpecificInfo.ChannelSetModInfo.pModifiedChannelSet;

	if (DosRequestMutexSem(hmtxUseWindowList, SEM_INDEFINITE_WAIT)==NO_ERROR)
	{
	  pTemp = pWindowListHead;
	  while ((pTemp) && (pTemp->pChannelSet!=pChannelSet)) pTemp=pTemp->pNext;
	  if ((pTemp) && (pTemp->iWindowCreated==1))
	  {
	    // Fine, found the properties window of this channel-set!
	    // Now post (!) messages to it to update some of its pages!
	    WinPostMsg(pTemp->hwndGeneralNBP, WM_UPDATECONTROLVALUES, (MPARAM) 0, (MPARAM) 0);
	    WinPostMsg(pTemp->hwndFormatCommonNBP, WM_UPDATECONTROLVALUES, (MPARAM) 0, (MPARAM) 0);
            WinPostMsg(pTemp->hwndFormatSpecificNBP, WM_UPDATECONTROLVALUES, (MPARAM) 0, (MPARAM) 0);
            WinPostMsg(pTemp->hwndChannelsNBP, WM_UPDATECONTROLVALUES, (MPARAM) 0, (MPARAM) 0);
	  }
	  DosReleaseMutexSem(hmtxUseWindowList);
	}
	break;

      case WAWE_EVENT_POSTCHANNELMOVE:
	// A channel has been moved!

	// First find the properties window descriptor of this channel!
	pChannelSet = pEventDescription->EventSpecificInfo.ChannelModInfo.pParentChannelSet;

	if (DosRequestMutexSem(hmtxUseWindowList, SEM_INDEFINITE_WAIT)==NO_ERROR)
	{
	  pTemp = pWindowListHead;
	  while ((pTemp) && (pTemp->pChannelSet!=pChannelSet)) pTemp=pTemp->pNext;
	  if ((pTemp) && (pTemp->iWindowCreated==1))
	  {
	    // Fine, found the properties window of this channel-set!
	    // Now post (!) messages to it to update some of its pages!
            WinPostMsg(pTemp->hwndChannelsNBP, WM_UPDATECONTROLVALUES, (MPARAM) 0, (MPARAM) 0);
	  }
	  DosReleaseMutexSem(hmtxUseWindowList);
	}
	break;

      default:
        // We don't do anything at events we don't know.
        break;
    }
  }
}

int WAWECALL plugin_Initialize(HWND hwndOwner,
                               WaWEPluginHelpers_p pWaWEPluginHelpers)
{
  if (!pWaWEPluginHelpers) return 0;

  // Save the helper functions for later usage!
  memcpy(&PluginHelpers, pWaWEPluginHelpers, sizeof(PluginHelpers));

  // Initialize list of visible windows!
  DosCreateMutexSem(NULL, // Name
                    &hmtxUseWindowList,
                    0L, // Non-shared
                    FALSE); // Unowned
  pWindowListHead = NULL;

  // Register for the events!
  PluginHelpers.fn_WaWEHelper_Global_SetEventCallback(plugin_EventCallback,
                                                      WAWE_EVENT_ALL);

  // Return success!
  return 1;
}

void WAWECALL plugin_PrepareForShutdown(void)
{
  // Unregister event callbacks!
  PluginHelpers.fn_WaWEHelper_Global_SetEventCallback(plugin_EventCallback,
						      WAWE_EVENT_NONE);

  // Destroy every visible window!
  while (pWindowListHead)
  {
    DestroyPropertiesWindow(pWindowListHead);
  }
  // Ok, now that every properties window is destroyed.
}

void WAWECALL plugin_Uninitialize(HWND hwndOwner)
{
  // Destroy semaphore
  DosRequestMutexSem(hmtxUseWindowList, SEM_INDEFINITE_WAIT); // Make sure that nobody uses the plugin now.
  DosCloseMutexSem(hmtxUseWindowList);
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

  // Maximum importance, so it'll be at the top of the popup menu
  pPluginDesc->iPluginImportance = WAWE_PLUGIN_IMPORTANCE_MAX;

  pPluginDesc->iPluginType = WAWE_PLUGIN_TYPE_PROCESSOR;

  // Plugin functions:
  pPluginDesc->fnAboutPlugin = plugin_About;
  pPluginDesc->fnConfigurePlugin = NULL;
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

