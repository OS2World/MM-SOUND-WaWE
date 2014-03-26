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

#ifndef __PPLAYPLAYBACK_H__
#define __PPLAYPLAYBACK_H__

#define INCL_TYPES
#define INCL_WIN
#define INCL_GPI
#define INCL_DOSRESOURCES
#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_OS2MM
#define INCL_MCIOS2
#define INCL_MMIOOS2
#include <os2.h>
#include <os2me.h>     // DART stuff and MMIO stuff

#include "WaWECommon.h"

extern int bIsPlaying; // TRUE is currently playing back something, FALSE otherwise.

// This callback is called by StartPlayback, to initialize the
// reading of the stream.
typedef void *(*pfn_Open_callback)(void *pUserParm);

// This callback is called by the playback thread to get
// more samples to be played back.
typedef WaWESize_t (*pfn_Read_callback)(void *hStream,
                                        int   iChannel,
                                        WaWESize_t ulNumOfSamples,
                                        WaWESample_p pSampleBuffer);

// This callback is called by the playback thread when it has
// stopped the playback, so the resources can be freed.
typedef void (*pfn_Close_callback)(void *hStream);


int  StartPlayback(void              *pUserParm,
                   pfn_Open_callback  pfnOpen,
                   pfn_Read_callback  pfnRead,
                   pfn_Close_callback pfnClose,
                   int iFreq,
                   int iChannels);

int  StopPlayback();

#endif
