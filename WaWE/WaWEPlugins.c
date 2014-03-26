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
 * Code dealing with list of plugins                                         *
 *****************************************************************************/

#include "WaWE.h"
#include <direct.h>

HMTX PluginListProtector_Sem = -1;
WaWEPlugin_p PluginListHead = NULL;

WaWEPluginList_p CreatePluginList(int iPluginType)
{
  WaWEPluginList_p pResult = NULL;
  WaWEPluginList_p pLast = NULL;
  WaWEPluginList_p pNew;
  WaWEPlugin_p temp;

  if (DosRequestMutexSem(PluginListProtector_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    temp = PluginListHead;
    while (temp)
    {
      if (temp->iPluginType == iPluginType)
      {
        pNew = (WaWEPluginList_p) dbg_malloc(sizeof(WaWEPluginList_t));
        if (pNew)
        {
          pNew->pNext = NULL;
          pNew->pPlugin = temp;
          if (!pResult)
          {
            pResult = pNew;
          } else
          {
            pLast->pNext = pNew;
          }
          pLast = pNew;
        }
      }
      temp = temp->pNext;
    }
    DosReleaseMutexSem(PluginListProtector_Sem);
  }
  return pResult;
}

void FreePluginList(WaWEPluginList_p pList)
{
  WaWEPluginList_p pTemp;

  while (pList)
  {
    pTemp = pList;
    pList = pList->pNext;
    dbg_free(pTemp);
  }
}

WaWEPlugin_p CreatePluginListElement(HWND hwndOwner, char *filename)
{
  HMODULE DLLHandle;
  APIRET rc;
  pfn_WaWEP_GetPluginInfo pDLL_GetPluginInfo;
  WaWEPlugin_p result = NULL;
  WaWEPluginHelpers_t PluginHelpers;

  // Open the DLL
  DLLHandle = NULLHANDLE;
  rc = DosLoadModule(NULL, 0, filename, &DLLHandle);
  if (rc!=NO_ERROR) return NULL;

  // Get the entry point
  rc = DosQueryProcAddr(DLLHandle, 0, "WaWE_GetPluginInfo", (PFN *) &pDLL_GetPluginInfo);
  if (rc==NO_ERROR)
  {
    result = (WaWEPlugin_p) dbg_malloc(sizeof(WaWEPlugin_t));
    if (!result)
    {
      DosFreeModule(DLLHandle);
      return NULL;
    }

    // Fill the info block about this dll:
    result->hmodDLL = DLLHandle;
    result->pPrev = NULL;
    result->pNext = NULL;

    // Then the info block (query from DLL)
    if (!((*pDLL_GetPluginInfo)(sizeof(WaWEPlugin_t), result, WAWE_APPVERSIONMAJOR, WAWE_APPVERSIONMINOR)))
    {
#ifdef DEBUG_BUILD
//      printf("[CreatePluginListElement] : GetPluginInfo() reported error!\n");
#endif
      dbg_free(result);
      DosFreeModule(DLLHandle);
      return NULL;
    }

    // Now initialize the plugin (if it requires it!)
    if (result->fnInitializePlugin)
    {
      FillPluginHelpersStructure(&PluginHelpers);
      if (!(result->fnInitializePlugin(hwndOwner, &PluginHelpers)))
      {
#ifdef DEBUG_BUILD
//        printf("[CreatePluginListElement] : fnInitializePlugin() reported error!\n");
#endif
        dbg_free(result);
        DosFreeModule(DLLHandle);
        return NULL;
      }
    }

#ifdef DEBUG_BUILD
//    printf("[CreatePluginListElement] : Plugin [%s] loaded, importance is %d\n",
//          filename, result->iPluginImportance);
#endif

    // Make sure the plugin does not overwrite these fields:
    result->hmodDLL = DLLHandle;
    result->pPrev = NULL;
    result->pNext = NULL;
  }
  // That was all for today.
  return result;
}

void LoadPluginsFunc(void *pParm)
{
  char achFileNamePath[CCHMAXPATHCOMP+10];
  HAB hab;
  HMQ hmq;
  HWND hwndDlg = (HWND) pParm;
  HWND hwndAboutPlugins;
  HWND hwndConfigurePlugins;
  HDIR hdirFindHandle;
  FILEFINDBUF3 FindBuffer;
  ULONG ulResultBufLen;
  ULONG ulFindCount;
  APIRET rc;
  WaWEPlugin_p NewPlugin;
  MENUITEM MenuItem;
  int iPluginCount;

  // Setup this thread for using PM (sending messages etc...)
  hab = WinInitialize(0);
  hmq = WinCreateMsgQueue(hab, 0);

#ifdef DEBUG_BUILD
  printf("Loading plugins...\n");
#endif
  WinSetDlgItemText(hwndDlg, ST_PLUGININFO, "Loading plugins...");

  if (DosRequestMutexSem(PluginListProtector_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    hdirFindHandle = HDIR_CREATE;
    ulResultBufLen = sizeof(FindBuffer);
    ulFindCount = 1;

    chdir("Plugins");

    rc = DosFindFirst( "*.DLL",  // File pattern
                       &hdirFindHandle,  // Search handle
                       FILE_NORMAL,      // Search attribute
                       &FindBuffer,      // Result buffer
                       ulResultBufLen,   // Result buffer length
                       &ulFindCount,     // Number of entries to find
                       FIL_STANDARD);    // Return level 1 file info

    if (rc == NO_ERROR)
    {
      do {
        sprintf(achFileNamePath, "%s", FindBuffer.achName);
#ifdef DEBUG_BUILD
        printf("  Check DLL [%s] : ", achFileNamePath); fflush(stdout);
#endif
        WinSetDlgItemText(hwndDlg, ST_PLUGININFO, achFileNamePath);

        NewPlugin = CreatePluginListElement(hwndDlg, achFileNamePath);
        if (NewPlugin)
        {
          NewPlugin->pNext = PluginListHead;
          if (PluginListHead)
            PluginListHead->pPrev = NewPlugin;
          PluginListHead = NewPlugin;
#ifdef DEBUG_BUILD
          printf("Found Plugin: %s\n", NewPlugin->achName); fflush(stdout);
#endif
        }
#ifdef DEBUG_BUILD
        else
          printf("Not a Plugin\n"); fflush(stdout);
#endif


        ulFindCount = 1; // Reset find count

        rc = DosFindNext( hdirFindHandle,  // Find handle
                          &FindBuffer,     // Result buffer
                          ulResultBufLen,  // Result buffer length
                          &ulFindCount);   // Number of entries to find
      } while (rc == NO_ERROR);

      // Close directory search handle
      rc = DosFindClose(hdirFindHandle);
    }
    chdir("..");
    DosReleaseMutexSem(PluginListProtector_Sem);
  }

#ifdef DEBUG_BUILD
  printf("Plugins loaded!\nConfiguring menu items...\n");
#endif

  // Ok, plugins loaded. Now set up the 'About' and 'Configure' menu items
  // of the main window!
  WinSetDlgItemText(hwndDlg, ST_PLUGININFO, "Configuring menu items...");

  if (DosRequestMutexSem(PluginListProtector_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    // Get hwnd of About->Plugins submenu!
    if (WinSendMsg(WinWindowFromID(hwndFrame, FID_MENU),
                   MM_QUERYITEM,
                   MPFROM2SHORT(IDM_ABOUTPLUGINS, TRUE),
                   (MPARAM) (&MenuItem)))
    {
      hwndAboutPlugins = MenuItem.hwndSubMenu;
    } else
      hwndAboutPlugins = NULL;

    // Get hwnd of Configure->Plugins submenu!
    if (WinSendMsg(WinWindowFromID(hwndFrame, FID_MENU),
                   MM_QUERYITEM,
                   MPFROM2SHORT(IDM_CONFIGUREPLUGINS, TRUE),
                   (MPARAM) (&MenuItem)))
    {
      hwndConfigurePlugins = MenuItem.hwndSubMenu;
    } else
      hwndConfigurePlugins = NULL;

    NewPlugin = PluginListHead;
    iPluginCount = 0;
    while (NewPlugin)
    {
      // Add to About->Plugins, if has About function
      if (NewPlugin->fnAboutPlugin)
      {
#ifdef DEBUG_BUILD
        printf("  Adding to About->Plugins: %s\n", NewPlugin->achName);
#endif
        MenuItem.iPosition = 0;
        MenuItem.afStyle = MIS_TEXT;
        MenuItem.afAttribute = 0;
        MenuItem.id = iPluginCount + ABOUTMENU_ID_BASE;
        MenuItem.hwndSubMenu = NULL;
        MenuItem.hItem = 0;
        WinSendMsg(hwndAboutPlugins,
                   MM_INSERTITEM,
                   (MPARAM) &MenuItem,
                   (MPARAM) (NewPlugin->achName));
      }
      // Add to Configure->Plugins, if has Configure function
      if (NewPlugin->fnConfigurePlugin)
      {
#ifdef DEBUG_BUILD
        printf("  Adding to Configure->Plugins: %s\n", NewPlugin->achName);
#endif
        MenuItem.iPosition = 0;
        MenuItem.afStyle = MIS_TEXT;
        MenuItem.afAttribute = 0;
        MenuItem.id = iPluginCount + CONFIGUREMENU_ID_BASE;
        MenuItem.hwndSubMenu = NULL;
        MenuItem.hItem = 0;
        WinSendMsg(hwndConfigurePlugins,
                   MM_INSERTITEM,
                   (MPARAM) &MenuItem,
                   (MPARAM) (NewPlugin->achName));
      }
      NewPlugin = NewPlugin->pNext; iPluginCount++;
    }
    DosReleaseMutexSem(PluginListProtector_Sem);
  }
#ifdef DEBUG_BUILD
  printf("Menu items configured!\n");
#endif


  WinPostMsg(hwndDlg, WM_CLOSE, 0, 0);

  WinDestroyMsgQueue(hmq);
  WinTerminate(hab);

#ifdef DEBUG_BUILD
  /*
  printf("The following plugins have been loaded:\n");
  if (DosRequestMutexSem(PluginListProtector_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    NewPlugin = PluginListHead;
    while (NewPlugin)
    {
      printf("  [%s] (type %d, importance %d)", NewPlugin->achName, NewPlugin->iPluginType, NewPlugin->iPluginImportance);
      if (NewPlugin->iPluginType == WAWE_PLUGIN_TYPE_IMPORT)
      {
        printf("  fnOpen() = 0x%p\n", NewPlugin->TypeSpecificInfo.ImportPluginInfo.fnOpen);
      } else
      {
        printf("\n");
      }
      NewPlugin = NewPlugin->pNext;
    }
    DosReleaseMutexSem(PluginListProtector_Sem);
  }
  */
#endif


  _endthread();
}

MRESULT EXPENTRY LoadingPluginsDlgProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  switch (msg)
  {
    case WM_CHAR:
      {
        // Filter out the ESC key, so it won't close our window.
	if ((SHORT1FROMMP(mp1) & KC_VIRTUALKEY) &&
	    (SHORT2FROMMP(mp2) == VK_ESC))
	{
	  return (MRESULT) TRUE;
        }
        break;
      }
  }
  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}

WaWEPlugin_p GetPluginByIndex(int iIndex)
{
  WaWEPlugin_p pResult = NULL;
  int i;

  if (DosRequestMutexSem(PluginListProtector_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    i = 0; pResult = PluginListHead;
    while ((pResult) && (i<iIndex))
    {
      i++;
      pResult = pResult->pNext;
    }

    DosReleaseMutexSem(PluginListProtector_Sem);
  }
  return pResult;
}

WaWEPlugin_p GetDefaultEditorPlugin()
{
  WaWEPlugin_p pResult = NULL;
  WaWEPlugin_p pTemp;

  // The default editor plugin is the edit plugin with the
  // greatest iPluginImportance value!

  if (DosRequestMutexSem(PluginListProtector_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    pTemp = PluginListHead;
    while (pTemp)
    {
      if (pTemp->iPluginType == WAWE_PLUGIN_TYPE_EDITOR)
      {
        if (!pResult) pResult = pTemp;
        else
          if (pResult->iPluginImportance < pTemp->iPluginImportance)
            pResult = pTemp;
      }
      pTemp = pTemp->pNext;
    }

    DosReleaseMutexSem(PluginListProtector_Sem);
  }
  return pResult;
}

WaWEPlugin_p GetImportPluginForFile(char *pchFileName)
{
  WaWEPlugin_p pResult = NULL;
  WaWEPlugin_p temp = NULL;
  WaWEImP_Open_Handle_p hFile;
  int iImportance;

  if (DosRequestMutexSem(PluginListProtector_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    // Go through all plugins, in order of importance,
    // and check them if they can open the file.
    // Return the first one, which reports success!
    for (iImportance = WAWE_PLUGIN_IMPORTANCE_MAX;
         iImportance >= WAWE_PLUGIN_IMPORTANCE_MIN;
         iImportance--)
    {
#ifdef DEBUG_BUILD
//      printf(" Imp=%d ", iImportance);
#endif

      temp = PluginListHead;
      while ((temp) && (!pResult))
      {
        if ((temp->iPluginType == WAWE_PLUGIN_TYPE_IMPORT) &&
            (temp->iPluginImportance == iImportance))
        {
#ifdef DEBUG_BUILD
//          printf("[GetImportPluginForFile] : Trying plugin [%s], importance %d\n",
//                 temp->achName, temp->iPluginImportance);
#endif
          hFile = temp->TypeSpecificInfo.ImportPluginInfo.fnOpen(pchFileName, WAWE_OPENMODE_TEST, hwndClient);
          if (hFile)
          {
            // Hey, found a plugin which can open it!
	    // Clean up what we have to
	    WaWEHelper_ChannelSetFormat_DeleteAllKeys(&(hFile->ChannelSetFormat));
            temp->TypeSpecificInfo.ImportPluginInfo.fnClose(hFile);
            pResult = temp;
          }
        }
        temp = temp->pNext;
      }
      if (pResult) break;
    }
    DosReleaseMutexSem(PluginListProtector_Sem);
  }
  return pResult;
}

WaWEPlugin_p GetExportPluginForFormat(WaWEExP_Create_Desc_p pFormatDesc)
{
  WaWEPlugin_p pResult = NULL;
  WaWEPlugin_p temp = NULL;
  int iImportance;

  if (DosRequestMutexSem(PluginListProtector_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    // Go through all plugins, in order of importance,
    // and check them if they can open the file.
    // Return the first one, which reports success!
    for (iImportance = WAWE_PLUGIN_IMPORTANCE_MAX;
         iImportance >= WAWE_PLUGIN_IMPORTANCE_MIN;
         iImportance--)
    {
      temp = PluginListHead;
      while ((temp) && (!pResult))
      {
        if ((temp->iPluginType == WAWE_PLUGIN_TYPE_EXPORT) &&
            (temp->iPluginImportance == iImportance))
        {
          if (temp->TypeSpecificInfo.ExportPluginInfo.fnIsFormatSupported(pFormatDesc))
          {
            // Hey, found a plugin which can write in this format!
            pResult = temp;
          }
        }
        temp = temp->pNext;
      }
      if (pResult) break;
    }
    DosReleaseMutexSem(PluginListProtector_Sem);
  }
  return pResult;
}

int Initialize_PluginList()
{
  int rc;

  if (PluginListProtector_Sem != -1) return TRUE;

  PluginListHead = NULL;

  rc = DosCreateMutexSem(NULL, &PluginListProtector_Sem, 0, FALSE);
  return (rc == NO_ERROR);
}

void PrepareAllPluginsForShutdown()
{
  WaWEPlugin_p temp;

#ifdef DEBUG_BUILD
//  printf("[PrepareAllPluginsForShutdown] : Calling fnPrepareForShutdown for plugins.\n");
#endif

  if (DosRequestMutexSem(PluginListProtector_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    temp = PluginListHead;
    while (temp)
    {
      if (temp->fnPrepareForShutdown)
        temp->fnPrepareForShutdown();
      temp = temp->pNext;
    }
    DosReleaseMutexSem(PluginListProtector_Sem);
  }
}


void UninitializeAllPlugins(HWND hwndOwner)
{
  WaWEPlugin_p temp;

#ifdef DEBUG_BUILD
  printf("[UninitializeAllPlugins] : Calling fnUninitialize for plugins.\n");
#endif

  if (DosRequestMutexSem(PluginListProtector_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    temp = PluginListHead;
    while (temp)
    {
      if (temp->fnUninitializePlugin)
        temp->fnUninitializePlugin(hwndOwner);
      temp = temp->pNext;
    }
    DosReleaseMutexSem(PluginListProtector_Sem);
  }
}

void ErasePluginList()
{
  WaWEPlugin_p temp;

  if (DosRequestMutexSem(PluginListProtector_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    while (PluginListHead)
    {
      temp = PluginListHead;
      PluginListHead=PluginListHead->pNext;
      DosFreeModule(temp->hmodDLL);
      dbg_free(temp);
    }
    DosReleaseMutexSem(PluginListProtector_Sem);
  }
}

void Uninitialize_PluginList()
{
  if (PluginListProtector_Sem==-1) return;

  UninitializeAllPlugins(HWND_DESKTOP);
  ErasePluginList();

  DosCloseMutexSem(PluginListProtector_Sem); PluginListProtector_Sem = -1;
}
