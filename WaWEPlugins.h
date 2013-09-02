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

#ifndef WAWEPLUGINS_H
#define WAWEPLUGINS_H

// Base ID for menu items in main menu:

#define ABOUTMENU_ID_BASE     8000
#define CONFIGUREMENU_ID_BASE 9000


typedef struct WaWEPluginList_t_struct {
  WaWEPlugin_p  pPlugin;
  void         *pNext;
} WaWEPluginList_t, *WaWEPluginList_p;

int Initialize_PluginList();
void Uninitialize_PluginList();

WaWEPluginList_p CreatePluginList(int iPluginType);
void FreePluginList(WaWEPluginList_p pList);

WaWEPlugin_p GetImportPluginForFile(char *pchFileName);
WaWEPlugin_p GetExportPluginForFormat(WaWEExP_Create_Desc_p pFormatDesc);
WaWEPlugin_p GetPluginByIndex(int iIndex);
WaWEPlugin_p GetDefaultEditorPlugin();

void LoadPluginsFunc(void *pParm);
MRESULT EXPENTRY LoadingPluginsDlgProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 );

void PrepareAllPluginsForShutdown();
void UninitializeAllPlugins(HWND hwndOwner);

#endif
