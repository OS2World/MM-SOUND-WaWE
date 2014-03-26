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
 * MPEG Audio export plugin                                                  *
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <process.h>
#include <string.h>
#include <limits.h>

#define INCL_TYPES
#define INCL_WIN
#define INCL_DOSRESOURCES
#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_GPI
#include <os2.h>

#include "lame.h"          // lame encoder interface
#include "WaWECommon.h"    // Common defines and structures
#include "XMPA-Resources.h" // Dlg window resources

// To be able to link vbrtag.obj, and use its structures, we have to
// include these internal lame stuffs:
#include "config.h"
#include "util.h"
#include "vbrtag.h"


#define MIN_TRACK 0
#define MAX_TRACK 9999

#define MIN_BITRATE 8
#define MAX_BITRATE 399

#define XMPA_ENCODINGMODE_CBR   0
#define XMPA_ENCODINGMODE_ABR   1
#define XMPA_ENCODINGMODE_VBR   2

#define XMPA_STEREOMODE_DEFAULT     -1
#define XMPA_STEREOMODE_STEREO       0
#define XMPA_STEREOMODE_JSTEREO      1
#define XMPA_STEREOMODE_DUALCHANNEL  2
#define XMPA_STEREOMODE_MONO         3
#define XMPA_STEREOMODE_FJSTEREO     4
//Note that XMPA_STEREOMODE_DUALCHANNEL is not supported by LAME

#define XMPA_VBRMODE_NEWALGORITHM  0
#define XMPA_VBRMODE_OLDALGORITHM  1

#define XMPA_WRITETAG_NONE         0
#define XMPA_WRITETAG_ID3V1        1
#define XMPA_WRITETAG_ID3V11       2

typedef struct XMPAPluginEncoderConfig_t_struct {

  int iEncodingMode; // See XMPA_ENCODINGMODE_* constants!

  // MPEG Frame types
  int bErrorProtection; // CRC for every frame?
  int bOriginal;        // Original flag
  int bCopyright;       // Copyright flag
  int bPrivate;         // Private flag

  // Other general settings
  int iStereoMode;      // See XMPA_STEREOMODE_* constants!
  int iAlgorithmQuality; // Algorithm quality setting (0..9)

  // Settings for CBR encoding
  int iCBRBitrate;

  // Settings for ABR encoding
  int iABRMeanBitrate;
  int bABRLimitMinBitrate;
  int iABRMinBitrate;
  int bABREnforceMinBitrate;
  int bABRLimitMaxBitrate;
  int iABRMaxBitrate;
  int bABRWriteXingVBRHeader;

  // Settings for VBR encoding
  int iVBRQuality;  // 0..9
  int iVBRMode;     // See XMPA_VBRMODE_* constants!
  int bVBRWriteXingVBRHeader;

  // ID3 TAG support (Only ID3v1.1 for now)
  int iWriteTAG; // See XMPA_WRITETAG_* constants!
  char achID3v1_SongTitle[31];
  char achID3v1_Artist[31];
  char achID3v1_Album[31];
  char achID3v1_Year[5];
  char achID3v1_Comment[30];
  char chID3v11_Track;
  char chID3v1_Genre;

} XMPAPluginEncoderConfig_t, *XMPAPluginEncoderConfig_p;

typedef struct XMPAHandle_t_struct {

  USHORT   cb;  // This is the size of structure, so it can be passed to WinLoadDlg()!

  XMPAPluginEncoderConfig_t  EncoderConfig;
  FILE *hFile;
  lame_global_flags *pLameHandle;

  WaWEExP_Create_Desc_t FileFormatDesc;

} XMPAHandle_t, *XMPAHandle_p;


// Global variables
// (Per process, because the DLL is initinstance/terminstance!)
HMODULE hmodOurDLLHandle;
WaWEPluginHelpers_t PluginHelpers;
char achUndoBuf[1024];

#define PLUGIN_NAME "MP3 Export plugin"
char *pchPluginName = PLUGIN_NAME;
char *pchAboutTitle = PLUGIN_NAME;
char *pchAboutMessage =
  "MP3 Export plugin v1.0 for WaWE\n"
  "Written by Doodle\n"
  "\n"
  "Using L.A.M.E. for encoding.\n"
  "\n"
  "Compilation date: "__DATE__" "__TIME__;


#define MAX_MODES 4
const char *modes[MAX_MODES] = { "Stereo", "Joint-Stereo", "Dual-Channel", "Single-Channel" };

// Tables from libmp3lame
#define MAX_GENRENAMES 149
static const char *const genre_names[MAX_GENRENAMES] =
{
    /*
     * NOTE: The spelling of these genre names is identical to those found in
     * Winamp and mp3info.
     */
    "Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge",
    "Hip-Hop", "Jazz", "Metal", "New Age", "Oldies", "Other", "Pop", "R&B",
    "Rap", "Reggae", "Rock", "Techno", "Industrial", "Alternative", "Ska",
    "Death Metal", "Pranks", "Soundtrack", "Euro-Techno", "Ambient", "Trip-Hop",
    "Vocal", "Jazz+Funk", "Fusion", "Trance", "Classical", "Instrumental",
    "Acid", "House", "Game", "Sound Clip", "Gospel", "Noise", "Alt. Rock",
    "Bass", "Soul", "Punk", "Space", "Meditative", "Instrumental Pop",
    "Instrumental Rock", "Ethnic", "Gothic", "Darkwave", "Techno-Industrial",
    "Electronic", "Pop-Folk", "Eurodance", "Dream", "Southern Rock", "Comedy",
    "Cult", "Gangsta Rap", "Top 40", "Christian Rap", "Pop/Funk", "Jungle",
    "Native American", "Cabaret", "New Wave", "Psychedelic", "Rave",
    "Showtunes", "Trailer", "Lo-Fi", "Tribal", "Acid Punk", "Acid Jazz",
    "Polka", "Retro", "Musical", "Rock & Roll", "Hard Rock", "Folk",
    "Folk/Rock", "National Folk", "Swing", "Fast-Fusion", "Bebob", "Latin",
    "Revival", "Celtic", "Bluegrass", "Avantgarde", "Gothic Rock",
    "Progressive Rock", "Psychedelic Rock", "Symphonic Rock", "Slow Rock",
    "Big Band", "Chorus", "Easy Listening", "Acoustic", "Humour", "Speech",
    "Chanson", "Opera", "Chamber Music", "Sonata", "Symphony", "Booty Bass",
    "Primus", "Porn Groove", "Satire", "Slow Jam", "Club", "Tango", "Samba",
    "Folklore", "Ballad", "Power Ballad", "Rhythmic Soul", "Freestyle", "Duet",
    "Punk Rock", "Drum Solo", "A Cappella", "Euro-House", "Dance Hall",
    "Goa", "Drum & Bass", "Club-House", "Hardcore", "Terror", "Indie",
    "BritPop", "Negerpunk", "Polsk Punk", "Beat", "Christian Gangsta Rap",
    "Heavy Metal", "Black Metal", "Crossover", "Contemporary Christian",
    "Christian Rock", "Merengue", "Salsa", "Thrash Metal", "Anime", "JPop",
    "Synthpop"
};

#define MAX_COMMONBITRATES 10
int aiCommonBitrates[MAX_COMMONBITRATES] =
{
  8, 32, 64, 80, 96, 128, 160, 192, 256, 320
};


// Helper functions, called from inside:

void ErrorMsg(char *Msg)
{
#ifdef DEBUG_BUILD
  printf("ErrorMsg: %s\n",Msg); fflush(stdout);
#endif
  WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                Msg, PLUGIN_NAME" Error", 0, MB_OK | MB_MOVEABLE | MB_ICONEXCLAMATION);
}

static PFNWP OldComboboxWndProc;
#define CBN_EFSETFOCUS  1000
#define CBN_EFKILLFOCUS 1001

MRESULT EXPENTRY NewComboboxWndProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  // Subclassed combobox window proc.
  // We pass SETFOCUS and KILLFOCUS messages to owner, that's the only
  // difference.
  if (msg==WM_CONTROL)
  {
    if (SHORT1FROMMP(mp1)==0x029b) // ID of ENTRY FIELD of COMBO BOX
    {
      if (SHORT2FROMMP(mp1)==EN_SETFOCUS)
	WinSendMsg(WinQueryWindow(hwnd, QW_OWNER),
		   WM_CONTROL,
		   MPFROM2SHORT(WinQueryWindowUShort(hwnd, QWS_ID), CBN_EFSETFOCUS),
		   (MPARAM) hwnd);
      if (SHORT2FROMMP(mp1)==EN_KILLFOCUS)
	WinSendMsg(WinQueryWindow(hwnd, QW_OWNER),
		   WM_CONTROL,
		   MPFROM2SHORT(WinQueryWindowUShort(hwnd, QWS_ID), CBN_EFKILLFOCUS),
                   (MPARAM) hwnd);
    }
  }

  return OldComboboxWndProc(hwnd, msg, mp1, mp2);
}

#ifdef DEBUG_BUILD
static void internal_PrintEncoderConfig(XMPAPluginEncoderConfig_p pConfig)
{
  printf("[internal_PrintEncoderConfig] : Configuration is the following\n");
  printf("iEncodingMode: %d\n", pConfig->iEncodingMode);
  printf("bErrorProtection: %d\n", pConfig->bErrorProtection);
  printf("bOriginal: %d\n", pConfig->bOriginal);
  printf("bCopyright: %d\n", pConfig->bCopyright);
  printf("bPrivate: %d\n", pConfig->bPrivate);
  printf("iStereoMode: %d\n", pConfig->iStereoMode);
  printf("iAlgorithmQuality: %d\n", pConfig->iAlgorithmQuality);
  printf("iCBRBitrate: %d\n", pConfig->iCBRBitrate);
  printf("iABRMeanBitrate: %d\n", pConfig->iABRMeanBitrate);
  printf("bABRLimitMinBitrate: %d\n", pConfig->bABRLimitMinBitrate);
  printf("iABRMinBitrate: %d\n", pConfig->iABRMinBitrate);
  printf("bABREnforceMinBitrate: %d\n", pConfig->bABREnforceMinBitrate);
  printf("bABRLimitMaxBitrate: %d\n", pConfig->bABRLimitMaxBitrate);
  printf("iABRMaxBitrate: %d\n", pConfig->iABRMaxBitrate);
  printf("bABRWriteXingVBRHeader: %d\n", pConfig->bABRWriteXingVBRHeader);
  printf("iVBRQuality: %d\n", pConfig->iVBRQuality);
  printf("iVBRMode: %d\n", pConfig->iVBRMode);
  printf("bVBRWriteXingVBRHeader: %d\n", pConfig->bVBRWriteXingVBRHeader);
  printf("iWriteTAG: %d\n", pConfig->iWriteTAG);
  printf("SongTitle: [%s]\n", pConfig->achID3v1_SongTitle);
  printf("Artist: [%s]\n", pConfig->achID3v1_Artist);
  printf("Album: [%s]\n", pConfig->achID3v1_Album);
  printf("Year: [%s]\n", pConfig->achID3v1_Year);
  printf("Comment: [%s]\n", pConfig->achID3v1_Comment);
  printf("Track: %d\n", pConfig->chID3v11_Track);
  printf("Genre: %d\n", pConfig->chID3v1_Genre);
}
#endif

