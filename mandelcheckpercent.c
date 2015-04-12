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
#include <sys/resource.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <sys/mman.h>

void usage(char *progname) {
  fprintf(stderr,"Usage: %s -i <dump file> [-m <max iterations>]\n", progname);
}

int main(int argc,char *argv[]) {
  long totpix;
  int highest_value=0;
  int nexthighest_value=0;
  static int *dumpbuffer=NULL;
  long width, height;
  int x, y;
  int o;
  FILE *dumpfile;
  char *dumpfilename=NULL;
  long totpix_done=0, maxiter_count=0, highval_count=0;
  unsigned long maxiter=1048576L;
  while ((o=getopt(argc, argv, "i:m:")) != EOF) {
    switch (o) {
    case 'i': dumpfilename=optarg; break;
    case 'm': maxiter=atoi(optarg); break;
    default: usage(argv[0]); return -1; break;
    }
  }

  if (dumpfilename==NULL) {
    usage(argv[0]);
    return -1;
  }

  if (!(dumpfile=fopen(dumpfilename, "r"))) {
    perror(dumpfilename);
    return -2;
  }
  if (fread(&width, sizeof(width), 1, dumpfile) != 1) {
    fprintf(stderr, "Error: File exists but no width/height.\n");
    return -5;
  }
  if (fread(&height, sizeof(height), 1, dumpfile) != 1) {
    fprintf(stderr, "Error: File exists but no width/height.\n");
    return -5;
  }
  width=ntohl(width);
  height=ntohl(height);
  totpix=(long)width*height;

  if ((dumpbuffer=(int *)mmap(NULL,
			      sizeof(width) + sizeof(height) + 
			      sizeof(int) * (width * height),
			       PROT_READ,
			       MAP_SHARED,
			       fileno(dumpfile),
			       0)) == MAP_FAILED) {
    perror("mmap()");
    return 4;
  }

  int highval_threshold = (95 * maxiter) / 100;
  for (y=0; y<height; y++) {
    fprintf(stderr, "%d\r", y);
    for (x=0; x<width; x++) {
      int val=ntohl(dumpbuffer[4 + x + y * width]);
      if (val != maxiter) {
	if (val != -1) totpix_done++;
	if (val > highval_threshold) highval_count++;
	if (val > highest_value) {
	  nexthighest_value = highest_value;
	  highest_value = val;
	} else if (val > nexthighest_value) {
	  nexthighest_value = val;
	}
      } else maxiter_count++;
    }
  }
  
  printf("%s: (%ld x %ld), %g%% done.\n", dumpfilename, width, height,
	 100*(double)totpix_done/(double)totpix);
  printf("%ld points out of %ld @maxiter (%g%%).\n", maxiter_count, totpix,
	 100*(double)maxiter_count/(double)totpix);
  printf("%ld - %g%% - high value points (within 5%% from maxiter).\n",
	 highval_count, 100*(double)highval_count/(double)totpix);
  printf("Highest two values: %d and %d.\n", nexthighest_value, highest_value);

  if (munmap(dumpbuffer, sizeof(long) * 2 + sizeof(int) * (width * height)) == -1) {
    perror("munmap()");
  }
  
  fclose(dumpfile);
  return 0;
}
