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
 * Worker-2 thread                                                           *
 *****************************************************************************/

#include "WaWE.h"

TID tidWorker2;
HQUEUE hqWorker2MessageQueue;

void OpenFile(char *pchFileName, WaWEPlugin_p pPlugin)
{
  WaWEImP_Open_Handle_p hFile;

  if (!pPlugin)
  {
#ifdef DEBUG_BUILD
    printf("Automatic plugin selection.\n");
#endif
    pPlugin = GetImportPluginForFile(pchFileName);
#ifdef DEBUG_BUILD
    if (pPlugin)
      printf("Selected plugin: %s\n", pPlugin->achName);
#endif
  }
  if (!pPlugin)
  {
    ErrorMsg("There is no plugin for this file!");
  } else
  {
    hFile = pPlugin->TypeSpecificInfo.ImportPluginInfo.fnOpen(pchFileName, WAWE_OPENMODE_NORMAL, hwndClient);
    if (!hFile)
    {
      ErrorMsg("Selected plugin could not open file!");
    } else
    {
#ifdef DEBUG_BUILD
      printf("Plugin opened file. Creating Channel-set from it!\n");
#endif
      if (!CreateChannelSetFromOpenHandle(pchFileName, pPlugin, hFile, TRUE))
      {
#ifdef DEBUG_BUILD
        printf("Internal error: Could not create channel-set from Open handle!\n");
        printf("The plugin might have returned a silly Open handle... Cleaning up!\n");
#endif
        // The normal Close function usually gets the ChannelSetFormat field cleared,
        // so we clear it here. We don't trust the plugins in this case. :)
        WaWEHelper_ChannelSetFormat_DeleteAllKeys(&(hFile->ChannelSetFormat));
        // Close the handle.
        pPlugin->TypeSpecificInfo.ImportPluginInfo.fnClose(hFile);
      }
    }
  }
}

void DoPopupMenuAction(WaWEPlugin_p pPlugin, WaWEState_p pWaWEState, int iItemID)
{
//  WinEnableWindow(hwndFrame, FALSE);
  pPlugin->TypeSpecificInfo.ProcessorPluginInfo.fnDoPopupMenuAction(iItemID,
                                                                    pWaWEState,
                                                                    hwndFrame);

  // Check all the channels and channel-sets, and redraw what changed!
  RedrawChangedChannelsAndChannelSets();
//  WinEnableWindow(hwndFrame, TRUE);
}

