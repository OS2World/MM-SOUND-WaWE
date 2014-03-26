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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <process.h> // for _beginthread()

#include "PPlayConfig.h"
#include "PPlayDART.h"

#include "PPlay-Resources.h"


// Buffer states:
#define BUFFER_EMPTY       0
#define BUFFER_USED        1

typedef struct _DARTConfiguration_struct
{
  int iDeviceID;
  int bDeviceShared;
  int iNumBufs;
  int iBufSize;
} DARTConfiguration_t, *DARTConfiguration_p;

// This is a structure, returned by the
// DART_QueryAvailableDevices() function,
// and destroyed by the
// DART_DestroyAvailableDeviceList() one!
typedef struct _DARTAvailableDevices {
  int iNumOfDevices;      // Number of detected AMP devices
  long *lDevNameIndex;    // Indexes to DevNames, pointing to the start position of the null-terminated strings
  char *DevNames;         // Names of devices
} DARTAvailableDevices_t, *DARTAvailableDevices_p;

typedef struct _MixBufferDesc_struct {
  int iBufferUsage;      // BUFFER_EMPTY or BUFFER_USED
  int bLastBuffer;       // TRUE, if this was the last buffer (DART callback will set the global bReachedLastBuffer)
} MixBufferDesc_t, *MixBufferDesc_p;


// ----   Variables   ----

DARTConfiguration_t DARTConfiguration;

static int bDARTDriverRunning = 0; // Non-zero if we're currently running

static int iDARTDeviceOrdNow;
static int iDARTFreqNow;
static int iDARTBitsNow;
static int iDARTChannelsNow;
static int iDARTNumBufsNow;
static int iDARTBufSizeNow;
static pfn_ReadStream_callback pfnReadStreamNow;
static pfn_PlaybackFinished_callback pfnPlaybackFinishedNow;


static MCI_BUFFER_PARMS BufferParms;     // Sound buffer parameters
static MCI_MIX_BUFFER *pMixBuffers;      // Sound buffers
static MCI_MIXSETUP_PARMS MixSetupParms; // Mixer setup parameters
static TID tidDARTFeederThread;          // Thread ID of DART feeder thread
static HEV hevAudioBufferPlayed;         // Event semaphore to indicate that an audio buffer has been played by DART
static int bReachedLastBuffer;           // TRUE, if a buffer with bLastBuffer reached!

static char achTextUndoBuf[1024];

#ifdef DEBUG_BUILD
static void MciError(ULONG ulError)
{
  char szBuffer[2048];
  ULONG rc;
  rc = mciGetErrorString(ulError, szBuffer, 2048);
  printf("[MciError] : %s\n",szBuffer);
}
#endif


//---------------------------------------------------------------------
// DART_DestroyAvailableDeviceList
//
// Destroys the list of available DART devices, allocated by the
// DART_QueryAvailableDevices() function.
//---------------------------------------------------------------------
static void DART_DestroyAvailableDeviceList(DARTAvailableDevices_p pAvailableDevices)
{
  if (!pAvailableDevices) return;
  free(pAvailableDevices->lDevNameIndex);
  free(pAvailableDevices->DevNames);
  free(pAvailableDevices);
}

