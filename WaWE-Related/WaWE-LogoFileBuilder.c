/*****************************************************************************
 * WaWE Logo File Builder                                                    *
 *                                                                           *
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <process.h>
#include <time.h>
#include <math.h>
#include <string.h>

#define INCL_TYPES
#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>

//#define USE_COMPRESSION

#define NUM_OF_FRAMES   300
#define MAX_FILE_SIZE   65536

#define SOURCE_FILENAME_MASK  "8igy%04d.bmp"

int aiSizeTable[NUM_OF_FRAMES];
char achBuffer[MAX_FILE_SIZE];

#ifdef USE_COMPRESSION
char achCompressed[MAX_FILE_SIZE];
int  iCompressedSize;
long lFullSize, lCompSize;

void Compress(char *pchBuffer, int iSize)
{
  int i, j;
  char chLast;
  int iRepCount;
  int iLastPos;

  iCompressedSize = 0;
  chLast = 0;
  iRepCount = -1;
  iLastPos = 0;
  i = 0;
  while (i<iSize-3)
  {
    if ((pchBuffer[i] == pchBuffer[i+1]) &&
        (pchBuffer[i] == pchBuffer[i+2]) &&
        (pchBuffer[i] == pchBuffer[i+3]))
    {
      // At least 4 stuffs the same!
      if (iRepCount<0)
      {
        if (i-iLastPos>0)
        {
//          printf("%d pieces of different stuffs, from %d to %d\n", i - iLastPos, iLastPos, i);
          achCompressed[iCompressedSize]=i - iLastPos; iCompressedSize++;
          for (j=iLastPos; j<i; j++)
          {
//            printf(" %d", pchBuffer[j]);
            achCompressed[iCompressedSize]=pchBuffer[j]; iCompressedSize++;
          }
//          printf("\n");
          iLastPos = i;
        }
        iRepCount = 1;
      }
      else
      {
        if (iRepCount<123)
          iRepCount++;
        else
        {
          iRepCount+=3;
//          printf("%d pieces of %x\n", iRepCount, pchBuffer[i]);
          achCompressed[iCompressedSize]=0x80 | iRepCount; iCompressedSize++;
          achCompressed[iCompressedSize]=pchBuffer[i]; iCompressedSize++;
          iRepCount = -1;
          i+=3;
          iLastPos = i;
        }
      }
    } else
    {
      if (iRepCount>0)
      {
        iRepCount+=3;
//        printf("%d pieces of %x\n", iRepCount, pchBuffer[i]);
        achCompressed[iCompressedSize]=0x80 | iRepCount; iCompressedSize++;
        achCompressed[iCompressedSize]=pchBuffer[i]; iCompressedSize++;
        iRepCount = -1;
        i+=3;
        iLastPos = i;
      } else
      if (i-iLastPos==126)
      {
//        printf("%d pieces of different stuffs, from %d to %d\n", i - iLastPos, iLastPos, i);
        achCompressed[iCompressedSize]=i - iLastPos; iCompressedSize++;
        for (j=iLastPos; j<i; j++)
        {
//            printf(" %d", pchBuffer[j]);
          achCompressed[iCompressedSize]=pchBuffer[j]; iCompressedSize++;
        }
//          printf("\n");
        iLastPos = i;
      }
    }
    i++;
  }
  if (iRepCount>0)
  {
    iRepCount+=3;
    if (iRepCount>126)
    {
      printf("Almost LAST: %d pieces of %x\n", 126, pchBuffer[i]);
      achCompressed[iCompressedSize]=0x80 | 126; iCompressedSize++;
      achCompressed[iCompressedSize]=pchBuffer[i]; iCompressedSize++;
      iRepCount-=126;
    }
    printf("LAST: %d pieces of %x\n", iRepCount, pchBuffer[i]);
    achCompressed[iCompressedSize]=0x80 | iRepCount; iCompressedSize++;
    achCompressed[iCompressedSize]=pchBuffer[i]; iCompressedSize++;
  } else
  if (i-iLastPos>-1)
  {
    i+=3;
    if (i>iSize) i = iSize;
    if (i - iLastPos>126)
    {
      printf("Almost LAST: %d pieces of different stuffs, from %d to %d\n", 126, iLastPos, i);
      achCompressed[iCompressedSize]=126; iCompressedSize++;
      for (j=iLastPos; j<iLastPos+126; j++)
      {
  //            printf(" %d", pchBuffer[j]);
        achCompressed[iCompressedSize]=pchBuffer[j]; iCompressedSize++;
      }
  //          printf("\n");
      iLastPos += 126;

    }
    printf("LAST: %d pieces of different stuffs, from %d to %d\n", i - iLastPos, iLastPos, i);
    achCompressed[iCompressedSize]=i - iLastPos; iCompressedSize++;
    for (j=iLastPos; j<i; j++)
    {
//            printf(" %d", pchBuffer[j]);
      achCompressed[iCompressedSize]=pchBuffer[j]; iCompressedSize++;
    }
//          printf("\n");
    iLastPos = i;
  } else
    printf("LAST: Nothing to do!\n");

}

int Decompress(char *pchCompBuffer, int iCompSize, char *pchDestBuffer, int iDestSize)
{
  int iDecSize = 0;
  int i, j;
  int iChunkSize;

  i = 0;
  while (i<iCompSize)
  {
    iChunkSize = pchCompBuffer[i] & 0x7f;
    if (pchCompBuffer[i] & 0x80)
    {
      for (j = 0; j<iChunkSize; j++)
      {
        pchDestBuffer[iDecSize] = pchCompBuffer[i+1]; iDecSize++;
        if (iDecSize>=iDestSize) return iDecSize;
      }
      i+=2;
    } else
    {
      i++;
      for (j = 0; j<iChunkSize; j++)
      {
        pchDestBuffer[iDecSize] = pchCompBuffer[i]; iDecSize++; i++;
        if (iDecSize>=iDestSize) return iDecSize;
      }
    }
  }
  return iDecSize;
}

void Decompress_to_file(char *pchFileName, char *pchBuffer, int iSize, int iSizeShouldBe)
{
  char Temp[65536];
  FILE *hFile;
  int iDecompressedSize;

  iDecompressedSize = Decompress(pchBuffer, iSize, Temp, 65536);
  if (iSizeShouldBe!=iDecompressedSize)
  {
    printf("  %d -> %d (%s)\n", iSizeShouldBe, iDecompressedSize, pchFileName);

    hFile = fopen("compare.bmp", "wb");
    fwrite(Temp, iDecompressedSize, 1, hFile);
    fclose(hFile);
  }

}
#endif

int main(int argc, char *argv[])
{
  int i;
  FILE *hFile, *hDestFile;
  char achFileName[256];


  printf("WaWE Logo File Builder\n");
  printf("Creating WaWELogo.dat file from %d GIF files\n", NUM_OF_FRAMES);

  printf("\n");
  printf("Creating index table...\n");
  for (i=0; i<NUM_OF_FRAMES; i++)
  {
    sprintf(achFileName, SOURCE_FILENAME_MASK, i);
    hFile = fopen(achFileName, "rb");
    if (!hFile)
    {
      printf("Error opening file: %s\n", achFileName);
      return 1;
    }
    fseek(hFile, 0, SEEK_END);
    aiSizeTable[i] = ftell(hFile);
    fclose(hFile);
    if (aiSizeTable[i] > MAX_FILE_SIZE)
    {
      printf("Too big file: %s\n", achFileName);
      return 1;
    }
  }

#ifdef USE_COMPRESSION
  lFullSize = 0;
  lCompSize = 0;
#endif
  printf("Creating dat file...\n");
  hDestFile = fopen("WaWELogo.dat", "wb");
  if (!hDestFile)
  {
    printf("Could not open destination file for writing!\n");
    return 1;
  }
  // Write header
  fprintf(hDestFile,"WaWE Logo File!%c", 0x1A);
  // Write number of frames
  i = NUM_OF_FRAMES;
  fwrite((char *) &i, sizeof(i), 1, hDestFile);
  // Write size table
  fwrite((char *) &aiSizeTable, sizeof(aiSizeTable), 1, hDestFile);

  for (i=0; i<NUM_OF_FRAMES; i++)
  {
    sprintf(achFileName, SOURCE_FILENAME_MASK, i);
    hFile = fopen(achFileName, "rb");
    if (!hFile)
    {
      printf("Error opening file: %s\n", achFileName);
      fclose(hDestFile);
      return 1;
    }
    fread((char *)  &achBuffer, aiSizeTable[i], 1, hFile);
#ifndef USE_COMPRESSION
    fwrite((char *) &achBuffer, aiSizeTable[i], 1, hDestFile);
#else
    Compress(&achBuffer, aiSizeTable[i]);
    lFullSize += aiSizeTable[i];
    lCompSize +=iCompressedSize;
    fwrite((char *) &achCompressed, iCompressedSize, 1, hDestFile);
    Decompress_to_file(achFileName, &achCompressed, iCompressedSize, aiSizeTable[i]);
    aiSizeTable[i] = iCompressedSize;
#endif

    fclose(hFile);
  }

#ifdef USE_COMPRESSION
  printf("Writing new size table!\n");
  fseek(hDestFile, 16+sizeof(int), SEEK_SET);
  // Write new size table with compressed size datas
  fwrite((char *) &aiSizeTable, sizeof(aiSizeTable), 1, hDestFile);
#endif

  fclose(hDestFile);

#ifdef USE_COMPRESSION
  printf("Compressed %d -> %d (%d%%)\n", lFullSize, lCompSize, lCompSize*100/lFullSize);
#endif

  return 0;
}