void SaveChannelSet(WaWEChannelSet_p pChannelSet, WaWEPlugin_p pExportPlugin, void *hFile, HWND hwndSavingDlg, char *pchTempFileName, char *pchDestFileName)
{
  WaWEChannelSet_p pTempChannelSet, pNextChannelSet;
  WaWEChannel_p pTempChannel;
  char achTempBuf[1024];
  int rc;

#ifdef DEBUG_BUILD
  printf("[Worker-2] : SaveChannelSet() Enter\n");
#endif

  rc = SaveChannelSetToOpenHandle(pChannelSet,
                                  pExportPlugin,
				  hFile,
				  hwndSavingDlg);

  // Close the file
  pExportPlugin->TypeSpecificInfo.ExportPluginInfo.fnClose(hFile, pChannelSet);

  // Dismiss the Saving progress dialog
  WinDismissDlg(hwndSavingDlg, TRUE);

  if (!rc)
  {
    // Delete temporary file if exists
    DosDelete(pchTempFileName);
    ErrorMsg("Error while saving the file!");
  }
  else
  {
    // Ok, the temporary file is there!
    // Now, let's see if the resulting filename is already opened by us!
    // If it is, then delete that channel-set, and reopen it with this!

    // Get the ChannelSetListProtector_Sem!
    if (DosRequestMutexSem(ChannelSetListProtector_Sem, SEM_INDEFINITE_WAIT)!=NO_ERROR)
    {
#ifdef DEBUG_BUILD
      printf("[Worker-2] : SaveChannelSet(): Could not get mutex semaphore!\n"); fflush(stdout);
#endif
      // Delete temporary file if exists
      DosDelete(pchTempFileName);
      ErrorMsg("Internal error [Worker-2, SaveChannelSet(), @1]");
      return;
    }

    // Change the filename of the saved channel-set to the destination one, because
    // it is that one from now!
    snprintf(pChannelSet->achFileName, sizeof(pChannelSet->achFileName),
             pchDestFileName);

    // Close channels with this filename!
    pTempChannelSet = ChannelSetListHead;
    while (pTempChannelSet)
    {
      if ((!stricmp(pTempChannelSet->achFileName, pchDestFileName)) &&
          (pTempChannelSet->pOriginalPlugin))
      {
	// Found a channel-set with the same file, having an import plugin!
#ifdef DEBUG_BUILD
        int rc;
        printf("[Worker-2] : Calling TempClose for file [%s]\n", pTempChannelSet->achFileName); fflush(stdout);
        rc =
#endif
	pTempChannelSet->pOriginalPlugin->TypeSpecificInfo.ImportPluginInfo.fnTempClose(pTempChannelSet->hOriginalFile);
#ifdef DEBUG_BUILD
        printf("[Worker-2] : TempClose rc = 0x%x\n", rc);
#endif
      }

      pTempChannelSet = pTempChannelSet->pNext;
    }
    DosReleaseMutexSem(ChannelSetListProtector_Sem);

    // Delete destination file, if exists
    rc = DosDelete(pchDestFileName);
#ifdef DEBUG_BUILD
    if (rc!=NO_ERROR)
    {
      printf("Note: Error while deleting dest file! rc = 0x%x\n", rc);
    }
#endif
    // Rename temp file to new file
    rc = DosMove(pchTempFileName, pchDestFileName);
    if (rc!=NO_ERROR)
    {
      ErrorMsg("Error while renaming temporary file to target file!");
#ifdef DEBUG_BUILD
      printf("[Worker-2] : Was trying to rename [%s] to [%s], rc = 0x%x\n", pchTempFileName, pchDestFileName, rc);
#endif
    }

    // Re-open channel-set if we closed it!
    // Delete channel-set if reopen failed!

    // Get the ChannelSetListProtector_Sem!
    if (DosRequestMutexSem(ChannelSetListProtector_Sem, SEM_INDEFINITE_WAIT)!=NO_ERROR)
    {
#ifdef DEBUG_BUILD
      printf("[Worker-2] : SaveChannelSet(): Could not get mutex semaphore!\n"); fflush(stdout);
#endif
      // Delete temporary file if exists
      DosDelete(pchTempFileName);
      ErrorMsg("Internal error [Worker-2, SaveChannelSet(), @2]");
      return;
    }

    // ReOpen channels with this filename!
    pTempChannelSet = ChannelSetListHead;
    while (pTempChannelSet)
    {
      pNextChannelSet = pTempChannelSet->pNext;
      if ((!stricmp(pTempChannelSet->achFileName, pchDestFileName)) &&
          (pTempChannelSet->hOriginalFile) &&
          (pTempChannelSet->hOriginalFile->bTempClosed))
      {
	// Found a channel-set with the same file, having a file handle, which is tempclosed!
#ifdef DEBUG_BUILD
	printf("[Worker-2] : Calling ReOpen for file [%s]\n", pTempChannelSet->achFileName); fflush(stdout);
#endif
	rc = pTempChannelSet->pOriginalPlugin->TypeSpecificInfo.ImportPluginInfo.fnReOpen(pTempChannelSet->achFileName, pTempChannelSet->hOriginalFile);
	if (!rc)
        {
          WaWEPlugin_p pPlugin;
          WaWEImP_Open_Handle_p hFile;

#ifdef DEBUG_BUILD
          printf("[Worker-2] : Could not reopen file [%s]\n", pTempChannelSet->achFileName); fflush(stdout);
          printf("[Worker-2] : The channel-set format might have been changed. Trying with automatic plugin selection!\n"); fflush(stdout);
#endif
          pTempChannelSet->pOriginalPlugin->TypeSpecificInfo.ImportPluginInfo.fnClose(pTempChannelSet->hOriginalFile);

          rc = 1; // Assume success

#ifdef DEBUG_BUILD
          printf("[Worker-2] : Automatic plugin selection.\n");
#endif
          pPlugin = GetImportPluginForFile(pTempChannelSet->achFileName);
#ifdef DEBUG_BUILD
          if (pPlugin)
            printf("[Worker-2] : Selected plugin: %s\n", pPlugin->achName);
#endif
          if (!pPlugin)
          {
            rc = 0;
            //ErrorMsg("There is no plugin for this file!");
          } else
          {
            hFile = pPlugin->TypeSpecificInfo.ImportPluginInfo.fnOpen(pTempChannelSet->achFileName, WAWE_OPENMODE_NORMAL, HWND_DESKTOP);
            if (!hFile)
            {
              //ErrorMsg("Selected plugin could not open file!");
              rc = 0;
            } else
            {
#ifdef DEBUG_BUILD
              printf("[Worker-2] : Plugin opened file. Updating Channel-set with it!\n");
#endif
              rc = 1;
            }
          }

          pTempChannelSet->pOriginalPlugin = pPlugin;
          pTempChannelSet->hOriginalFile = hFile;
        }

        if (!rc)
        {
          sprintf(achTempBuf,
                  "Could not reopen channel-set file! (%s)\n"
                  "\n"
                  "Channel-set will be closed!", pTempChannelSet->achFileName);
	  ErrorMsg(achTempBuf);
	  DosReleaseMutexSem(ChannelSetListProtector_Sem);
	  WaWEHelper_Global_DeleteChannelSetEx(pTempChannelSet, TRUE, FALSE);
          DosRequestMutexSem(ChannelSetListProtector_Sem, SEM_INDEFINITE_WAIT);
          pNextChannelSet = ChannelSetListHead; // Start from all over again!
	} else
        {
          DosReleaseMutexSem(ChannelSetListProtector_Sem);
#ifdef DEBUG_BUILD
          printf("[Worker-2] : Updating reopened chset\n"); fflush(stdout);
#endif
          UpdateReOpenedChannelSet(pTempChannelSet, FALSE);
#ifdef DEBUG_BUILD
          printf("[Worker-2] : Updating reopened chset - DONE!\n"); fflush(stdout);
#endif
          DosRequestMutexSem(ChannelSetListProtector_Sem, SEM_INDEFINITE_WAIT);
          // Set it to non-modified, both the channel-set and the channels inside!
          pTempChannelSet->bModified = FALSE; // Just reloaded, so not modified.
          pTempChannel = pTempChannelSet->pChannelListHead;
          while (pTempChannel)
          {
            pTempChannel->bModified = FALSE;
            pTempChannel = pTempChannel->pNext;
          }
	}
      }

      pTempChannelSet = pNextChannelSet;
    }

    // Also, make sure that if it was a new channel-set, so we didn't have to reopen it,
    // it will be still marked as non-modified!
    pChannelSet->bModified = FALSE; // Just reloaded, so not modified.
    pTempChannel = pChannelSet->pChannelListHead;
    while (pTempChannel)
    {
      pTempChannel->bModified = FALSE;
      pTempChannel = pTempChannel->pNext;
    }
    // Make sure the channel-set names and other stuffs will be redrawn!
    pChannelSet->bRedrawNeeded = 1;

    DosReleaseMutexSem(ChannelSetListProtector_Sem);

    // Redraw everything that's changed!
    RedrawChangedChannelsAndChannelSets();
  }

#ifdef DEBUG_BUILD
  printf("[Worker-2] : SaveChannelSet() Leave\n");
#endif
}

