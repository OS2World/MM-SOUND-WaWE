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
 * Common header file for the WaWE application and the Plugins               *
 *****************************************************************************/

#ifndef WAWECOMMON_H
#define WAWECOMMON_H

#include <limits.h> // for LONG_MAX and LONG_MIN

// Define application name and version
#define WAWE_SHORTAPPNAME        "WaWE"
#define WAWE_LONGAPPNAME         "The Warped Wave Editor"
#define WAWE_APPVERSION          "0.78"
#define WAWE_APPVERSIONMAJOR     0
#define WAWE_APPVERSIONMINOR     78

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 Some pre-definitions
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

typedef struct WaWEChannel_t_struct *WaWEChannel_p;
typedef struct WaWEChannelSet_t_struct *WaWEChannelSet_p;
typedef struct WaWEState_t_struct *WaWEState_p;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 Now the real definitions
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// Position info: 64 bits signed
typedef __int64 WaWEPosition_t;
typedef WaWEPosition_t *WaWEPosition_p;

// Size info: 64 bits signed
typedef __int64 WaWESize_t;
typedef WaWESize_t *WaWESize_p;

// One sample: 32 bits signed
typedef long WaWESample_t;
typedef WaWESample_t *WaWESample_p;

// Maximum and minimum value of a WaWE sample
#define MAX_WAWESAMPLE  LONG_MAX
#define MIN_WAWESAMPLE  LONG_MIN

// Description of a PCM format:
typedef struct WaWEFormat_t_struct {
  int iFrequency;
  int iBits;
  int iIsSigned;
} WaWEFormat_t, *WaWEFormat_p;

// Description of a channel-set format is a string,
// which we will handle!
typedef char *  WaWEChannelSetFormat_t;
typedef char ** WaWEChannelSetFormat_p;

// Description of a region of selection:
typedef struct WaWESelectedRegion_t_struct {

  WaWEPosition_t Start;
  WaWEPosition_t End;

  void *pNext; // doubly linked list
  void *pPrev;
} WaWESelectedRegion_t, *WaWESelectedRegion_p;

// Plugin helpers and exported function calling conventions:
#define WAWECALL __syscall
#define WAWEDECLSPEC __declspec(dllexport)

// Description of WaWE events
#define WAWE_EVENT_NONE                            0x0000
#define WAWE_EVENT_ALL                             0xffff

#define WAWE_EVENT_POSTSAMPLEREAD                  0x0001
#define WAWE_EVENT_POSTSAMPLEWRITE                 0x0002
#define WAWE_EVENT_POSTSAMPLEINSERT                0x0004
#define WAWE_EVENT_POSTSAMPLEDELETE                0x0008

#define WAWE_EVENT_PRECHANNELDELETE                0x0010
#define WAWE_EVENT_POSTCHANNELDELETE               0x0020
#define WAWE_EVENT_POSTCHANNELCREATE               0x0040
#define WAWE_EVENT_POSTCHANNELCHANGE               0x0080

#define WAWE_EVENT_PRECHANNELSETDELETE             0x0100
#define WAWE_EVENT_POSTCHANNELSETDELETE            0x0200
#define WAWE_EVENT_POSTCHANNELSETCREATE            0x0400
#define WAWE_EVENT_POSTCHANNELSETCHANGE            0x0800
#define WAWE_EVENT_POSTCHANNELSETREADPACKEDSAMPLES 0x1000

#define WAWE_EVENT_POSTCHANNELPOSITIONCHANGE       0x2000
#define WAWE_EVENT_POSTCHANNELMOVE                 0x4000

typedef struct WaWEEventDesc_SampleMod_t_struct {
  WaWEChannel_p      pModifiedChannel;     // The channel in which the sample read/write/insert/delete occured
  WaWEPosition_t     ModificationStartPos; // The starting position of read/write/insert/delete
  WaWESize_t         ModificationSize;     // The number of bytes successfully read/written/inserted/deleted
} WaWEEventDesc_SampleMod_t, *WaWEEventDesc_SampleMod_p;

typedef struct WaWEEventDesc_ChannelMod_t_struct {
  WaWEChannel_p      pModifiedChannel;     // The channel that has been deleted/created/changed/moved
  WaWEChannelSet_p   pParentChannelSet;    // The channel-set of channel. This can be used for POSTCHANNELDELETE, to
                                           // get the channel-set from which the channel has been deleted.
} WaWEEventDesc_ChannelMod_t, *WaWEEventDesc_ChannelMod_p;

typedef struct WaWEEventDesc_ChannelSetMod_t_struct {
  WaWEChannelSet_p      pModifiedChannelSet;     // The channel-set that has been deleted/created/changed
} WaWEEventDesc_ChannelSetMod_t, *WaWEEventDesc_ChannelSetMod_p;

typedef struct WaWEEventDesc_ChannelSetReadPackedSamples_t_struct {
  WaWEChannelSet_p   pChannelSet;          // The channel-set in which the packed sample read occured
  WaWEPosition_t     StartPos;             // The starting position of read
  WaWESize_t         Size;                 // The number of samples successfully read
} WaWEEventDesc_ChannelSetReadPackedSamples_t, *WaWEEventDesc_ChannelSetReadPackedSamples_p;

