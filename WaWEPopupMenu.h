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

#ifndef WAWEPOPUPMENU_H
#define WAWEPOPUPMENU_H


// --- Defines for control IDs inside popup menu
// We reserve Dialog IDs from 6000 to 6999 for popup menu stuff
#define DID_POPUPMENU_FIRST             6000

#define DID_POPUPMENU_NEW               6000
#define DID_POPUPMENU_NEWCHANNEL        6001
#define DID_POPUPMENU_NEWCHANNELSET     6002

#define DID_POPUPMENU_DELETE            6003
#define DID_POPUPMENU_DELETECHANNEL     6004
#define DID_POPUPMENU_DELETECHANNELSET  6005

#define DID_POPUPMENU_FIRST_PLUGIN      6050

#define DID_POPUPMENU_LAST              6999

int Initialize_PopupMenu();
void Uninitialize_PopupMenu();

HWND CreatePopupMenu(WaWEChannel_p pCurrChannel, WaWEChannelSet_p pCurrChannelSet);
int GetPopupMenuItem(int iItemID,
                     WaWEPlugin_p *ppPlugin, int *piPluginEntryID,
                     WaWEChannelSet_p *ppCurrChannelSet, WaWEChannel_p *ppCurrChannel);
void DestroyPopupMenu(HWND hwndPopupMenu);

#endif
