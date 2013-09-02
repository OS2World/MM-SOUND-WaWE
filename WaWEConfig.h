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
 * WaWE configuration module                                                 *
 *                                                                           *
 * This module contains the current configuration of WaWE, and the functions *
 * to modify it.                                                             *
 *****************************************************************************/

#ifndef WAWECONFIG_H
#define WAWECONFIG_H

#define WAWE_TIMEFORMAT_MSEC        0
#define WAWE_TIMEFORMAT_HHMMSSMSEC  1
#define WAWE_TIMEFORMAT_SAMPLES     2

typedef struct _WaWEConfiguration_struct
{
  // Settings of cache

  int bUseCache;                   // Cache ON/OFF
  unsigned int uiCachePageSize;    // Size of one cache page
  unsigned int uiNumOfCachePages;  // Number of cache pages

  // Interface settings
  int iTimeFormat;                 // Defaults to WAWE_TIMEFORMAT_MSEC

  // Remembering stuffs
  char achLastOpenFolder[1024];    // Storage for last open folder

} WaWEConfiguration_t, *WaWEConfiguration_p;

extern WaWEConfiguration_t WaWEConfiguration;

void LoadWaWEConfig();
void SaveWaWEConfig();
void DoWaWEConfigureWindow(HWND hwndOwner);

#endif
