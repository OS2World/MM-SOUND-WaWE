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
 * Popup Menu handling routines                                              *
 *****************************************************************************/

#include "WaWE.h"

typedef struct WaWEPopupMenu_t_struct{

  // Identifying the popup menu entry:
  HWND hwndPopupMenu;
  int  iItemID;

  // What to call with what parameter, if this is selected
  WaWEPlugin_p  pOwnerPlugin;
  int           iOwnerEntryID;

  // What was the active channel and channel-set when built the menu
  WaWEChannelSet_p pCurrentChannelSet;
  WaWEChannel_p    pCurrentChannel;

  // Pointer to submenu, or NULL if it's not a submenu
  void         *pSubMenu;
  // Pointer to next menuitem
  void         *pNext;

} WaWEPopupMenu_t, *WaWEPopupMenu_p;

// Current popup menu descriptor
WaWEPopupMenu_p pPopupMenuDescriptor;
HMTX  hmtxUsePopupMenuDescriptor;


static void core_DestroyPopupMenu(HWND hwndPopupMenu, WaWEPopupMenu_p *ppListHead)
{
  WaWEPopupMenu_p pTemp, pToDelete, pPrev;

  pTemp = *ppListHead;
  pPrev = NULL;
  while (pTemp)
  {
    if (pTemp->hwndPopupMenu == hwndPopupMenu)
    {
      // If it has a submenu, destroy the submenu first!
      if (pTemp->pSubMenu)
      {
        WaWEPopupMenu_p pSubMenu = pTemp->pSubMenu;
        core_DestroyPopupMenu(pSubMenu->hwndPopupMenu, &(pTemp->pSubMenu));
      }
      // Now remember that we'll have to free this!
      pToDelete = pTemp;
      pTemp = pTemp->pNext;

      dbg_free(pToDelete);
      if (pPrev)
        pPrev->pNext = pTemp;
      else
        *ppListHead = pTemp;

    } else
    {
      pPrev = pTemp;
      pTemp = pTemp->pNext;
    }
  }
#ifdef DEBUG_BUILD
//  printf("[core_DestroyPopupMenu] : hwndPopupMenu = %p\n", hwndPopupMenu); fflush(stdout);
#endif

  WinDestroyWindow(hwndPopupMenu);
}

