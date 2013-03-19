#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <netinet/in.h>


typedef unsigned char tByte;
typedef uint16_t tShort;
typedef uint32_t tLong;
typedef uint64_t tVeryLong;
typedef float tReal;
typedef double tQuiteReal;
typedef long double tVeryReal;

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

uint16_t swapShort(uint16_t v) {
  uint16_t v2=v;
  unsigned char *p=(unsigned char *)&v2, tmp;
  tmp=p[0];
  p[0]=p[1];
  p[1]=tmp;
  return v2;
}

uint32_t swapLong(uint32_t v) {
  uint32_t v2=v;
  unsigned char *p=(unsigned char *)&v2, tmp;
  tmp=p[0];
  p[0]=p[3];
  p[3]=tmp;
  tmp=p[1];
  p[1]=p[2];
  p[2]=tmp;
  return v2;
}

void TargaFixHeader(TargaHeader *h) {
  h->CMapIndex =  swapShort(htons(h->CMapIndex));
  h->CMapLength = swapShort(htons(h->CMapLength));
  h->ImgXOrigin = swapShort(htons(h->ImgXOrigin));
  h->ImgYOrigin = swapShort(htons(h->ImgYOrigin));
  h->ImgWidth =   swapShort(htons(h->ImgWidth));
  h->ImgHeight =  swapShort(htons(h->ImgHeight));
}

void TargaFixFooter(TargaFooter *f) {
  f->ExtensionAreaOffset =      swapLong(htonl(f->ExtensionAreaOffset));
  f->DeveloperDirectoryOffset = swapLong(htonl(f->DeveloperDirectoryOffset));
}

void TargaFixExtensionArea(TargaExtensionArea *e) {
  e->ExtensionSize = swapShort(htons(e->ExtensionSize));
  e->Month = swapShort(htons(e->Month));
  e->Day = swapShort(htons(e->Day));
  e->Year = swapShort(htons(e->Year));
  e->Hour = swapShort(htons(e->Hour));
  e->Minute = swapShort(htons(e->Minute));
  e->Second = swapShort(htons(e->Second));
  e->JTHours = swapShort(htons(e->JTHours));
  e->JTMinutes = swapShort(htons(e->JTMinutes));
  e->JTSeconds = swapShort(htons(e->JTSeconds));
  e->VersionNo = swapShort(htons(e->VersionNo));
  e->KeyColour = swapLong(htonl(e->KeyColour));
  e->PixelRatioNumerator = swapShort(htons(e->PixelRatioNumerator));
  e->PixelRatioDenominator = swapShort(htons(e->PixelRatioDenominator));
  e->GammaNumerator = swapShort(htons(e->GammaNumerator));
  e->GammaDenominator = swapShort(htons(e->GammaDenominator));
  e->ColourCorrectionTable = swapLong(htonl(e->ColourCorrectionTable));
  e->PostageStamp = swapLong(htonl(e->PostageStamp));
  e->ScanLineTable = swapLong(htonl(e->ScanLineTable));
}

TargaHandle TargaOpen(char *filename, int width, int height, char *author, char *jobname) {
  TargaHeader fileheader;
  int restart=1;
  TargaHandle newhandle=(TargaHandle)malloc(sizeof(struct TargaHandle_s));
  if (!newhandle) return NULL;
  memset(newhandle, 0, sizeof(struct TargaHandle_s));
  newhandle->width=width;
  newhandle->height=height;
  
  if (((newhandle->file) = fopen(filename, "r+")) != NULL) {
    if (fread(&fileheader, sizeof(fileheader), 1, newhandle->file) == 1) {
      TargaFixHeader(&fileheader);
      if (fileheader.ImgWidth == width && fileheader.ImgHeight == height) {
	fseek(newhandle->file,0L,SEEK_END);
	newhandle->currentsize=(ftell(newhandle->file)-(18+fileheader.IDFieldSize))/3;
	if ((newhandle->currentsize > 0) &&
	    (newhandle->currentsize < ((unsigned long)(newhandle->width)*(unsigned long)(newhandle->height)))) {
	  restart=0;
	}
      }
    }
    if (restart) {
      fprintf(stderr, "Truncating %s\n", filename);
      fclose(newhandle->file);
      newhandle->currentsize = 0;
      newhandle->file=NULL;
    } else {
      fprintf(stderr, "%s contains %lu pixels. Appending - ignoring new creator and job name\n", filename, newhandle->currentsize);
      fseek(newhandle->file,
	    (long)((newhandle->currentsize * 3) + sizeof(TargaHeader)),
	    SEEK_SET);
    }
  }
  
  if (!(newhandle->file) && ((newhandle->file) = fopen(filename, "w")) == NULL) {
    perror(filename);
    free(newhandle);
    return NULL;
  }
  
  memset(&fileheader, 0, 18);
  if (jobname) fileheader.IDFieldSize=strlen(jobname)+1;
  fileheader.ImageType = 2;
  fileheader.ImgWidth = width;
  fileheader.ImgHeight = height;
  fileheader.ImgColourDepth = 24;
  fileheader.ImgDescription = 0x20;
  TargaFixHeader(&fileheader);
  fwrite(&fileheader, sizeof(fileheader), 1, newhandle->file);
  if (jobname) fwrite(jobname, 1, strlen(jobname)+1, newhandle->file);
  if (author) newhandle->author=strdup(author);
  if (jobname) newhandle->jobname=strdup(jobname);
  return newhandle;
}

