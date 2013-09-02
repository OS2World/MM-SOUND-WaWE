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
 * Main header file                                                          *
 *****************************************************************************/

#ifndef WAWE_H
#define WAWE_H

// First include all needed standard header files
#include <stdlib.h>
#include <stdio.h>
#include <process.h>
#include <string.h>
#include <math.h>

// Then the needed OS/2 API stuffs
#define INCL_TYPES
#define INCL_WIN
#define INCL_WINSTDFILE
#define INCL_DOSRESOURCES
#define INCL_DOSMISC
#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_GPI
#define INCL_OS2MM
#define INCL_MMIOOS2
#define INCL_MMIO_CODEC
#include <os2.h>
#include <os2me.h>     // DART stuff and MMIO stuff
#include <mmioos2.h>   // mmioFOURCC

// Now the common support stuff
#include "WaWEMemory.h" // Memory leak detection

// Now include the WaWE defines/functions/structures
#include "WaWE-Resources.h"
#include "WaWECommon.h"      // Common defines and structures
#include "WaWEFrameRegulation.h"
#include "WaWEPlugins.h"     // Handling of plugins
#include "WaWEChannelSets.h" // Channel sets
#include "WaWEScreen.h"      // Screen update/redraw/handle functions
#include "WaWEWorker1.h"     // Worker1 thread stuff (Drawing)
#include "WaWEWorker2.h"     // Worker2 thread stuff (Opening/Saving/Plugin functions)
#include "WaWEHelpers.h"     // Helpers for managing channels/channel-sets
#include "WaWESelection.h"   // Selected region list handling routines
#include "WaWEPopupMenu.h"   // Popup Menu handling
#include "WaWEConfig.h"      // WaWE Configuration
#include "WaWEEvents.h"      // Event callback list handling

// Some macros to be used commonly, if needed:
#define SIGNEDSHORT1FROMMP(mp)      ((SHORT)(ULONG)(mp))
#define SIGNEDSHORT2FROMMP(mp)      ((SHORT)((ULONG)mp >> 16))

// Then a list of exported functions and global variables of WaWE.C

void ErrorMsg(char *Msg);

extern HWND hwndFrame;
extern HWND hwndClient;

extern WaWEChannelSet_p pActiveChannelSet;

extern unsigned long tidPM; // Thread-ID of PM Thread

extern char *WinStateSave_AppName;
extern char *WinStateSave_MainWindowKey;
extern char *WinStateSave_CreateNewChannelSetWindowKey;
extern char *WinStateSave_ZoomSetWindowKey;


// Messages to be *posted* to hwndClient:

// Load plugins
#define WM_LOADPLUGINS      (WM_USER+111)
// Create a ChannelSet window
#define WM_CREATECSWINDOW   (WM_USER+112)
// Create a Channel window
#define WM_CREATECWINDOW    (WM_USER+113)
// Destroy a (any kind of) window created by this thread
#define WM_DESTROYWINDOW  (WM_USER+114)
// Update window title
#define WM_UPDATEWINDOWTITLE (WM_USER+115)

// Redraw changed channels and channelsets
#define WM_REDRAWCHANGEDCHANNELSANDCHANNELSETS (WM_USER+116)

#endif
