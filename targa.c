#include <netinet/in.h>
#include "targa.h"

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
  f->ExtensionAreaOffset = swapLong(htonl(f->ExtensionAreaOffset));
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

TargaHandle TargaOpen(char *filename, int width, int height, char *author, char *jobname, int writing) {
  TargaHeader fileheader;
  TargaFooter filefooter;
  TargaExtensionArea fileextension;
  char *initial_mode="w+";
  if (!writing) initial_mode="r";
  TargaHandle newhandle=(TargaHandle)malloc(sizeof(struct TargaHandle_s));
  if (!newhandle) {
    perror("malloc()");
    return NULL;
  }

  memset(newhandle, 0, sizeof(struct TargaHandle_s));
  newhandle->width=width;
  newhandle->height=height;

  if (((newhandle->file) = fopen(filename, initial_mode)) != NULL) {
    if (fread(&fileheader, sizeof(fileheader), 1, newhandle->file) == 1) {
      TargaFixHeader(&fileheader);
      if (fileheader.ImgWidth == width && fileheader.ImgHeight == height) {
	fseek(newhandle->file,0L,SEEK_END);
	newhandle->currentsize=(ftell(newhandle->file)-(18+fileheader.IDFieldSize))/3;
      }
    }
    if (writing) {
      fprintf(stderr, "Truncating %s\n", filename);
      fclose(newhandle->file);
      newhandle->currentsize = 0;
      newhandle->file=NULL;
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
    } else {
      newhandle->width = fileheader.ImgWidth;
      newhandle->height = fileheader.ImgHeight;
      fseek(newhandle->file, -sizeof(TargaFooter), SEEK_END);
      if (fread(&filefooter, sizeof(TargaFooter), 1, newhandle->file) != 1) {
	fprintf(stderr, "Failed to read footer\n");
	return NULL;
      }
      TargaFixFooter(&filefooter);
      fseek(newhandle->file, filefooter.ExtensionAreaOffset, SEEK_SET);
      if (fread(&fileextension, sizeof(fileextension), 1, newhandle->file) != 1) {
	fprintf(stderr, "Failed to read extension area\n");
	return NULL;
      }
      TargaFixExtensionArea(&fileextension);
      newhandle->author = strdup(fileextension.AuthorName);
      newhandle->jobname = strdup(fileextension.JobName);
      newhandle->comment1 = strdup(fileextension.AuthorComments1);
      newhandle->comment2 = strdup(fileextension.AuthorComments2);
      newhandle->comment3 = strdup(fileextension.AuthorComments3);
      newhandle->comment4 = strdup(fileextension.AuthorComments4);
    }
  } else {
    perror(filename);
  }

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
  return (!fseek(h->file, (long)(sizeof(struct TargaHeader_s)+strlen(h->jobname)+1+(pos*3)), SEEK_SET));
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
  if (h->author != NULL) free(h->author);
  if (h->jobname != NULL) free(h->jobname);
  if (h->comment1 != NULL) free(h->comment1);
  if (h->comment2 != NULL) free(h->comment2);
  if (h->comment3 != NULL) free(h->comment3);
  if (h->comment4 != NULL) free(h->comment4);
  free(h);
}

