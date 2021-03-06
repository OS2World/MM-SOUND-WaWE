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
 * Frame regulation library                                                  *
 *                                                                           *
 * Include file for global variables of FrameReg. module, to be included in  *
 * the DLLInstance structure definition.                                     *
 *****************************************************************************/

#ifndef WAWEFRAMEREGULATION_H
#define WAWEFRAMEREGULATION_H

extern ULONG ulInitTimeStamp;     // Timestamp, when the FrameRegulator has been initialized. Exported, so other threads can use it!
extern ULONG ulCurrTimeStamp;     // Current time, since boot of OS/2, in msecs. Exported, so other threads can use it!

// Usable functions:
// See the .c code for description!

int StartFrameRegulation(HEV hevSem, float flFPS);
void StopFrameRegulation();

void SetupFrameRegulation(float flFPS);
void Initialize_ulInitTimeStamp();

int Initialize_FrameRegulation();
void Uninitialize_FrameRegulation();


#endif