typedef struct WaWEEventDesc_ChannelPositionChange_t_struct {
  WaWEChannel_p      pChannel;             // The channel in which the position has been changed
  WaWEPosition_t     OldCurrentPos;        // The original Current Position
  WaWEPosition_t     NewCurrentPos;        // The new Current Position
} WaWEEventDesc_ChannelPositionChange_t, *WaWEEventDesc_ChannelPositionChange_p;

typedef struct WaWEEventDesc_t_struct {
  int iEventType; // See WAWE_EVENT_* defines!

  // Event specific description:
  union {
    WaWEEventDesc_SampleMod_t                     SampleModInfo;
    WaWEEventDesc_ChannelMod_t                    ChannelModInfo;
    WaWEEventDesc_ChannelSetMod_t                 ChannelSetModInfo;
    WaWEEventDesc_ChannelSetReadPackedSamples_t   ChannelSetReadPackedSamplesInfo;
    WaWEEventDesc_ChannelPositionChange_t         ChannelPosChangeInfo;
  } EventSpecificInfo;

} WaWEEventDesc_t, *WaWEEventDesc_p;

// -----------------------------------------------
// --- WaWE Plugin Helper function definitions ---
// -----------------------------------------------

// First, the definition of the event callback function
typedef void WAWECALL (*pfn_WaWEP_EventCallback)(WaWEEventDesc_p pEventDescription);

// Here come the definitions of all Plugin Helpers
// currently available.

// Error codes returned by WaWEHelpers:
//   All went ok:
#define WAWEHELPER_NOERROR                       0
//   Some error, given buffer possibly not used
#define WAWEHELPER_ERROR_INVALIDPARAMETER        1
#define WAWEHELPER_ERROR_INTERNALERROR           2
//   Some error, given buffer possibly partially used
#define WAWEHELPER_ERROR_PARTIALDATA             3
#define WAWEHELPER_ERROR_OUTOFMEMORY             4
#define WAWEHELPER_ERROR_NODATA                  5
//   Error returned by FormatSpec functions
#define WAWEHELPER_ERROR_KEYNOTFOUND             6
#define WAWEHELPER_ERROR_BUFFERTOOSMALL          7
//   Error returned by Delete wawehelpers
#define WAWEHELPER_ERROR_ELEMENTINUSE            8

typedef int WAWECALL (*pfn_WaWEHelper_Channel_ReadSample)(WaWEChannel_p  pChannel,
                                                          WaWEPosition_t StartPos,
                                                          WaWESize_t     ReadSize,
                                                          WaWESample_p   pSampleBuffer,
                                                          WaWESize_p     pCouldReadSize);

typedef int WAWECALL (*pfn_WaWEHelper_Channel_WriteSample)(WaWEChannel_p  pChannel,
                                                           WaWEPosition_t StartPos,
                                                           WaWESize_t     WriteSize,
                                                           WaWESample_p   pSampleBuffer,
                                                           WaWESize_p     pCouldWriteSize);

typedef int WAWECALL (*pfn_WaWEHelper_Channel_InsertSample)(WaWEChannel_p  pChannel,
                                                            WaWEPosition_t StartPos,
                                                            WaWESize_t     InsertSize,
                                                            WaWESample_p   pSampleBuffer,
                                                            WaWESize_p     pCouldInsertSize);

typedef int WAWECALL (*pfn_WaWEHelper_Channel_DeleteSample)(WaWEChannel_p  pChannel,
                                                            WaWEPosition_t StartPos,
                                                            WaWESize_t     DeleteSize,
                                                            WaWESize_p     pCouldDeleteSize);

typedef int WAWECALL (*pfn_WaWEHelper_Channel_SetCurrentPosition)(WaWEChannel_p  pChannel,
                                                                  WaWEPosition_t NewCurrentPosition);

typedef int WAWECALL (*pfn_WaWEHelper_Channel_Changed)(WaWEChannel_p pChannel);

// Defines for pInsertAfter parameter of WaWEHelper_ChannelSet_CreateChannel:
#define WAWECHANNEL_TOP      ((WaWEChannel_p) -1)
#define WAWECHANNEL_BOTTOM   ((WaWEChannel_p) -2)
typedef int WAWECALL (*pfn_WaWEHelper_ChannelSet_CreateChannel)(WaWEChannelSet_p pChannelSet,
                                                                WaWEChannel_p  pInsertAfterThis,
                                                                char *pchProposedName,
                                                                WaWEChannel_p *ppChannel);

typedef int WAWECALL (*pfn_WaWEHelper_ChannelSet_DeleteChannel)(WaWEChannel_p pChannel);
typedef int WAWECALL (*pfn_WaWEHelper_ChannelSet_MoveChannel)(WaWEChannel_p pChannel,
                                                              WaWEChannel_p pMoveAfterThis);