static void internal_SetEncoderConfigFromFormatSpec(XMPAPluginEncoderConfig_p pConfig, WaWEChannelSetFormat_p pFormat)
{
  int rc;
  char achBuffer[1024];
  int i;

  // First the mpeg frame settings
  rc = PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKey(pFormat,
                                                            "MPEG Audio/Error Protection", 0,
                                                            achBuffer, sizeof(achBuffer));
  if (rc==WAWEHELPER_NOERROR)
    pConfig->bErrorProtection = (!stricmp(achBuffer, "Yes"));

  rc = PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKey(pFormat,
                                                            "original", 0,
                                                            achBuffer, sizeof(achBuffer));
  if (rc==WAWEHELPER_NOERROR)
    pConfig->bOriginal = (!stricmp(achBuffer, "Yes"));

  rc = PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKey(pFormat,
                                                            "copyright", 0,
                                                            achBuffer, sizeof(achBuffer));
  if (rc==WAWEHELPER_NOERROR)
    pConfig->bCopyright = 1;

  rc = PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKey(pFormat,
                                                            "MPEG Audio/Private Extension", 0,
                                                            achBuffer, sizeof(achBuffer));
  if (rc==WAWEHELPER_NOERROR)
    pConfig->bPrivate = (!stricmp(achBuffer, "Yes"));

  rc = PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKey(pFormat,
                                                            "MPEG Audio/Mode", 0,
                                                            achBuffer, sizeof(achBuffer));
  if (rc==WAWEHELPER_NOERROR)
  {
    int i;
    for (i=0; i<MAX_MODES; i++)
    {
      if (!stricmp(achBuffer, modes[i]))
        pConfig->iStereoMode = i;
    }
  }

  rc = PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKey(pFormat,
                                                            "bitrate", 0,
                                                            achBuffer, sizeof(achBuffer));
  if (rc==WAWEHELPER_NOERROR)
  {
    pConfig->iEncodingMode = XMPA_ENCODINGMODE_CBR;

    pConfig->iCBRBitrate =
      pConfig->iABRMeanBitrate = atoi(achBuffer) / 1000;
  }

  rc = PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKey(pFormat,
                                                            "VBR", 0,
                                                            achBuffer, sizeof(achBuffer));
  if (rc==WAWEHELPER_NOERROR)
  {
    if (!stricmp(achBuffer, "Yes"))
    {
      pConfig->iEncodingMode = XMPA_ENCODINGMODE_VBR;
      pConfig->bVBRWriteXingVBRHeader = 1;
    }
  }

  rc = PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKey(pFormat,
                                                            "bitrate/nominal bitrate", 0,
                                                            achBuffer, sizeof(achBuffer));
  if (rc==WAWEHELPER_NOERROR)
  {
    pConfig->iEncodingMode = XMPA_ENCODINGMODE_ABR;
    pConfig->iABRMeanBitrate = atoi(achBuffer) / 1000;
    pConfig->bABRWriteXingVBRHeader = 1;
  }

  rc = PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKey(pFormat,
                                                            "bitrate/upper bitrate", 0,
                                                            achBuffer, sizeof(achBuffer));
  if (rc==WAWEHELPER_NOERROR)
  {
    pConfig->iEncodingMode = XMPA_ENCODINGMODE_ABR;
    pConfig->bABRLimitMaxBitrate = 1;
    pConfig->iABRMaxBitrate = atoi(achBuffer) / 1000;
    pConfig->bABRWriteXingVBRHeader = 1;
  }

  rc = PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKey(pFormat,
                                                            "bitrate/lower bitrate", 0,
                                                            achBuffer, sizeof(achBuffer));
  if (rc==WAWEHELPER_NOERROR)
  {
    pConfig->iEncodingMode = XMPA_ENCODINGMODE_ABR;
    pConfig->bABRLimitMinBitrate = 1;
    pConfig->iABRMinBitrate = atoi(achBuffer) / 1000;
    pConfig->bABRWriteXingVBRHeader = 1;
  }

  rc = PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKey(pFormat,
                                                            "title", 0,
                                                            achBuffer, sizeof(achBuffer));
  if (rc==WAWEHELPER_NOERROR)
  {
    pConfig->iWriteTAG = XMPA_WRITETAG_ID3V1;
    strncpy(pConfig->achID3v1_SongTitle, achBuffer, sizeof(pConfig->achID3v1_SongTitle));
  }

  rc = PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKey(pFormat,
                                                            "artist", 0,
                                                            achBuffer, sizeof(achBuffer));
  if (rc==WAWEHELPER_NOERROR)
  {
    pConfig->iWriteTAG = XMPA_WRITETAG_ID3V1;
    strncpy(pConfig->achID3v1_Artist, achBuffer, sizeof(pConfig->achID3v1_Artist));
  }

  rc = PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKey(pFormat,
                                                            "album", 0,
                                                            achBuffer, sizeof(achBuffer));
  if (rc==WAWEHELPER_NOERROR)
  {
    pConfig->iWriteTAG = XMPA_WRITETAG_ID3V1;
    strncpy(pConfig->achID3v1_Album, achBuffer, sizeof(pConfig->achID3v1_Album));
  }

  rc = PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKey(pFormat,
                                                            "date", 0,
                                                            achBuffer, sizeof(achBuffer));
  if (rc==WAWEHELPER_NOERROR)
  {
    pConfig->iWriteTAG = XMPA_WRITETAG_ID3V1;
    strncpy(pConfig->achID3v1_Year, achBuffer, sizeof(pConfig->achID3v1_Year));
  }

  rc = PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKey(pFormat,
                                                            "comment", 0,
                                                            achBuffer, sizeof(achBuffer));
  if (rc==WAWEHELPER_NOERROR)
  {
    pConfig->iWriteTAG = XMPA_WRITETAG_ID3V1;
    strncpy(pConfig->achID3v1_Comment, achBuffer, sizeof(pConfig->achID3v1_Comment));
  }

  rc = PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKey(pFormat,
                                                            "genre", 0,
                                                            achBuffer, sizeof(achBuffer));
  if (rc==WAWEHELPER_NOERROR)
  {
    for (i=0; i<MAX_GENRENAMES; i++)
    {
      if (!stricmp(achBuffer, genre_names[i]))
      {
        pConfig->iWriteTAG = XMPA_WRITETAG_ID3V1;
        pConfig->chID3v1_Genre = i;
        break;
      }
    }
  }

  rc = PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKey(pFormat,
                                                            "track number", 0,
                                                            achBuffer, sizeof(achBuffer));
  if (rc==WAWEHELPER_NOERROR)
  {
    pConfig->iWriteTAG = XMPA_WRITETAG_ID3V11;
    pConfig->chID3v11_Track = atoi(achBuffer);
  }
}

static void internal_SetDefaultEncoderConfig(XMPAPluginEncoderConfig_p pConfig)
{
  if (!pConfig) return;

  memset(pConfig, 0, sizeof(XMPAPluginEncoderConfig_t));

  pConfig->iEncodingMode = XMPA_ENCODINGMODE_VBR;
  // MPEG Frame types
  pConfig->bErrorProtection = 0;
  pConfig->bOriginal = 0;
  pConfig->bCopyright = 0;
  pConfig->bPrivate = 0;
  pConfig->iStereoMode = XMPA_STEREOMODE_DEFAULT; // Let LAME choose the best.
  // Other general settings
  pConfig->iAlgorithmQuality = 2; // 5 is the default according to LAME docs, but 2 is the high quality
  // Settings for CBR encoding
  pConfig->iCBRBitrate = 128;
  // Settings for ABR encoding
  pConfig->iABRMeanBitrate = 128;
  pConfig->bABRLimitMinBitrate = 0;
  pConfig->iABRMinBitrate = 80;
  pConfig->bABREnforceMinBitrate = 0;
  pConfig->bABRLimitMaxBitrate = 0;
  pConfig->iABRMaxBitrate = 192;
  pConfig->bABRWriteXingVBRHeader = 1;
  // Settings for VBR encoding
  pConfig->iVBRQuality = 4;  // 4 is default, according to LAME docs
  pConfig->iVBRMode = XMPA_VBRMODE_NEWALGORITHM;
  pConfig->bVBRWriteXingVBRHeader = 1;
  // ID3 TAG support (Only ID3v1.1 for now)
  pConfig->iWriteTAG = XMPA_WRITETAG_NONE;
}

