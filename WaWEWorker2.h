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

#ifndef WAWEWORKER2_H
#define WAWEWORKER2_H

// --- Worker messages ---

// Shutdown:
#define WORKER2_MSG_SHUTDOWN                       0
//    No parameters needed for it

// Open/Import file:
#define WORKER2_MSG_OPEN                           1
typedef struct WaWEWorker2Msg_Open_t_struct {
  WaWEPlugin_p  pImportPlugin;
  char          achFileName[CCHMAXPATHCOMP];
} WaWEWorker2Msg_Open_t, *WaWEWorker2Msg_Open_p;

// Call a popup menu action proc
#define WORKER2_MSG_DOPOPUPMENUACTION              2
typedef struct WaWEWorker2Msg_DoPopupMenuAction_t_struct {
  WaWEPlugin_p  pPlugin;
  WaWEState_t   WaWEState;
  int           iItemID;
} WaWEWorker2Msg_DoPopupMenuAction_t, *WaWEWorker2Msg_DoPopupMenuAction_p;

// Save a given channel-set into a given export plugin file handle,
// and show progress in progress indicator window
#define WORKER2_MSG_SAVECHANNELSET                 3
#define TEMPFILENAME_LEN  512
typedef struct WaWEWorker2Msg_SaveChannelSet_t_struct {
  WaWEChannelSet_p pChannelSet;     // ChannelSet to save
  WaWEPlugin_p     pExportPlugin;   // Export plugin to use
  void            *hFile;           // Already opened file handle (opened with export plugin above!)
  char             achTempFileName[TEMPFILENAME_LEN]; // Temporary file name (with full path) to opened file handle
  char             achDestFileName[TEMPFILENAME_LEN]; // Destination file name (with full path) to rename temp file to
  HWND             hwndSavingDlg;   // Dialog window handle to Saving progress window
} WaWEWorker2Msg_SaveChannelSet_t, *WaWEWorker2Msg_SaveChannelSet_p;


// --- Worker-2 exported functions ---

int Initialize_Worker2();
int AddNewWorker2Msg(int iEventCode, int iEventParmSize, void *pEventParm);
void Uninitialize_Worker2();

#endif
