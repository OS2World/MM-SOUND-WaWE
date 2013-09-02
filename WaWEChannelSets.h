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

#ifndef WAWECHANNELSETS_H
#define WAWECHANNELSETS_H

extern HMTX             ChannelSetListProtector_Sem;
extern WaWEChannelSet_p ChannelSetListHead;
extern WaWEChannelSet_p pLastChannelSet;
extern int              iNextChannelSetID;
extern int              bChannelSetListChanged;

int Initialize_ChannelSetList();
void Uninitialize_ChannelSetList();

WaWEChannelSet_p GetChannelSetFromFileName(char *pchFileName);

WaWEChannelSet_p CreateChannelSetFromOpenHandle(char *pchFileName, WaWEPlugin_p pPlugin, WaWEImP_Open_Handle_p hFile, int bRedrawNow);
int GetNumOfChannelsFromChannelSet(WaWEChannelSet_p pChannelSet);
int DeleteChannelSet(WaWEChannelSet_p pChannelSet);

int GetNumOfModifiedChannelSets();

int SaveChannelSetToOpenHandle(WaWEChannelSet_p pChannelSet, WaWEPlugin_p pPlugin, void *hFile, HWND hwndSavingDlg);

int UpdateReOpenedChannelSet(WaWEChannelSet_p pChannelSet, int bRedrawNow);

#endif