typedef int WAWECALL (*pfn_WaWEHelper_ChannelSet_ReadPackedSamples)(WaWEChannelSet_p  pChannelSet,     // This fn reads samples from channelset (all channels)
                                                                    WaWEPosition_t    StartPos,        // in packed format, so in samples from different channels
                                                                    WaWESize_t        ReadSize,        // after each other. The output will *not* be WaWESample_t format,
                                                                    char             *pchSampleBuffer, // but converted to the given pRequestedFormat's iBits and iIsSigned
                                                                    WaWEFormat_p      pRequestedFormat,// format!
                                                                    WaWESize_p        pCouldReadSize);

typedef int WAWECALL (*pfn_WaWEHelper_ChannelSet_Changed)(WaWEChannelSet_p pChannelSet);

typedef int WAWECALL (*pfn_WaWEHelper_ChannelSet_StartChSetFormatUsage)(WaWEChannelSet_p  pChannelSet);

typedef int WAWECALL (*pfn_WaWEHelper_ChannelSet_StopChSetFormatUsage)(WaWEChannelSet_p  pChannelSet, int iChanged);

typedef int WAWECALL (*pfn_WaWEHelper_ChannelSetFormat_WriteKey)(WaWEChannelSetFormat_p pChannelSetFormat,
                                                                 char *pchKeyName,
                                                                 char *pchKeyValue);

typedef int WAWECALL (*pfn_WaWEHelper_ChannelSetFormat_GetKeySize)(WaWEChannelSetFormat_p pChannelSetFormat,
                                                                   char *pchKeyName,
                                                                   int iKeyNum,
                                                                   int *piKeyValueSize);

typedef int WAWECALL (*pfn_WaWEHelper_ChannelSetFormat_ReadKey)(WaWEChannelSetFormat_p pChannelSetFormat,
                                                                char *pchKeyName,
                                                                int iKeyNum,
                                                                char *pchResultValueBuff,
                                                                int iResultValueBuffSize);

typedef int WAWECALL (*pfn_WaWEHelper_ChannelSetFormat_DeleteKey)(WaWEChannelSetFormat_p pChannelSetFormat,
                                                                  char *pchKeyName,
                                                                  int iKeyNum);

typedef int WAWECALL (*pfn_WaWEHelper_ChannelSetFormat_DeleteKeysStartingWith)(WaWEChannelSetFormat_p pChannelSetFormat,
                                                                               char *pchKeyName);

typedef int WAWECALL (*pfn_WaWEHelper_ChannelSetFormat_DeleteKeysAndSubkeys)(WaWEChannelSetFormat_p pChannelSetFormat,
                                                                             char *pchKeyName);

typedef int WAWECALL (*pfn_WaWEHelper_ChannelSetFormat_DeleteAllKeys)(WaWEChannelSetFormat_p pChannelSetFormat);

typedef int WAWECALL (*pfn_WaWEHelper_ChannelSetFormat_GetNumOfKeys)(WaWEChannelSetFormat_p pChannelSetFormat, int *piNumOfKeys);

typedef int WAWECALL (*pfn_WaWEHelper_ChannelSetFormat_GetKeySizeByPosition)(WaWEChannelSetFormat_p pChannelSetFormat,
                                                                             int iKeyPosition,
                                                                             int *piKeyNameSize,
									     int *piKeyValueSize);

typedef int WAWECALL (*pfn_WaWEHelper_ChannelSetFormat_ReadKeyByPosition)(WaWEChannelSetFormat_p pChannelSetFormat,
                                                                          int iKeyPosition,
                                                                          char *pchResultNameBuff,
									  int iResultNameBuffSize,
									  char *pchResultValueBuff,
									  int iResultValueBuffSize);

typedef int WAWECALL (*pfn_WaWEHelper_ChannelSetFormat_DeleteKeyByPosition)(WaWEChannelSetFormat_p pChannelSetFormat,
                                                                            int iKeyPosition);

typedef int WAWECALL (*pfn_WaWEHelper_ChannelSetFormat_ModifyKeyByPosition)(WaWEChannelSetFormat_p pChannelSetFormat,
                                                                            int iKeyPosition,
                                                                            char *pchNewName,
                                                                            char *pchNewValue);

typedef int WAWECALL (*pfn_WaWEHelper_ChannelSetFormat_MoveKey)(WaWEChannelSetFormat_p pChannelSetFormat,
								int iOldKeyPosition,
                                                                int iNewKeyPosition);


typedef int WAWECALL (*pfn_WaWEHelper_Global_CreateChannelSet)(WaWEChannelSet_p *ppChannelSet,
                                                               WaWEFormat_p      pOriginalFormat,
                                                               int               iNumOfChannels,
                                                               char             *pchProposedName,
                                                               char             *pchProposedFileName);

typedef int WAWECALL (*pfn_WaWEHelper_Global_DeleteChannelSet)(WaWEChannelSet_p pChannelSet);

typedef int WAWECALL (*pfn_WaWEHelper_Global_DeleteChannelSetEx)(WaWEChannelSet_p pChannelSet, int bClosePluginFile, int bRedrawNow);

// Defines for pInsertAfter parameter of WaWEHelper_Global_MoveChannelSet:
#define WAWECHANNELSET_TOP      ((WaWEChannelSet_p) -1)
#define WAWECHANNELSET_BOTTOM   ((WaWEChannelSet_p) -2)
typedef int WAWECALL (*pfn_WaWEHelper_Global_MoveChannelSet)(WaWEChannelSet_p pChannelSet, WaWEChannelSet_p pInsertAfter, int bRedrawNow);

