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

#ifndef WAWEHELPERS_H
#define WAWEHELPERS_H

// The return codes and type definitions are in WaWECommon.h, because
// they have to be accessible from outside, from plugins too!

void FillPluginHelpersStructure(WaWEPluginHelpers_p pPluginHelpers);

// Functions related to channels:

int  WAWECALL WaWEHelper_Channel_ReadSample(WaWEChannel_p  pChannel,
                                            WaWEPosition_t StartPos,
                                            WaWESize_t     ReadSize,
                                            WaWESample_p   pSampleBuffer,
                                            WaWESize_p     pCouldReadSize);

int  WAWECALL WaWEHelper_Channel_WriteSample(WaWEChannel_p  pChannel,
                                             WaWEPosition_t StartPos,
                                             WaWESize_t     WriteSize,
                                             WaWESample_p   pSampleBuffer,
                                             WaWESize_p     pCouldWriteSize);

int  WAWECALL WaWEHelper_Channel_InsertSample(WaWEChannel_p  pChannel,
                                              WaWEPosition_t StartPos,
                                              WaWESize_t     InsertSize,
                                              WaWESample_p   pSampleBuffer,
                                              WaWESize_p     pCouldInsertSize);

int  WAWECALL WaWEHelper_Channel_DeleteSample(WaWEChannel_p  pChannel,
                                              WaWEPosition_t StartPos,
                                              WaWESize_t     DeleteSize,
                                              WaWESize_p     pCouldDeleteSize);

int WAWECALL WaWEHelper_Channel_SetCurrentPosition(WaWEChannel_p  pChannel,
                                                   WaWEPosition_t NewCurrentPosition);

int  WAWECALL WaWEHelper_Channel_Changed(WaWEChannel_p pChannel);

// Functions related to channel-sets:

int  WAWECALL WaWEHelper_ChannelSet_CreateChannel(WaWEChannelSet_p pChannelSet,
                                                  WaWEChannel_p    pInsertAfterThis,
                                                  char            *pchProposedName,
                                                  WaWEChannel_p   *ppChannel);

int  WAWECALL WaWEHelper_ChannelSet_DeleteChannel(WaWEChannel_p pChannel);

int  WAWECALL WaWEHelper_ChannelSet_MoveChannel(WaWEChannel_p pChannel,
                                                WaWEChannel_p pMoveAfterThis);

int  WAWECALL WaWEHelper_ChannelSet_ReadPackedSamples(WaWEChannelSet_p  pChannelSet,
                                                      WaWEPosition_t    StartPos,
                                                      WaWESize_t        ReadSize,
                                                      char             *pchSampleBuffer,
                                                      WaWEFormat_p      pRequestedFormat,
                                                      WaWESize_p        pCouldReadSize);

int  WAWECALL WaWEHelper_ChannelSet_Changed(WaWEChannelSet_p pChannelSet);

int  WAWECALL WaWEHelper_ChannelSet_StartChSetFormatUsage(WaWEChannelSet_p  pChannelSet);

int  WAWECALL WaWEHelper_ChannelSet_StopChSetFormatUsage(WaWEChannelSet_p  pChannelSet, int iChanged);


// Functions related to channel-set format description:

int  WAWECALL WaWEHelper_ChannelSetFormat_WriteKey(WaWEChannelSetFormat_p pChannelSetFormat,
                                                   char *pchKeyName,
                                                   char *pchKeyValue);

int  WAWECALL WaWEHelper_ChannelSetFormat_GetKeySize(WaWEChannelSetFormat_p pChannelSetFormat,
                                                     char *pchKeyName,
                                                     int iKeyNum,
                                                     int *piKeyValueSize);

int  WAWECALL WaWEHelper_ChannelSetFormat_ReadKey(WaWEChannelSetFormat_p pChannelSetFormat,
                                                  char *pchKeyName,
                                                  int iKeyNum,
                                                  char *pchResultValueBuff,
                                                  int iResultValueBuffSize);

int  WAWECALL WaWEHelper_ChannelSetFormat_DeleteKey(WaWEChannelSetFormat_p pChannelSetFormat,
                                                    char *pchKeyName,
                                                    int iKeyNum);

int  WAWECALL WaWEHelper_ChannelSetFormat_DeleteKeysStartingWith(WaWEChannelSetFormat_p pChannelSetFormat,
                                                                 char *pchKeyName);

int  WAWECALL WaWEHelper_ChannelSetFormat_DeleteKeysAndSubkeys(WaWEChannelSetFormat_p pChannelSetFormat,
                                                               char *pchKeyName);

int  WAWECALL WaWEHelper_ChannelSetFormat_DeleteAllKeys(WaWEChannelSetFormat_p pChannelSetFormat);

int  WAWECALL WaWEHelper_ChannelSetFormat_GetNumOfKeys(WaWEChannelSetFormat_p pChannelSetFormat, int *piNumOfKeys);

int  WAWECALL WaWEHelper_ChannelSetFormat_GetKeySizeByPosition(WaWEChannelSetFormat_p pChannelSetFormat,
                                                               int iKeyPosition,
                                                               int *piKeyNameSize,
                                                               int *piKeyValueSize);

int  WAWECALL WaWEHelper_ChannelSetFormat_ReadKeyByPosition(WaWEChannelSetFormat_p pChannelSetFormat,
                                                            int iKeyPosition,
                                                            char *pchResultNameBuff,
                                                            int iResultNameBuffSize,
                                                            char *pchResultValueBuff,
                                                            int iResultValueBuffSize);

int  WAWECALL WaWEHelper_ChannelSetFormat_DeleteKeyByPosition(WaWEChannelSetFormat_p pChannelSetFormat,
                                                              int iKeyPosition);

int  WAWECALL WaWEHelper_ChannelSetFormat_ModifyKeyByPosition(WaWEChannelSetFormat_p pChannelSetFormat,
                                                              int iKeyPosition,
                                                              char *pchNewName,
                                                              char *pchNewValue);

int  WAWECALL WaWEHelper_ChannelSetFormat_MoveKey(WaWEChannelSetFormat_p pChannelSetFormat,
                                                  int iOldKeyPosition,
                                                  int iNewKeyPosition);


// Functions above channel-sets:

int  WAWECALL WaWEHelper_Global_CreateChannelSet(WaWEChannelSet_p *ppChannelSet,
                                                 WaWEFormat_p      pOriginalFormat,
                                                 int               iNumOfChannels,
                                                 char             *pchProposedName,
                                                 char             *pchProposedFileName);

int  WAWECALL WaWEHelper_Global_DeleteChannelSet(WaWEChannelSet_p pChannelSet);

int  WAWECALL WaWEHelper_Global_DeleteChannelSetEx(WaWEChannelSet_p pChannelSet, int bClosePluginFile, int bRedrawNow);

int  WAWECALL WaWEHelper_Global_MoveChannelSet(WaWEChannelSet_p pChannelSet, WaWEChannelSet_p pInsertAfter, int bRedrawNow);

int  WAWECALL WaWEHelper_Global_RedrawChanges(void);

#endif