static void InitializeExportDialogControls(HWND hwnd, XMPAHandle_p pHandle)
{
  char achTemp[128];
  char *pchTemp;
  long lID, lIDToSelect;
  int i;
  ULONG ulTemp;

  if (!pHandle)
    return;

  // --- Encoding mode groupbox ---
  // Encoding mode combobox
  WinSendDlgItemMsg(hwnd, CX_ENCODINGMODE, LM_DELETEALL, (MPARAM) 0, (MPARAM) 0);
  lID = (long) WinSendDlgItemMsg(hwnd, CX_ENCODINGMODE, LM_INSERTITEM, (MPARAM) LIT_END,
				 (MPARAM) "CBR");
  WinSendDlgItemMsg(hwnd, CX_ENCODINGMODE, LM_SETITEMHANDLE, (MPARAM) lID,
		    (MPARAM) XMPA_ENCODINGMODE_CBR);
  lIDToSelect = lID;

  lID = (long) WinSendDlgItemMsg(hwnd, CX_ENCODINGMODE, LM_INSERTITEM, (MPARAM) LIT_END,
				 (MPARAM) "ABR");
  WinSendDlgItemMsg(hwnd, CX_ENCODINGMODE, LM_SETITEMHANDLE, (MPARAM) lID,
		    (MPARAM) XMPA_ENCODINGMODE_ABR);
  if (pHandle->EncoderConfig.iEncodingMode == XMPA_ENCODINGMODE_ABR) lIDToSelect = lID;
  lID = (long) WinSendDlgItemMsg(hwnd, CX_ENCODINGMODE, LM_INSERTITEM, (MPARAM) LIT_END,
				 (MPARAM) "VBR");
  WinSendDlgItemMsg(hwnd, CX_ENCODINGMODE, LM_SETITEMHANDLE, (MPARAM) lID,
		    (MPARAM) XMPA_ENCODINGMODE_VBR);
  if (pHandle->EncoderConfig.iEncodingMode == XMPA_ENCODINGMODE_VBR) lIDToSelect = lID;

  WinSendDlgItemMsg(hwnd, CX_ENCODINGMODE, LM_SELECTITEM, (MPARAM) lIDToSelect, (MPARAM) TRUE);

  // --- General settings groupbox ---
  // Stereo mode combobox
  WinSendDlgItemMsg(hwnd, CX_STEREOMODE, LM_DELETEALL, (MPARAM) 0, (MPARAM) 0);
  lID = (long) WinSendDlgItemMsg(hwnd, CX_STEREOMODE, LM_INSERTITEM, (MPARAM) LIT_END,
				 (MPARAM) ("Autoselect best"));
  WinSendDlgItemMsg(hwnd, CX_STEREOMODE, LM_SETITEMHANDLE, (MPARAM) lID,
		    (MPARAM) XMPA_STEREOMODE_DEFAULT);
  lIDToSelect = lID;

  for (i=0; i<MAX_MODES; i++)
  {
    lID = (long) WinSendDlgItemMsg(hwnd, CX_STEREOMODE, LM_INSERTITEM, (MPARAM) LIT_END,
				   (MPARAM) (modes[i]));
    WinSendDlgItemMsg(hwnd, CX_STEREOMODE, LM_SETITEMHANDLE, (MPARAM) lID,
		      (MPARAM) i);
    if (pHandle->EncoderConfig.iStereoMode == i)
      lIDToSelect = lID;
  }

  WinSendDlgItemMsg(hwnd, CX_STEREOMODE, LM_SELECTITEM, (MPARAM) lIDToSelect, (MPARAM) TRUE);

  // Algorithm quality combobox
  WinSendDlgItemMsg(hwnd, CX_ALGORITHMQUALITY, LM_DELETEALL, (MPARAM) 0, (MPARAM) 0);
  lIDToSelect = LIT_NONE;

  for (i=0; i<10; i++)
  {
    switch (i)
    {
      case 0:
	sprintf(achTemp, "0 (Best and slowest)");
	break;
      case 2:
	sprintf(achTemp, "2 (Recommended)");
	break;
      case 5:
	sprintf(achTemp, "5 (Default. Good speed, reasonable quality)");
	break;
      case 7:
	sprintf(achTemp, "7 (Very fast, OK quality)");
	break;
      case 9:
	sprintf(achTemp, "9 (Fastest, poor quality)");
	break;
      default:
	sprintf(achTemp, "%d", i);
	break;
    }
    lID = (long) WinSendDlgItemMsg(hwnd, CX_ALGORITHMQUALITY, LM_INSERTITEM, (MPARAM) LIT_END,
				   (MPARAM) achTemp);
    WinSendDlgItemMsg(hwnd, CX_ALGORITHMQUALITY, LM_SETITEMHANDLE, (MPARAM) lID,
		      (MPARAM) i);
    if (pHandle->EncoderConfig.iAlgorithmQuality == i)
      lIDToSelect = lID;
  }

  WinSendDlgItemMsg(hwnd, CX_ALGORITHMQUALITY, LM_SELECTITEM, (MPARAM) lIDToSelect, (MPARAM) TRUE);

  // --- MPEG Frame settings ---
  WinCheckButton(hwnd, CB_ERRORPROTECTION, pHandle->EncoderConfig.bErrorProtection);
  WinCheckButton(hwnd, CB_ORIGINAL, pHandle->EncoderConfig.bOriginal);
  WinCheckButton(hwnd, CB_COPYRIGHT, pHandle->EncoderConfig.bCopyright);
  WinCheckButton(hwnd, CB_PRIVATEEXTENSION, pHandle->EncoderConfig.bPrivate);

  // --- CBR settigns ---
  // Set inactive colors
  ulTemp = WinQuerySysColor(HWND_DESKTOP, SYSCLR_BUTTONMIDDLE, 0L);
  WinSetPresParam(WinWindowFromID(hwnd, CX_CBRBITRATE),
		  PP_DISABLEDBACKGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  ulTemp = WinQuerySysColor(HWND_DESKTOP, SYSCLR_BUTTONDARK, 0L);
  WinSetPresParam(WinWindowFromID(hwnd, CX_CBRBITRATE),
		  PP_DISABLEDFOREGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  // Set limits and initial values
  WinSendDlgItemMsg(hwnd, CX_CBRBITRATE, LM_DELETEALL, (MPARAM) 0, (MPARAM) 0);
  for (i=0; i<MAX_COMMONBITRATES; i++)
  {
    sprintf(achTemp, "%d", aiCommonBitrates[i]);
    lID = (long) WinSendDlgItemMsg(hwnd, CX_CBRBITRATE, LM_INSERTITEM, (MPARAM) LIT_END,
				   (MPARAM) achTemp);
    WinSendDlgItemMsg(hwnd, CX_CBRBITRATE, LM_SETITEMHANDLE, (MPARAM) lID,
		      (MPARAM) aiCommonBitrates[i]);
  }
  sprintf(achTemp, "%d", pHandle->EncoderConfig.iCBRBitrate);
  WinSetDlgItemText(hwnd, CX_CBRBITRATE, achTemp);

  // --- ABR settings ---
  // Set inactive colors
  ulTemp = WinQuerySysColor(HWND_DESKTOP, SYSCLR_BUTTONMIDDLE, 0L);
  WinSetPresParam(WinWindowFromID(hwnd, CX_ABRAVGBITRATE),
		  PP_DISABLEDBACKGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  WinSetPresParam(WinWindowFromID(hwnd, CX_ABRMINBITRATE),
		  PP_DISABLEDBACKGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  WinSetPresParam(WinWindowFromID(hwnd, CX_ABRMAXBITRATE),
		  PP_DISABLEDBACKGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  ulTemp = WinQuerySysColor(HWND_DESKTOP, SYSCLR_BUTTONDARK, 0L);
  WinSetPresParam(WinWindowFromID(hwnd, CB_ABRWRITEXINGVBRHEADERFRAME),
		  PP_DISABLEDFOREGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  WinSetPresParam(WinWindowFromID(hwnd, CX_ABRAVGBITRATE),
		  PP_DISABLEDFOREGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  WinSetPresParam(WinWindowFromID(hwnd, CX_ABRMINBITRATE),
		  PP_DISABLEDFOREGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  WinSetPresParam(WinWindowFromID(hwnd, CX_ABRMAXBITRATE),
		  PP_DISABLEDFOREGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  WinSetPresParam(WinWindowFromID(hwnd, CB_ABRLIMITMAXBITRATE),
		  PP_DISABLEDFOREGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  WinSetPresParam(WinWindowFromID(hwnd, CB_ABRLIMITMINBITRATE),
		  PP_DISABLEDFOREGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  WinSetPresParam(WinWindowFromID(hwnd, CB_FORCEMINBITRATEATSILENCE),
		  PP_DISABLEDFOREGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  // Set limits and initial values
  // Average bitrate
  WinSendDlgItemMsg(hwnd, CX_ABRAVGBITRATE, LM_DELETEALL, (MPARAM) 0, (MPARAM) 0);
  for (i=0; i<MAX_COMMONBITRATES; i++)
  {
    sprintf(achTemp, "%d", aiCommonBitrates[i]);
    lID = (long) WinSendDlgItemMsg(hwnd, CX_ABRAVGBITRATE, LM_INSERTITEM, (MPARAM) LIT_END,
				   (MPARAM) achTemp);
    WinSendDlgItemMsg(hwnd, CX_ABRAVGBITRATE, LM_SETITEMHANDLE, (MPARAM) lID,
		      (MPARAM) aiCommonBitrates[i]);
  }
  sprintf(achTemp, "%d", pHandle->EncoderConfig.iABRMeanBitrate);
  WinSetDlgItemText(hwnd, CX_ABRAVGBITRATE, achTemp);

  // Min bitrate
  WinCheckButton(hwnd, CB_ABRLIMITMINBITRATE, pHandle->EncoderConfig.bABRLimitMinBitrate);
  WinSendDlgItemMsg(hwnd, CX_ABRMINBITRATE, LM_DELETEALL, (MPARAM) 0, (MPARAM) 0);
  for (i=0; i<MAX_COMMONBITRATES; i++)
  {
    sprintf(achTemp, "%d", aiCommonBitrates[i]);
    lID = (long) WinSendDlgItemMsg(hwnd, CX_ABRMINBITRATE, LM_INSERTITEM, (MPARAM) LIT_END,
				   (MPARAM) achTemp);
    WinSendDlgItemMsg(hwnd, CX_ABRMINBITRATE, LM_SETITEMHANDLE, (MPARAM) lID,
		      (MPARAM) aiCommonBitrates[i]);
  }
  sprintf(achTemp, "%d", pHandle->EncoderConfig.iABRMinBitrate);
  WinSetDlgItemText(hwnd, CX_ABRMINBITRATE, achTemp);
  WinCheckButton(hwnd, CB_FORCEMINBITRATEATSILENCE, pHandle->EncoderConfig.bABREnforceMinBitrate);

  // Max bitrate
  WinCheckButton(hwnd, CB_ABRLIMITMAXBITRATE, pHandle->EncoderConfig.bABRLimitMaxBitrate);
  WinSendDlgItemMsg(hwnd, CX_ABRMAXBITRATE, LM_DELETEALL, (MPARAM) 0, (MPARAM) 0);
  for (i=0; i<MAX_COMMONBITRATES; i++)
  {
    sprintf(achTemp, "%d", aiCommonBitrates[i]);
    lID = (long) WinSendDlgItemMsg(hwnd, CX_ABRMAXBITRATE, LM_INSERTITEM, (MPARAM) LIT_END,
				   (MPARAM) achTemp);
    WinSendDlgItemMsg(hwnd, CX_ABRMAXBITRATE, LM_SETITEMHANDLE, (MPARAM) lID,
		      (MPARAM) aiCommonBitrates[i]);
  }
  sprintf(achTemp, "%d", pHandle->EncoderConfig.iABRMaxBitrate);
  WinSetDlgItemText(hwnd, CX_ABRMAXBITRATE, achTemp);

  WinCheckButton(hwnd,CB_ABRWRITEXINGVBRHEADERFRAME, pHandle->EncoderConfig.bABRWriteXingVBRHeader);

  // --- VBR settings ---
  // Set inactive colors
  ulTemp = WinQuerySysColor(HWND_DESKTOP, SYSCLR_BUTTONMIDDLE, 0L);
  WinSetPresParam(WinWindowFromID(hwnd, CX_VBRQUALITY),
		  PP_DISABLEDBACKGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  WinSetPresParam(WinWindowFromID(hwnd, CX_VBRMODE),
		  PP_DISABLEDBACKGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  ulTemp = WinQuerySysColor(HWND_DESKTOP, SYSCLR_BUTTONDARK, 0L);
  WinSetPresParam(WinWindowFromID(hwnd, CX_VBRQUALITY),
		  PP_DISABLEDFOREGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  WinSetPresParam(WinWindowFromID(hwnd, CX_VBRMODE),
		  PP_DISABLEDFOREGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  WinSetPresParam(WinWindowFromID(hwnd, CB_VBRWRITEXINGVBRHEADERFRAME),
		  PP_DISABLEDFOREGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  // Set limits and initial values
  // VBR quality
  WinSendDlgItemMsg(hwnd, CX_VBRQUALITY, LM_DELETEALL, (MPARAM) 0, (MPARAM) 0);
  lIDToSelect = LIT_NONE;

  for (i=0; i<10; i++)
  {
    switch (i)
    {
      case 0:
	sprintf(achTemp, "0 (Best)");
	break;
      case 4:
	sprintf(achTemp, "4 (Default)");
	break;
      case 9:
	sprintf(achTemp, "9 (Worst)");
	break;
      default:
	sprintf(achTemp, "%d", i);
	break;
    }
    lID = (long) WinSendDlgItemMsg(hwnd, CX_VBRQUALITY, LM_INSERTITEM, (MPARAM) LIT_END,
				   (MPARAM) achTemp);
    WinSendDlgItemMsg(hwnd, CX_VBRQUALITY, LM_SETITEMHANDLE, (MPARAM) lID,
		      (MPARAM) i);
    if (pHandle->EncoderConfig.iVBRQuality == i)
      lIDToSelect = lID;
  }

  WinSendDlgItemMsg(hwnd, CX_VBRQUALITY, LM_SELECTITEM, (MPARAM) lIDToSelect, (MPARAM) TRUE);

  // VBR mode
  WinSendDlgItemMsg(hwnd, CX_VBRMODE, LM_DELETEALL, (MPARAM) 0, (MPARAM) 0);

  lID = (long) WinSendDlgItemMsg(hwnd, CX_VBRMODE, LM_INSERTITEM, (MPARAM) LIT_END,
				 (MPARAM) "New algorithm");
  WinSendDlgItemMsg(hwnd, CX_VBRMODE, LM_SETITEMHANDLE, (MPARAM) lID,
		    (MPARAM) XMPA_VBRMODE_NEWALGORITHM);
  lIDToSelect = lID;

  lID = (long) WinSendDlgItemMsg(hwnd, CX_VBRMODE, LM_INSERTITEM, (MPARAM) LIT_END,
				 (MPARAM) "Old algorithm");
  WinSendDlgItemMsg(hwnd, CX_VBRMODE, LM_SETITEMHANDLE, (MPARAM) lID,
		    (MPARAM) XMPA_VBRMODE_OLDALGORITHM);
  if (pHandle->EncoderConfig.iVBRMode == XMPA_VBRMODE_OLDALGORITHM)
    lIDToSelect = lID;

  WinSendDlgItemMsg(hwnd, CX_VBRMODE, LM_SELECTITEM, (MPARAM) lIDToSelect, (MPARAM) TRUE);

  WinCheckButton(hwnd,CB_VBRWRITEXINGVBRHEADERFRAME, pHandle->EncoderConfig.bVBRWriteXingVBRHeader);

  // --- ID3 tag ---
  // Set inactive colors
  ulTemp = WinQuerySysColor(HWND_DESKTOP, SYSCLR_BUTTONMIDDLE, 0L);
  WinSetPresParam(WinWindowFromID(hwnd, EF_ID3TITLE),
		  PP_DISABLEDBACKGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  WinSetPresParam(WinWindowFromID(hwnd, EF_ID3ARTIST),
		  PP_DISABLEDBACKGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  WinSetPresParam(WinWindowFromID(hwnd, EF_ID3ALBUM),
		  PP_DISABLEDBACKGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  WinSetPresParam(WinWindowFromID(hwnd, EF_ID3YEAR),
		  PP_DISABLEDBACKGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  WinSetPresParam(WinWindowFromID(hwnd, EF_ID3COMMENT),
		  PP_DISABLEDBACKGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  WinSetPresParam(WinWindowFromID(hwnd, EF_ID3TRACK),
		  PP_DISABLEDBACKGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  WinSetPresParam(WinWindowFromID(hwnd, CX_ID3GENRE),
		  PP_DISABLEDBACKGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  ulTemp = WinQuerySysColor(HWND_DESKTOP, SYSCLR_BUTTONDARK, 0L);
  WinSetPresParam(WinWindowFromID(hwnd, EF_ID3TITLE),
		  PP_DISABLEDFOREGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  WinSetPresParam(WinWindowFromID(hwnd, EF_ID3ARTIST),
		  PP_DISABLEDFOREGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  WinSetPresParam(WinWindowFromID(hwnd, EF_ID3ALBUM),
		  PP_DISABLEDFOREGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  WinSetPresParam(WinWindowFromID(hwnd, EF_ID3YEAR),
		  PP_DISABLEDFOREGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  WinSetPresParam(WinWindowFromID(hwnd, EF_ID3COMMENT),
		  PP_DISABLEDFOREGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  WinSetPresParam(WinWindowFromID(hwnd, EF_ID3TRACK),
		  PP_DISABLEDFOREGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  WinSetPresParam(WinWindowFromID(hwnd, CX_ID3GENRE),
		  PP_DISABLEDFOREGROUNDCOLOR,
		  (ULONG) sizeof(ulTemp),
		  (PVOID) &ulTemp);
  // Set limits and initial values
  WinCheckButton(hwnd, RB_WRITEIDV1TAG, (pHandle->EncoderConfig.iWriteTAG == XMPA_WRITETAG_ID3V1));
  WinCheckButton(hwnd, RB_WRITEIDV11TAG, (pHandle->EncoderConfig.iWriteTAG == XMPA_WRITETAG_ID3V11));
  WinCheckButton(hwnd, RB_NOIDTAG, (pHandle->EncoderConfig.iWriteTAG == XMPA_WRITETAG_NONE));
  WinSendDlgItemMsg(hwnd, EF_ID3TITLE, EM_SETTEXTLIMIT,
		    (MPARAM) (sizeof(pHandle->EncoderConfig.achID3v1_SongTitle)-1), (MPARAM) 0);
  WinSetDlgItemText(hwnd, EF_ID3TITLE, pHandle->EncoderConfig.achID3v1_SongTitle);
  WinSendDlgItemMsg(hwnd, EF_ID3ARTIST, EM_SETTEXTLIMIT,
		    (MPARAM) (sizeof(pHandle->EncoderConfig.achID3v1_Artist)-1), (MPARAM) 0);
  WinSetDlgItemText(hwnd, EF_ID3ARTIST, pHandle->EncoderConfig.achID3v1_Artist);
  WinSendDlgItemMsg(hwnd, EF_ID3ALBUM, EM_SETTEXTLIMIT,
		    (MPARAM) (sizeof(pHandle->EncoderConfig.achID3v1_Album)-1), (MPARAM) 0);
  WinSetDlgItemText(hwnd, EF_ID3ALBUM, pHandle->EncoderConfig.achID3v1_Album);
  WinSendDlgItemMsg(hwnd, EF_ID3YEAR, EM_SETTEXTLIMIT,
		    (MPARAM) (sizeof(pHandle->EncoderConfig.achID3v1_Year)-1), (MPARAM) 0);
  WinSetDlgItemText(hwnd, EF_ID3YEAR, pHandle->EncoderConfig.achID3v1_Year);
  WinSendDlgItemMsg(hwnd, EF_ID3COMMENT, EM_SETTEXTLIMIT,
		    (MPARAM) (sizeof(pHandle->EncoderConfig.achID3v1_Comment)-1), (MPARAM) 0);
  WinSetDlgItemText(hwnd, EF_ID3COMMENT, pHandle->EncoderConfig.achID3v1_Comment);

  WinSendDlgItemMsg(hwnd, EF_ID3TRACK, EM_SETTEXTLIMIT,
		    (MPARAM) 5, (MPARAM) 0);
  sprintf(achTemp, "%d", pHandle->EncoderConfig.chID3v11_Track);
  WinSetDlgItemText(hwnd, EF_ID3TRACK, achTemp);
  WinSendDlgItemMsg(hwnd, CX_ID3GENRE, LM_DELETEALL, (MPARAM) 0, (MPARAM) 0);
  lIDToSelect = LIT_NONE;
  for (i=0; i<MAX_GENRENAMES; i++)
  {
    lID = (long) WinSendDlgItemMsg(hwnd, CX_ID3GENRE, LM_INSERTITEM, (MPARAM) LIT_END,
				   (MPARAM) (genre_names[i]));
    WinSendDlgItemMsg(hwnd, CX_ID3GENRE, LM_SETITEMHANDLE, (MPARAM) lID, (MPARAM) i);
    if (i==pHandle->EncoderConfig.chID3v1_Genre)
      lIDToSelect = lID;
  }

  WinSendDlgItemMsg(hwnd, CX_ID3GENRE, LM_SELECTITEM,
		    (MPARAM) lIDToSelect, (MPARAM) TRUE);

  // --- Lame info ---
  sprintf(achTemp, "Using LAME version %s.", get_lame_version());
  WinSetDlgItemText(hwnd, ST_USINGLAMEVERSION, achTemp);
  pchTemp = (char *) get_lame_url();
  if (!pchTemp)
    pchTemp = "<URL is unknown, sorry!>";
  WinSetDlgItemText(hwnd, ST_LAMEURL, pchTemp);

  // Now subclass every dropdown combobox, because we want to
  // get the SETFOCUS and KILLFOCUS events!
  OldComboboxWndProc = WinSubclassWindow(WinWindowFromID(hwnd, CX_CBRBITRATE), (PFNWP) NewComboboxWndProc);
  WinSubclassWindow(WinWindowFromID(hwnd, CX_ABRAVGBITRATE), (PFNWP) NewComboboxWndProc);
  WinSubclassWindow(WinWindowFromID(hwnd, CX_ABRMINBITRATE), (PFNWP) NewComboboxWndProc);
  WinSubclassWindow(WinWindowFromID(hwnd, CX_ABRMAXBITRATE), (PFNWP) NewComboboxWndProc);
}

static void ResizeExportDialog(HWND hwnd)
{
  SWP Swp1, Swp2, Swp3;
  ULONG CXDLGFRAME, CYDLGFRAME, CYTITLEBAR;
  int iStartX, iComboWidth;
  int iGBHeight, iStaticTextHeight, iEntryFieldHeight, iCheckboxHeight;
  int iDisabledWindowUpdate=0;

  if (WinIsWindowVisible(hwnd))
  {
    WinEnableWindowUpdate(hwnd, FALSE);
    iDisabledWindowUpdate = 1;
  }

  CXDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
  CYDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);
  CYTITLEBAR = WinQuerySysValue(HWND_DESKTOP, SV_CYTITLEBAR);

  // Reposition/resize controls in window

  // There are three columns of controls in this window.
  // We'll start with the middle one, to know the final height of the window.
  // Then we go for the left column, then the right, and then resize the window
  // to perfectly fit.

  // To be able to start with the middle column, we have to know its X position,
  // where to start from!
  // So, let's see what we'll have to the left:
  // - 2*CXDLGFRAME (window frame + space)
  // - CXDLGFRAME (groupbox left frame)
  // - CXDLGFRAME (space)
  // - "Algorithm quality" static text width
  // - CXDLGFRAME (space)
  // - space for listbox for algorithm quality (2*CXDLGFRAME + static text width)
  // - CXDLGFRAME (space)
  // - CXDLGFRAME (groupbox right frame)
  // - CXDLGFRAME (space between groupboxes)
  // Here we can start the middle column!

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_ALGORITHMQUALITY), &Swp1);

  iStaticTextHeight = Swp1.cy;
  iEntryFieldHeight = iStaticTextHeight + CYDLGFRAME;

  iStartX = 10*CXDLGFRAME + 2*Swp1.cx;

  // ======== Middle column ==========

  // --- Settings for VBR encoding ---
  WinQueryWindowPos(WinWindowFromID(hwnd, CB_VBRWRITEXINGVBRHEADERFRAME), &Swp2);
  iCheckboxHeight = Swp2.cy;
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_CBRKBPS), &Swp3);
  iGBHeight =
    CYDLGFRAME + iCheckboxHeight +
    CYDLGFRAME + iEntryFieldHeight +
    CYDLGFRAME + iEntryFieldHeight +
    CYDLGFRAME + iStaticTextHeight;
  WinSetWindowPos(WinWindowFromID(hwnd, GB_SETTINGSFORVBRENCODING),
		  HWND_TOP,
		  iStartX, 2*CYDLGFRAME,
		  Swp2.cx + Swp3.cx + 2*CXDLGFRAME,
		  iGBHeight,
                  SWP_SIZE | SWP_MOVE);
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_SETTINGSFORVBRENCODING), &Swp1);

  WinSetWindowPos(WinWindowFromID(hwnd, CB_VBRWRITEXINGVBRHEADERFRAME),
		  HWND_TOP,
		  Swp1.x+CXDLGFRAME, Swp1.y+CYDLGFRAME,
		  0, 0,
		  SWP_MOVE);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_VBRQUALITY),
		  HWND_TOP,
		  Swp1.x + CXDLGFRAME,
		  Swp1.y + Swp1.cy - iStaticTextHeight - CYDLGFRAME - iEntryFieldHeight,
		  0, 0,
		  SWP_MOVE);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_VBRQUALITY), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, CX_VBRQUALITY),
		  HWND_TOP,
		  Swp2.x + Swp2.cx + CXDLGFRAME,
		  Swp2.y - 6*Swp2.cy,
		  Swp1.cx - CXDLGFRAME - Swp2.cx - CXDLGFRAME -CXDLGFRAME,
		  Swp2.cy*7+CYDLGFRAME,
		  SWP_MOVE | SWP_SIZE);

  WinSetWindowPos(WinWindowFromID(hwnd, ST_VBRMODE),
		  HWND_TOP,
		  Swp1.x + CXDLGFRAME,
		  Swp1.y + Swp1.cy - iStaticTextHeight - CYDLGFRAME - iEntryFieldHeight - CYDLGFRAME - iEntryFieldHeight,
		  0, 0,
		  SWP_MOVE);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_VBRQUALITY), &Swp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_VBRMODE), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, CX_VBRMODE),
		  HWND_TOP,
		  Swp2.x + Swp2.cx + CXDLGFRAME,
		  Swp3.y - 3*Swp2.cy,
		  Swp1.cx - CXDLGFRAME - Swp2.cx - CXDLGFRAME -CXDLGFRAME,
		  Swp2.cy*4+CYDLGFRAME,
		  SWP_MOVE | SWP_SIZE);

  // --- Settings for ABR encoding ---
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_SETTINGSFORVBRENCODING), &Swp3);
  iGBHeight =
    CYDLGFRAME + iCheckboxHeight +
    2*CYDLGFRAME + iEntryFieldHeight +
    CYDLGFRAME + iCheckboxHeight +
    2*CYDLGFRAME +
    CYDLGFRAME + iCheckboxHeight +
    CYDLGFRAME + iEntryFieldHeight +
    CYDLGFRAME + iCheckboxHeight +
    2*CYDLGFRAME +
    CYDLGFRAME + iEntryFieldHeight +
    CYDLGFRAME + iStaticTextHeight +
    CYDLGFRAME;
  WinSetWindowPos(WinWindowFromID(hwnd, GB_SETTINGSFORABRENCODING),
		  HWND_TOP,
		  Swp3.x, Swp3.y + Swp3.cy + CYDLGFRAME,
		  Swp3.cx,
		  iGBHeight,
                  SWP_SIZE | SWP_MOVE);
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_SETTINGSFORABRENCODING), &Swp1);

  WinSetWindowPos(WinWindowFromID(hwnd, CB_ABRWRITEXINGVBRHEADERFRAME),
		  HWND_TOP,
		  Swp1.x+CXDLGFRAME,
		  Swp1.y+CYDLGFRAME,
                  0, 0,
		  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, CB_ABRWRITEXINGVBRHEADERFRAME), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_ABRMAXBITRATE),
		  HWND_TOP,
		  Swp1.x+CXDLGFRAME,
		  Swp2.y+iCheckboxHeight + 3*CYDLGFRAME,
		  0, 0,
		  SWP_MOVE);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_ABRMAXKBPS), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_ABRMAXKBPS),
		  HWND_TOP,
		  Swp1.x + Swp1.cx - 2*CXDLGFRAME - Swp3.cx,
		  Swp2.y + iCheckboxHeight + 3*CYDLGFRAME,
		  0, 0,
		  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_ABRMAXKBPS), &Swp3);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_ABRMAXBITRATE), &Swp2);
  iComboWidth = Swp2.cx;
  WinSetWindowPos(WinWindowFromID(hwnd, CX_ABRMAXBITRATE),
		  HWND_TOP,
		  Swp3.x - CXDLGFRAME - iComboWidth,
		  Swp3.y - 5*Swp3.cy,
		  iComboWidth,
		  Swp3.cy*6+CYDLGFRAME,
		  SWP_MOVE | SWP_SIZE);

  WinSetWindowPos(WinWindowFromID(hwnd, CB_ABRLIMITMAXBITRATE),
		  HWND_TOP,
		  Swp2.x,
		  Swp2.y + iEntryFieldHeight + CYDLGFRAME,
                  0, 0,
		  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, CB_ABRLIMITMAXBITRATE), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, CB_FORCEMINBITRATEATSILENCE),
		  HWND_TOP,
		  Swp2.x,
		  Swp2.y + iCheckboxHeight + 3*CYDLGFRAME,
                  0, 0,
		  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, CB_FORCEMINBITRATEATSILENCE), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_ABRMINBITRATE),
		  HWND_TOP,
		  Swp2.x,
		  Swp2.y + iCheckboxHeight + CYDLGFRAME,
                  0, 0,
		  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_ABRMINBITRATE), &Swp1);
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_SETTINGSFORABRENCODING), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_ABRMINKBPS),
		  HWND_TOP,
		  Swp2.x + Swp2.cx - 2*CXDLGFRAME - Swp3.cx,
		  Swp1.y,
		  0, 0,
		  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_ABRMINKBPS), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, CX_ABRMINBITRATE),
		  HWND_TOP,
		  Swp3.x - CXDLGFRAME - iComboWidth,
		  Swp3.y - 5*Swp3.cy,
		  iComboWidth,
		  Swp3.cy*6+CYDLGFRAME,
		  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_ABRMINBITRATE), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, CB_ABRLIMITMINBITRATE),
		  HWND_TOP,
		  Swp2.x,
		  Swp2.y + iEntryFieldHeight + CYDLGFRAME,
                  0, 0,
		  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, CB_ABRLIMITMINBITRATE), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_AVERAGEBITRATE),
		  HWND_TOP,
		  Swp2.x,
		  Swp2.y + iCheckboxHeight + 3*CYDLGFRAME,
                  0, 0,
		  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_AVERAGEBITRATE), &Swp1);
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_SETTINGSFORABRENCODING), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_ABRAVGKBPS),
		  HWND_TOP,
		  Swp2.x + Swp2.cx - 2*CXDLGFRAME - Swp3.cx,
		  Swp1.y,
		  0, 0,
		  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_ABRAVGKBPS), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, CX_ABRAVGBITRATE),
		  HWND_TOP,
		  Swp3.x - CXDLGFRAME - iComboWidth,
		  Swp3.y - 5*Swp3.cy,
		  iComboWidth,
		  Swp3.cy*6+CYDLGFRAME,
		  SWP_MOVE | SWP_SIZE);
  // --- Settings for CBR encoding ---
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_SETTINGSFORABRENCODING), &Swp3);
  iGBHeight =
    CYDLGFRAME + iEntryFieldHeight +
    CYDLGFRAME + iStaticTextHeight +
    CYDLGFRAME;
  WinSetWindowPos(WinWindowFromID(hwnd, GB_SETTINGSFORCBRENCODING),
		  HWND_TOP,
		  Swp3.x, Swp3.y + Swp3.cy + CYDLGFRAME,
		  Swp3.cx,
		  iGBHeight,
                  SWP_SIZE | SWP_MOVE);
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_SETTINGSFORCBRENCODING), &Swp1);

  WinSetWindowPos(WinWindowFromID(hwnd, ST_CBRBITRATE),
		  HWND_TOP,
		  Swp1.x+CXDLGFRAME, Swp1.y+2*CYDLGFRAME,
		  0, 0,
		  SWP_MOVE);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_CBRKBPS), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_CBRKBPS),
		  HWND_TOP,
		  Swp1.x + Swp1.cx - 2*CXDLGFRAME - Swp3.cx,
		  Swp1.y + 2*CYDLGFRAME,
		  0, 0,
		  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_CBRKBPS), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, CX_CBRBITRATE),
		  HWND_TOP,
		  Swp3.x - CXDLGFRAME - iComboWidth,
		  Swp3.y - 5*Swp3.cy,
		  iComboWidth,
		  Swp3.cy*6+CYDLGFRAME,
		  SWP_MOVE | SWP_SIZE);

  // --- Settings for MPEG Frame settings ---
  iGBHeight =
    CYDLGFRAME + iCheckboxHeight +
    CYDLGFRAME + iCheckboxHeight +
    CYDLGFRAME + iCheckboxHeight +
    CYDLGFRAME + iCheckboxHeight +
    CYDLGFRAME + iStaticTextHeight;
  WinSetWindowPos(WinWindowFromID(hwnd, GB_MPEGFRAMESETTINGS),
		  HWND_TOP,
		  2*CXDLGFRAME, 2*CYDLGFRAME,
		  iStartX - 2*CXDLGFRAME - CXDLGFRAME,
		  iGBHeight,
                  SWP_SIZE | SWP_MOVE);
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_MPEGFRAMESETTINGS), &Swp1);

  WinSetWindowPos(WinWindowFromID(hwnd, CB_PRIVATEEXTENSION),
		  HWND_TOP,
		  Swp1.x+CXDLGFRAME, Swp1.y+CYDLGFRAME,
		  Swp1.cx - 2*CXDLGFRAME, iCheckboxHeight,
		  SWP_MOVE | SWP_SIZE);

  WinSetWindowPos(WinWindowFromID(hwnd, CB_COPYRIGHT),
		  HWND_TOP,
		  Swp1.x+CXDLGFRAME, Swp1.y+CYDLGFRAME+ 1*(CYDLGFRAME + iCheckboxHeight),
		  Swp1.cx - 2*CXDLGFRAME, iCheckboxHeight,
		  SWP_MOVE | SWP_SIZE);
  WinSetWindowPos(WinWindowFromID(hwnd, CB_ORIGINAL),
		  HWND_TOP,
		  Swp1.x+CXDLGFRAME, Swp1.y+CYDLGFRAME+ 2*(CYDLGFRAME + iCheckboxHeight),
		  Swp1.cx - 2*CXDLGFRAME, iCheckboxHeight,
		  SWP_MOVE | SWP_SIZE);
  WinSetWindowPos(WinWindowFromID(hwnd, CB_ERRORPROTECTION),
		  HWND_TOP,
		  Swp1.x+CXDLGFRAME, Swp1.y+CYDLGFRAME+ 3*(CYDLGFRAME + iCheckboxHeight),
		  Swp1.cx - 2*CXDLGFRAME, iCheckboxHeight,
		  SWP_MOVE | SWP_SIZE);

  // --- Settings for General settings ---
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_MPEGFRAMESETTINGS), &Swp3);
  iGBHeight =
    CYDLGFRAME + iEntryFieldHeight +
    CYDLGFRAME + iEntryFieldHeight +
    CYDLGFRAME + iStaticTextHeight +
    CYDLGFRAME;
  WinSetWindowPos(WinWindowFromID(hwnd, GB_GENERALSETTINGS),
		  HWND_TOP,
		  Swp3.x, Swp3.y + Swp3.cy + CYDLGFRAME,
		  Swp3.cx,
		  iGBHeight,
                  SWP_SIZE | SWP_MOVE);
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_GENERALSETTINGS), &Swp1);

  WinSetWindowPos(WinWindowFromID(hwnd, ST_ALGORITHMQUALITY),
		  HWND_TOP,
		  Swp1.x+CXDLGFRAME, Swp1.y+2*CYDLGFRAME,
		  0, 0,
		  SWP_MOVE);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_ALGORITHMQUALITY), &Swp2);

  iComboWidth = Swp1.cx - 2*CXDLGFRAME - Swp2.cx - CXDLGFRAME;
  WinSetWindowPos(WinWindowFromID(hwnd, CX_ALGORITHMQUALITY),
		  HWND_TOP,
		  Swp1.x + Swp1.cx - CXDLGFRAME - iComboWidth,
		  Swp2.y - 5*iEntryFieldHeight - CYDLGFRAME,
		  iComboWidth,
		  iEntryFieldHeight*6+CYDLGFRAME,
		  SWP_MOVE | SWP_SIZE);

  WinSetWindowPos(WinWindowFromID(hwnd, ST_STEREOMODE),
		  HWND_TOP,
		  Swp1.x+CXDLGFRAME, Swp2.y+CYDLGFRAME+iEntryFieldHeight,
		  0, 0,
		  SWP_MOVE);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_STEREOMODE), &Swp2);

  WinSetWindowPos(WinWindowFromID(hwnd, CX_STEREOMODE),
		  HWND_TOP,
		  Swp1.x + Swp1.cx - CXDLGFRAME - iComboWidth,
		  Swp2.y - 5*iEntryFieldHeight - CYDLGFRAME,
		  iComboWidth,
		  iEntryFieldHeight*6+CYDLGFRAME,
		  SWP_MOVE | SWP_SIZE);

  // --- Settings for Encoding mode ---
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_SETTINGSFORCBRENCODING), &Swp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_GENERALSETTINGS), &Swp3);
  WinSetWindowPos(WinWindowFromID(hwnd, GB_ENCODINGMODE),
		  HWND_TOP,
		  Swp3.x, Swp3.y + Swp3.cy + CYDLGFRAME,
		  Swp3.cx,
		  Swp2.y + Swp2.cy - (Swp3.y + Swp3.cy + CYDLGFRAME),
                  SWP_SIZE | SWP_MOVE);
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_ENCODINGMODE), &Swp1);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_ENCODINGMODE), HWND_TOP,
		  Swp1.x + CXDLGFRAME,
		  Swp1.y + Swp1.cy - iStaticTextHeight - CYDLGFRAME - iEntryFieldHeight,
		  0, 0,
		  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_ENCODINGMODE), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, CX_ENCODINGMODE),
		  HWND_TOP,
		  Swp2.x + Swp2.cx + 2*CXDLGFRAME,
		  Swp2.y - 4*iEntryFieldHeight - CYDLGFRAME,
		  Swp1.cx - (Swp2.x + Swp2.cx + CXDLGFRAME),
		  iEntryFieldHeight*5+CYDLGFRAME,
		  SWP_MOVE | SWP_SIZE);

  WinSetWindowPos(WinWindowFromID(hwnd, MLE_ENCODINGMODEHELP), HWND_TOP,
		  Swp1.x + CXDLGFRAME, Swp1.y + 2*CYDLGFRAME,
		  Swp1.cx - 2*CXDLGFRAME,
		  Swp2.y - Swp1.y - 4*CYDLGFRAME,
                  SWP_MOVE | SWP_SIZE);

  // --- Settings for ID3 Tag ---
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_SETTINGSFORCBRENCODING), &Swp3);
  iGBHeight =
    CYDLGFRAME + iEntryFieldHeight +
    CYDLGFRAME + iEntryFieldHeight +
    CYDLGFRAME + iEntryFieldHeight +
    CYDLGFRAME + iEntryFieldHeight +
    CYDLGFRAME + iEntryFieldHeight +
    CYDLGFRAME + iEntryFieldHeight +
    CYDLGFRAME + iEntryFieldHeight +
    iCheckboxHeight +
    iCheckboxHeight +
    CYDLGFRAME + iCheckboxHeight +
    CYDLGFRAME + iStaticTextHeight +
    CYDLGFRAME;

  WinSetWindowPos(WinWindowFromID(hwnd, GB_IDTAG),
		  HWND_TOP,
		  Swp3.x + Swp3.cx + CXDLGFRAME,
		  Swp3.y + Swp3.cy - iGBHeight,
		  Swp3.cx,
		  iGBHeight,
                  SWP_SIZE | SWP_MOVE);
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_IDTAG), &Swp1);

  WinSetWindowPos(WinWindowFromID(hwnd, ST_GENRE),
		  HWND_TOP,
		  Swp1.x+CXDLGFRAME, Swp1.y+2*CYDLGFRAME,
		  0, 0,
		  SWP_MOVE);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_GENRE), &Swp2);

  WinSetWindowPos(WinWindowFromID(hwnd, ST_TRACK),
		  HWND_TOP,
		  Swp1.x+CXDLGFRAME, Swp2.y+CYDLGFRAME+iEntryFieldHeight,
		  0, 0,
		  SWP_MOVE);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_TRACK), &Swp2);

  WinSetWindowPos(WinWindowFromID(hwnd, ST_COMMENT),
		  HWND_TOP,
		  Swp1.x+CXDLGFRAME, Swp2.y+CYDLGFRAME+iEntryFieldHeight,
		  0, 0,
		  SWP_MOVE);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_COMMENT), &Swp2);

  iComboWidth = Swp1.cx - 2*CXDLGFRAME - Swp2.cx - CXDLGFRAME - 2*CXDLGFRAME;

  WinSetWindowPos(WinWindowFromID(hwnd, ST_YEAR),
		  HWND_TOP,
		  Swp1.x+CXDLGFRAME, Swp2.y+CYDLGFRAME+iEntryFieldHeight,
		  0, 0,
		  SWP_MOVE);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_YEAR), &Swp2);

  WinSetWindowPos(WinWindowFromID(hwnd, ST_ALBUM),
		  HWND_TOP,
		  Swp1.x+CXDLGFRAME, Swp2.y+CYDLGFRAME+iEntryFieldHeight,
		  0, 0,
		  SWP_MOVE);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_ALBUM), &Swp2);

  WinSetWindowPos(WinWindowFromID(hwnd, ST_ARTIST),
		  HWND_TOP,
		  Swp1.x+CXDLGFRAME, Swp2.y+CYDLGFRAME+iEntryFieldHeight,
		  0, 0,
		  SWP_MOVE);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_ARTIST), &Swp2);

  WinSetWindowPos(WinWindowFromID(hwnd, ST_TITLE),
		  HWND_TOP,
		  Swp1.x+CXDLGFRAME, Swp2.y+CYDLGFRAME+iEntryFieldHeight,
		  0, 0,
		  SWP_MOVE);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_TITLE), &Swp2);


  WinSetWindowPos(WinWindowFromID(hwnd, RB_NOIDTAG),
		  HWND_TOP,
		  Swp1.x+CXDLGFRAME, Swp2.y+CYDLGFRAME+iEntryFieldHeight,
		  0, 0,
		  SWP_MOVE);
  WinQueryWindowPos(WinWindowFromID(hwnd, RB_NOIDTAG), &Swp2);

  WinSetWindowPos(WinWindowFromID(hwnd, RB_WRITEIDV11TAG),
		  HWND_TOP,
		  Swp1.x+CXDLGFRAME, Swp2.y+iCheckboxHeight,
		  0, 0,
		  SWP_MOVE);
  WinQueryWindowPos(WinWindowFromID(hwnd, RB_WRITEIDV11TAG), &Swp2);

  WinSetWindowPos(WinWindowFromID(hwnd, RB_WRITEIDV1TAG),
		  HWND_TOP,
		  Swp1.x+CXDLGFRAME, Swp2.y+iCheckboxHeight,
		  0, 0,
		  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_GENRE), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, CX_ID3GENRE),
		  HWND_TOP,
		  Swp1.x + Swp1.cx - 2*CXDLGFRAME - iComboWidth - CXDLGFRAME,
		  Swp2.y - 10*iEntryFieldHeight - CYDLGFRAME,
		  iComboWidth + 2*CXDLGFRAME,
		  iEntryFieldHeight*11+CYDLGFRAME,
		  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_TRACK), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, EF_ID3TRACK),
		  HWND_TOP,
		  Swp1.x + Swp1.cx - 2*CXDLGFRAME - iComboWidth,
		  Swp2.y,
		  iComboWidth,
		  iEntryFieldHeight-CYDLGFRAME,
		  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_COMMENT), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, EF_ID3COMMENT),
		  HWND_TOP,
		  Swp1.x + Swp1.cx - 2*CXDLGFRAME - iComboWidth,
		  Swp2.y,
		  iComboWidth,
		  iEntryFieldHeight-CYDLGFRAME,
		  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_YEAR), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, EF_ID3YEAR),
		  HWND_TOP,
		  Swp1.x + Swp1.cx - 2*CXDLGFRAME - iComboWidth,
		  Swp2.y,
		  iComboWidth,
		  iEntryFieldHeight-CYDLGFRAME,
		  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_ALBUM), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, EF_ID3ALBUM),
		  HWND_TOP,
		  Swp1.x + Swp1.cx - 2*CXDLGFRAME - iComboWidth,
		  Swp2.y,
		  iComboWidth,
		  iEntryFieldHeight-CYDLGFRAME,
		  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_ARTIST), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, EF_ID3ARTIST),
		  HWND_TOP,
		  Swp1.x + Swp1.cx - 2*CXDLGFRAME - iComboWidth,
		  Swp2.y,
		  iComboWidth,
		  iEntryFieldHeight-CYDLGFRAME,
		  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_TITLE), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, EF_ID3TITLE),
		  HWND_TOP,
		  Swp1.x + Swp1.cx - 2*CXDLGFRAME - iComboWidth,
		  Swp2.y,
		  iComboWidth,
		  iEntryFieldHeight-CYDLGFRAME,
		  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, GB_IDTAG), &Swp1);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_USINGLAMEVERSION), HWND_TOP,
		  Swp1.x + CXDLGFRAME,
		  Swp1.y - iStaticTextHeight - CYDLGFRAME,
		  Swp1.cx - 2*CXDLGFRAME,
		  iStaticTextHeight,
		  SWP_SIZE | SWP_MOVE);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_LAMEURL), HWND_TOP,
		  Swp1.x + CXDLGFRAME,
		  Swp1.y - 2*iStaticTextHeight - 2*CYDLGFRAME,
		  Swp1.cx - 2*CXDLGFRAME,
		  iStaticTextHeight,
		  SWP_SIZE | SWP_MOVE);

  WinSetWindowPos(WinWindowFromID(hwnd, PB_ENCODE), HWND_TOP,
		  Swp1.x,
		  2*CYDLGFRAME,
		  0, 0,
		  SWP_MOVE);

  WinQueryWindowPos(WinWindowFromID(hwnd, PB_CANCEL), &Swp2);
  WinSetWindowPos(WinWindowFromID(hwnd, PB_CANCEL), HWND_TOP,
		  Swp1.x + Swp1.cx - Swp2.cx,
		  2*CYDLGFRAME,
		  0, 0,
                  SWP_MOVE);

  WinSetWindowPos(hwnd, HWND_TOP,
		  0, 0,
		  Swp1.x + Swp1.cx + 2*CXDLGFRAME,
		  Swp1.y + Swp1.cy + CYTITLEBAR + 2*CYDLGFRAME,
                  SWP_SIZE);

  if (iDisabledWindowUpdate)
    WinEnableWindowUpdate(hwnd, TRUE);
}