void TargaWrite(TargaHandle h, unsigned char r,unsigned char g,unsigned char  b) {
  fwrite(&b,1,1,h->file);
  fwrite(&g,1,1,h->file);
  fwrite(&r,1,1,h->file);
}

void TargaAddComment(TargaHandle h, char *comment) {
  if (!h || !comment) return;
  if (!(h->comment1))
    h->comment1=strdup(comment);
  else if (!(h->comment2))
    h->comment2=strdup(comment);
  else if (!(h->comment3))
    h->comment3=strdup(comment);
  else if (!(h->comment4))
    h->comment4=strdup(comment);
}

void TargaRead(TargaHandle h, unsigned char *r, unsigned char *g, unsigned char *b) {
  *b=fgetc(h->file);
  *g=fgetc(h->file);
  *r=fgetc(h->file);
}

int TargaSeek(TargaHandle h, unsigned long pos) {
  TargaHeader fileheader;
  fseek(h->file, 0, SEEK_SET);
  if (fread(&fileheader, sizeof(fileheader), 1, h->file) == 1) {
    TargaFixHeader(&fileheader);
    return (!fseek(h->file, (long)(18+fileheader.IDFieldSize+(pos*3)), SEEK_SET));
  }
  return 0;
}

void TargaEnd(TargaHandle h) {
  fseek(h->file,0L,SEEK_END);
}

void TargaFinish(TargaHandle h) {
  TargaFooter f;
  TargaExtensionArea ea;
  time_t now_t;
  struct tm *now;
  if (h->author || h->jobname || h->comment1 || h->comment2 || h->comment3 || h->comment4) {
    f.DeveloperDirectoryOffset=0;
    strcpy(f.Signature, "TRUEVISION-XFILE.");
    fseek(h->file,0L,SEEK_END);
    f.ExtensionAreaOffset=ftell(h->file);
    memset(&ea, 0, sizeof(ea));
    ea.ExtensionSize=495;
    if (h->author) strncpy(ea.AuthorName, h->author, sizeof(ea.AuthorName)-1);
    if (h->jobname) strncpy(ea.JobName, h->jobname, sizeof(ea.JobName)-1);
    if (h->comment1) strncpy(ea.AuthorComments1, h->comment1, sizeof(ea.AuthorComments1)-1);
    if (h->comment2) strncpy(ea.AuthorComments2, h->comment2, sizeof(ea.AuthorComments2)-1);
    if (h->comment3) strncpy(ea.AuthorComments3, h->comment3, sizeof(ea.AuthorComments3)-1);
    if (h->comment4) strncpy(ea.AuthorComments4, h->comment4, sizeof(ea.AuthorComments4)-1);
    time(&now_t);
    now=localtime(&now_t);
    ea.Month=now->tm_mon+1;
    ea.Day=now->tm_mday;
    ea.Year=now->tm_year+1900;
    ea.Hour=now->tm_hour;
    ea.Minute=now->tm_min;
    ea.Second=now->tm_sec;
    TargaFixExtensionArea(&ea);
    fwrite(&ea, sizeof(ea), 1, h->file);
    TargaFixFooter(&f);
    fwrite(&f, sizeof(f), 1, h->file);
  }
}

void TargaClose(TargaHandle h) {
  fclose(h->file);
}

typedef unsigned char byte;

#define max(x,y) (((x)>(y))?(x):(y))
#define min(x,y) (((x)<(y))?(x):(y))

#define EDGE(val1, val2, maxiter) ((((val1) == (maxiter)) && ((val2) != (maxiter))))