typedef int WAWECALL (*pfn_WaWEHelper_Global_SetEventCallback)(pfn_WaWEP_EventCallback pfnEventCallback, int iEventType);

// The following call can be used by threads running in the background, so if they
// modify something, they can notify WaWE to update its screen to reflect the changes!
typedef int WAWECALL (*pfn_WaWEHelper_Global_RedrawChanges)(void);


// Structure describing all the available plugin helpers
typedef struct WaWEPluginHelpers_t_struct {

  // Functions related to channels:

  pfn_WaWEHelper_Channel_ReadSample           fn_WaWEHelper_Channel_ReadSample;
  pfn_WaWEHelper_Channel_WriteSample          fn_WaWEHelper_Channel_WriteSample;
  pfn_WaWEHelper_Channel_InsertSample         fn_WaWEHelper_Channel_InsertSample;
  pfn_WaWEHelper_Channel_DeleteSample         fn_WaWEHelper_Channel_DeleteSample;
  pfn_WaWEHelper_Channel_SetCurrentPosition   fn_WaWEHelper_Channel_SetCurrentPosition;
  pfn_WaWEHelper_Channel_Changed              fn_WaWEHelper_Channel_Changed;

  // Functions related to channel-sets:

  pfn_WaWEHelper_ChannelSet_CreateChannel     fn_WaWEHelper_ChannelSet_CreateChannel;
  pfn_WaWEHelper_ChannelSet_DeleteChannel     fn_WaWEHelper_ChannelSet_DeleteChannel;
  pfn_WaWEHelper_ChannelSet_MoveChannel       fn_WaWEHelper_ChannelSet_MoveChannel;

  pfn_WaWEHelper_ChannelSet_ReadPackedSamples fn_WaWEHelper_ChannelSet_ReadPackedSamples;
  pfn_WaWEHelper_ChannelSet_Changed           fn_WaWEHelper_ChannelSet_Changed;
  pfn_WaWEHelper_ChannelSet_StartChSetFormatUsage fn_WaWEHelper_ChannelSet_StartChSetFormatUsage;
  pfn_WaWEHelper_ChannelSet_StopChSetFormatUsage  fn_WaWEHelper_ChannelSet_StopChSetFormatUsage;

  // Functions related to channel-set format description:

  pfn_WaWEHelper_ChannelSetFormat_WriteKey                fn_WaWEHelper_ChannelSetFormat_WriteKey;
  pfn_WaWEHelper_ChannelSetFormat_GetKeySize              fn_WaWEHelper_ChannelSetFormat_GetKeySize;
  pfn_WaWEHelper_ChannelSetFormat_ReadKey                 fn_WaWEHelper_ChannelSetFormat_ReadKey;
  pfn_WaWEHelper_ChannelSetFormat_DeleteKey               fn_WaWEHelper_ChannelSetFormat_DeleteKey;
  pfn_WaWEHelper_ChannelSetFormat_DeleteKeysStartingWith  fn_WaWEHelper_ChannelSetFormat_DeleteKeysStartingWith;
  pfn_WaWEHelper_ChannelSetFormat_DeleteKeysAndSubkeys    fn_WaWEHelper_ChannelSetFormat_DeleteKeysAndSubkeys;
  pfn_WaWEHelper_ChannelSetFormat_DeleteAllKeys           fn_WaWEHelper_ChannelSetFormat_DeleteAllKeys;
  pfn_WaWEHelper_ChannelSetFormat_GetNumOfKeys            fn_WaWEHelper_ChannelSetFormat_GetNumOfKeys;
  pfn_WaWEHelper_ChannelSetFormat_GetKeySizeByPosition    fn_WaWEHelper_ChannelSetFormat_GetKeySizeByPosition;
  pfn_WaWEHelper_ChannelSetFormat_ReadKeyByPosition       fn_WaWEHelper_ChannelSetFormat_ReadKeyByPosition;
  pfn_WaWEHelper_ChannelSetFormat_DeleteKeyByPosition     fn_WaWEHelper_ChannelSetFormat_DeleteKeyByPosition;
  pfn_WaWEHelper_ChannelSetFormat_ModifyKeyByPosition     fn_WaWEHelper_ChannelSetFormat_ModifyKeyByPosition;
  pfn_WaWEHelper_ChannelSetFormat_MoveKey                 fn_WaWEHelper_ChannelSetFormat_MoveKey;

  // Functions above channel-sets:

  pfn_WaWEHelper_Global_CreateChannelSet      fn_WaWEHelper_Global_CreateChannelSet;
  pfn_WaWEHelper_Global_DeleteChannelSet      fn_WaWEHelper_Global_DeleteChannelSet;
  pfn_WaWEHelper_Global_DeleteChannelSetEx    fn_WaWEHelper_Global_DeleteChannelSetEx;
  pfn_WaWEHelper_Global_MoveChannelSet        fn_WaWEHelper_Global_MoveChannelSet;
  pfn_WaWEHelper_Global_SetEventCallback      fn_WaWEHelper_Global_SetEventCallback;
  pfn_WaWEHelper_Global_RedrawChanges         fn_WaWEHelper_Global_RedrawChanges;

} WaWEPluginHelpers_t, *WaWEPluginHelpers_p;

