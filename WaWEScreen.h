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

#ifndef WAWESCREEN_H
#define WAWESCREEN_H

// --- Exported variables ---

extern int iChannelSetHeaderHeight;
extern int iChannelHeaderHeight;
extern int iChannelEditAreaHeight;
extern int iDefaultFontHeight;
extern int iScrollBarHeight;
extern int iSizerHeight;

extern WaWEChannelSet_p pActiveChannelSet;

// --- Exported functions ---

void CreateNewChannelSetWindow(WaWEChannelSet_p pChannelSet);
void CreateNewChannelWindow(WaWEChannel_p pChannel, int iEditAreaHeight);

void UpdateChannelSelectionString(WaWEChannel_p pChannel);
void UpdateChannelPositionString(WaWEChannel_p pChannel);
void UpdateChannelSetLengthString(WaWEChannelSet_p pChannelSet);
void UpdateChannelSetPointerPositionString(WaWEChannelSet_p pChannelSet);
int UpdateChannelSetWindowSizes();
void UpdateChannelSetsByMainScrollbar();

MRESULT ProcessWMCHARMessage(MPARAM mp1, MPARAM mp2);
void SetActiveChannelSet(WaWEChannelSet_p pChannelSet);

int RedrawChangedChannelsAndChannelSets(); // This should be called from most places
int RedrawChangedChannelsAndChannelSets_PMThreadWorker(); // and this is the core, which is called from PM thread

int Initialize_Screen(HAB hab);
void Uninitialize_Screen();

// --- Defines for control IDs inside ChannelSet window
// We reserve Dialog IDs from 5000 to 5999 for channelset stuff
#define CHSET_DID_BASE                  5000
#define DID_CHANNELSET_HEADERBG         CHSET_DID_BASE +  0
#define DID_CHANNELSET_NAME             CHSET_DID_BASE +  1
#define DID_CHANNELSET_FORMAT           CHSET_DID_BASE +  2
#define DID_CHANNELSET_LENGTH           CHSET_DID_BASE +  3
#define DID_CHANNELSET_ZOOM             CHSET_DID_BASE +  4
#define DID_CHANNELSET_ZOOMIN           CHSET_DID_BASE +  5
#define DID_CHANNELSET_ZOOMOUT          CHSET_DID_BASE +  6
#define DID_CHANNELSET_ZOOMSET          CHSET_DID_BASE +  7
#define DID_CHANNELSET_POINTERPOSITION  CHSET_DID_BASE +  8
#define DID_CHANNELSET_WORKWITHCHSET    CHSET_DID_BASE +  9
#define DID_CHANNELSET_HORZSCROLL       CHSET_DID_BASE + 10
#define DID_CHANNELSET_SIZER            CHSET_DID_BASE + 11

#define DID_CHANNELSET_CH_HEADERBG_BASE    CHSET_DID_BASE +100
#define DID_CHANNELSET_CH_NAME_BASE        CHSET_DID_BASE +200
#define DID_CHANNELSET_CH_SELECTED_BASE    CHSET_DID_BASE +300
#define DID_CHANNELSET_CH_POSITION_BASE    CHSET_DID_BASE +400
#define DID_CHANNELSET_CH_EDITAREA_BASE    CHSET_DID_BASE +500

#endif