int isedge(unsigned int *buffer, int width, int height, int x, int y, unsigned int maxiter) {
  if ((x > 0) &&
      (EDGE(buffer[x + y*height], buffer[x - 1 + y*height], maxiter))) return 1;
  if ((x > 0) &&
      (EDGE(buffer[x + y*height], buffer[x - 1 + y*height], maxiter))) return 1;
  if ((x < (width - 1)) &&
      (EDGE(buffer[x + y*height], buffer[x + 1 + y*height], maxiter))) return 1;
  if ((x < (width - 1)) &&
      (EDGE(buffer[x + y*height], buffer[x + 1 + y*height], maxiter))) return 1;

  if ((y > 0) &&
      (EDGE(buffer[x + y*height], buffer[x + (y - 1)*height], maxiter))) return 1;
  if ((y > 0) &&
      (EDGE(buffer[x + y*height], buffer[x + (y - 1)*height], maxiter))) return 1;
  if ((y < (height - 1)) &&
      (EDGE(buffer[x + y*height], buffer[x + (y + 1)*height], maxiter))) return 1;
  if ((y < (height - 1)) &&
      (EDGE(buffer[x + y*height], buffer[x + (y + 1)*height], maxiter))) return 1;
  return 0;
}

void usage(char *progname) {
  fprintf(stderr, "Usage: %s -i <infile.mdump> -o <outfile.tga> [-d <deffile.def>] [-m <bailout>]\n", progname);
}

void handler(int s)
{
  int status;
  while (waitpid(-1, &status, WNOHANG)>0);
  signal(SIGCHLD, handler);
}

int main(int argc, char *argv[])
{
  FILE *infile;
  unsigned int width, height, *buffer, i, n;
  unsigned long wtmp, htmp;
  unsigned int maxiter=1048576L;
  int x, y;

  char *infilename=NULL;
  char *outfilename=NULL;
  char *deffilename=NULL;
  int o;

  byte r,g,b;
  TargaHandle th=NULL;
  static char tmpbuf1[1024], tmpbuf2[1024];
  long double def_x, def_y, def_dx;

  signal(SIGCHLD, handler);

  while ((o=getopt(argc, argv, "i:o:d:m:")) != -1) {
    switch (o) {
    case 'i': infilename=optarg; break;
    case 'o': outfilename=optarg; break;
    case 'd': deffilename=optarg; break;
    case 'm': maxiter=atoi(optarg); break;
    default: usage(argv[0]); return -1; break;
    }
  }

  if (!infilename || !outfilename) {
    usage(argv[0]);
    return -1;
  }
    
  if (!(infile=fopen(infilename, "rb"))) { perror(infilename); return -2; }
  if (fread(&wtmp, sizeof(wtmp), 1, infile) != 1) {
    perror("fread()");
    return -2;
  }
  if (fread(&htmp, sizeof(htmp), 1, infile) != 1) {
    perror("fread()");
    return -2;
  }
  width=ntohl(wtmp);
  height=ntohl(htmp);
  printf("[%d x %d]\n", width, height);
  if (!(buffer=(unsigned int *)malloc(width*height*sizeof(unsigned int)))) {
    perror("malloc()");
    return -3;
  }
  if ((n=fread(buffer, sizeof(unsigned int), width*height, infile)) != width*height) {
    height=n/width;
    printf("fread() returned %u bytes. height is %u\n", n, height);
    /*    return -4;*/
  }

  fclose(infile);

  for (i=0; i<n; i++) buffer[i]=ntohl(buffer[i]);

  sprintf(tmpbuf1, "%s %s", __FILE__, __DATE__);
  sprintf(tmpbuf2, "%s", infilename);

  if (!(th = TargaOpen(outfilename, width, height, tmpbuf1, tmpbuf2))) return -2;

  sprintf(tmpbuf1, "Made by mandelmakeoutline\n");

  TargaAddComment(th, tmpbuf1);

  if (deffilename) {
    if (!(infile=fopen(deffilename, "r"))) perror(deffilename);
    else {
      if (fscanf(infile, "%60Lf\n", &def_x) != 1) {
	perror("fscanf()");
	return -2;
      }
      if (fscanf(infile, "%60Lf\n", &def_y) != 1) {
	perror("fscanf()");
	return -2;
      }
      if (fscanf(infile, "%60Lf\n", &def_dx) != 1) {
	perror("fscanf()");
	return -2;
      }
      fclose(infile);
      sprintf(tmpbuf1, "x=%.50Lg", def_x);
      TargaAddComment(th, tmpbuf1);
      sprintf(tmpbuf1, "y=%.50Lg", def_y);
      TargaAddComment(th, tmpbuf1);
      sprintf(tmpbuf1, "dx=%.50Lg", def_dx);
      TargaAddComment(th, tmpbuf1);
    }
  }

  for (y=0; y<height; y++) {
    fprintf(stderr,"  Line: %d\r",y);
    for (x=0; x<width; x++) {
      if (isedge(buffer, width, height, x, y, maxiter)) {
	r = 0;
	g = 0;
	b = 0;
      } else {
	r = 255;
	g = 255;
	b = 255;
      }
      TargaWrite(th, r, g, b);
    }
  }
  TargaFinish(th);
  TargaClose(th);
  free(buffer);
  return 0;
}