//---------------------------------------------------------------------
// DART_QueryAvailableDevices
//
// Queries the number of available DART devices, and makes a list of
// them. See the header file for the definition of 
// _DARTAvailableDevices  structure!
//---------------------------------------------------------------------
static DARTAvailableDevices_p DART_QueryAvailableDevices()
{
  MCI_SYSINFO_PARMS SysInfo;
  MCI_SYSINFO_LOGDEVICE SysInfoLogDevice;
  char Buffer[1024];
  int iNumOfDartDevices = 0;
  int rc;
  DARTAvailableDevices_p result = NULL;
  long lActStrPos;

  // First query the number of DART devices!

  memset(&SysInfo, 0, sizeof(SysInfo));
  memset(Buffer, 0, sizeof(Buffer));
  SysInfo.usDeviceType = MCI_DEVTYPE_AUDIO_AMPMIX;
  SysInfo.pszReturn = (PSZ) Buffer;
  SysInfo.ulRetSize = 1024;

  rc = mciSendCommand(0,                                // Don't know device ID yet
		      MCI_SYSINFO,
		      MCI_SYSINFO_QUANTITY | MCI_WAIT,
		      (PVOID) &SysInfo,
		      0);                               // No user parameter
  if (rc != MCIERR_SUCCESS)
    return NULL; // Could not query the number of DART devices

  iNumOfDartDevices = atoi(Buffer);
  if (iNumOfDartDevices)
  {
    int i;
    // Ok, there are devices to make a list from, so allocate memory for
    // the result!

    result = (DARTAvailableDevices_p) malloc(sizeof(DARTAvailableDevices_t));
    if (!result) return NULL; // Could not allocate memory!

    result->iNumOfDevices = 0;
    result->lDevNameIndex = NULL;
    result->DevNames = NULL;
    lActStrPos = 0;

    // Go through all the devices:
    for (i = 0; i<=iNumOfDartDevices; i++)
    {
      // Get the install name of this Dart device
      memset(&SysInfo, 0, sizeof(SysInfo));
      memset(Buffer, 0, sizeof(Buffer));
      SysInfo.usDeviceType = MCI_DEVTYPE_AUDIO_AMPMIX;
      SysInfo.ulNumber = i;
      SysInfo.pszReturn = (PSZ) Buffer;
      SysInfo.ulRetSize = 1024;
      rc = mciSendCommand(0, // Device ID
			  MCI_SYSINFO,
			  MCI_SYSINFO_INSTALLNAME | MCI_WAIT,
			  (PVOID) &SysInfo,
			  0);
      if (rc == MCIERR_SUCCESS)
      {
        // Using the install name, we can query the description too:
	// Get the description for this install name
	memset(&SysInfo, 0, sizeof(SysInfo));
	memset(&SysInfoLogDevice, 0, sizeof(SysInfoLogDevice));
	strcpy(SysInfoLogDevice.szInstallName, Buffer);
  
	SysInfo.ulItem = MCI_SYSINFO_QUERY_DRIVER;
	SysInfo.pSysInfoParm = &SysInfoLogDevice;
	rc = mciSendCommand(0, // Device ID
			    MCI_SYSINFO,
			    MCI_SYSINFO_ITEM | MCI_WAIT,
			    (PVOID) &SysInfo,
			    0);
  
	if (rc == MCIERR_SUCCESS)
	{
	  if (i==0)
	    // Default device
	    sprintf(Buffer, "Default [%s (%s)]", SysInfoLogDevice.szProductInfo, SysInfoLogDevice.szVersionNumber);
	  else
            // Not the default device
	    sprintf(Buffer, "%s (%s)", SysInfoLogDevice.szProductInfo, SysInfoLogDevice.szVersionNumber);

          // Save this information in the result structure!
	  result->iNumOfDevices++;

       	  if (!(result->lDevNameIndex))
	    result->lDevNameIndex = (long *)malloc(result->iNumOfDevices*sizeof(long));
	  else
            result->lDevNameIndex = (long *)realloc(result->lDevNameIndex, result->iNumOfDevices*sizeof(long));

	  result->lDevNameIndex[result->iNumOfDevices-1] = lActStrPos;

          if (!(result->DevNames))
	    result->DevNames = (char *)malloc(strlen(Buffer)+1);
	  else
            result->DevNames = (char *)realloc(result->DevNames, lActStrPos+strlen(Buffer)+1);

	  strcpy(result->DevNames+lActStrPos, Buffer);
          lActStrPos+=strlen(Buffer)+1;
	}
      }
    }
  }
  return result;
}


void DART_SetDefaultConfig()
{
  DARTConfiguration.iDeviceID = 0;     // Default device
  DARTConfiguration.bDeviceShared = 1; // Use the device shared!
  DARTConfiguration.iNumBufs = 2;      // Use two 8k buffers
  DARTConfiguration.iBufSize = 8192;
}

void DART_LoadConfig(FILE *hFile)
{
  char *pchKey, *pchValue;

  pchKey = NULL;
  pchValue = NULL;

  // Go to the beginning of the file
  fseek(hFile, 0, SEEK_SET);
  // Process every entry of the config file
  while (GetConfigFileEntry(hFile, &pchKey, &pchValue))
  {
    // Found a key/value pair!
    if (!stricmp(pchKey, "DART_iDeviceID"))
      DARTConfiguration.iDeviceID = atoi(pchValue);
    else
    if (!stricmp(pchKey, "DART_bDeviceShared"))
      DARTConfiguration.bDeviceShared = atoi(pchValue);
    else
    if (!stricmp(pchKey, "DART_iNumBufs"))
      DARTConfiguration.iNumBufs = atoi(pchValue);
    else
    if (!stricmp(pchKey, "DART_iBufSize"))
      DARTConfiguration.iBufSize = atoi(pchValue);

    free(pchKey); pchKey = NULL;
    free(pchValue); pchValue = NULL;
  }
  // Ok, config file processed.
}