// --------------------------------------------
// --- Type specific description of plugins ---
// --------------------------------------------

// -- Some general Plugin functions --
// These functions are optional, but all the plugins can have them.

// About Plugin function
typedef void WAWECALL (*pfn_WaWEP_About)(HWND hwndOwner);
// Configure Plugin function
typedef void WAWECALL (*pfn_WaWEP_Configure)(HWND hwndOwner);
// Initialize Plugin function (should return 1 for success)
typedef int WAWECALL (*pfn_WaWEP_Initialize)(HWND hwndOwner,
                                             WaWEPluginHelpers_p pWaWEPluginHelpers);
// Prepare for shutdown Plugin function
typedef void WAWECALL (*pfn_WaWEP_PrepareForShutdown)(void);
// Uninitialize Plugin function
typedef void WAWECALL (*pfn_WaWEP_Uninitialize)(HWND hwndOwner);

//  -- Stuffs for Import Plugins --
typedef struct WaWEImP_Open_Handle_t_struct {
  int iFrequency;
  int iBits;
  int iIsSigned;
  int iChannels;

  char *pchProposedChannelSetName;  // The plugin can propose a name for channel-set
  char **apchProposedChannelName;   // and names for channels inside. All of these can be null.

  WaWEChannelSetFormat_t ChannelSetFormat;  // Format specific description. Used to find automatically a plugin
                                            // which can save this format. It will be set to NULL by WaWE, after
                                            // the file is opened, so use it only to report the format to WaWE,
                                            // and don't use it afterwards!
                                            // (Also, you don't have to free it at Close time, but you should
                                            // recreate it at reopen() time, to see what has changed.)

  int  bTempClosed;                 // TRUE if the file handle is TempClosed (see Import plugin functions)

  void *pPluginSpecific;
} WaWEImP_Open_Handle_t, *WaWEImP_Open_Handle_p;

#define WAWE_OPENMODE_NORMAL  0
#define WAWE_OPENMODE_TEST    1

#define WAWE_SEEK_SET 0
#define WAWE_SEEK_CUR 1
#define WAWE_SEEK_END 2

typedef WaWEImP_Open_Handle_p WAWECALL (*pfn_WaWEImP_Open)(char *pchFileName, int iOpenMode, HWND hwndOwner);
typedef WaWEPosition_t        WAWECALL (*pfn_WaWEImP_Seek)(WaWEImP_Open_Handle_p pHandle,
                                                           WaWEPosition_t Offset,
                                                           int iOrigin);
typedef WaWESize_t            WAWECALL (*pfn_WaWEImP_Read)(WaWEImP_Open_Handle_p pHandle,
                                                           char *pchBuffer,
                                                           WaWESize_t cBytes);
typedef long                  WAWECALL (*pfn_WaWEImP_Close)(WaWEImP_Open_Handle_p pHandle);
typedef long                  WAWECALL (*pfn_WaWEImP_TempClose)(WaWEImP_Open_Handle_p pHandle);
typedef long                  WAWECALL (*pfn_WaWEImP_ReOpen)(char *pchFileName,
                                                             WaWEImP_Open_Handle_p pHandle);

typedef struct WaWEImportPluginInfo_t_struct {

  pfn_WaWEImP_Open      fnOpen;         // Open/Test a given file
  pfn_WaWEImP_Seek      fnSeek;         // Seek in a file
  pfn_WaWEImP_Read      fnRead;         // Read decoded bytes from file
  pfn_WaWEImP_Close     fnClose;        // Close file handle
  pfn_WaWEImP_TempClose fnTempClose;    // Release the file, but don't close the open handle, but be prepared to reopen it using the same settings as before!
  pfn_WaWEImP_ReOpen    fnReOpen;       // Reopen the file handle using the old settings

} WaWEImportPluginInfo_t, *WaWEImportPluginInfo_p;

//  -- Stuffs for Export Plugins --
typedef struct WaWEExP_Create_Desc_t_struct {
  int iFrequency;
  int iBits;
  int iIsSigned;
  int iChannels;

  WaWEChannelSetFormat_t ChannelSetFormat;
} WaWEExP_Create_Desc_t, *WaWEExP_Create_Desc_p;

typedef long                  WAWECALL (*pfn_WaWEExP_IsFormatSupported)(WaWEExP_Create_Desc_p pFormatDesc);

typedef void *                WAWECALL (*pfn_WaWEExP_Create)(char *pchFileName,
                                                             WaWEExP_Create_Desc_p pFormatDesc,
                                                             HWND hwndOwner);
typedef WaWESize_t            WAWECALL (*pfn_WaWEExP_Write)(void *pHandle,
                                                            char *pchBuffer,
                                                            WaWESize_t cBytes);
typedef long                  WAWECALL (*pfn_WaWEExP_Close)(void *pHandle, WaWEChannelSet_p pSavedChannelSet);

