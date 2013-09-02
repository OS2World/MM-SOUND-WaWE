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

#ifndef WAWEWORKER1_H
#define WAWEWORKER1_H

// --- Worker messages ---

// Shutdown:
#define WORKER1_MSG_SHUTDOWN                       0
//    No parameters needed for it

// Redraw a given part of a given channel
#define WORKER1_MSG_CHANNELDRAW                    1
typedef struct WaWEWorker1Msg_ChannelDraw_t_struct {
  WaWEChannel_p pChannel;
  int           bDontOptimize;
} WaWEWorker1Msg_ChannelDraw_t, *WaWEWorker1Msg_ChannelDraw_p;

// Set the new current position
#define WORKER1_MSG_SETCHANNELCURRENTPOSITION      2
typedef struct WaWEWorker1Msg_SetChannelCurrentPosition_t_struct {
  WaWEChannel_p pChannel;
} WaWEWorker1Msg_SetChannelCurrentPosition_t, *WaWEWorker1Msg_SetChannelCurrentPosition_p;

// Update the Shown Selection regions
#define WORKER1_MSG_UPDATESHOWNSELECTION           3
typedef struct WaWEWorker1Msg_UpdateShownSelection_t_struct {
  WaWEChannel_p pChannel;
} WaWEWorker1Msg_UpdateShownSelection_t, *WaWEWorker1Msg_UpdateShownSelection_p;

// Update channel-set size, position, length and other strings
// (called when time format is changed)
#define WORKER1_MSG_UPDATEPOSITIONANDSIZESTRINGS   4
//    No parameters needed for it

// --- Worker exported functions ---

int Initialize_Worker1();
int AddNewWorker1Msg(int iEventCode, int iEventParmSize, void *pEventParm);
void Uninitialize_Worker1();

// Misc function(s), used by other parts of the code:

int CalculateRegionIntersect(WaWEPosition_t Start1, WaWEPosition_t End1,
                             WaWEPosition_t Start2, WaWEPosition_t End2,
                             WaWEPosition_p pIntersectStart, WaWEPosition_p pIntersectEnd);

__int64 GCD(__int64 llValue1, __int64 llValue2);

// --- Worker exported variable(s) ---
#define WORKER1STATE_SHUTTEDDOWN   -1
#define WORKER1STATE_IDLE           0
#define WORKER1STATE_REDRAWING      1
#define WORKER1STATE_WORKINGOTHER   2
extern int iWorker1State;

#endif