void DART_SaveConfig(FILE *hFile)
{
  fprintf(hFile, "\n");
  fprintf(hFile, "; ----------------------------------------------------------\n");
  fprintf(hFile, "; DART configuration\n");
  fprintf(hFile, "; ----------------------------------------------------------\n");
  fprintf(hFile, "\n");
  fprintf(hFile, "DART_iDeviceID = %d\n",
          DARTConfiguration.iDeviceID);
  fprintf(hFile, "DART_bDeviceShared = %d\n",
          DARTConfiguration.bDeviceShared);
  fprintf(hFile, "DART_iNumBufs = %d\n",
          DARTConfiguration.iNumBufs);
  fprintf(hFile, "DART_iBufSize = %d\n",
          DARTConfiguration.iBufSize);
}


MRESULT EXPENTRY fnDARTNotebookPage( HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2 )
{
  char sTemp[256];
  int iTemp, iToSelect, iItemCount;
  DARTAvailableDevices_p pDevs;

  switch (msg)
  {
    case WM_SETDEFAULTVALUES:
      DART_SetDefaultConfig();
      // Now set stuffs
      // ---
      iItemCount = (int) WinSendMsg(WinWindowFromID(hwnd, CX_SOUNDDEVICE), LM_QUERYITEMCOUNT, NULL, NULL);
      iToSelect = 0;
      for (iTemp = 0; iTemp<iItemCount; iTemp++)
      {
        if (PluginConfiguration.iDeviceToUse == (int) WinSendMsg(WinWindowFromID(hwnd, CX_SOUNDDEVICE), LM_QUERYITEMHANDLE, MPFROMSHORT(iTemp), (MPARAM) NULL))
          iToSelect = iTemp;
      }
      WinSendMsg(WinWindowFromID(hwnd, CX_SOUNDDEVICE), LM_SELECTITEM,
                 (MPARAM) (iToSelect), (MPARAM) (TRUE));


      // ---
      // Set Shared flag:
      WinSendMsg(WinWindowFromID(hwnd, CB_USEDEVICESHARED), BM_SETCHECK,
                 (MPARAM) DARTConfiguration.bDeviceShared, (MPARAM) (0));
      // Set Buffer size:
      WinSetDlgItemText(hwnd, EF_SIZEOFBUF, (MPARAM) itoa(DARTConfiguration.iBufSize, sTemp, 10));
      // Set number of buffers:
      WinSetDlgItemText(hwnd, EF_NUMOFBUFS, (MPARAM) itoa(DARTConfiguration.iNumBufs, sTemp, 10));

      return (MRESULT) TRUE;
    case WM_INITDLG:
      // Initialization of this page

      // Fill the sound device combo with list of available sound devices
      iToSelect = 0;

      // Fill the Sound devices combo box, and set from the configuration:
      // First one is the No Sound device, second is the default device.
      iTemp = (int) WinSendMsg(WinWindowFromID(hwnd, CX_SOUNDDEVICE), LM_INSERTITEM,
                               (MPARAM) LIT_END, (MPARAM) ("No Sound"));
      WinSendMsg(WinWindowFromID(hwnd, CX_SOUNDDEVICE), LM_SETITEMHANDLE,
                 (MPARAM) iTemp, (MPARAM) (-1));

      iToSelect = iTemp;

      pDevs = DART_QueryAvailableDevices();
      if (pDevs)
      {
        int i;
        for (i=0; i<pDevs->iNumOfDevices; i++)
        {
          iTemp = (int) WinSendMsg(WinWindowFromID(hwnd, CX_SOUNDDEVICE), LM_INSERTITEM,
                                   (MPARAM) LIT_END, (MPARAM) ((pDevs->DevNames)+(pDevs->lDevNameIndex[i])));
          WinSendMsg(WinWindowFromID(hwnd, CX_SOUNDDEVICE), LM_SETITEMHANDLE,
                     (MPARAM) iTemp, (MPARAM) i);

          if (i == DARTConfiguration.iDeviceID)
            iToSelect = iTemp;
#ifdef DEBUG_BUILD
          printf("DART Dev %d: %s\n", i, (pDevs->DevNames)+(pDevs->lDevNameIndex[i]));
#endif
        }
        DART_DestroyAvailableDeviceList(pDevs);
        WinSendMsg(WinWindowFromID(hwnd, CX_SOUNDDEVICE), LM_SELECTITEM,
                   (MPARAM) (iToSelect), (MPARAM) (TRUE));
      }
      // Set Shared flag:
      WinSendMsg(WinWindowFromID(hwnd, CB_USEDEVICESHARED), BM_SETCHECK,
	     (MPARAM) DARTConfiguration.bDeviceShared, (MPARAM) (0));
      // Set Buffer size:
      WinSetDlgItemText(hwnd, EF_SIZEOFBUF, (MPARAM) itoa(DARTConfiguration.iBufSize, sTemp, 10));
      // Set number of buffers:
      WinSetDlgItemText(hwnd, EF_NUMOFBUFS, (MPARAM) itoa(DARTConfiguration.iNumBufs, sTemp, 10));

      break;
    case WM_CONTROL:  // ------------------- WM_CONTROL messages ---------------------------
      switch SHORT1FROMMP(mp1) {
        case EF_SIZEOFBUF:         // - - - - - - - Message from entry field - - - - - - -
	  switch (SHORT2FROMMP(mp1)) {
	    case EN_SETFOCUS:                 // Entered into entry field
	      // Save old text from here:
	      WinQueryDlgItemText(hwnd, SHORT1FROMMP(mp1),
				  1024, achTextUndoBuf);
	      break;
	    case EN_KILLFOCUS:                // Leaving entry field
	      {
		// Check the text there, and restore old one, if it's invalid!
		WinQueryDlgItemText(hwnd, SHORT1FROMMP(mp1),
				    256, sTemp);
		iTemp = atoi(sTemp);
		if ((iTemp>=512) && (iTemp<=256*1024))
		{
		  DARTConfiguration.iBufSize = iTemp;
		  WinSetDlgItemText(hwnd, SHORT1FROMMP(mp1), (MPARAM) itoa(iTemp, sTemp, 10));
		}
                else
		  WinSetDlgItemText(hwnd, SHORT1FROMMP(mp1), achTextUndoBuf);

		break;
	      }
	    default:
	      break;
          }
          return (MPARAM) TRUE;
	  break; // End of EF_SIZEOFBUF processing
        case EF_NUMOFBUFS:           // - - - - - - - Message from entry field - - - - - - -
	  switch (SHORT2FROMMP(mp1)) {
	    case EN_SETFOCUS:                 // Entered into entry field
	      // Save old text from here:
	      WinQueryDlgItemText(hwnd, SHORT1FROMMP(mp1),
				  1024, achTextUndoBuf);
	      break;
	    case EN_KILLFOCUS:                // Leaving entry field
	      {
		// Check the text there, and restore old one, if it's invalid!
		WinQueryDlgItemText(hwnd, SHORT1FROMMP(mp1),
				    256, sTemp);
		iTemp = atoi(sTemp);
		if ((iTemp>=2) && (iTemp<=64))
		{
		  DARTConfiguration.iNumBufs = iTemp;
		  WinSetDlgItemText(hwnd, SHORT1FROMMP(mp1), (MPARAM) itoa(iTemp, sTemp, 10));
		}
                else
		  WinSetDlgItemText(hwnd, SHORT1FROMMP(mp1), achTextUndoBuf);

		break;
	      }
	    default:
	      break;
          }
          return (MPARAM) TRUE;
          break; // End of EF_NUMOFBUFS processing
          // -------------------------------- Combo boxes --------------------

        case CX_SOUNDDEVICE:
          if (SHORT2FROMMP(mp1) == LN_SELECT)
          {
            DARTConfiguration.iDeviceID = (int) (WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1),
                                                                   LM_QUERYSELECTION, (MPARAM) LIT_CURSOR, 0))-1;
	  }
	  break;
          // -------------------------------- Checkboxes --------------------
	case CB_USEDEVICESHARED:
	  if (SHORT2FROMMP(mp1) == BN_CLICKED)
	  {
            DARTConfiguration.bDeviceShared =
              WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1));
          }
          break;
        default:
	  break;
      } // end of switch inside WM_CONTROL
      break;
    default:
      break;
  } // end of switch of msg

  // Call the default procedure if it has not been processed yet!
  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}

