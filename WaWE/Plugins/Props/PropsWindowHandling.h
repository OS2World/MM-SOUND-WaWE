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

#ifndef __PROPSWINDOWHANDLING_H__
#define __PROPSWINDOWHANDLING_H__

typedef struct PropertiesWindowList_t_struct {
  WaWEChannelSet_p  pChannelSet;

  HWND              hwndOwner;
  HMODULE           hmodOurDLLHandle;

  HWND              hwndPropertiesWindow; // Main window handle to properties window
  HWND              hwndPropertiesNB;     // Window handle to the notebook inside the properties dialog window
  HWND              hwndGeneralNBP;       // Window handle to the General properties notebook page
  unsigned long     ulGeneralNBPID;       // Notebook Page ID
  HWND              hwndFormatCommonNBP;  // Window handle to the Format/Common properties notebook page
  unsigned long     ulFormatCommonNBPID;  // Notebook Page ID
  HWND              hwndFormatSpecificNBP; // Window handle to the Format/Specific properties notebook page
  unsigned long     ulFormatSpecificNBPID; // Notebook Page ID
  HWND              hwndChannelsNBP;      // Window handle to the Channels properties notebook page
  unsigned long     ulChannelsNBPID;      // Notebook Page ID

  TID               tidWindowThread;      // Thread ID of the thread processing window messages
  int               iWindowCreated;       // The thread reports its state in this variable

  char              achOldBitsPerSample[32];

  // Parameters to samplerate converter thread:
  HWND              hwndConvertSamplerateProgressWindow;
  int               bConvertInProgress;
  int               iSamplerateToConvertTo;
  int               bSamplerateConverterCancelRequest;
  int               bSamplerateConversionCancelled;
  TID               tidSamplerateConvertTID;
  void             *pNext;
} PropertiesWindowList_t, *PropertiesWindowList_p;

// This message has to be posted or sent to the notebook page to update its values:
#define WM_UPDATECONTROLVALUES    (WM_USER+123)

extern HMTX hmtxUseWindowList; // Mutex semaphore to protect window list from concurrent access!
extern PropertiesWindowList_p pWindowListHead;

int CreateNewPropertiesWindow(PropertiesWindowList_p pNewWindowDesc,
                              HMODULE hmodOurDLLHandle,
                              WaWEChannelSet_p pChannelSet,
                              HWND hwndOwner);

void DestroyPropertiesWindow(PropertiesWindowList_p pNewWindowDesc);

#endif
