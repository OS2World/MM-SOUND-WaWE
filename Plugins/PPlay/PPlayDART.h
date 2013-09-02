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

#ifndef __PPLAYDART_H__
#define __PPLAYDART_H__

#include <stdlib.h>
#include <stdio.h>

#define INCL_TYPES
#define INCL_WIN
#define INCL_GPI
#define INCL_DOSRESOURCES
#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>

void DART_SetDefaultConfig();
void DART_LoadConfig(FILE *hFile);
void DART_SaveConfig(FILE *hFile);

MRESULT EXPENTRY fnDARTNotebookPage( HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2 );

typedef unsigned long (*pfn_ReadStream_callback)(unsigned long ulNumOfBytes,
                                                 void *pSampleBuffer);

typedef void (*pfn_PlaybackFinished_callback)();

int  DART_StartPlayback(int iFreq,
                        int iBits,
                        int iChannels,
                        pfn_ReadStream_callback pfnReadStream,
                        pfn_PlaybackFinished_callback pfnPlaybackFinished);

void DART_StopPlayback();

#endif