static void DisableAndEnableCBRStuffs(HWND hwnd)
{
  long lID;
  int bPartEnabled;

  lID = (long) WinSendDlgItemMsg(hwnd, CX_ENCODINGMODE, LM_QUERYSELECTION, (MPARAM) 0, (MPARAM) 0);
  bPartEnabled = (((long) WinSendDlgItemMsg(hwnd, CX_ENCODINGMODE, LM_QUERYITEMHANDLE, (MPARAM) lID, (MPARAM) 0)) == XMPA_ENCODINGMODE_CBR);

  WinEnableWindow(WinWindowFromID(hwnd, CX_CBRBITRATE), bPartEnabled);
}

static void DisableAndEnableABRStuffs(HWND hwnd)
{
  long lID;
  int bPartEnabled;
  int bSubPart1Enabled;

  lID = (long) WinSendDlgItemMsg(hwnd, CX_ENCODINGMODE, LM_QUERYSELECTION, (MPARAM) 0, (MPARAM) 0);
  bPartEnabled = (((long) WinSendDlgItemMsg(hwnd, CX_ENCODINGMODE, LM_QUERYITEMHANDLE, (MPARAM) lID, (MPARAM) 0)) == XMPA_ENCODINGMODE_ABR);
  WinEnableWindow(WinWindowFromID(hwnd, CX_ABRAVGBITRATE), bPartEnabled);
  WinEnableWindow(WinWindowFromID(hwnd, CB_ABRWRITEXINGVBRHEADERFRAME), bPartEnabled);
  WinEnableWindow(WinWindowFromID(hwnd, CB_ABRLIMITMINBITRATE), bPartEnabled);
  bSubPart1Enabled = (WinQueryButtonCheckstate(hwnd, CB_ABRLIMITMINBITRATE) == 1);
  WinEnableWindow(WinWindowFromID(hwnd, CX_ABRMINBITRATE), bPartEnabled && bSubPart1Enabled);
  WinEnableWindow(WinWindowFromID(hwnd, CB_FORCEMINBITRATEATSILENCE), bPartEnabled && bSubPart1Enabled);
  WinEnableWindow(WinWindowFromID(hwnd, CB_ABRLIMITMAXBITRATE), bPartEnabled);
  bSubPart1Enabled = (WinQueryButtonCheckstate(hwnd, CB_ABRLIMITMAXBITRATE) == 1);
  WinEnableWindow(WinWindowFromID(hwnd, CX_ABRMAXBITRATE), bPartEnabled && bSubPart1Enabled);
}

