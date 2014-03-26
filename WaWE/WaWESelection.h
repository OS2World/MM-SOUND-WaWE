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

#ifndef WAWESELECTION_H
#define WAWESELECTION_H

#define WAWE_SELECTIONMODE_REPLACE       0
#define WAWE_SELECTIONMODE_ADD           1
#define WAWE_SELECTIONMODE_SUB           2
#define WAWE_SELECTIONMODE_XOR           3

void CreateRequiredSelectedRegionList(WaWEChannel_p pChannel,
                                      WaWEPosition_t BasePosition,
                                      WaWEPosition_t TargetPosition,
                                      int iSelectionMode);

void MakeRequiredRegionListLastValid(WaWEChannel_p pChannel);

#endif