void Worker2ThreadFunc(void *pParm)
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
    iEventCode = WORKER2_MSG_SHUTDOWN; // Set as default!
    rc = DosReadQueue(hqWorker2MessageQueue,
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
        case WORKER2_MSG_SHUTDOWN:
            {
#ifdef DEBUG_BUILD
              printf("[Worker-2] : Shutting down\n");
#endif
              // We don't have to do anything here.
              break;
            }
        case WORKER2_MSG_OPEN:
            {
              WaWEWorker2Msg_Open_p  pOpenMsgParm = pEventParm;
              if (pOpenMsgParm)
              {
#ifdef DEBUG_BUILD
                if (pOpenMsgParm->pImportPlugin)
                  printf("[Worker-2] : Opening file [%s] using plugin [%s]\n",
                         pOpenMsgParm->achFileName, pOpenMsgParm->pImportPlugin->achName);
                else
                  printf("[Worker-2] : Opening file [%s] using automatic plugin detection\n",
                         pOpenMsgParm->achFileName);
#endif
                OpenFile(pOpenMsgParm->achFileName, pOpenMsgParm->pImportPlugin);
                // Then free the message!
                dbg_free(pOpenMsgParm); pEventParm = NULL;
              }
              break;
            }
        case WORKER2_MSG_DOPOPUPMENUACTION:
          {
            WaWEWorker2Msg_DoPopupMenuAction_p pPopupMsgParm = pEventParm;
            if (pPopupMsgParm)
            {
              DoPopupMenuAction(pPopupMsgParm->pPlugin, &(pPopupMsgParm->WaWEState), pPopupMsgParm->iItemID);
              dbg_free(pPopupMsgParm); pEventParm = NULL;
	    }
            break;
          }
	case WORKER2_MSG_SAVECHANNELSET:
	  {
            WaWEWorker2Msg_SaveChannelSet_p pSaveChannelSetMsgParm = pEventParm;
            if (pSaveChannelSetMsgParm)
            {
              SaveChannelSet(pSaveChannelSetMsgParm->pChannelSet,
			     pSaveChannelSetMsgParm->pExportPlugin,
			     pSaveChannelSetMsgParm->hFile,
			     pSaveChannelSetMsgParm->hwndSavingDlg,
                             &(pSaveChannelSetMsgParm->achTempFileName[0]),
                             &(pSaveChannelSetMsgParm->achDestFileName[0]));
              dbg_free(pSaveChannelSetMsgParm); pEventParm = NULL;
	    }
            break;
          }
        default:
#ifdef DEBUG_BUILD
            printf("[Worker-2] : Unknown message: %d\n", iEventCode);
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
      printf("[Worker-2] : DosReadQueue error, rc=%d\n", rc);
#endif
  } while ((rc==NO_ERROR) && (iEventCode!=WORKER2_MSG_SHUTDOWN));

  // Uninitialize PM
  WinDestroyMsgQueue(hmq);
  WinTerminate(hab);
  _endthread();
}