static void DisableAndEnableVBRStuffs(HWND hwnd)
{
  long lID;
  int bPartEnabled;

  lID = (long) WinSendDlgItemMsg(hwnd, CX_ENCODINGMODE, LM_QUERYSELECTION, (MPARAM) 0, (MPARAM) 0);
  bPartEnabled = (((long) WinSendDlgItemMsg(hwnd, CX_ENCODINGMODE, LM_QUERYITEMHANDLE, (MPARAM) lID, (MPARAM) 0)) == XMPA_ENCODINGMODE_VBR);
  WinEnableWindow(WinWindowFromID(hwnd, CX_VBRQUALITY), bPartEnabled);
  WinEnableWindow(WinWindowFromID(hwnd, CX_VBRMODE), bPartEnabled);
  WinEnableWindow(WinWindowFromID(hwnd, CB_VBRWRITEXINGVBRHEADERFRAME), bPartEnabled);
}

static void DisableAndEnableID3Stuffs(HWND hwnd)
{
  int bPartEnabled;
  int bSubPart1Enabled;

  // Enable/Disable ID3 tag stuffs
  bPartEnabled = (WinQueryButtonCheckstate(hwnd, RB_NOIDTAG) == 0);
  bSubPart1Enabled = (WinQueryButtonCheckstate(hwnd, RB_WRITEIDV11TAG) == 1);
  WinEnableWindow(WinWindowFromID(hwnd, EF_ID3TITLE), bPartEnabled);
  WinEnableWindow(WinWindowFromID(hwnd, EF_ID3ARTIST), bPartEnabled);
  WinEnableWindow(WinWindowFromID(hwnd, EF_ID3ALBUM), bPartEnabled);
  WinEnableWindow(WinWindowFromID(hwnd, EF_ID3YEAR), bPartEnabled);
  WinEnableWindow(WinWindowFromID(hwnd, EF_ID3COMMENT), bPartEnabled);
  WinEnableWindow(WinWindowFromID(hwnd, EF_ID3TRACK), bPartEnabled && bSubPart1Enabled);
  WinEnableWindow(WinWindowFromID(hwnd, CX_ID3GENRE), bPartEnabled);
}


