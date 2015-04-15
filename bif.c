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

void usage(char *progname) {
  fprintf(stderr,"Usage: %s -o <out file> -w <width> -h <height> -m <max iterations> [-M <initial iterations>]\n", progname);
}

typedef struct fileheader_s {
  int width;
  int height;
  int maxiter;
  int reserved;
} fileheader;

int main(int argc,char *argv[]) {
  int o, x, y;

  char *dumpfilename=NULL;
  FILE *dumpfile;

  fileheader header;

  int i;

  long width=0;
  long height=0;
  unsigned long maxiter=1048576L;
  unsigned long miniter=50000;

  int *dumpbuffer=NULL;
  int *ybuffer=NULL;

  while ((o=getopt(argc, argv, "o:w:h:m:M:")) != EOF) {
    switch (o) {
    case 'o': dumpfilename=optarg; break;
    case 'w': width=atoi(optarg); break;
    case 'h': height=atoi(optarg); break;
    case 'm': maxiter=atoi(optarg); break;
    case 'M': miniter=atoi(optarg); break;
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

  lseek(fileno(dumpfile), sizeof(header) + sizeof(int)*(width*height-1), SEEK_SET);
  int tmpval=0;
  if (write(fileno(dumpfile), &tmpval, sizeof(tmpval)) == -1) {
    perror("write()");
    return -5;
  }

  if ((dumpbuffer=(int *)mmap(NULL,
			      sizeof(header) + 
			      sizeof(int) * (width * height),
			       PROT_READ|PROT_WRITE,
			       MAP_SHARED,
			       fileno(dumpfile),
			       0)) == MAP_FAILED) {
    perror("mmap()");
    return 4;
  }
  
  fprintf(stderr, "Initializing file...\n");

  header.width=htonl(width);
  header.height=htonl(height);
  header.maxiter=htonl(maxiter);
  header.reserved=0;

  memcpy(dumpbuffer, &header, sizeof(header));

  for (y=0; y<height; y++) {
    fprintf(stderr, "%d\r", y);
    for (x=0; x<width; x++) {
      dumpbuffer[4 + x + y * width] = 0;
    }
  }
  
  fprintf(stderr, "Done.    \n");
  ybuffer=(int *)malloc(height * sizeof(int));

  for (x=0; x<width; x++) {
    double xval=XMIN + (XMAX - XMIN)*(double)x/(double)width;
    fprintf(stderr, "%d     %g	\r", x, xval);
    for (y=0; y<height; y++) ybuffer[y]=0;
    int done=0;
    int final_at=0;
    int y;
    double yval=0.5;
    i = 0;
    while (!done) {
      yval = xval * yval * (1 - yval);
      y=height - height * ((yval - YMIN)/(YMAX - YMIN))- 1;
      if (i++ > miniter && y >= 0 && y < height) {
	if (ybuffer[y] < maxiter) {
	  if (++ybuffer[y] == maxiter && final_at == 0) {
	    final_at = 100;
	  }
	}
      }
      if (final_at > 0 && !(--final_at)) {
	done=1;
      }
    }
    for (y=0; y<height; y++) {
      dumpbuffer[4 + x + y * width] = htonl(ybuffer[y]);
    }
  }

  free(ybuffer);

  fprintf(stderr, "Done.    \n");
  if (munmap(dumpbuffer, sizeof(header) + sizeof(int) * (width * height)) == -1) {
    perror("munmap()");
  }

  fclose(dumpfile);

  return 0;
}