void DestroyPopupMenu(HWND hwndPopupMenu)
{
  if (DosRequestMutexSem(hmtxUsePopupMenuDescriptor, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    core_DestroyPopupMenu(hwndPopupMenu, &pPopupMenuDescriptor);
    DosReleaseMutexSem(hmtxUsePopupMenuDescriptor);
  }
}

static int core_GetPopupMenuItem(WaWEPopupMenu_p pMenuListHead,
                                 int iItemID,
                                 WaWEPlugin_p *ppPlugin, int *piPluginEntryID,
                                 WaWEChannelSet_p *ppCurrChannelSet, WaWEChannel_p *ppCurrChannel)
{
  WaWEPopupMenu_p pTemp;

#ifdef DEBUG_BUILD
//  printf("core_GetPopupMenuItem: Enter for %d\n", iItemID); fflush(stdout);
#endif

  pTemp = pMenuListHead;
  while (pTemp)
  {
    if (pTemp->iItemID == iItemID)
    {
      // Found it!
      *ppPlugin = pTemp->pOwnerPlugin;
      *piPluginEntryID = pTemp->iOwnerEntryID;
      *ppCurrChannelSet = pTemp->pCurrentChannelSet;
      *ppCurrChannel = pTemp->pCurrentChannel;
#ifdef DEBUG_BUILD
//      printf("core_GetPopupMenuItem: Found in list! (pPlugin = %p)\n", *ppPlugin); fflush(stdout);
#endif

      return 1;
    }
    // If it has a submenu, search in there too!
    if (pTemp->pSubMenu)
    {
      if (core_GetPopupMenuItem(pTemp->pSubMenu,
                                iItemID,
                                ppPlugin, piPluginEntryID,
                                ppCurrChannelSet, ppCurrChannel))
        return 1;
    }

    pTemp = pTemp->pNext;
  }
#ifdef DEBUG_BUILD
//  printf("core_GetPopupMenuItem: Leave, not found\n"); fflush(stdout);
#endif

  return 0;
}


int GetPopupMenuItem(int iItemID,
                     WaWEPlugin_p *ppPlugin, int *piPluginEntryID,
                     WaWEChannelSet_p *ppCurrChannelSet, WaWEChannel_p *ppCurrChannel)
{
  int iResult = 0;

  // Check parameters
  if ((!ppPlugin) || (!piPluginEntryID) || (!ppCurrChannelSet) || (!ppCurrChannel))
    return 0;

  if (DosRequestMutexSem(hmtxUsePopupMenuDescriptor, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    iResult = core_GetPopupMenuItem(pPopupMenuDescriptor,
                                    iItemID,
                                    ppPlugin, piPluginEntryID,
                                    ppCurrChannelSet, ppCurrChannel);
    DosReleaseMutexSem(hmtxUsePopupMenuDescriptor);
  }
  return iResult;
}

static HWND CreateSubNewPopupMenu(WaWEPopupMenu_p pDesc, int bHasCurrentChannelSet)
{
  HWND hwndPopupMenu;
  MENUITEM MenuItem;
  WaWEPopupMenu_p pNewDesc;

  hwndPopupMenu = WinCreateWindow(HWND_DESKTOP, // Parent
                                  WC_MENU,      // Class
                                  "WaWE PopUp SubMenu for New", // pszName
                                  0,   // flStyle
                                  0, 0, // coords
                                  0, 0, // size
                                  hwndClient, // Owner
                                  HWND_TOP,   // Sibling
                                  DID_POPUPMENU_NEW, // ID
                                  NULL,  // pCtrlData
                                  NULL); // pPresParams

//  printf("SubNewPopup = %x\n", hwndPopupMenu);

  if (hwndPopupMenu)
  {
    // Add the New->Channel-Set and the New->Channel items!
    pNewDesc = dbg_malloc(sizeof(WaWEPopupMenu_t));

    if (pNewDesc)
    {
      pNewDesc->pOwnerPlugin = NULL;
      pNewDesc->iOwnerEntryID = DID_POPUPMENU_NEWCHANNELSET;

      pNewDesc->pCurrentChannelSet = pDesc->pCurrentChannelSet;
      pNewDesc->pCurrentChannel = pDesc->pCurrentChannel;

      pNewDesc->hwndPopupMenu = hwndPopupMenu;
      pNewDesc->iItemID = pNewDesc->iOwnerEntryID;
              
      pNewDesc->pSubMenu = NULL;
      pNewDesc->pNext = pDesc->pSubMenu;
      pDesc->pSubMenu = pNewDesc;

      MenuItem.iPosition = MIT_END;
      MenuItem.afStyle = MIS_TEXT;
      MenuItem.afAttribute = 0;
      MenuItem.id = pNewDesc->iItemID;
      MenuItem.hwndSubMenu = NULLHANDLE;
      MenuItem.hItem = NULL;

      WinSendMsg(hwndPopupMenu, MM_INSERTITEM, (MPARAM) (&MenuItem), (MPARAM) ("Channel-~Set"));
    }

    if (bHasCurrentChannelSet)
    {
      // We'll have New->Channel menu entry only if there is a channel-set in which we can create a new channel!
      pNewDesc = dbg_malloc(sizeof(WaWEPopupMenu_t));
  
      if (pNewDesc)
      {
        pNewDesc->pOwnerPlugin = NULL;
        pNewDesc->iOwnerEntryID = DID_POPUPMENU_NEWCHANNEL;
  
        pNewDesc->pCurrentChannelSet = pDesc->pCurrentChannelSet;
        pNewDesc->pCurrentChannel = pDesc->pCurrentChannel;
  
        pNewDesc->hwndPopupMenu = hwndPopupMenu;
        pNewDesc->iItemID = pNewDesc->iOwnerEntryID;
                
        pNewDesc->pSubMenu = NULL;
        pNewDesc->pNext = pDesc->pSubMenu;
        pDesc->pSubMenu = pNewDesc;
  
        MenuItem.iPosition = MIT_END;
        MenuItem.afStyle = MIS_TEXT;
        MenuItem.afAttribute = 0;
        MenuItem.id = pNewDesc->iItemID;
        MenuItem.hwndSubMenu = NULLHANDLE;
        MenuItem.hItem = NULL;
  
        WinSendMsg(hwndPopupMenu, MM_INSERTITEM, (MPARAM) (&MenuItem), (MPARAM) ("~Channel"));
      }
    }
  }
  return hwndPopupMenu;
}

static HWND CreateSubDeletePopupMenu(WaWEPopupMenu_p pDesc, int bHasCurrentChannel)
{
  HWND hwndPopupMenu;
  MENUITEM MenuItem;
  WaWEPopupMenu_p pNewDesc;

  hwndPopupMenu = WinCreateWindow(HWND_DESKTOP, // Parent
                                  WC_MENU,      // Class
                                  "WaWE PopUp SubMenu for Delete", // pszName
                                  0,   // flStyle
                                  0, 0, // coords
                                  0, 0, // size
                                  hwndClient, // Owner
                                  HWND_TOP,   // Sibling
                                  DID_POPUPMENU_DELETE, // ID
                                  NULL,  // pCtrlData
                                  NULL); // pPresParams

//  printf("SubDeletePopup = %x\n", hwndPopupMenu);

  if (hwndPopupMenu)
  {
    // Add the Delete->Channel-Set and the Delete->Channel items!
    pNewDesc = dbg_malloc(sizeof(WaWEPopupMenu_t));

    if (pNewDesc)
    {
      pNewDesc->pOwnerPlugin = NULL;
      pNewDesc->iOwnerEntryID = DID_POPUPMENU_DELETECHANNELSET;

      pNewDesc->pCurrentChannelSet = pDesc->pCurrentChannelSet;
      pNewDesc->pCurrentChannel = pDesc->pCurrentChannel;

      pNewDesc->hwndPopupMenu = hwndPopupMenu;
      pNewDesc->iItemID = pNewDesc->iOwnerEntryID;
              
      pNewDesc->pSubMenu = NULL;
      pNewDesc->pNext = pDesc->pSubMenu;
      pDesc->pSubMenu = pNewDesc;

      MenuItem.iPosition = MIT_END;
      MenuItem.afStyle = MIS_TEXT;
      MenuItem.afAttribute = 0;
      MenuItem.id = pNewDesc->iItemID;
      MenuItem.hwndSubMenu = NULLHANDLE;
      MenuItem.hItem = NULL;

      WinSendMsg(hwndPopupMenu, MM_INSERTITEM, (MPARAM) (&MenuItem), (MPARAM) ("Channel-~Set"));
    }

    if (bHasCurrentChannel)
    {
      // We'll have Delete->Channel menu entry only if there is channel-set!
      pNewDesc = dbg_malloc(sizeof(WaWEPopupMenu_t));
  
      if (pNewDesc)
      {
        pNewDesc->pOwnerPlugin = NULL;
        pNewDesc->iOwnerEntryID = DID_POPUPMENU_DELETECHANNEL;
  
        pNewDesc->pCurrentChannelSet = pDesc->pCurrentChannelSet;
        pNewDesc->pCurrentChannel = pDesc->pCurrentChannel;
  
        pNewDesc->hwndPopupMenu = hwndPopupMenu;
        pNewDesc->iItemID = pNewDesc->iOwnerEntryID;
                
        pNewDesc->pSubMenu = NULL;
        pNewDesc->pNext = pDesc->pSubMenu;
        pDesc->pSubMenu = pNewDesc;
  
        MenuItem.iPosition = MIT_END;
        MenuItem.afStyle = MIS_TEXT;
        MenuItem.afAttribute = 0;
        MenuItem.id = pNewDesc->iItemID;
        MenuItem.hwndSubMenu = NULLHANDLE;
        MenuItem.hItem = NULL;
  
        WinSendMsg(hwndPopupMenu, MM_INSERTITEM, (MPARAM) (&MenuItem), (MPARAM) ("~Channel"));
      }
    }
  }
  return hwndPopupMenu;
}


static void InsertBasePopupMenuItems(HWND hwndPopupMenu,
                                     WaWEChannel_p pCurrChannel,
                                     WaWEChannelSet_p pCurrChannelSet)
{
  MENUITEM MenuItem;
  WaWEPopupMenu_p pNewDesc;


  // The 'New' item

  pNewDesc = dbg_malloc(sizeof(WaWEPopupMenu_t));
  if (pNewDesc)
  {
    pNewDesc->pOwnerPlugin = NULL; // WaWE System
    pNewDesc->iOwnerEntryID = DID_POPUPMENU_NEW;

    pNewDesc->pCurrentChannelSet = pCurrChannelSet;
    pNewDesc->pCurrentChannel = pCurrChannel;

    pNewDesc->hwndPopupMenu = hwndPopupMenu;
    pNewDesc->iItemID = pNewDesc->iOwnerEntryID; // The same as that, for System Entries
                  
    pNewDesc->pSubMenu = NULL;
    pNewDesc->pNext = pPopupMenuDescriptor;
    pPopupMenuDescriptor = pNewDesc;

    MenuItem.iPosition = MIT_END;
    MenuItem.afStyle = MIS_TEXT;
    MenuItem.afAttribute = 0;
    MenuItem.id = pNewDesc->iItemID;
    MenuItem.hwndSubMenu = NULLHANDLE;
    MenuItem.hItem = NULL;

    MenuItem.afStyle |= MIS_SUBMENU;
    MenuItem.hwndSubMenu = CreateSubNewPopupMenu(pNewDesc, (pCurrChannelSet==NULL)?0:1);

    WinSendMsg(hwndPopupMenu, MM_INSERTITEM, (MPARAM) (&MenuItem), (MPARAM) ("~New"));
  }

  // The 'Delete' item
  if (pCurrChannelSet)
  {
    // There will be Delete menu only if there is something to delete!
  
    pNewDesc = dbg_malloc(sizeof(WaWEPopupMenu_t));
    if (pNewDesc)
    {
      pNewDesc->pOwnerPlugin = NULL; // WaWE System
      pNewDesc->iOwnerEntryID = DID_POPUPMENU_DELETE;
  
      pNewDesc->pCurrentChannelSet = pCurrChannelSet;
      pNewDesc->pCurrentChannel = pCurrChannel;
  
      pNewDesc->hwndPopupMenu = hwndPopupMenu;
      pNewDesc->iItemID = pNewDesc->iOwnerEntryID; // The same as that, for System Entries
                    
      pNewDesc->pSubMenu = NULL;
      pNewDesc->pNext = pPopupMenuDescriptor;
      pPopupMenuDescriptor = pNewDesc;
  
      MenuItem.iPosition = MIT_END;
      MenuItem.afStyle = MIS_TEXT;
      MenuItem.afAttribute = 0;
      MenuItem.id = pNewDesc->iItemID;
      MenuItem.hwndSubMenu = NULLHANDLE;
      MenuItem.hItem = NULL;
  
      MenuItem.afStyle |= MIS_SUBMENU;
      MenuItem.hwndSubMenu = CreateSubDeletePopupMenu(pNewDesc, (pCurrChannel==NULL)?0:1);
  
      WinSendMsg(hwndPopupMenu, MM_INSERTITEM, (MPARAM) (&MenuItem), (MPARAM) ("~Delete"));
    }
  }
}

static HWND CreateSubPopupMenu(WaWEState_p pWaWEState,
                               WaWEPopupMenu_p pDesc,
                               int *piNextFreeID)
{
  HWND hwndPopupMenu;
  MENUITEM MenuItem;
  WaWEPopupMenu_p pNewDesc;
  int rc;

  // Ask the plugin for submenus it want to put into this menu item!
  hwndPopupMenu = WinCreateWindow(HWND_DESKTOP, // Parent
                                  WC_MENU,      // Class
                                  "WaWE PopUp SubMenu", // pszName
                                  0,   // flStyle
                                  0, 0, // coords
                                  0, 0, // size
                                  hwndClient, // Owner
                                  HWND_TOP,   // Sibling
                                  (*piNextFreeID)++, // ID
                                  NULL,  // pCtrlData
                                  NULL); // pPresParams

//  printf("SubPopup = %x\n", hwndPopupMenu);

  if (hwndPopupMenu)
  {
    int iNumOfPopupMenuEntries;
    WaWEPopupMenuEntries_p pPopupMenuEntries;

    // Ask the plugin for menu entries!
    rc = pDesc->pOwnerPlugin->TypeSpecificInfo.ProcessorPluginInfo.fnGetPopupMenuEntries(pDesc->iOwnerEntryID,
                                                                                         pWaWEState,
                                                                                         &iNumOfPopupMenuEntries,
                                                                                         &pPopupMenuEntries);
    if ((rc) && (iNumOfPopupMenuEntries>0) && (pPopupMenuEntries))
    {
      int i;
      // Add every entry into menu
      for (i=0; i<iNumOfPopupMenuEntries; i++)
      {
        pNewDesc = dbg_malloc(sizeof(WaWEPopupMenu_t));

        if (pNewDesc)
        {
          pNewDesc->pOwnerPlugin = pDesc->pOwnerPlugin;
          pNewDesc->iOwnerEntryID = pPopupMenuEntries[i].iEntryID;

          pNewDesc->pCurrentChannelSet = pDesc->pCurrentChannelSet;
          pNewDesc->pCurrentChannel = pDesc->pCurrentChannel;

          pNewDesc->hwndPopupMenu = hwndPopupMenu;
          pNewDesc->iItemID = (*piNextFreeID)++;
                  
          pNewDesc->pSubMenu = NULL;
          pNewDesc->pNext = pDesc->pSubMenu;
          pDesc->pSubMenu = pNewDesc;

          MenuItem.iPosition = MIT_END;
          MenuItem.afStyle = MIS_TEXT;
          MenuItem.afAttribute = 0;
          MenuItem.id = pNewDesc->iItemID;
          MenuItem.hwndSubMenu = NULLHANDLE;
          MenuItem.hItem = NULL;

          if (pPopupMenuEntries[i].iEntryStyle == WAWE_POPUPMENU_STYLE_SUBMENU)
          {
            MenuItem.afStyle |= MIS_SUBMENU;
            MenuItem.hwndSubMenu = CreateSubPopupMenu(pWaWEState, pNewDesc, piNextFreeID);
          }

          WinSendMsg(hwndPopupMenu, MM_INSERTITEM, (MPARAM) (&MenuItem), (MPARAM) (pPopupMenuEntries[i].pchEntryText));
        }
      }
      pDesc->pOwnerPlugin->TypeSpecificInfo.ProcessorPluginInfo.fnFreePopupMenuEntries(iNumOfPopupMenuEntries,
                                                                                       pPopupMenuEntries);
    }
#ifdef DEBUG_BUILD
    else
    {
      printf("Plugin reported error. rc = %d\n", rc); fflush(stdout);
    }
#endif
  }
  return hwndPopupMenu;
}

HWND CreatePopupMenu(WaWEChannel_p pCurrChannel, WaWEChannelSet_p pCurrChannelSet)
{
  HWND hwndPopupMenu = NULLHANDLE;
  WaWEPopupMenu_p pDesc;
  int iNextFreeID;
#ifdef DEBUG_BUILD
//  printf("[CreatePopupMenu] : Ch: %p  ChSet: %p\n", pCurrChannel, pCurrChannelSet); fflush(stdout);
#endif

  if (DosRequestMutexSem(hmtxUsePopupMenuDescriptor, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {

    // If there is a popup menu already, then destroy it!
    if (pPopupMenuDescriptor)
      core_DestroyPopupMenu(pPopupMenuDescriptor->hwndPopupMenu, &pPopupMenuDescriptor);

    iNextFreeID = DID_POPUPMENU_FIRST_PLUGIN;

    if (DosRequestMutexSem(ChannelSetListProtector_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
    {
      WaWEPluginList_p pProcessorPluginList, pTempPlugin;
  
      pProcessorPluginList = CreatePluginList(WAWE_PLUGIN_TYPE_PROCESSOR);
  
      hwndPopupMenu = WinCreateWindow(HWND_DESKTOP, // Parent
                                      WC_MENU,      // Class
                                      "WaWE PopUp Menu", // pszName
                                      0,   // flStyle
                                      0, 0, // coords
                                      0, 0, // size
                                      hwndClient, // Owner
                                      HWND_TOP,   // Sibling
                                      iNextFreeID++, // ID
                                      NULL,  // pCtrlData
                                      NULL); // pPresParams

//      printf("Popup = %x\n", hwndPopupMenu);

      if (hwndPopupMenu)
      {
        int rc;
        char *pchText;
        POINTL PointL;
        MENUITEM MenuItem;
        int iNumOfInserted;
        int iImportance;
        WaWEState_t WaWEState;
  
        // Insert items into popup menu!
        iNumOfInserted = 0;

        // Insert the base popup menu items, supported by WaWE itself!
        InsertBasePopupMenuItems(hwndPopupMenu, pCurrChannel, pCurrChannelSet);

        // Insert the items of plugins
        WaWEState.pCurrentChannelSet = pCurrChannelSet;
        WaWEState.pCurrentChannel = pCurrChannel;
        WaWEState.pChannelSetListHead = ChannelSetListHead;
  
        // Query plugins for items, possibly in order of importance!
        for (iImportance = WAWE_PLUGIN_IMPORTANCE_MAX; iImportance>=WAWE_PLUGIN_IMPORTANCE_MIN; iImportance--)
        {
          pTempPlugin = pProcessorPluginList;
          while (pTempPlugin)
          {
            if (pTempPlugin->pPlugin->iPluginImportance == iImportance)
            {
              int iNumOfPopupMenuEntries;
              WaWEPopupMenuEntries_p pPopupMenuEntries;
  
              // Ask the plugin for menu entries!
#ifdef DEBUG_BUILD
//              printf("Calling GetPopupMenuEntries of plugin [%s]\n", pTempPlugin->pPlugin->achName); fflush(stdout);
#endif
              rc = pTempPlugin->pPlugin->TypeSpecificInfo.ProcessorPluginInfo.fnGetPopupMenuEntries(WAWE_POPUPMENU_ID_ROOT,
                                                                                                    &WaWEState,
                                                                                                    &iNumOfPopupMenuEntries,
                                                                                                    &pPopupMenuEntries);
              if ((rc) && (iNumOfPopupMenuEntries>0) && (pPopupMenuEntries))
              {
                int i;
                // If these are gonna be the first plugin entries,
                // then insert the separator here first!
                if (iNumOfInserted==0)
                {
                  // Insert a separator
                  MenuItem.iPosition = MIT_END;
                  MenuItem.afStyle = MIS_SEPARATOR;
                  MenuItem.afAttribute = 0;
                  MenuItem.id = iNextFreeID++;
                  MenuItem.hwndSubMenu = NULLHANDLE;
                  MenuItem.hItem = 0;
                  pchText = "Separator";
                  WinSendMsg(hwndPopupMenu, MM_INSERTITEM, (MPARAM) (&MenuItem), (MPARAM) pchText);
            
                  // Add this item into popup menu descriptor!
                  pDesc = dbg_malloc(sizeof(WaWEPopupMenu_t));
                  if (pDesc)
                  {
                    pDesc->pOwnerPlugin = NULL;
                    pDesc->iOwnerEntryID = 0;
                    pDesc->pCurrentChannelSet = pCurrChannelSet;
                    pDesc->pCurrentChannel = pCurrChannel;
                    pDesc->hwndPopupMenu = hwndPopupMenu;
                    pDesc->iItemID = MenuItem.id;
                    pDesc->pSubMenu = NULL;
                    pDesc->pNext = pPopupMenuDescriptor;
                    pPopupMenuDescriptor = pDesc;
                  }
                }
                // Add every entry into menu
                for (i=0; i<iNumOfPopupMenuEntries; i++)
                {
                  pDesc = dbg_malloc(sizeof(WaWEPopupMenu_t));
                  if (pDesc)
                  {
                    pDesc->pOwnerPlugin = pTempPlugin->pPlugin;
                    pDesc->iOwnerEntryID = pPopupMenuEntries[i].iEntryID;
                    pDesc->pCurrentChannelSet = pCurrChannelSet;
                    pDesc->pCurrentChannel = pCurrChannel;
                    pDesc->hwndPopupMenu = hwndPopupMenu;
                    pDesc->iItemID = iNextFreeID++;
                    
                    pDesc->pSubMenu = NULL;
                    pDesc->pNext = pPopupMenuDescriptor;
                    pPopupMenuDescriptor = pDesc;
  
                    MenuItem.iPosition = MIT_END;
                    MenuItem.afStyle = MIS_TEXT;
                    MenuItem.afAttribute = 0;
                    MenuItem.id = pDesc->iItemID;
                    MenuItem.hwndSubMenu = NULLHANDLE;
                    MenuItem.hItem = NULL;
  
                    if (pPopupMenuEntries[i].iEntryStyle == WAWE_POPUPMENU_STYLE_SUBMENU)
                    {
                      MenuItem.afStyle |= MIS_SUBMENU;
                      MenuItem.hwndSubMenu = CreateSubPopupMenu(&WaWEState, pDesc, &iNextFreeID);
                    }
  
                    pchText = pPopupMenuEntries[i].pchEntryText;
                    WinSendMsg(hwndPopupMenu, MM_INSERTITEM, (MPARAM) (&MenuItem), (MPARAM) pchText);
  
                    iNumOfInserted++;
                  }
                }
                pTempPlugin->pPlugin->TypeSpecificInfo.ProcessorPluginInfo.fnFreePopupMenuEntries(iNumOfPopupMenuEntries,
                                                                                                  pPopupMenuEntries);
              }
            }
            pTempPlugin = pTempPlugin->pNext;
          }
        }


        if (iNumOfInserted==0)
        {
#ifdef POPUPMENU_HAS_EMPTY_INFORMATIONAL_ITEM
          // If there are no items inserted, then insert one informational item!
          MenuItem.iPosition = MIT_END;
          MenuItem.afStyle = MIS_TEXT | MIS_STATIC;
          MenuItem.afAttribute = 0;
          MenuItem.id = iNextFreeID++;
          MenuItem.hwndSubMenu = NULLHANDLE;
          MenuItem.hItem = 0;
          pchText = "<No processor plugin entries available>";
          WinSendMsg(hwndPopupMenu, MM_INSERTITEM, (MPARAM) (&MenuItem), (MPARAM) pchText);

          // Add this item into popup menu descriptor!
          pDesc = dbg_malloc(sizeof(WaWEPopupMenu_t));
          if (pDesc)
          {
            pDesc->pOwnerPlugin = NULL;
            pDesc->iOwnerEntryID = 0;
            pDesc->pCurrentChannelSet = pCurrChannelSet;
            pDesc->pCurrentChannel = pCurrChannel;
            pDesc->hwndPopupMenu = hwndPopupMenu;
            pDesc->iItemID = MenuItem.id;
            pDesc->pSubMenu = NULL;
            pDesc->pNext = pPopupMenuDescriptor;
            pPopupMenuDescriptor = pDesc;
          }
#endif
        }

#ifdef POPUPMENU_HAS_LAST_SEPARATOR
        // Add a last separator
        MenuItem.iPosition = MIT_END;
        MenuItem.afStyle = MIS_SEPARATOR;
        MenuItem.afAttribute = 0;
        MenuItem.id = iNextFreeID++;
        MenuItem.hwndSubMenu = NULLHANDLE;
        MenuItem.hItem = 0;
        pchText = "Separator";
        WinSendMsg(hwndPopupMenu, MM_INSERTITEM, (MPARAM) (&MenuItem), (MPARAM) pchText);
  
        // Add this item into popup menu descriptor!
        pDesc = dbg_malloc(sizeof(WaWEPopupMenu_t));
        if (pDesc)
        {
          pDesc->pOwnerPlugin = NULL;
          pDesc->iOwnerEntryID = 0;
          pDesc->pCurrentChannelSet = pCurrChannelSet;
          pDesc->pCurrentChannel = pCurrChannel;
          pDesc->hwndPopupMenu = hwndPopupMenu;
          pDesc->iItemID = MenuItem.id;
          pDesc->pSubMenu = NULL;
          pDesc->pNext = pPopupMenuDescriptor;
          pPopupMenuDescriptor = pDesc;
        }
#endif

        WinQueryPointerPos(HWND_DESKTOP, &PointL);
        rc = WinPopupMenu(HWND_DESKTOP,
                          hwndClient,
                          hwndPopupMenu,
                          PointL.x, PointL.y, // Coord
                          0, // idItem
                          PU_HCONSTRAIN |
                          PU_VCONSTRAIN |
                          PU_KEYBOARD | PU_MOUSEBUTTON1 | PU_MOUSEBUTTON2 | PU_MOUSEBUTTON2);
      }
  
      FreePluginList(pProcessorPluginList);
  
      DosReleaseMutexSem(ChannelSetListProtector_Sem);
    }
    DosReleaseMutexSem(hmtxUsePopupMenuDescriptor);
  }

#ifdef DEBUG_BUILD
//  printf("[CreatePopupMenu] : hwndPopupMenu = %p\n", hwndPopupMenu); fflush(stdout);
#endif
  return hwndPopupMenu;
}

int Initialize_PopupMenu()
{
  int rc;
  pPopupMenuDescriptor = NULL;
  rc = DosCreateMutexSem(NULL, &hmtxUsePopupMenuDescriptor, 0, FALSE);
  return (rc == NO_ERROR);
}

void Uninitialize_PopupMenu()
{
  if (pPopupMenuDescriptor)
    DestroyPopupMenu(pPopupMenuDescriptor->hwndPopupMenu);

  DosCloseMutexSem(hmtxUsePopupMenuDescriptor); hmtxUsePopupMenuDescriptor = -1;
}