static void DisableAndEnableByEncodingMode(HWND hwnd)
{
  DisableAndEnableCBRStuffs(hwnd);
  DisableAndEnableABRStuffs(hwnd);
  DisableAndEnableVBRStuffs(hwnd);
}

static void SetHelpByEncodingMode(HWND hwnd)
{
  long lID;
  char *pchText;

  lID = (long) WinSendDlgItemMsg(hwnd, CX_ENCODINGMODE, LM_QUERYSELECTION, (MPARAM) 0, (MPARAM) 0);
  switch ((long) WinSendDlgItemMsg(hwnd, CX_ENCODINGMODE, LM_QUERYITEMHANDLE, (MPARAM) lID, (MPARAM) 0))
  {
    case XMPA_ENCODINGMODE_CBR:
      pchText = "This is the default encoding mode, and also the most basic. "
	"In this mode, the bitrate will be the same for the whole file. "
	"It means that each part of your mp3 file will be using the same number of bits. "
	"The musical passage beeing a difficult one to encode or an easy one, the encoder "
	"will use the same bitrate, so the quality of your mp3 is variable. "
	"Complex parts will be of a lower quality than the easiest ones. The main advantage "
	"is that the final files size won't change and can be accurately predicted.";
      break;
    case XMPA_ENCODINGMODE_ABR:
      pchText = "In this mode, the encoder will maintain an average bitrate while "
	"using higher bitrates for the parts of your music that need more bits. The result "
	"will be of higher quality than CBR encoding but the average file size will remain "
	"predictible, so this mode is highly recommended over CBR. This encoding mode is similar "
	"to what is referred as vbr in AAC or Liquid Audio (2 other compression technologies).";
      break;
    case XMPA_ENCODINGMODE_VBR:
      pchText = "In this mode, you choose the desired quality on a scale from 9 "
	"(lowest quality/biggest distortion) to 0 (highest quality/lowest distortion). Then encoder "
	"tries to maintain the given quality in the whole file by choosing the optimal number of bits "
	"to spend for each part of your music. The main advantage is that you are able to specify "
	"the quality level that you want to reach, but the inconvenient is that the final file size "
	"is totally unpredictible.";
      break;
    default:
      pchText = "No help for this entry.";
      break;
  }
  WinSetDlgItemText(hwnd, MLE_ENCODINGMODEHELP, pchText);
}


MRESULT EXPENTRY ExportDialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  XMPAHandle_p pHandle;
  SHORT sTemp;

  switch (msg)
  {
    case WM_INITDLG:
      {
        // Store pointer to XMPAHandle structure into QWL_USER window word!
	pHandle = (XMPAHandle_p) mp2;
	WinSetWindowULong(hwnd, QWL_USER, (ULONG) pHandle);

	// Now resize window and arrange its controls to look nice!
	ResizeExportDialog(hwnd);

	// Fill dialog window controls with values and set its initial ones!
	InitializeExportDialogControls(hwnd, pHandle);

	// Disable and enable controls based on other controls
	DisableAndEnableID3Stuffs(hwnd);
	DisableAndEnableByEncodingMode(hwnd);

	// Set help by encoding mode
        SetHelpByEncodingMode(hwnd);

	// Ok, stuffs initialized.
	return (MRESULT) 0; // 0 means not to get focus yet!
      }

    case WM_CONTROL:
      switch (SHORT1FROMMP(mp1)) // Let's see control window ID
      {
	case CX_ENCODINGMODE:
	  if (SHORT2FROMMP(mp1)==LN_SELECT)
	  {
	    long lID;
	    int iSelectedMode;

            // An item has been selected! (selection changed)

            // Query selection
	    lID = (long) WinSendDlgItemMsg(hwnd, CX_ENCODINGMODE, LM_QUERYSELECTION, (MPARAM) 0, (MPARAM) 0);
	    iSelectedMode = (int) WinSendDlgItemMsg(hwnd, CX_ENCODINGMODE, LM_QUERYITEMHANDLE, (MPARAM) lID, (MPARAM) 0);

	    pHandle = (XMPAHandle_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pHandle)
              pHandle->EncoderConfig.iEncodingMode = iSelectedMode;

	    // Update GUI
	    DisableAndEnableByEncodingMode(hwnd);
	    SetHelpByEncodingMode(hwnd);

            // Event processed.
            return (MRESULT) 0;
	  }
	  break;
	case CX_STEREOMODE:
	  if (SHORT2FROMMP(mp1)==LN_SELECT)
	  {
	    long lID;
	    int iSelectedMode;

            // An item has been selected! (selection changed)

            // Query selection
	    lID = (long) WinSendDlgItemMsg(hwnd, CX_STEREOMODE, LM_QUERYSELECTION, (MPARAM) 0, (MPARAM) 0);
	    iSelectedMode = (int) WinSendDlgItemMsg(hwnd, CX_STEREOMODE, LM_QUERYITEMHANDLE, (MPARAM) lID, (MPARAM) 0);

	    pHandle = (XMPAHandle_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pHandle)
              pHandle->EncoderConfig.iStereoMode = iSelectedMode;

            // Event processed.
            return (MRESULT) 0;
	  }
	  break;
	case CX_ALGORITHMQUALITY:
	  if (SHORT2FROMMP(mp1)==LN_SELECT)
	  {
	    long lID;
	    int iSelected;

            // An item has been selected! (selection changed)

            // Query selection
	    lID = (long) WinSendDlgItemMsg(hwnd, CX_ALGORITHMQUALITY, LM_QUERYSELECTION, (MPARAM) 0, (MPARAM) 0);
	    iSelected = (int) WinSendDlgItemMsg(hwnd, CX_ALGORITHMQUALITY, LM_QUERYITEMHANDLE, (MPARAM) lID, (MPARAM) 0);

	    pHandle = (XMPAHandle_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pHandle)
              pHandle->EncoderConfig.iAlgorithmQuality = iSelected;

            // Event processed.
            return (MRESULT) 0;
	  }
	  break;
	case CB_ERRORPROTECTION:
	  if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	  {
            // The button control has been clicked
	    pHandle = (XMPAHandle_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pHandle)
	      pHandle->EncoderConfig.bErrorProtection = WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1));

            // Event processed.
            return (MRESULT) 0;
	  }
	  break;
	case CB_ORIGINAL:
	  if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	  {
            // The button control has been clicked
	    pHandle = (XMPAHandle_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pHandle)
	      pHandle->EncoderConfig.bOriginal = WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1));

            // Event processed.
            return (MRESULT) 0;
	  }
	  break;
	case CB_COPYRIGHT:
	  if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	  {
            // The button control has been clicked
	    pHandle = (XMPAHandle_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pHandle)
	      pHandle->EncoderConfig.bCopyright = WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1));

            // Event processed.
            return (MRESULT) 0;
	  }
	  break;
       case CB_PRIVATEEXTENSION:
	  if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	  {
            // The button control has been clicked
	    pHandle = (XMPAHandle_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pHandle)
	      pHandle->EncoderConfig.bPrivate = WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1));

            // Event processed.
            return (MRESULT) 0;
	  }
	  break;

	case CX_CBRBITRATE:
	  if (SHORT2FROMMP(mp1)==CBN_EFSETFOCUS)
	  {
            // Entering to control, save old value!
	    WinQueryDlgItemText(hwnd, SHORT1FROMMP(mp1), sizeof(achUndoBuf), achUndoBuf);
            return (MRESULT) 0;
	  }
	  if (SHORT2FROMMP(mp1)==CBN_EFKILLFOCUS)
	  {
	    // Leaving control. Check new value, and restore old,
	    // if invalid value has been entered!

	    if (!WinQueryDlgItemShort(hwnd, SHORT1FROMMP(mp1), &sTemp, FALSE))
              sTemp = -1;

	    if ((sTemp<MIN_BITRATE) || (sTemp>MAX_BITRATE))
	    {
              // Invalid bitrate entered, restore old value!
	      WinSetDlgItemText(hwnd, SHORT1FROMMP(mp1), achUndoBuf);
	      WinMessageBox(HWND_DESKTOP, hwnd,
			    "Please enter a valid bitrate value!",
			    "Invalid value entered",
			    0,
                            MB_OK | MB_ERROR | MB_MOVEABLE);
	    } else
	    {
	      // Valid bitrate, store it!
              pHandle = (XMPAHandle_p) WinQueryWindowULong(hwnd, QWL_USER);
	      if (pHandle)
		pHandle->EncoderConfig.iCBRBitrate = sTemp;
	    }
            // Event processed.
            return (MRESULT) 0;
	  }
          break;
	case CX_ABRAVGBITRATE:

	  if (SHORT2FROMMP(mp1)==CBN_EFSETFOCUS)
	  {
            // Entering to control, save old value!
	    WinQueryDlgItemText(hwnd, SHORT1FROMMP(mp1), sizeof(achUndoBuf), achUndoBuf);
            return (MRESULT) 0;
	  }
	  if (SHORT2FROMMP(mp1)==CBN_EFKILLFOCUS)
	  {
	    // Leaving control. Check new value, and restore old,
	    // if invalid value has been entered!

	    if (!WinQueryDlgItemShort(hwnd, SHORT1FROMMP(mp1), &sTemp, FALSE))
	      sTemp = -1;

	    if ((sTemp<MIN_BITRATE) || (sTemp>MAX_BITRATE))
	    {
              // Invalid bitrate entered, restore old value!
	      WinSetDlgItemText(hwnd, SHORT1FROMMP(mp1), achUndoBuf);
	      WinMessageBox(HWND_DESKTOP, hwnd,
			    "Please enter a valid bitrate value!",
			    "Invalid value entered",
			    0,
                            MB_OK | MB_ERROR | MB_MOVEABLE);
	    } else
	    {
	      // Valid bitrate, store it!
              pHandle = (XMPAHandle_p) WinQueryWindowULong(hwnd, QWL_USER);
	      if (pHandle)
		pHandle->EncoderConfig.iABRMeanBitrate = sTemp;
	    }
            // Event processed.
            return (MRESULT) 0;
	  }
	  break;
	case CB_ABRLIMITMINBITRATE:
	  if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	  {
            // The button control has been clicked
	    pHandle = (XMPAHandle_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pHandle)
	      pHandle->EncoderConfig.bABRLimitMinBitrate = WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1));

	    // Update enabled/disabled state
	    DisableAndEnableABRStuffs(hwnd);

            // Event processed.
            return (MRESULT) 0;
	  }
	  break;
	case CX_ABRMINBITRATE:
	  if (SHORT2FROMMP(mp1)==CBN_EFSETFOCUS)
	  {
            // Entering to control, save old value!
	    WinQueryDlgItemText(hwnd, SHORT1FROMMP(mp1), sizeof(achUndoBuf), achUndoBuf);
            return (MRESULT) 0;
	  }
	  if (SHORT2FROMMP(mp1)==CBN_EFKILLFOCUS)
	  {
	    // Leaving control. Check new value, and restore old,
	    // if invalid value has been entered!

	    if (!WinQueryDlgItemShort(hwnd, SHORT1FROMMP(mp1), &sTemp, FALSE))
              sTemp = -1;

	    if ((sTemp<MIN_BITRATE) || (sTemp>MAX_BITRATE))
	    {
              // Invalid bitrate entered, restore old value!
	      WinSetDlgItemText(hwnd, SHORT1FROMMP(mp1), achUndoBuf);
	      WinMessageBox(HWND_DESKTOP, hwnd,
			    "Please enter a valid bitrate value!",
			    "Invalid value entered",
			    0,
                            MB_OK | MB_ERROR | MB_MOVEABLE);
	    } else
	    {
	      // Valid bitrate, store it!
              pHandle = (XMPAHandle_p) WinQueryWindowULong(hwnd, QWL_USER);
	      if (pHandle)
		pHandle->EncoderConfig.iABRMinBitrate = sTemp;
	    }
            // Event processed.
            return (MRESULT) 0;
	  }
	  break;
	case CB_FORCEMINBITRATEATSILENCE:
	  if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	  {
            // The button control has been clicked
	    pHandle = (XMPAHandle_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pHandle)
	      pHandle->EncoderConfig.bABREnforceMinBitrate = WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1));

            // Event processed.
            return (MRESULT) 0;
	  }
	  break;
	case CB_ABRLIMITMAXBITRATE:
	  if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	  {
            // The button control has been clicked
	    pHandle = (XMPAHandle_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pHandle)
	      pHandle->EncoderConfig.bABRLimitMaxBitrate = WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1));

	    // Update enabled/disabled state
	    DisableAndEnableABRStuffs(hwnd);

            // Event processed.
            return (MRESULT) 0;
	  }
	  break;
	case CX_ABRMAXBITRATE:
	  if (SHORT2FROMMP(mp1)==CBN_EFSETFOCUS)
	  {
            // Entering to control, save old value!
	    WinQueryDlgItemText(hwnd, SHORT1FROMMP(mp1), sizeof(achUndoBuf), achUndoBuf);
            return (MRESULT) 0;
	  }
	  if (SHORT2FROMMP(mp1)==CBN_EFKILLFOCUS)
	  {
	    // Leaving control. Check new value, and restore old,
	    // if invalid value has been entered!

	    if (!WinQueryDlgItemShort(hwnd, SHORT1FROMMP(mp1), &sTemp, FALSE))
	      sTemp = -1;

	    if ((sTemp<MIN_BITRATE) || (sTemp>MAX_BITRATE))
	    {
              // Invalid bitrate entered, restore old value!
	      WinSetDlgItemText(hwnd, SHORT1FROMMP(mp1), achUndoBuf);
	      WinMessageBox(HWND_DESKTOP, hwnd,
			    "Please enter a valid bitrate value!",
			    "Invalid value entered",
			    0,
                            MB_OK | MB_ERROR | MB_MOVEABLE);
	    } else
	    {
	      // Valid bitrate, store it!
              pHandle = (XMPAHandle_p) WinQueryWindowULong(hwnd, QWL_USER);
	      if (pHandle)
		pHandle->EncoderConfig.iABRMaxBitrate = sTemp;
	    }
            // Event processed.
            return (MRESULT) 0;
	  }
	  break;
	case CX_VBRQUALITY:
	  if (SHORT2FROMMP(mp1)==LN_SELECT)
	  {
	    long lID;
	    int iSelected;

            // An item has been selected! (selection changed)

            // Query selection
	    lID = (long) WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), LM_QUERYSELECTION, (MPARAM) 0, (MPARAM) 0);
	    iSelected = (int) WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), LM_QUERYITEMHANDLE, (MPARAM) lID, (MPARAM) 0);

	    pHandle = (XMPAHandle_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pHandle)
              pHandle->EncoderConfig.iVBRQuality = iSelected;

            // Event processed.
            return (MRESULT) 0;
	  }
	  break;
	case CB_VBRWRITEXINGVBRHEADERFRAME:
	  if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	  {
            // The button control has been clicked
	    pHandle = (XMPAHandle_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pHandle)
	      pHandle->EncoderConfig.bVBRWriteXingVBRHeader = WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1));

            // Event processed.
            return (MRESULT) 0;
	  }
	  break;
	case CB_ABRWRITEXINGVBRHEADERFRAME:
	  if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	  {
            // The button control has been clicked
	    pHandle = (XMPAHandle_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pHandle)
	      pHandle->EncoderConfig.bABRWriteXingVBRHeader = WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1));

            // Event processed.
            return (MRESULT) 0;
	  }
	  break;
	case CX_VBRMODE:
	  if (SHORT2FROMMP(mp1)==LN_SELECT)
	  {
	    long lID;
	    int iSelected;

            // An item has been selected! (selection changed)

            // Query selection
	    lID = (long) WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), LM_QUERYSELECTION, (MPARAM) 0, (MPARAM) 0);
	    iSelected = (int) WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), LM_QUERYITEMHANDLE, (MPARAM) lID, (MPARAM) 0);

	    pHandle = (XMPAHandle_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pHandle)
              pHandle->EncoderConfig.iVBRMode = iSelected;

            // Event processed.
            return (MRESULT) 0;
	  }
	  break;
	case RB_WRITEIDV1TAG:
	  if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	  {
            // The button control has been clicked
	    pHandle = (XMPAHandle_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pHandle)
	      pHandle->EncoderConfig.iWriteTAG = XMPA_WRITETAG_ID3V1;

            // Modify disabled/enabled stuffs
            DisableAndEnableID3Stuffs(hwnd);

            // Event processed.
            return (MRESULT) 0;
	  }
	  break;
	case RB_WRITEIDV11TAG:
	  if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	  {
            // The button control has been clicked
	    pHandle = (XMPAHandle_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pHandle)
	      pHandle->EncoderConfig.iWriteTAG = XMPA_WRITETAG_ID3V11;

            // Modify disabled/enabled stuffs
            DisableAndEnableID3Stuffs(hwnd);

            // Event processed.
            return (MRESULT) 0;
	  }
	  break;
	case RB_NOIDTAG:
	  if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	  {
            // The button control has been clicked
	    pHandle = (XMPAHandle_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pHandle)
	      pHandle->EncoderConfig.iWriteTAG = XMPA_WRITETAG_NONE;

            // Modify disabled/enabled stuffs
            DisableAndEnableID3Stuffs(hwnd);

            // Event processed.
            return (MRESULT) 0;
	  }
	  break;
	case CX_ID3GENRE:
	  if (SHORT2FROMMP(mp1)==LN_SELECT)
	  {
	    long lID;
	    int iSelected;

            // An item has been selected! (selection changed)

            // Query selection
	    lID = (long) WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), LM_QUERYSELECTION, (MPARAM) 0, (MPARAM) 0);
	    iSelected = (int) WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), LM_QUERYITEMHANDLE, (MPARAM) lID, (MPARAM) 0);

	    pHandle = (XMPAHandle_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pHandle)
              pHandle->EncoderConfig.chID3v1_Genre = iSelected;

            // Event processed.
            return (MRESULT) 0;
	  }
	  break;
	case EF_ID3TRACK:
	  if (SHORT2FROMMP(mp1)==EN_SETFOCUS)
	  {
            // Entering to control, save old value!
	    WinQueryDlgItemText(hwnd, SHORT1FROMMP(mp1), sizeof(achUndoBuf), achUndoBuf);
            return (MRESULT) 0;
	  }
	  if (SHORT2FROMMP(mp1)==EN_KILLFOCUS)
	  {
	    // Leaving control. Check new value, and restore old,
	    // if invalid value has been entered!

	    if (!WinQueryDlgItemShort(hwnd, SHORT1FROMMP(mp1), &sTemp, FALSE))
              sTemp = -1;

	    if ((sTemp<MIN_TRACK) || (sTemp>MAX_TRACK))
	    {
	      // Invalid track number entered, restore old value!

	      WinSetDlgItemText(hwnd, SHORT1FROMMP(mp1), achUndoBuf);
	      WinMessageBox(HWND_DESKTOP, hwnd,
			    "Please enter a valid track number!",
			    "Invalid value entered",
			    0,
                            MB_OK | MB_ERROR | MB_MOVEABLE);
	    } else
	    {
	      // Valid track number, store it!
              pHandle = (XMPAHandle_p) WinQueryWindowULong(hwnd, QWL_USER);
	      if (pHandle)
		pHandle->EncoderConfig.chID3v11_Track = sTemp;
	    }
            // Event processed.
            return (MRESULT) 0;
	  }
          break;

	default:
	  break;
      }
      break;

    default:
      break;
  } // End of switch msg
  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}