int Initialize_Worker2()
{
  char achQueueName[100];
  int rc;
  PPIB pib;
  PTIB tib;

  DosGetInfoBlocks(&tib, &pib);
  sprintf(achQueueName, "\\QUEUES\\WaWE\PID%d\\Worker2MsgQ", pib->pib_ulpid);

#ifdef DEBUG_BUILD
//  printf("Creating mq for worker-2 thread: %s\n", achQueueName);
#endif

  rc = DosCreateQueue(&hqWorker2MessageQueue,
                      QUE_FIFO | QUE_CONVERT_ADDRESS,
                      achQueueName);
  if (rc!=NO_ERROR)
  {
#ifdef DEBUG_BUILD
    printf("Error: Could not create mq for worker-2 thread!\n");
#endif
    return 0;
  }

  // Start worker thread!
  tidWorker2 = _beginthread(Worker2ThreadFunc, 0, 1024*1024, NULL);
  // Set priority of Worker thread to low, so it won't hang PM updating...
  rc = DosSetPriority(PRTYS_THREAD, PRTYC_REGULAR, -30, tidWorker2);

#ifdef DEBUG_BUILD
  printf("Worker-2 thread created, tid=%d\n", tidWorker2, rc);
#endif

  return 1;
}

int AddNewWorker2Msg(int iEventCode, int iEventParmSize, void *pEventParm)
{
  int rc;
#ifdef DEBUG_BUILD
//  printf("[AddNewWorker2Msg] : EventCode = %d\n", iEventCode);
#endif

  rc = DosWriteQueue(hqWorker2MessageQueue,
                     iEventCode,
                     iEventParmSize,
                     pEventParm,
                     0L);
  return (rc == NO_ERROR);
}

void Uninitialize_Worker2()
{
  // Add new message of shutting down
  // Make sure it gets it!
  while (!AddNewWorker2Msg(WORKER2_MSG_SHUTDOWN, 0, NULL)) DosSleep(64);
  // Wait for it to die
  DosWaitThread(&tidWorker2, DCWW_WAIT);
  // Then close queue
  DosCloseQueue(hqWorker2MessageQueue);
}
