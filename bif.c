#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdarg.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <sys/mman.h>

#define XMIN 2.5
#define XMAX 4.0
#define YMAX 1.0
#define YMIN 0.0

#define MINITER 50

void usage(char *progname) {
  fprintf(stderr,"Usage: %s -o <out file> -w <width> -h <height> -m <max iterations>\n", progname);
}

int main(int argc,char *argv[]) {
  int o;

  char *dumpfilename=NULL;
  FILE *dumpfile;

  int i;

  long width=0;
  long height=0;
  unsigned long maxiter=1048576L;

  static int *dumpbuffer=NULL;

  while ((o=getopt(argc, argv, "o:w:h:m:")) != EOF) {
    switch (o) {
    case 'o': dumpfilename=optarg; break;
    case 'w': width=atoi(optarg); break;
    case 'h': height=atoi(optarg); break;
    case 'm': maxiter=atoi(optarg); break;
    default: usage(argv[0]); return -1; break;
    }
  }

  if (!dumpfilename) {
    usage(argv[0]);
    return -1;
  }
  
  if (!(dumpfile=fopen(dumpfilename, "w+"))) {
    perror(dumpfilename);
    return -2;
  }

  lseek(fileno(dumpfile), sizeof(width) + sizeof(height) + sizeof(int)*(width*height-1), SEEK_SET);
  int tmpval=0;
  if (write(fileno(dumpfile), &tmpval, sizeof(tmpval)) == -1) {
    perror("write()");
    return -5;
  }

  if ((dumpbuffer=(int *)mmap(NULL,
			      sizeof(width) + sizeof(width) + 
			      sizeof(int) * (width * height),
			       PROT_READ|PROT_WRITE,
			       MAP_SHARED,
			       fileno(dumpfile),
			       0)) == MAP_FAILED) {
    perror("mmap()");
    return 4;
  }
  
  fprintf(stderr, "Initializing file...\n");

  dumpbuffer[0]=htonl(width);
  dumpbuffer[2]=htonl(height);
  msync(&(dumpbuffer[0]), 4 * sizeof(int), MS_ASYNC);
  for (int y=0; y<height; y++) {
    fprintf(stderr, "%d\r", y);
    for (int x=0; x<width; x++) {
      dumpbuffer[4 + x + y * width] = 0;
    }
    msync(&(dumpbuffer[4 + y*width]), width * sizeof(int), MS_ASYNC);
  }
  
  fprintf(stderr, "Done.    \n");

  for (int x=0; x<width; x++) {
    double xval=XMIN + (XMAX - XMIN)*(double)x/(double)width;
    fprintf(stderr, "%d     %g	\r", x, xval);
    int done=0;
    int y;
    double yval=0.5;
    i = 0;
    while (!done) {
      yval = xval * yval * (1 - yval);
      y=height - height * ((yval - YMIN)/(YMAX - YMIN))- 1;
      if (i++ > MINITER && y >= 0 && y < height) {
	if ((++dumpbuffer[4 + x + y * width]) >= maxiter) {
	  done = 1;
	}
      }
    }
    for (y=0; y<height; y++) {
      dumpbuffer[4 + x + y * width] = htonl(dumpbuffer[4 + x + y * width]);
    }
  }
  
  fprintf(stderr, "Done.    \n");
  if (munmap(dumpbuffer, sizeof(long) * 2 + sizeof(int) * (width * height)) == -1) {
    perror("munmap()");
  }

  fclose(dumpfile);

  return 0;
}