//---------------------------------------------------------------------
// DARTEventFunc
//
// This function is called by DART, when an event occures, like end of 
// playback of a buffer, etc...
//---------------------------------------------------------------------
static LONG APIENTRY DARTEventFunc(ULONG ulStatus,
                                  PMCI_MIX_BUFFER pBuffer,
                                  ULONG ulFlags)
{
  if (ulFlags && MIX_WRITE_COMPLETE)
  { // Playback of buffer completed!

    // Get pointer to buffer description
    MixBufferDesc_p pBufDesc;

    if (pBuffer)
    {
      pBufDesc = (MixBufferDesc_p) (*pBuffer).ulUserParm;

      if (pBufDesc)
      {
        // Set the buffer to be empty
        pBufDesc->iBufferUsage = BUFFER_EMPTY;
        // Check if it was the last buffer!
        if (pBufDesc->bLastBuffer)
          bReachedLastBuffer = 1;
        // And notify DART feeder thread that it will have to work a bit.
        DosPostEventSem(hevAudioBufferPlayed);
      }
    }
  }
  return TRUE;
}

//---------------------------------------------------------------------
// DARTFeederThreadFunc
//
// This is the DART feeder thread, that creates and queues audio data
// into DART.
//---------------------------------------------------------------------
static VOID DARTFeederThreadFunc(PVOID Param)
{
  int i;
  ULONG ulPostCount;
  MixBufferDesc_p pBufDesc;
  unsigned long ulRead;

#ifdef DEBUG_BUILD
  printf("[DARTFeederThreadFunc] : Starting DARTFeeder thread (bDARTDriverRunning = %d)\n", bDARTDriverRunning);
#endif

  while (bDARTDriverRunning)
  {
    // Now process messages!

    // If we've reached the last buffer, tell the system that it should really
    // stop us now!
    if ((bReachedLastBuffer) && (pfnPlaybackFinishedNow))
      pfnPlaybackFinishedNow();

    // Fill unused buffers with audio data, and send the buffers to DART
    for (i=0; i<iDARTNumBufsNow; i++)
    {
      // Check all buffers, and if there is an empty one,
      pBufDesc = (MixBufferDesc_p) pMixBuffers[i].ulUserParm;
      if (pBufDesc->iBufferUsage == BUFFER_EMPTY)
      {
        // This is an empty buffer, so fill it!
        if (pfnReadStreamNow)
          ulRead = pfnReadStreamNow(iDARTBufSizeNow,
                                    pMixBuffers[i].pBuffer);
        else
          ulRead = 0;

        if (ulRead!=iDARTBufSizeNow)
        {
          // Reached the end of the buffer!
          pBufDesc->bLastBuffer = 1;
#ifdef DEBUG_BUILD
          printf("[DARTFeederThreadFunc] : Reached end of buffer!\n");
#endif
        }
        else
          pBufDesc->bLastBuffer = 0;

        // set that it's filled now,
        pBufDesc->iBufferUsage = BUFFER_USED;
        

        // and send it to DART to be queued
	MixSetupParms.pmixWrite(MixSetupParms.ulMixHandle, &(pMixBuffers[i]), 1);
      }
    }
    // Wait for a buffer to be empty
    if (DosWaitEventSem(hevAudioBufferPlayed, 250 )==NO_ERROR) // Wait max 250 msec!
      DosResetEventSem(hevAudioBufferPlayed, &ulPostCount);
    else
      DosSleep(0);
  } // End of while (bDARTDriverRunning == 0!)

#ifdef DEBUG_BUILD
  printf("[DARTFeederThreadFunc] : Halting DARTFeeder thread\n");
#endif
  _endthread();
}


