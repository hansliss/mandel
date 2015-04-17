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

#define XMIN -1.5
#define XMAX 1.5
#define YMAX -0.4
#define YMIN 0.4

void usage(char *progname) {
  fprintf(stderr,"Usage: %s -o <out file> -w <width> -h <height> -m <max iterations> [-M <initial iterations>] [-F <final iterations after reaching maxiter>]\n", progname);
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

  long width=0;
  long height=0;
  unsigned long maxiter=1048576L;
  unsigned long miniter=50000;
  unsigned long finalrun=1000000;

  double a = 1.4;
  double b = 0.3;

  int *dumpbuffer=NULL;

  while ((o=getopt(argc, argv, "o:w:h:m:M:F:")) != EOF) {
    switch (o) {
    case 'o': dumpfilename=optarg; break;
    case 'w': width=atoi(optarg); break;
    case 'h': height=atoi(optarg); break;
    case 'm': maxiter=atoi(optarg); break;
    case 'M': miniter=atoi(optarg); break;
    case 'F': finalrun=atoi(optarg); break;
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

  double xval = 0.5;
  double yval = 0.5;
  int count, maxcount=0;
  int xpmax=0, ypmax=0, xpmin = width-1, ypmin = height-1;
  int done=0, final_at=0;
  int mitigator=0;
  int lastcount=0, maxiter_hits=0;
  while (!done) {
    if ((++mitigator) % 100 == 0 && maxcount != lastcount) {
      fprintf(stderr, "(%d,%d),(%d,%d) maxcount=%d     \r", xpmin,ypmin,xpmax,ypmax,maxcount);
      mitigator = 0;
      lastcount=maxcount;
    }

    double newy = b * xval;
    double newx = 1 - a * xval * xval + yval;
    xval = newx;
    yval = newy;

    int x = width * (xval - XMIN) / (XMAX - XMIN);
    int y = height - height * ((yval - YMIN) / (YMAX - YMIN)) - 1;

    if (x < xpmin) xpmin = x;
    if (x > xpmax) xpmax = x;
    if (y < ypmin) ypmin = y;
    if (y > ypmax) ypmax = y;

    if (x > 0 && y > 0 && x < width && y < height) {
      if (dumpbuffer[4 + x + y * width] < maxiter) {
	if ((count = ++dumpbuffer[4 + x + y * width]) >= maxiter) {
	  maxiter_hits++;
	  if (maxiter_hits >= 50000 && final_at == 0) {
	    final_at = finalrun;
	  }
	}
	if (count > maxcount) maxcount = count;
	if (final_at > 0 && !(--final_at)) {
	  done = 1;
	}
      }
    }
  }

  for (x=0; x < width; x++) {
    for (y=0; y < height; y++) {
      if (dumpbuffer[4 + x + y * width] != 0) {
	dumpbuffer[4 + x + y * width] = htonl(dumpbuffer[4 + x + y * width]);
      }
    }
  }

  fprintf(stderr, "Done.    \n");
  if (munmap(dumpbuffer, sizeof(header) + sizeof(int) * (width * height)) == -1) {
    perror("munmap()");
  }

  fclose(dumpfile);

  return 0;
}