typedef struct WaWEExportPluginInfo_t_struct {

  pfn_WaWEExP_IsFormatSupported  fnIsFormatSupported;
  pfn_WaWEExP_Create             fnCreate;
  pfn_WaWEExP_Write              fnWrite;
  pfn_WaWEExP_Close              fnClose;

} WaWEExportPluginInfo_t, *WaWEExportPluginInfo_p;

//  -- Stuffs for Editor Plugins --

typedef struct WaWEEdP_Draw_Desc_t_struct {
  // Description of image buffer to draw into
  HPS hpsImageBuffer;

  // Description of which part to (re)draw
  int iDrawAreaLeftX;
  int iDrawAreaRightX;

  // The channel to draw
  WaWEChannel_p pChannelDesc;

  // First and last sample to draw into area
  WaWEPosition_t FirstSamplePos;
  WaWEPosition_t LastSamplePos;

} WaWEEdP_Draw_Desc_t, *WaWEEdP_Draw_Desc_p;

typedef void WAWECALL (*pfn_WaWEEdP_Draw)(WaWEEdP_Draw_Desc_p pDrawDesc);

typedef struct WaWEEditorPluginInfo_t_struct {

  pfn_WaWEEdP_Draw fnDraw;

} WaWEEditorPluginInfo_t, *WaWEEditorPluginInfo_p;

//  -- Stuffs for Processor Plugins --

// Popup menu IDs:
#define WAWE_POPUPMENU_ID_ROOT    0

// Popup menu entry styles:
#define WAWE_POPUPMENU_STYLE_SUBMENU   0
#define WAWE_POPUPMENU_STYLE_MENUITEM  1

typedef struct WaWEPopupMenuEntries_t_struct {
  char        *pchEntryText;
  int          iEntryID;
  int          iEntryStyle;
} WaWEPopupMenuEntries_t, *WaWEPopupMenuEntries_p;


typedef int  WAWECALL (*pfn_WaWEProcP_GetPopupMenuEntries)(int iPopupMenuID,
                                                           WaWEState_p pWaWEState,
                                                           int *piNumOfPopupMenuEntries,
                                                           WaWEPopupMenuEntries_p *ppPopupMenuEntries);

typedef void WAWECALL (*pfn_WaWEProcP_FreePopupMenuEntries)(int iNumOfPopupMenuEntries,
                                                            WaWEPopupMenuEntries_p pPopupMenuEntries);

typedef int  WAWECALL (*pfn_WaWEProcP_DoPopupMenuAction)(int iPopupMenuID,
                                                         WaWEState_p pWaWEState,
                                                         HWND hwndOwner);

typedef struct WaWEProcessorPluginInfo_t_struct {

  // Functions to build the popup menu
  pfn_WaWEProcP_GetPopupMenuEntries   fnGetPopupMenuEntries;
  pfn_WaWEProcP_FreePopupMenuEntries  fnFreePopupMenuEntries;
  // Function to process something by selecting a popup menu entry
  // If it returns true, then the screen should be redrawn.
  pfn_WaWEProcP_DoPopupMenuAction     fnDoPopupMenuAction;

} WaWEProcessorPluginInfo_t, *WaWEProcessorPluginInfo_p;


// --------------------------------------
// --- General description of plugins ---
// --------------------------------------

#define WAWE_PLUGIN_TYPE_IMPORT                 0
#define WAWE_PLUGIN_TYPE_EXPORT                 1
#define WAWE_PLUGIN_TYPE_EDITOR                 2
#define WAWE_PLUGIN_TYPE_PROCESSOR              3

#define WAWE_PLUGIN_DESCRIPTION_LEN  1024
#define WAWE_PLUGIN_AUTHOR_LEN        128
#define WAWE_PLUGIN_NAME_LEN          128

#define WAWE_PLUGIN_IMPORTANCE_MIN      0
#define WAWE_PLUGIN_IMPORTANCE_MAX    100

typedef struct WaWEPlugin_t_struct {

  // These are filled by the plugin's GetPluginInfo fn:
  char  achName[WAWE_PLUGIN_NAME_LEN];

  int   iPluginImportance;

  int   iPluginType;

  // General plugin functions (can be null!):
  pfn_WaWEP_About               fnAboutPlugin;
  pfn_WaWEP_Configure           fnConfigurePlugin;
  pfn_WaWEP_Initialize          fnInitializePlugin;
  pfn_WaWEP_PrepareForShutdown  fnPrepareForShutdown;
  pfn_WaWEP_Uninitialize        fnUninitializePlugin;

  // Type specific plugin functions:
  union {
    WaWEImportPluginInfo_t                ImportPluginInfo;
    WaWEExportPluginInfo_t                ExportPluginInfo;
    WaWEEditorPluginInfo_t                EditorPluginInfo;
    WaWEProcessorPluginInfo_t             ProcessorPluginInfo;
  } TypeSpecificInfo;

  // These are filled by WaWE:

  HMODULE hmodDLL;  // Loaded DLL Handle
  void *pNext; // Plugins are doubly linked
  void *pPrev;

} WaWEPlugin_t, *WaWEPlugin_p;