void WAWECALL plugin_About(HWND hwndOwner)
{
  WinMessageBox(HWND_DESKTOP, hwndOwner,
                pchAboutMessage, pchAboutTitle, 0, MB_OK | MB_MOVEABLE | MB_INFORMATION);
}

int WAWECALL plugin_Initialize(HWND hwndOwner,
                               WaWEPluginHelpers_p pWaWEPluginHelpers)
{
  if (!pWaWEPluginHelpers) return 0;

  // Save the helper functions for later usage!
  memcpy(&PluginHelpers, pWaWEPluginHelpers, sizeof(PluginHelpers));

  // Return success!
  return 1;
}

void WAWECALL plugin_Uninitialize(HWND hwndOwner)
{
  return;
}

void *                WAWECALL plugin_Create(char *pchFileName,
                                             WaWEExP_Create_Desc_p pFormatDesc,
                                             HWND hwndOwner)
{
  XMPAHandle_p pHandle;
  HWND hwndDlg;
  SWP swpDlg, swpParent;
  SHORT shTemp;

  if ((pFormatDesc->iChannels<1) ||
      (pFormatDesc->iChannels>2))
    return 0;

  if (pFormatDesc->iBits>(sizeof(long long)*8)) return 0;

  // Create a handle
  pHandle = (XMPAHandle_p) malloc(sizeof(XMPAHandle_t));
  if (!pHandle)
  {
#ifdef DEBUG_BUILD
    printf("[plugin_Create] : Out of memory!\n"); fflush(stdout);
#endif
    ErrorMsg("Out of memory!");
    return NULL;
  }

  // Set default settings
  memset(pHandle, 0, sizeof(XMPAHandle_t));
  pHandle->cb = sizeof(XMPAHandle_t);
  internal_SetDefaultEncoderConfig(&(pHandle->EncoderConfig));
  // Save file format info
  memcpy(&(pHandle->FileFormatDesc), pFormatDesc, sizeof(pHandle->FileFormatDesc));

  // Set up encoder config from format-specific strings!
  internal_SetEncoderConfigFromFormatSpec(&(pHandle->EncoderConfig), &(pFormatDesc->ChannelSetFormat));

  // Dialog window
  hwndDlg = WinLoadDlg(HWND_DESKTOP,
                       hwndOwner, // Owner will be the WaWE app itself
                       ExportDialogProc, // Dialog proc
                       hmodOurDLLHandle, // Source module handle
                       DLG_MP3ENCODERSETTINGS,
                       pHandle);
  if (!hwndDlg)
  {
    ErrorMsg("Could not create dialog window!");

    // Clean up
    free(pHandle); pHandle = NULL;
    return NULL;
  }

  // Center dialog in screen
  if (WinQueryWindowPos(hwndDlg, &swpDlg))
    if (WinQueryWindowPos(HWND_DESKTOP, &swpParent))
    {
      // Center dialog box within the screen
      int ix, iy;
      ix = swpParent.x + (swpParent.cx - swpDlg.cx)/2;
      iy = swpParent.y + (swpParent.cy - swpDlg.cy)/2;
      WinSetWindowPos(hwndDlg, HWND_TOP, ix, iy, 0, 0,
                      SWP_MOVE);
    }
  WinSetWindowPos(hwndDlg, HWND_TOP, 0, 0, 0, 0,
                  SWP_SHOW | SWP_ACTIVATE | SWP_ZORDER);
  WinSetFocus(HWND_DESKTOP, hwndDlg);

  // Process the dialog!
  if (WinProcessDlg(hwndDlg)!=PB_ENCODE)
  {
    // The user pressed cancel!
#ifdef DEBUG_BUILD
    printf("[plugin_Create] : The user has pressed cancel.\n");
#endif

    WinDestroyWindow(hwndDlg);
    // Clean up
    free(pHandle); pHandle = NULL;

    return NULL;
  }

  // Ok, the user has perssed the Encode button.
  // The dialog window proc has already modified most of the encoder settings,
  // except the mp3 ID tag, so now we query those!

  WinQueryDlgItemText(hwndDlg, EF_ID3TITLE,
		      sizeof(pHandle->EncoderConfig.achID3v1_SongTitle),
		      pHandle->EncoderConfig.achID3v1_SongTitle);
  WinQueryDlgItemText(hwndDlg, EF_ID3ARTIST,
		      sizeof(pHandle->EncoderConfig.achID3v1_Artist),
		      pHandle->EncoderConfig.achID3v1_Artist);
  WinQueryDlgItemText(hwndDlg, EF_ID3ALBUM,
		      sizeof(pHandle->EncoderConfig.achID3v1_Album),
		      pHandle->EncoderConfig.achID3v1_Album);
  WinQueryDlgItemText(hwndDlg, EF_ID3YEAR,
		      sizeof(pHandle->EncoderConfig.achID3v1_Year),
		      pHandle->EncoderConfig.achID3v1_Year);
  WinQueryDlgItemText(hwndDlg, EF_ID3COMMENT,
		      sizeof(pHandle->EncoderConfig.achID3v1_Comment),
		      pHandle->EncoderConfig.achID3v1_Comment);
  shTemp = 0;
  WinQueryDlgItemShort(hwndDlg, EF_ID3TRACK,
                      &shTemp, FALSE);
  pHandle->EncoderConfig.chID3v11_Track = shTemp;

  // Destroy window
  WinDestroyWindow(hwndDlg);

  // Open the file
  pHandle->hFile = fopen(pchFileName, "wb+");
  if (!(pHandle->hFile))
  {
    ErrorMsg("Could not create file!\n");
    free(pHandle);
    return NULL;
  }

  // Set up the encoding!
  pHandle->pLameHandle = lame_init();
  if (!(pHandle->pLameHandle))
  {
    ErrorMsg("Could not initialize LAME encoder!\n");
    fclose(pHandle->hFile);
    free(pHandle);
    return NULL;
  }

  lame_set_in_samplerate(pHandle->pLameHandle, pFormatDesc->iFrequency);
  lame_set_num_channels(pHandle->pLameHandle, pFormatDesc->iChannels);

  if (pHandle->EncoderConfig.iEncodingMode == XMPA_ENCODINGMODE_VBR)
    lame_set_bWriteVbrTag(pHandle->pLameHandle, pHandle->EncoderConfig.bVBRWriteXingVBRHeader?1:0);
  else
  if (pHandle->EncoderConfig.iEncodingMode == XMPA_ENCODINGMODE_ABR)
    lame_set_bWriteVbrTag(pHandle->pLameHandle, pHandle->EncoderConfig.bABRWriteXingVBRHeader?1:0);
  else
    lame_set_bWriteVbrTag(pHandle->pLameHandle, 0);

  lame_set_quality(pHandle->pLameHandle, pHandle->EncoderConfig.iAlgorithmQuality);
  if (pHandle->EncoderConfig.iStereoMode!=XMPA_STEREOMODE_DEFAULT)
    lame_set_mode(pHandle->pLameHandle, pHandle->EncoderConfig.iStereoMode);

  lame_set_copyright(pHandle->pLameHandle, pHandle->EncoderConfig.bCopyright);
  lame_set_original(pHandle->pLameHandle, pHandle->EncoderConfig.bOriginal);
  lame_set_error_protection(pHandle->pLameHandle, pHandle->EncoderConfig.bErrorProtection);
  lame_set_extension(pHandle->pLameHandle, pHandle->EncoderConfig.bPrivate);

  switch (pHandle->EncoderConfig.iEncodingMode)
  {
    case XMPA_ENCODINGMODE_VBR:
      if (pHandle->EncoderConfig.iVBRMode == XMPA_VBRMODE_NEWALGORITHM)
	lame_set_VBR(pHandle->pLameHandle, vbr_mtrh);
      else
        lame_set_VBR(pHandle->pLameHandle, vbr_rh);
      lame_set_VBR_q(pHandle->pLameHandle, pHandle->EncoderConfig.iVBRQuality);
      break;
    case XMPA_ENCODINGMODE_ABR:
      lame_set_VBR(pHandle->pLameHandle, vbr_abr);
      lame_set_brate(pHandle->pLameHandle, pHandle->EncoderConfig.iABRMeanBitrate);
      lame_set_VBR_mean_bitrate_kbps(pHandle->pLameHandle, pHandle->EncoderConfig.iABRMeanBitrate);
      if (pHandle->EncoderConfig.bABRLimitMinBitrate)
	lame_set_VBR_min_bitrate_kbps(pHandle->pLameHandle, pHandle->EncoderConfig.iABRMinBitrate);
      if (pHandle->EncoderConfig.bABRLimitMaxBitrate)
        lame_set_VBR_max_bitrate_kbps(pHandle->pLameHandle, pHandle->EncoderConfig.iABRMaxBitrate);
      if (pHandle->EncoderConfig.bABREnforceMinBitrate)
	lame_set_VBR_hard_min(pHandle->pLameHandle, 1);
      break;
    case XMPA_ENCODINGMODE_CBR:
    default:
      lame_set_VBR(pHandle->pLameHandle, vbr_off);
      lame_set_brate(pHandle->pLameHandle, pHandle->EncoderConfig.iCBRBitrate);
      break;
  }

  // Set up ID3 stuff!
  if (pHandle->EncoderConfig.iWriteTAG != XMPA_WRITETAG_NONE)
  {
    char achNum[32];
    // We have to write mp3 ID tag!
    id3tag_init(pHandle->pLameHandle);
    // We support ID3V1 tags only... yet.
    id3tag_v1_only(pHandle->pLameHandle);

    id3tag_set_title(pHandle->pLameHandle, pHandle->EncoderConfig.achID3v1_SongTitle);
    id3tag_set_artist(pHandle->pLameHandle, pHandle->EncoderConfig.achID3v1_Artist);
    id3tag_set_album(pHandle->pLameHandle, pHandle->EncoderConfig.achID3v1_Album);
    id3tag_set_year(pHandle->pLameHandle, pHandle->EncoderConfig.achID3v1_Year);
    id3tag_set_comment(pHandle->pLameHandle, pHandle->EncoderConfig.achID3v1_Comment);
    if (pHandle->EncoderConfig.iWriteTAG == XMPA_WRITETAG_ID3V11)
    {
      // The Track is included only in ID3 v1.1 tags!
      sprintf(achNum, "%d", pHandle->EncoderConfig.chID3v11_Track);
      id3tag_set_track(pHandle->pLameHandle, achNum);
    }
    sprintf(achNum, "%d", pHandle->EncoderConfig.chID3v1_Genre);
    id3tag_set_genre(pHandle->pLameHandle, achNum);
  }

  if (lame_init_params(pHandle->pLameHandle)<0)
  {
    lame_close(pHandle->pLameHandle);
    fclose(pHandle->hFile);
    free(pHandle);

    ErrorMsg("Error initializing LAME with the given parameters!\n"
	     "It might be that LAME does not support the combination of "
	     "settings you gave.\n\n"
	     "Please try again with different settings!");

    return NULL;
  }

#ifdef DEBUG_BUILD
  internal_PrintEncoderConfig(&(pHandle->EncoderConfig));
//  lame_print_internals(pHandle->pLameHandle);
//  lame_print_config(pHandle->pLameHandle);
#endif


  return pHandle;
}


