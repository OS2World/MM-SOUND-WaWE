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

#ifndef __PPLAYCONFIG_H__
#define __PPLAYCONFIG_H__

#include <stdlib.h>
#include <stdio.h> // for FILE

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

// For subsystems
#define WM_SETDEFAULTVALUES   (WM_USER + 100)

// iDeviceToUse:
#define DEVICE_NOSOUND  -1
#define DEVICE_DART      0
#define DEVICE_UNIAUD    1

typedef struct _PluginConfiguration_struct
{
  // See DEVICE_* defines!
  int iDeviceToUse;

  // If the following is true, then the stuffs to be played back
  // will always be converted to the default audio output format,
  // for the cases when for example the output device doesn't support
  // 6 channels, or given samplerates.
  // If the following is not true, then the audio output device
  // will be initialized to match the format of the media to be played back.
  int bAlwaysConvertToDefaultConfig;

  // Default (standard) format to use
  int iDefaultFreq;
  int iDefaultBits;
  int iDefaultChannels;

  // Slow machine mode (don't depend on channel's current position, because that
  // might not be updated frequently enough!)
  int bSlowMachineMode;

} PluginConfiguration_t, *PluginConfiguration_p;

extern PluginConfiguration_t PluginConfiguration;


void LoadPluginConfig();
void SavePluginConfig();
void DoConfigureWindow(HWND hwndOwner, HMODULE hmodOurDLLHandle);
int  GetConfigFileEntry(FILE *hFile, char **ppchKey, char **ppchValue);

#endif
