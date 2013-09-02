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
#include <string.h>
#include <process.h>
#include "PPlayPlayback.h"
#include "PPlayConfig.h"
#include "PPlayDART.h"

int bIsPlaying = FALSE;
int bStopping = FALSE;

// If bIsPlaying is TRUE, then these static variables tell the
// config to use for the playback.
static int iSourceFreq;
static int iSourceChannels;
static int iDestinationFreq;
static int iDestinationChannels;
static int iDestinationBits;

static int iPlaybackDriver; // Driver used to do the playback

static void *hStream; // Handle to the stream
static pfn_Read_callback  pfnReadStream;  // Function to call to read the stream
static pfn_Close_callback pfnCloseStream; // Function to call to close the stream


static unsigned long fnReadStream(unsigned long ulNumOfBytes,
                                  void *pSampleBuffer)
{
  unsigned char ucBytePerSample;
  WaWESize_t WaWESamplesToReadPerChannel;
  WaWESample_p *WaWESampleBuffers;
  WaWESize_t   *WaWESampleBufferSizes;
  unsigned long ulProducedSamples;
  int iChannel;
  int iCurrSChannel; // Current source channel
  unsigned long ulSRCCounter; // Sample Rate Conversion counter


  // This is the function which will be called by the playback driver when it
  // wants some more data.
  // This function has to read the WaWE samples using the stream read callback,
  // then convert it to the requested format, for the audio playback driver.

  // Initialize variables
  ucBytePerSample = (iDestinationBits + 7)/8;

  // Set the sample buffer to silent first!
  if (iDestinationBits==8)
  {
    // 8 bit samples are unsigned, so their "silent" position is at the
    // middle of thir range, so the silence is 128!
    memset(pSampleBuffer, 0x80, ulNumOfBytes);
  } else
  {
    // Other samples are signed, so their "silent" position is at the
    // middle of their range, so the silence is 0!
    memset(pSampleBuffer, 0x00, ulNumOfBytes);
  }

  // Calculate required WaWE samples!
  WaWESamplesToReadPerChannel = ulNumOfBytes;
  WaWESamplesToReadPerChannel /= ucBytePerSample; // Get the number of all samples
  WaWESamplesToReadPerChannel /= iDestinationChannels; // Get the number of samples per channel
  // Also take into consideration the effect of frequency conversion
  WaWESamplesToReadPerChannel =
    (WaWESamplesToReadPerChannel * iSourceFreq) / iDestinationFreq;

  // Goooooood. Allocate memory for the per-channel WaWESample buffers!

  WaWESampleBuffers = (WaWESample_p *) malloc(sizeof(WaWESample_p) * iSourceChannels);
  if (!WaWESampleBuffers)
  {
#ifdef DEBUG_BUILD
    printf("[fnReadStream] : Out of memory allocating WaWESampleBuffers for %d channels!\n",
           (int) iSourceChannels);
#endif
    return 0;
  }
  memset(WaWESampleBuffers, 0, sizeof(WaWESample_p) * iSourceChannels);

  // Then allocate memory for buffer sizes!
  WaWESampleBufferSizes = (WaWESize_t *) malloc(sizeof(WaWESize_t) * iSourceChannels);
  if (!WaWESampleBufferSizes)
  {
#ifdef DEBUG_BUILD
    printf("[fnReadStream] : Out of memory allocating WaWESampleBufferSizes for %d channels!\n",
           (int) iSourceChannels);
#endif
    free(WaWESampleBuffers);
    return 0;
  }
  memset(WaWESampleBufferSizes, 0, sizeof(WaWESize_t) * iSourceChannels);

  // Then allocate a sample buffer for all channels, and read the samples in there!
  for (iChannel = 0; iChannel<iSourceChannels; iChannel++)
  {
    WaWESampleBuffers[iChannel] = (WaWESample_p) malloc(sizeof(WaWESample_t)*WaWESamplesToReadPerChannel);
    if (WaWESampleBuffers[iChannel])
    {
      // Read this channel data!
      WaWESampleBufferSizes[iChannel] = pfnReadStream(hStream,
                                                      iChannel,
                                                      WaWESamplesToReadPerChannel,
                                                      WaWESampleBuffers[iChannel]);

#ifdef DEBUG_BUILD
//      printf("Ch%d read %d samples\n", iChannel, (int) WaWESampleBufferSizes[iChannel]);
#endif
    }
  }


  // At this point we have all the channels read in WaWESample format, in
  // Source freq and channels. We should go through it then, and convert it
  // to the destination format!
  ulProducedSamples = 0;
  for (iChannel = 0; iChannel<iDestinationChannels; iChannel++)
  {
    void *pDest;
    void *pLastDest;
    WaWEPosition_t WaWESamplePos;
    signed long long llMixedSample;
    int bGotAtLeastOneSample;
    unsigned long ulProducedSamplesPerChannel;

    // So, create one channel into destination buffer!

    pLastDest = ((unsigned char *) pSampleBuffer) + ulNumOfBytes;
    pDest = ((unsigned char *) pSampleBuffer) + iChannel*ucBytePerSample; // Starting from the right channel!

    ulProducedSamplesPerChannel = 0;
    WaWESamplePos = 0; // Current position in source buffer
    ulSRCCounter = 0;

    while (pDest < pLastDest)
    {
      // Ok, we should get a sample that we can put at this
      // position of destination buffer.
      // For this, we have to go through the source channels,
      // and mix together its values.

      iCurrSChannel = iChannel % iSourceChannels;
      llMixedSample = 0;

      bGotAtLeastOneSample = 0;
      while (iCurrSChannel < iSourceChannels)
      {
        // Do the mixing of sample from iCurrSChannel into the llMixedSample
        // variable!
        llMixedSample += WaWESampleBuffers[iCurrSChannel][WaWESamplePos];

        if (WaWESamplePos < WaWESampleBufferSizes[iCurrSChannel])
          bGotAtLeastOneSample = 1;

        iCurrSChannel += iDestinationChannels;
      }

      if (bGotAtLeastOneSample)
        ulProducedSamplesPerChannel++;

      // Do cutting to avoid clicks
      if (llMixedSample > MAX_WAWESAMPLE) llMixedSample = MAX_WAWESAMPLE;
      if (llMixedSample < MIN_WAWESAMPLE) llMixedSample = MIN_WAWESAMPLE;

      // Convert sample to destination bit-depth!
      llMixedSample = llMixedSample >> (8*sizeof(WaWESample_t) - iDestinationBits);

      // Special thing for the 8 bits version, it has to be unsigned!
      if (iDestinationBits==8)
        llMixedSample = 0x80 + llMixedSample;

      // Put the final sample into destination buffer!
      memcpy(pDest, &llMixedSample, ucBytePerSample);

      // Move in destination buffer to the next position for same channel!
      pDest = ((unsigned char *) pDest) + iDestinationChannels*ucBytePerSample;

      // Move in source buffer(s) to next position, based on samplerate conversion!
      ulSRCCounter += iSourceFreq;
      while (ulSRCCounter >= iDestinationFreq)
      {
        ulSRCCounter -= iDestinationFreq;
        WaWESamplePos++;
      }
    }

#ifdef DEBUG_BUILD
//    printf("Produced samples for channel %d: %d\n", iChannel, (int) ulProducedSamplesPerChannel);
#endif

    if (ulProducedSamplesPerChannel > ulProducedSamples)
      ulProducedSamples = ulProducedSamplesPerChannel;
  }

  // Free the WaWESample buffers
  for (iChannel = 0; iChannel<iSourceChannels; iChannel++)
  {
    if (WaWESampleBuffers[iChannel])
    {
      free(WaWESampleBuffers[iChannel]);
      WaWESampleBuffers[iChannel] = NULL;
    }
  }
  free(WaWESampleBuffers);
  free(WaWESampleBufferSizes);

  // Calculate bytes from samples and
  // return with the result!
  return (ulProducedSamples * ucBytePerSample * iDestinationChannels);
}