//---------------------------------------------------------------------
// DART_StartPlayback
//
// Opens the DART device for playback. Then queries if that device is
// capable of handling the given sound format. If it is, sets up,
// allocates buffers, and starts the mixing thread and the playback.
//---------------------------------------------------------------------
int  DART_StartPlayback(int iFreq,
                        int iBits,
                        int iChannels,
                        pfn_ReadStream_callback pfnReadStream,
                        pfn_PlaybackFinished_callback pfnPlaybackFinished)
{
  int iDeviceOrd;
  int bOpenShared;
  int iNumBufs;
  int iBufSize;
  MCI_AMP_OPEN_PARMS AmpOpenParms;
  MCI_GENERIC_PARMS GenericParms;
  int iOpenMode;
  int rc;

  // If we're already running, do nothing, but return failure!
  if (bDARTDriverRunning) return 0;

  // Initialize some variables
  iDeviceOrd = DARTConfiguration.iDeviceID;
  bOpenShared = DARTConfiguration.bDeviceShared;
  iNumBufs = DARTConfiguration.iNumBufs;
  iBufSize = DARTConfiguration.iBufSize;

  bReachedLastBuffer = 0;

  // If this is the device -1, it means, it is the NO_SOUND device.
  // In this case, simply return success!
  if (iDeviceOrd == -1)
  {
    // Store the new settings in global variables
    iDARTDeviceOrdNow = iDeviceOrd;
    iDARTFreqNow = iFreq;
    iDARTBitsNow = iBits;
    iDARTChannelsNow = iChannels;
    iDARTNumBufsNow = iNumBufs;
    iDARTBufSizeNow = iBufSize;
    pfnReadStreamNow = pfnReadStream;
    pfnPlaybackFinishedNow = pfnPlaybackFinished;

    // TODO!
    // It is not implemented yet!
    // We should start a special feeder thread, which only gets data at
    // a given time intervalls, but does nothing with it.

    // Will have to return 1, and set bDARTDriverRunning to 1!

    return 0;
  }

  // First thing is to try to open a given DART device!
  memset(&AmpOpenParms, 0, sizeof(MCI_AMP_OPEN_PARMS));
  // pszDeviceType should contain the device type in low word, and device ordinal in high word!
  AmpOpenParms.pszDeviceType = (PSZ) (MCI_DEVTYPE_AUDIO_AMPMIX | (iDeviceOrd << 16));

  iOpenMode = MCI_WAIT | MCI_OPEN_TYPE_ID;
  if (bOpenShared) iOpenMode |= MCI_OPEN_SHAREABLE;
#ifdef DEBUG_BUILD
  printf("[DART_StartPlayback] : Opening DART, device %d\n", iDeviceOrd);
#endif
  rc = mciSendCommand( 0, MCI_OPEN,
                       iOpenMode,
		       (PVOID) &AmpOpenParms, 0);
  if (rc==MCIERR_SUCCESS)
  {
#ifdef DEBUG_BUILD
    printf("[DART_StartPlayback] : DART opened, now setting up  (device ID = %d)!\n", AmpOpenParms.usDeviceID);
#endif
    // Save the device ID we got from DART!
    // We will use this in the next calls!
    iDeviceOrd = AmpOpenParms.usDeviceID;

    // Now query this device if it supports the given freq/bits/channels!
    memset(&(MixSetupParms), 0, sizeof(MCI_MIXSETUP_PARMS));
    MixSetupParms.ulBitsPerSample = iBits;
    MixSetupParms.ulFormatTag = MCI_WAVE_FORMAT_PCM;
    MixSetupParms.ulSamplesPerSec = iFreq;
    MixSetupParms.ulChannels = iChannels;
    MixSetupParms.ulFormatMode = MCI_PLAY;
    MixSetupParms.ulDeviceType = MCI_DEVTYPE_WAVEFORM_AUDIO;
    MixSetupParms.pmixEvent = DARTEventFunc;
    rc = mciSendCommand (iDeviceOrd, MCI_MIXSETUP,
			 MCI_WAIT | MCI_MIXSETUP_QUERYMODE,
			 &(MixSetupParms), 0);
    if (rc!=MCIERR_SUCCESS)
    { // The device cannot handle this format!
      // Close DART, and exit with error code!
#ifdef DEBUG_BUILD
      printf("[DART_StartPlayback] : Cannot handle this format!\n");
      MciError(rc);
#endif
      mciSendCommand(iDeviceOrd, MCI_CLOSE, MCI_WAIT, &GenericParms, 0);
      return 0;
    }
    // The device can handle this format, so initialize!
    rc = mciSendCommand(iDeviceOrd, MCI_MIXSETUP,
			MCI_WAIT | MCI_MIXSETUP_INIT,
			&(MixSetupParms), 0);
    if (rc!=MCIERR_SUCCESS)
    { // The device could not be opened!
      // Close DART, and exit with error code!
#ifdef DEBUG_BUILD
      printf("[DART_StartPlayback] : Error opening the DART device!\n");
#endif
      mciSendCommand(iDeviceOrd, MCI_CLOSE, MCI_WAIT, &GenericParms, 0);
      return 0;
    }
    // Ok, the device is initialized.
    // Now we should allocate buffers. For this, we need a place where
    // the buffer descriptors will be:
    pMixBuffers = (MCI_MIX_BUFFER *) malloc(sizeof(MCI_MIX_BUFFER)*iNumBufs);
    if (!pMixBuffers)
    { // Not enough memory!
      // Close DART, and exit with error code!
#ifdef DEBUG_BUILD
      printf("[DART_StartPlayback] : Error allocating memory for bufferlist!\n");
#endif
      mciSendCommand(iDeviceOrd, MCI_CLOSE, MCI_WAIT, &GenericParms, 0);
      return 0;
    }
    // Now that we have the place for buffer list, we can ask DART for the
    // buffers!
    BufferParms.ulNumBuffers = iNumBufs;             // Number of buffers
    BufferParms.ulBufferSize = iBufSize;             // each with this size
    BufferParms.pBufList = pMixBuffers;              // getting descriptorts into this list
    // Allocate buffers!
    rc = mciSendCommand(iDeviceOrd, MCI_BUFFER,
			MCI_WAIT | MCI_ALLOCATE_MEMORY,
			&(BufferParms), 0);
    if ((rc!=MCIERR_SUCCESS) || (iNumBufs != BufferParms.ulNumBuffers) || (BufferParms.ulBufferSize==0))
    { // Could not allocate memory!
      // Close DART, and exit with error code!
#ifdef DEBUG_BUILD
      printf("[DART_StartPlayback] : Could not allocate memory for DART buffers!\n");
#endif
      free(pMixBuffers); pMixBuffers = NULL;
      mciSendCommand(iDeviceOrd, MCI_CLOSE, MCI_WAIT, &GenericParms, 0);
      return 0;
    }
    // Ok, we have all the buffers allocated, let's mark them!
    {
      int i;
      for (i=0; i<iNumBufs; i++)
      {
        MixBufferDesc_p pBufferDesc = (MixBufferDesc_p) malloc(sizeof(MixBufferDesc_t));;
        // Check if this buffer was really allocated by DART
	if ((!(pMixBuffers[i].pBuffer)) || (!pBufferDesc))
	{ // Wrong buffer!
          // Close DART, and exit with error code!
#ifdef DEBUG_BUILD
	  printf("[DART_StartPlayback] : Wrong memory allocated for DART buffer %d!\n", i);
#endif
          // Free buffer descriptions
          { int j;
            for (j=0; j<i; j++) free((void *)(pMixBuffers[j].ulUserParm));
          }
          // and cleanup
          mciSendCommand(iDeviceOrd, MCI_BUFFER, MCI_WAIT | MCI_DEALLOCATE_MEMORY, &(BufferParms), 0);
          free(pMixBuffers); pMixBuffers = NULL;
          mciSendCommand(iDeviceOrd, MCI_CLOSE, MCI_WAIT, &GenericParms, 0);
          return 0;
        }
        // Set initial buffer description
        pBufferDesc->iBufferUsage = BUFFER_EMPTY;
        pBufferDesc->bLastBuffer = FALSE;

	pMixBuffers[i].ulBufferLength = BufferParms.ulBufferSize;
	pMixBuffers[i].ulUserParm = (ULONG) pBufferDesc; // User parameter: Description of buffer
	pMixBuffers[i].ulFlags = 0; // Some stuff should be flagged here for DART, like end of
	                            // audio data, but as we will continously send
	                            // audio data, there will be no end.:)
	memset(pMixBuffers[i].pBuffer, 0, iBufSize);
      }
    }
    // Ok, buffers ready and marked
#ifdef DEBUG_BUILD
    printf("[DART_StartPlayback] : Stage 1 done!\n");
    fflush(stdout);
#endif

    bDARTDriverRunning = TRUE;
    // Create event semaphore
    if (DosCreateEventSem(NULL, &(hevAudioBufferPlayed), 0, FALSE)!=NO_ERROR)
    {
      // Could not create event semaphore!
      {
        int i;
        for (i=0; i<iNumBufs; i++) free((void *)(pMixBuffers[i].ulUserParm));
      }
      mciSendCommand(iDeviceOrd, MCI_BUFFER, MCI_WAIT | MCI_DEALLOCATE_MEMORY, &(BufferParms), 0);
      free(pMixBuffers); pMixBuffers = NULL;
      mciSendCommand(iDeviceOrd, MCI_CLOSE, MCI_WAIT, &GenericParms, 0);
      return 0;
    }

    // Store the new settings in global variables
    iDARTDeviceOrdNow = iDeviceOrd;
    iDARTFreqNow = iFreq;
    iDARTBitsNow = iBits;
    iDARTChannelsNow = iChannels;
    iDARTNumBufsNow = iNumBufs;
    iDARTBufSizeNow = iBufSize;
    pfnReadStreamNow = pfnReadStream;
    pfnPlaybackFinishedNow = pfnPlaybackFinished;

    // Start dartfeeder thread
    tidDARTFeederThread = _beginthread( DARTFeederThreadFunc,
                                        NULL,
                                        4*1024*1024,
                                        NULL);
    if (tidDARTFeederThread==-1)
    { // Could not create new thread!
#ifdef DEBUG_BUILD
      printf("[DART_StartPlayback] : Could not create DARTFeeder thread!\n");
#endif
      DosCloseEventSem(hevAudioBufferPlayed);
      {
        int i;
        for (i=0; i<iNumBufs; i++) free((void *)(pMixBuffers[i].ulUserParm));
      }
      mciSendCommand(iDeviceOrd, MCI_BUFFER, MCI_WAIT | MCI_DEALLOCATE_MEMORY, &(BufferParms), 0);
      free(pMixBuffers); pMixBuffers = NULL;
      mciSendCommand(iDeviceOrd, MCI_CLOSE, MCI_WAIT, &GenericParms, 0);
      return 0;
    }
    DosSetPriority(PRTYS_THREAD, PRTYC_FOREGROUNDSERVER, 28, tidDARTFeederThread); // Burst the priority
  }

  return (rc==MCIERR_SUCCESS);
}

