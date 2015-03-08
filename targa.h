#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

typedef unsigned char tByte;
typedef uint16_t tShort;
typedef uint32_t tLong;
typedef uint64_t tVeryLong;

typedef struct TargaHandle_s {
  FILE *file;
  int width, height;
  unsigned long currentsize;
  char *author;
  char *jobname;
  char *comment1;
  char *comment2;
  char *comment3;
  char *comment4;
} *TargaHandle;

#pragma pack(1)

typedef struct TargaHeader_s {
  tByte IDFieldSize; // Size of the ID field, if any.
  tByte ColourMaptype; // 0=does not use a colour map, 1=uses a colour map
  tByte ImageType; // 0=no image data, 1=uncompr. cmap, 2=uncompr. truecolour,
                  // 3=uncompr. b&w, 9=RLE cmap, 10=RLE true colour, 11=RLE b&w
  tShort CMapIndex;
  tShort CMapLength;
  tByte CMapSizePerEntry;
  tShort ImgXOrigin;
  tShort ImgYOrigin;
  tShort ImgWidth;
  tShort ImgHeight;
  tByte ImgColourDepth;
  tByte ImgDescription; // bit 3-0: # attr bits/pixel, bit 5,4: origin: 00=BL, 01=BR, 10=TL, 11=TR, bit 7,6=0
} TargaHeader;

typedef struct TargaFooter_s {
  tLong ExtensionAreaOffset;
  tLong DeveloperDirectoryOffset;
  char Signature[18];
} TargaFooter;

typedef struct TargaExtensionArea_s {
  tShort ExtensionSize;
  char AuthorName[41];
  char AuthorComments1[81];
  char AuthorComments2[81];
  char AuthorComments3[81];
  char AuthorComments4[81];
  tShort Month;
  tShort Day;
  tShort Year;
  tShort Hour;
  tShort Minute;
  tShort Second;
  char JobName[41];
  tShort JTHours;
  tShort JTMinutes;
  tShort JTSeconds;
  char SWID[41];
  tShort VersionNo;
  tByte VersionLtr;
  tLong KeyColour;
  tShort PixelRatioNumerator;
  tShort PixelRatioDenominator;
  tShort GammaNumerator;
  tShort GammaDenominator;
  tLong ColourCorrectionTable;
  tLong PostageStamp;
  tLong ScanLineTable;
  tByte AttributesType;
} TargaExtensionArea;

#pragma pack()

void TargaFixHeader(TargaHeader *h);
void TargaFixFooter(TargaFooter *f);
void TargaFixExtensionArea(TargaExtensionArea *e);
TargaHandle TargaOpen(char *filename, int width, int height, char *author, char *jobname);
int TargaSeek(TargaHandle h, unsigned long pos);
void TargaWrite(TargaHandle h, unsigned char r,unsigned char g,unsigned char  b);
void TargaAddComment(TargaHandle h, char *comment);
void TargaRead(TargaHandle h, unsigned char *r, unsigned char *g, unsigned char *b);
void TargaEnd(TargaHandle h);
void TargaFinish(TargaHandle h);
void TargaClose(TargaHandle h);