static void StopPlaybackThreadFunc(void *pParm)
{
  StopPlayback();

  _endthread();
}

static void fnPlaybackFinished()
{
  // Stop the playback, if we've reached the end of stream!
  // Just make sure we won't make it from this thread, 'cause we're
  // called from the audio playback thread, which will be
  // stopped by the StopPlayback function

  _beginthread(StopPlaybackThreadFunc,
               NULL,
               4*1024*1024,
               NULL);
}

int  StartPlayback(void              *pUserParm,
                   pfn_Open_callback  pfnOpen,
                   pfn_Read_callback  pfnRead,
                   pfn_Close_callback pfnClose,
                   int iFreq,
                   int iChannels)
{
  // Do nothing if already playing something!
  if (bIsPlaying) return 0;

  // If not playing, then try to set up the things!
  bIsPlaying = TRUE;

  // First, check the parameters!
  if ((!pfnOpen) || (!pfnRead) || (!pfnClose) || (!iFreq) || (!iChannels))
  {
    bIsPlaying = FALSE;
    return 0;
  }

  iPlaybackDriver = PluginConfiguration.iDeviceToUse;
  pfnReadStream = pfnRead;
  pfnCloseStream = pfnClose;

  // Let's see if we have to do freq and channel conversion!
  iSourceFreq = iFreq;
  iSourceChannels = iChannels;
  if (PluginConfiguration.bAlwaysConvertToDefaultConfig)
  {
    // We'll have to do a conversion to the default sound format,
    // so the sound driver will always be opened with that format!
    iDestinationFreq = PluginConfiguration.iDefaultFreq;
    iDestinationChannels = PluginConfiguration.iDefaultChannels;
  } else
  {
    // We don't have to use the default sound format, so
    // use the same as the source one, without conversion!
    iDestinationFreq = iSourceFreq;
    iDestinationChannels = iSourceChannels;
  }
  iDestinationBits = PluginConfiguration.iDefaultBits;

  // Ok, our local variables are set.
  // Try to initialize the stream now!

  hStream = pfnOpen(pUserParm);
  if (!hStream)
  {
    bIsPlaying = FALSE;
    return 0; // Fail, if could not open the stream!
  }

  // Ok, the stream is opened!
  // Now we can try to start the playback with the selected driver!
  // That will call our function to get the samples, when needed!

  switch (PluginConfiguration.iDeviceToUse)
  {
    case DEVICE_DART:
      if (!DART_StartPlayback(iDestinationFreq,      // Format to initialize to
                              iDestinationBits,
                              iDestinationChannels,
                              fnReadStream,          // Callback to call for more samples
                              fnPlaybackFinished))    // Callback to call when the read callback runs out of samples, and we've played that last buffer, too!
      {
#ifdef DEBUG_BUILD
        printf("[StartPlayback] : DART driver could not start playback!\n");
        printf("[StartPlayback] : Parameters were: Freq=%d, Bits=%d, Chn=%d\n",
              iDestinationFreq, iDestinationBits, iDestinationChannels);
#endif
        pfnClose(hStream);
        bIsPlaying = FALSE;
        return 0; // Return failure!
      } else
      {
        // Good, the DART driver could set itself up for the required configuration,
        // and could start the playback!
        // Return success then!
        return 1;
      }
    default:
#ifdef DEBUG_BUILD
      printf("[StartPlayback] : Unsupported driver! (%d)\n", PluginConfiguration.iDeviceToUse);
#endif
      pfnClose(hStream);
      bIsPlaying = FALSE;
      return 0; // Return failure!
  }
}

int  StopPlayback()
{
  if (!bIsPlaying) return 0;
  if (bStopping) return 1;

  bStopping = 1;

  // First stop the playback driver
  switch (iPlaybackDriver)
  {
    case DEVICE_DART:
      DART_StopPlayback();
      break;
    default:
#ifdef DEBUG_BUILD
      printf("[StopPlayback] : Unsupported driver! (%d)\n", iPlaybackDriver);
#endif
      break;
  }

  // Then close the stream!
  pfnCloseStream(hStream); hStream = NULL;

  bIsPlaying = FALSE;
  bStopping = 0;
  return 1;
}