//---------------------------------------------------------------------
// DART_StopPlayback
//
// Signals end to the dart feeder thread, then stops playback, frees
// audio buffers, closes DART.
//---------------------------------------------------------------------

void DART_StopPlayback()
{
  MCI_GENERIC_PARMS GenericParms;
  int rc;

  if (!bDARTDriverRunning) return;

  if (iDARTDeviceOrdNow == -1)
  {
    // It is the NOSOUND device, so stop that!
    // TODO!
    return;
  }

  // Otherwise it's a device handled by DART, so stop DART!

#ifdef DEBUG_BUILD
  printf("[DART_StopPlayback] : Sending MCI_STOP to DART\n");
#endif

  // Stop DART playback
  rc = mciSendCommand(iDARTDeviceOrdNow, MCI_STOP, MCI_WAIT, &GenericParms, 0);
  if (rc!=MCIERR_SUCCESS)
  {
#ifdef DEBUG_BUILD
    printf("[DART_StopPlayback] : Could not stop DART playback!\n");
#endif
  }

#ifdef DEBUG_BUILD
  printf("[DART_StopPlayback] : Waiting for DART-Feeder thread to stop\n");
#endif

  // Stop dartfeeder thread
  bDARTDriverRunning = FALSE;
  // Wait for the thread to be dead
  DosWaitThread(&(tidDARTFeederThread), DCWW_WAIT);

#ifdef DEBUG_BUILD
  printf("[DART_StopPlayback] : Cleaning up\n");
#endif

  // Close event semaphore
  DosCloseEventSem(hevAudioBufferPlayed);

  // Free memory of buffer descriptions
  {
    int i;
    for (i=0; i<iDARTNumBufsNow; i++) free((void *)(pMixBuffers[i].ulUserParm));
  }

  // Deallocate buffers
  rc = mciSendCommand(iDARTDeviceOrdNow, MCI_BUFFER, MCI_WAIT | MCI_DEALLOCATE_MEMORY, &(BufferParms), 0);
  if (rc!=MCIERR_SUCCESS)
  {
#ifdef DEBUG_BUILD
    printf("[DART_StopPlayback] : Could not deallocate memory!\n");
#endif
  }

  // Free bufferlist
  free(pMixBuffers); pMixBuffers = NULL;

  // Close dart
  rc = mciSendCommand(iDARTDeviceOrdNow, MCI_CLOSE, MCI_WAIT, &GenericParms, 0);
  if (rc!=MCIERR_SUCCESS)
  {
#ifdef DEBUG_BUILD
    printf("[DART_StopPlayback] : Could not close DART device!\n");
#endif
  }

#ifdef DEBUG_BUILD
  printf("[DART_StopPlayback] : Done\n");
#endif
}