// --------------------------------
// --- The main plugin function ---
// --------------------------------

// The WaWE Plugins have one function to export,
// and it must be called 'WaWE_GetPluginInfo'.
// It should return 1 for success and 0 for failure.

typedef WAWEDECLSPEC long WAWECALL (*pfn_WaWEP_GetPluginInfo)(long lPluginDescSize,
                                                              WaWEPlugin_p pPluginDesc,
                                                              int  iWaWEMajorVersion,
                                                              int  iWaWEMinorVersion);

// -------------------------------------
// --- Definition of WaWE structures ---
// -------------------------------------

// Description of one cache page

typedef struct WaWEChannelSetCachePage_t_struct
{
  WaWEPosition_t   PageNumber;          // Number of this page, or -1 if empty!
  unsigned int     uiNumOfBytesOnPage;  // Number of bytes in this page (0 if empty)
  unsigned char   *puchCachedData;      // (Null if empty)
} WaWEChannelSetCachePage_t, *WaWEChannelSetCachePage_p;

// Channel-set cache
typedef struct WaWEChannelSetCache_t_struct
{
  // There is no need to mutex semaphore to protect the cache from concurrent access,
  // as only the WaWEHelper_Channel_Read() function will use it, and it's already
  // serialized on ChunkList...

  int bUseCache; // TRUE if the cache is turned on and in use, FALSE if the channel-set is uncached.

  unsigned int uiCachePageSize;    // Size of one cache page
  unsigned int uiNumOfCachePages;  // Number of cache pages

  WaWESize_t   FileSize;           // Size of the whole file

  WaWEChannelSetCachePage_p  CachePages;  // uiNumOfCachePages cache pages, last used first.

} WaWEChannelSetCache_t, *WaWEChannelSetCache_p;

// One chunk:
#define WAWE_CHUNKTYPE_ORIGINAL   0
#define WAWE_CHUNKTYPE_MODIFIED   1
typedef struct WaWEChunk_t_struct {

  // Type of chunk:
  int            iChunkType; // ORIGINAL or MODIFIED

  // Virtual position in list of chunks
  WaWEPosition_t VirtPosStart;
  WaWEPosition_t VirtPosEnd;

  // Fields used for ORIGINAL chunks:
  WaWEPlugin_p           pPlugin;      // Import plugin, used to read ORIGINAL chunks
  WaWEImP_Open_Handle_p  hFile;        // file handle for import plugin
  int                    iChannelNum;  // Number of channel of that file
  WaWEPosition_t         RealPosStart; // Starting position in that file

  // Fields used for MODIFIED chunks:
  WaWESample_p           pSampleBuffer; // Memory area, used to store data for MODIFIED chunks

  // Linked-list management:
  void *pNext; // Chunks are doubly linked
  void *pPrev;
} WaWEChunk_t, *WaWEChunk_p;

// One channel:
//
// To avoid deadlocks in code, always request the mutex semaphores in this structure
// in the following order:
//
// - hmtxUseRequiredShownSelectedRegionList; (acquired/released by some WaWEHelpers)
// - hmtxUseShownSelectedRegionList;
// - hmtxUseLastValidSelectedRegionList;
// - hmtxUseImageBuffer;
// - hmtxUseChunkList (acquired/released by WaWEHelpers)
//
#define WAWE_CHANNEL_NAME_LEN   1024
typedef struct WaWEChannel_t_struct {

  USHORT         usStructureSize;    // This is here only to be able to pass a pointer to this structure
                                     // directly to WinCreateWindow() API, as that wants this (read PM*.INF files!)

  // The parent channel-set
  WaWEChannelSet_p pParentChannelSet;

  // The list of chunks
  WaWEChunk_p    pChunkListHead;
  HMTX           hmtxUseChunkList;

  // Virtual format of this channel
  // (Virtual, because it's always 32bits signed in practice,
  //  but can be saved in its format)
  // This field must have the same values as pParentChannelSet->OriginalFormat, and
  // it's here only as a mirror of that field!)
  WaWEFormat_t   VirtualFormat;

  // Name of channel
  char           achName[WAWE_CHANNEL_NAME_LEN];

  // ID of channel inside channelset
  int            iChannelID;

  // Length of channel in number of samples
  WaWESize_t     Length;

  // Current position inside this channel
  WaWEPosition_t CurrentPosition;

  // List of shown selected regions inside this channel
  HMTX           hmtxUseShownSelectedRegionList;  // Mutex semaphore to protect selected region list
  WaWESelectedRegion_p  pShownSelectedRegionListHead;
  WaWESize_t     ShownSelectedLength;  // Statistical data about selected length and
  int            iShownSelectedPieces; // number of selection pieces

  // List of required selected regions to be shown
  // The worker thread will redraw this, and set the ShownSelectedRegionList to this,
  // if asked for redrawing.
  HMTX           hmtxUseRequiredShownSelectedRegionList;
  WaWESelectedRegion_p  pRequiredShownSelectedRegionListHead;

  // List of last valid selected regions
  // When the user is about to select a region, this is the base region list to
  // which the selection is relative.
  HMTX           hmtxUseLastValidSelectedRegionList;
  WaWESelectedRegion_p  pLastValidSelectedRegionListHead;

  // Variables for implementing selected regions, manipulated by the
  // window function:
  int            bSelecting;              // Bool variable: we're currently selecting or not.
  WaWEPosition_t SelectionStartSamplePos; // Start position, where the user started selection (started dragging)

  // GUI-specific stuffs
  WaWEPlugin_p   pEditorPlugin;         // Plugin, used to draw the stuff
  HMTX           hmtxUseImageBuffer;    // Mutex semaphore to protect access to the following three:
  HPS            hpsCurrentImageBuffer; //  - Lastly rendered image, or a text of "Rendering..."
  WaWEPosition_t FirstSamplePos;        //  - Begin of viewable part, shown in lastly rendered image
  WaWEPosition_t LastSamplePos;         //  - End of viewable part, shown in lastly rendered image
  HWND           hwndEditAreaWindow;    // Window handle, which has this stuff
  int            bWindowCreated;        // True if the window is ready
  int            bRedrawNeeded;         // If the channel has to be redraw, it is set to 1

  int            bModified;             // True, if needs saving (so, modified)

  // The followings are used to redraw the channel. If they are the same
  // as their normal versions, no need to redraw. Otherwise we should redraw it according to these.
  HMTX           hmtxUseRequiredFields; // Semaphore to protect the fields form concurrent access
  WaWEPosition_t RequiredFirstSamplePos;
  WaWEPosition_t RequiredLastSamplePos;
  int            RequiredWidth;
  int            RequiredHeight;
  // The following is used to change current position marker
  WaWEPosition_t RequiredCurrentPosition;
  
  void *pNext; // Channels are doubly linked
  void *pPrev;
} WaWEChannel_t;

