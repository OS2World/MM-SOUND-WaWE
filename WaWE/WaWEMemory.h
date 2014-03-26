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
 * WaWEMemory : Debug version of memory handling functions to be able to     *
 *              detect memory leaks                                          *
 *                                                                           *
 * v1.0 - Initial version                                                    *
 *****************************************************************************/
#ifndef WAWEMEMORY_H
#define WAWEMEMORY_H

#define dbg_malloc(a) dbg_malloc_core(a, __FILE__, __LINE__, 0)
#define dbg_malloc_huge(a) dbg_malloc_core(a, __FILE__, __LINE__, 1)
void *dbg_malloc_core( unsigned int numbytes, char *pchSrcFile, int iLineNum, int bIsHuge );

#define dbg_free(a) dbg_free_core(a, __FILE__, __LINE__)
#define dbg_realloc(a,b) dbg_realloc_core(a, b, __FILE__, __LINE__)
void dbg_free_core( void *ptr, char *pchSrcFile, int iLineNum );
void *dbg_realloc_core( void *old_blk, size_t size, char *pchSrcFile, int iLineNum );

void dbg_ReportMemoryLeak();

int Initialize_MemoryUsageDebug();
void Uninitialize_MemoryUsageDebug();

extern long dbg_allocated_memory;
extern long dbg_max_allocated_memory;

#endif