WaWESize_t            WAWECALL plugin_Write(void *pHandle,
                                            char *pchBuffer,
                                            WaWESize_t cBytes)
{
  XMPAHandle_p pXMPAHandle;
  unsigned char *pchMP3Buf;
  int            iMP3BufSize;
  int            iBytesPerSample;
  int            iNumSamples;
  int            i, iSample, bFailed;
  int            iEncoded;
  long         **pplChannelSamples;
  long long      llTemp;

  if (!pHandle) return 0;

  pXMPAHandle = (XMPAHandle_p) pHandle;

  if ((pXMPAHandle->FileFormatDesc.iChannels<1) ||
      (pXMPAHandle->FileFormatDesc.iChannels>2)) return 0;
  if (pXMPAHandle->FileFormatDesc.iBits>(sizeof(long long)*8)) return 0;

  iBytesPerSample = (pXMPAHandle->FileFormatDesc.iBits+7)/8;
#ifdef DEBUG_BUILD
//  printf("[plugin_Write] : iBytesPerSample = %d\n", iBytesPerSample); fflush(stdout);
#endif

  // Separate the audio samples to arrays, because LAME wants it in that form.
  pplChannelSamples = (long **) malloc(sizeof(long *) * pXMPAHandle->FileFormatDesc.iChannels);
  if (!pplChannelSamples)
  {
    // Not enough memory!
    return 0;
  }

  iNumSamples = cBytes / iBytesPerSample / pXMPAHandle->FileFormatDesc.iChannels;
  iMP3BufSize = (iNumSamples * 5) / 4 + 7200;

#ifdef DEBUG_BUILD
//  printf("[plugin_Write] : iNumSamples = %d\n", iNumSamples); fflush(stdout);
//  printf("[plugin_Write] : iMP3BufSize = %d\n", iMP3BufSize); fflush(stdout);
#endif

  pchMP3Buf = (unsigned char *) malloc(iMP3BufSize);
  if (!pchMP3Buf)
  {
    // Not enough memory!
    free(pplChannelSamples);
    return 0;
  }

  bFailed = FALSE;
  for (i=0; i<pXMPAHandle->FileFormatDesc.iChannels; i++)
  {
    pplChannelSamples[i] = (long *) malloc(sizeof(long) * iNumSamples);
    if (!(pplChannelSamples[i])) bFailed = TRUE;
  }

  if (bFailed)
  {
    // Could not allocate every buffer we wanted to!
    free(pchMP3Buf);
    for (i=0; i<pXMPAHandle->FileFormatDesc.iChannels; i++)
    {
      if (pplChannelSamples[i])
        free(pplChannelSamples[i]);
    }
    free(pplChannelSamples);
    return 0;
  }


#ifdef DEBUG_BUILD
//  printf("[plugin_Write] : Convert samples\n"); fflush(stdout);
#endif

  // Also convert samples to a common format so we only have to use one function of LAME.
  for (i=0; i<pXMPAHandle->FileFormatDesc.iChannels; i++)
    for (iSample=0; iSample<iNumSamples; iSample++)
    {
      llTemp = 0;
#ifdef DEBUG_BUILD
//      printf("m"); fflush(stdout);
#endif
      memcpy(&llTemp,
	     pchBuffer+
	     iSample*iBytesPerSample*pXMPAHandle->FileFormatDesc.iChannels+
	     i*iBytesPerSample, iBytesPerSample);
      // Scale it up to be a long-long
      llTemp <<= (sizeof(llTemp)*8 - pXMPAHandle->FileFormatDesc.iBits);
      // Make sign ok
      if (pXMPAHandle->FileFormatDesc.iIsSigned==0)
	llTemp += LONGLONG_MIN;
      // Scale it down to be long
      llTemp >>= (sizeof(llTemp)*8 - sizeof(long)*8);

#ifdef DEBUG_BUILD
//      printf("s[%d,%d]", i, iSample); fflush(stdout);
#endif
      // Store sample
      pplChannelSamples[i][iSample] = (long) llTemp;
    }


#ifdef DEBUG_BUILD
//  printf("[plugin_Write] : Encode samples\n"); fflush(stdout);
#endif

  // Send buffers to encoder
  if (pXMPAHandle->FileFormatDesc.iChannels==1)
  {
    // Mono input
    iEncoded = lame_encode_buffer_long2(pXMPAHandle->pLameHandle,
					pplChannelSamples[0],
					pplChannelSamples[0],
					iNumSamples,
					pchMP3Buf,
					iMP3BufSize);
  } else
  {
    // Stereo input
    iEncoded = lame_encode_buffer_long2(pXMPAHandle->pLameHandle,
					pplChannelSamples[0],
					pplChannelSamples[1],
					iNumSamples,
					pchMP3Buf,
					iMP3BufSize);
  }

#ifdef DEBUG_BUILD
//  printf("[plugin_Write] : Encoded %d bytes\n", iEncoded); fflush(stdout);
#endif

  if (iEncoded>0)
  {
    // Fine, it has encoded some bytes, let's store it in the file!
    fwrite(pchMP3Buf, 1, iEncoded, pXMPAHandle->hFile);
  }

#ifdef DEBUG_BUILD
//  printf("[plugin_Write] : Free...\n"); fflush(stdout);
#endif


  free(pchMP3Buf);
  for (i=0; i<pXMPAHandle->FileFormatDesc.iChannels; i++)
  {
    if (pplChannelSamples[i])
      free(pplChannelSamples[i]);
  }
  free(pplChannelSamples);

#ifdef DEBUG_BUILD
//  printf("[plugin_Write] : Return\n"); fflush(stdout);
#endif

  return cBytes;
}


long                  WAWECALL plugin_Close(void *pHandle, WaWEChannelSet_p pSavedChannelSet)
{
  XMPAHandle_p pXMPAHandle;
  unsigned char *pchMP3Buf;
  int iEncoded;

  if (!pHandle) return 0;

  pXMPAHandle = (XMPAHandle_p) pHandle;

#ifdef DEBUG_BUILD
//  printf("[plugin_Close] : Flush buffers\n"); fflush(stdout);
#endif

  // Flush buffers
  pchMP3Buf = (unsigned char *) malloc(7200);
  iEncoded = lame_encode_flush(pXMPAHandle->pLameHandle, pchMP3Buf, 7200);
  if (iEncoded>0)
    fwrite(pchMP3Buf, 1, iEncoded, pXMPAHandle->hFile);

  // Write VBR stuff if required
  if (
      ((pXMPAHandle->EncoderConfig.iEncodingMode==XMPA_ENCODINGMODE_VBR) &&
       (pXMPAHandle->EncoderConfig.bVBRWriteXingVBRHeader))
      ||
      ((pXMPAHandle->EncoderConfig.iEncodingMode==XMPA_ENCODINGMODE_ABR) &&
       (pXMPAHandle->EncoderConfig.bABRWriteXingVBRHeader))
     )
  {

#ifdef DEBUG_BUILD
//    printf("[plugin_Close] : Write VBR tag\n"); fflush(stdout);
#endif


    // The LAME API is not really designed for being in a separate DLL, unfortunately.
    // We'd have to use the lame_mp3_tags_fid() API here, which should get a file
    // handle, but we don't want to mess with file handles across runtime libraries!

    // So, we include that function here, and link the functions it uses statically to
    // this DLL, so those will use the runtime library of this module.

    if (pXMPAHandle->pLameHandle->bWriteVbrTag)
    {
      /* Map VBR_q to Xing quality value: 0=worst, 100=best */
      int     nQuality = ((9-pXMPAHandle->pLameHandle->VBR_q) * 100) / 9;

#ifdef DEBUG_BUILD
//      printf("[plugin_Close] : nQuality is %d\n", nQuality); fflush(stdout);
#endif

      /* Write Xing header again */
      if (!fseek(pXMPAHandle->hFile, 0, SEEK_SET))
      {
        int rc;
#ifdef DEBUG_BUILD
//	printf("[plugin_Close] : Calling PutVbrTag()\n"); fflush(stdout);
#endif
	rc = PutVbrTag(pXMPAHandle->pLameHandle,
		       pXMPAHandle->hFile,
		       nQuality);
#ifdef DEBUG_BUILD
//	printf("[plugin_Close] : PutVbrTag() rc = %d\n", rc); fflush(stdout);
#endif
      }
    }
  }

#ifdef DEBUG_BUILD
//  printf("[plugin_Close] : Cleanup\n"); fflush(stdout);
#endif

  // Cleanup!
  lame_close(pXMPAHandle->pLameHandle);
  fclose(pXMPAHandle->hFile);
  free(pXMPAHandle);

  if (pSavedChannelSet)
  {
    // Modify channel-set format, set FourCC to "MPA"!
    PluginHelpers.fn_WaWEHelper_ChannelSetFormat_DeleteKeysAndSubkeys(&(pSavedChannelSet->ChannelSetFormat),
                                                                      "FourCC");
    PluginHelpers.fn_WaWEHelper_ChannelSetFormat_WriteKey(&(pSavedChannelSet->ChannelSetFormat),
                                                          "FourCC",
                                                          "MPA");
  }

#ifdef DEBUG_BUILD
//  printf("[plugin_Close] : Return\n"); fflush(stdout);
#endif
  return 1;
}

long                  WAWECALL plugin_IsFormatSupported(WaWEExP_Create_Desc_p pFormatDesc)
{
  char achTemp[64];
  int rc;

  if ((pFormatDesc->iChannels<1) ||
      (pFormatDesc->iChannels>2))
    return 0;

  if (pFormatDesc->iBits>(sizeof(long long)*8)) return 0;

  rc = PluginHelpers.fn_WaWEHelper_ChannelSetFormat_ReadKey(&(pFormatDesc->ChannelSetFormat),
                                                            "FourCC", 0,
                                                            achTemp, sizeof(achTemp));
#ifdef DEBUG_BUILD
//  printf("[plugin_IsFormatSupported] : rc = %d, achTemp = [%s]\n", rc, achTemp);
#endif

  if ((rc==WAWEHELPER_NOERROR) &&
      ((!strcmp(achTemp, "MPA")) ||
       (!strcmp(achTemp, "MPA "))))
  {
#ifdef DEBUG_BUILD
//    printf("[plugin_IsFormatSupported] : Yes!\n");
#endif
    return 1;
  }
  return 0;
}

// Exported functions:

WAWEDECLSPEC long WAWECALL WaWE_GetPluginInfo(long lPluginDescSize,
                                              WaWEPlugin_p pPluginDesc,
                                              int  iWaWEMajorVersion,
                                              int  iWaWEMinorVersion)
{
  // Check parameters and do version check:
  if ((!pPluginDesc) || (sizeof(WaWEPlugin_t)!=lPluginDescSize)) return 0;
  if ((iWaWEMajorVersion!=WAWE_APPVERSIONMAJOR) ||
      (iWaWEMinorVersion!=WAWE_APPVERSIONMINOR)) return 0;

  // Version seems to be Ok.
  // So, report info about us!
  strcpy(pPluginDesc->achName, pchPluginName);

  // Make the importance of this plugin high, so if we report that we can handle
  // a file, we'll be used. Hey, who else would handle MP3/MP2 files if not us?
  pPluginDesc->iPluginImportance = WAWE_PLUGIN_IMPORTANCE_MAX - 5;

  pPluginDesc->iPluginType = WAWE_PLUGIN_TYPE_EXPORT;

  // Plugin functions:
  pPluginDesc->fnAboutPlugin = plugin_About;
  pPluginDesc->fnConfigurePlugin = NULL;
  pPluginDesc->fnInitializePlugin = plugin_Initialize;
  pPluginDesc->fnPrepareForShutdown = NULL;
  pPluginDesc->fnUninitializePlugin = plugin_Uninitialize;

  pPluginDesc->TypeSpecificInfo.ExportPluginInfo.fnIsFormatSupported = plugin_IsFormatSupported;
  pPluginDesc->TypeSpecificInfo.ExportPluginInfo.fnCreate            = plugin_Create;
  pPluginDesc->TypeSpecificInfo.ExportPluginInfo.fnWrite             = plugin_Write;
  pPluginDesc->TypeSpecificInfo.ExportPluginInfo.fnClose             = plugin_Close;

  // Save module handle for later use
  // (We will need it when we want to load some resources from DLL)
  hmodOurDLLHandle = pPluginDesc->hmodDLL;

  // Return success!
  return 1;
}
