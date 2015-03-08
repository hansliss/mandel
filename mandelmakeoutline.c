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

#include "targa.h"

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

  if (!(th = TargaOpen(outfilename, width, height, tmpbuf1, tmpbuf2, 1))) return -2;

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
