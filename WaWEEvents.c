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
 * Code dealing with list of event callbacks                                 *
 *****************************************************************************/

#include "WaWE.h"

typedef struct WaWEEventCallbackList_t_struct {
  pfn_WaWEP_EventCallback pEventCallback;  // The callback to call
  int                     iEventCode;      // and the event to call the callback for.
  void                   *pNext;
} WaWEEventCallbackList_t, *WaWEEventCallbackList_p;


static HMTX CallbackListProtector_Sem = -1;
static WaWEEventCallbackList_p CallbackListHead = NULL;

int Initialize_EventCallbackList()
{
  int rc;

  if (CallbackListProtector_Sem != -1) return TRUE;

  CallbackListHead = NULL;

  rc = DosCreateMutexSem(NULL, &CallbackListProtector_Sem, 0, FALSE);
  return (rc == NO_ERROR);
}

void Uninitialize_EventCallbackList()
{
  WaWEEventCallbackList_p temp;

  if (CallbackListProtector_Sem==-1) return;

  if (DosRequestMutexSem(CallbackListProtector_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    while (CallbackListHead)
    {
      temp = CallbackListHead;
#ifdef DEBUG_BUILD
      printf("[Uninitialize_EventCallbackList] : (Plugin fault?) Destroying registered callback: 0x%p for 0x%x events\n",
             temp->pEventCallback, temp->iEventCode);
#endif
      CallbackListHead=CallbackListHead->pNext;
      dbg_free(temp);
    }
    DosReleaseMutexSem(CallbackListProtector_Sem);
  }

  DosCloseMutexSem(CallbackListProtector_Sem); CallbackListProtector_Sem = -1;
}

int SetEventCallback(int iEventCode, pfn_WaWEP_EventCallback pEventCallback)
{
  int iResult = 1;
  WaWEEventCallbackList_p pTempEvent, pPrev;

  if (!pEventCallback) return 0;

  if (DosRequestMutexSem(CallbackListProtector_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    pPrev = NULL;
    pTempEvent = CallbackListHead;
    while ((pTempEvent) && (pTempEvent->pEventCallback!=pEventCallback))
    {
      pPrev = pTempEvent;
      pTempEvent = pTempEvent->pNext;
    }

    if (iEventCode == WAWE_EVENT_NONE)
    {
      // The callback has to be removed from the list!
      if (pTempEvent)
      {
        // Fine, found in list, so remove it from there!
        if (pPrev)
          pPrev->pNext = pTempEvent->pNext;
        else
          CallbackListHead = pTempEvent->pNext;

        dbg_free(pTempEvent);
      } else
      {
#ifdef DEBUG_BUILD
        printf("[SetEventCallback] : Could not remove event callback (0x%p), not found in list!\n", pEventCallback);
#endif
        iResult = 0;
      }
    } else
    {
      // The callback has to be changed or has to be registered!
      if (pTempEvent)
      {
        // This callback is already in the list, so
        // only change its subscribed event type!
        pTempEvent->iEventCode = iEventCode;
      } else
      {
        pTempEvent = (WaWEEventCallbackList_p) dbg_malloc(sizeof(WaWEEventCallbackList_t));
        if (!pTempEvent)
        {
#ifdef DEBUG_BUILD
          printf("[SetEventCallback] : Out of memory, event callback (0x%p) could not be registered!\n", pEventCallback);
#endif
          iResult = 0;
        } else
        {
          pTempEvent->iEventCode = iEventCode;
          pTempEvent->pEventCallback = pEventCallback;
          pTempEvent->pNext = CallbackListHead;
          CallbackListHead = pTempEvent;
        }
      }
    }

    DosReleaseMutexSem(CallbackListProtector_Sem);
  } else
    iResult = 0;

  return iResult;
}

void CallEventCallbacks(WaWEEventDesc_p pEventDescription)
{
  WaWEEventCallbackList_p pTempEvent;

  if (DosRequestMutexSem(CallbackListProtector_Sem, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    pTempEvent = CallbackListHead;
    while (pTempEvent)
    {
      if ((pTempEvent->iEventCode) & (pEventDescription->iEventType))
      {
        // This callback is registered for this event, so call it!
        (pTempEvent->pEventCallback)(pEventDescription);
      }

      pTempEvent = pTempEvent->pNext;
    }
    DosReleaseMutexSem(CallbackListProtector_Sem);
  }
}