// A set of channels:
#define WAWE_CHANNELSET_NAME_LEN       1024
#define WAWE_CHANNELSET_FILENAME_LEN    512
typedef struct WaWEChannelSet_t_struct {

  USHORT         usStructureSize;    // This is here only to be able to pass a pointer to this structure
                                     // directly to WinCreateWindow() API, as that wants this (read PM*.INF files!)

  // List of channels inside
  HMTX           hmtxUseChannelList; // If you want to poke with channel list, make sure to get this semaphore first!
  WaWEChannel_p  pChannelListHead;   // This is the list of channels in the channel-set.

  WaWEFormat_t   OriginalFormat;   // The format of the Channel-set (has a mirror in pChannel->VirtualFormat)
  int            iNumOfChannels;   // Number of channels (so don't have to count pChannelListHead all the time)
  WaWESize_t     Length;           // Length of the longest channel inside (in sample number)

  char           achName[WAWE_CHANNELSET_NAME_LEN];
  char           achFileName[WAWE_CHANNELSET_FILENAME_LEN];

  WaWEPlugin_p           pOriginalPlugin;      // Import plugin, used to read ORIGINAL chunks, if created from file. NULL otherwise.
  WaWEImP_Open_Handle_p  hOriginalFile;        // file handle for import plugin, or NULL
  HMTX                   hmtxUseChannelSetCache; // Mutex semaphore to protect concurrent access to ChannelSetCache, and it's also used to make import plugin seek and read functions atomic together.
  WaWEChannelSetCache_t  ChannelSetCache;      // Cache descriptor for this Channel-set

  HMTX                   hmtxUseChannelSetFormat; // Mutex semaphore to protect concurrent access to ChannelSetFormat!
  WaWEChannelSetFormat_t ChannelSetFormat;     // Channel-Set original format, or NULL if unknown

  int            iChannelSetID;

  // GUI-specific stuffs
  HWND           hwndChannelSet;       // Window handle
  int            iHeight;              // Current height
  int            iWidth;               // Current width
  int            iYPos;                // Current Y position in hwndClient
  WaWESize_t     ZoomCount;            // Current zooming
  WaWESize_t     ZoomDenom;
  WaWEPosition_t FirstVisiblePosition; // First sample to show at the left side
  WaWEPosition_t PointerPosition;      // Current Pointer position
  int            bWorkWithChannelSet;  // Flag to show if channels are grouped or not
  int            bWindowCreated;       // True if the window is ready
  int            bRedrawNeeded;        // If a new channel has been created, or deleted, it is set to 1

  int            bModified;            // True, if needs saving (so, modified)

  long           lUsageCounter;        // If not zero, it's currently being used

  void *pNext; // Channel sets are doubly linked
  void *pPrev;
} WaWEChannelSet_t, *WaWEChannelSet_p;

// Structure, describing the state of WaWE screen.
// It's passed to Processor plugins.
typedef struct WaWEState_t_struct {

  WaWEChannelSet_p pCurrentChannelSet;  // The actual channelset
  WaWEChannel_p    pCurrentChannel;     // The actual channel in actual channelset
  // Please note, that pCurrentChannel can be NULL, if the user asked for the popup-menu
  // on the channel-set header!

  WaWEChannelSet_p pChannelSetListHead; // List of all the channelsets

} WaWEState_t, *WaWEState_p;


#endif
