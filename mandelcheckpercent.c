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

typedef struct fileheader_s {
  int width;
  int height;
  int maxiter;
  int reserved;
} fileheader;

int main(int argc,char *argv[]) {
  long totpix;
  long highest_value=0;
  long nexthighest_value=0;
  static int *dumpbuffer=NULL;
  static int histbuffer[20];
  fileheader header;
  int x, y, i;
  int o;
  FILE *dumpfile;
  char *dumpfilename=NULL;
  long totpix_done=0, maxiter_count=0, highval_count=0;
  unsigned long maxiter=0;
  while ((o=getopt(argc, argv, "i:m:")) != EOF) {
    switch (o) {
    case 'i': dumpfilename=optarg; break;
    case 'm': maxiter=atoi(optarg); break;
    default: usage(argv[0]); return -1; break;
    }
  }
  
  for (i=0; i<20; i++) {
    histbuffer[i] = 0;
  }

  if (dumpfilename==NULL) {
    usage(argv[0]);
    return -1;
  }

  if (!(dumpfile=fopen(dumpfilename, "r"))) {
    perror(dumpfilename);
    return -2;
  }
  if (fread(&header, sizeof(header), 1, dumpfile) != 1) {
    fprintf(stderr, "Error: File exists but no width/height.\n");
    return -5;
  }
  header.width=ntohl(header.width);
  header.height=ntohl(header.height);
  header.maxiter=ntohl(header.maxiter);

  if (maxiter == 0) {
    maxiter = header.maxiter;
    printf("Maxiter set to %d as read from file.\n", header.maxiter);
  }

  totpix=(long)header.width*header.height;

  if ((dumpbuffer=(int *)mmap(NULL,
			      sizeof(header) + 
			      sizeof(int) * (header.width * header.height),
			       PROT_READ,
			       MAP_SHARED,
			       fileno(dumpfile),
			       0)) == MAP_FAILED) {
    perror("mmap()");
    return 4;
  }

  int highval_threshold = (95 * maxiter) / 100;
  for (y=0; y<header.height; y++) {
    fprintf(stderr, "%d\r", y);
    for (x=0; x<header.width; x++) {
      int val=ntohl(dumpbuffer[4 + x + y * header.width]);
      if (val != -1) {
	totpix_done++;
	if (val < maxiter) histbuffer[(20 * val) / maxiter]++;
      }
      if (val != maxiter) {
	if (val > highval_threshold) highval_count++;
	if (val > highest_value) {
	  nexthighest_value = highest_value;
	  highest_value = val;
	} else if (val < highest_value && val > nexthighest_value) {
	  nexthighest_value = val;
	}
      } else maxiter_count++;
    }
  }
  
  printf("%s: (%d x %d), %g%% done.\n", dumpfilename, header.width, header.height,
	 100*(double)totpix_done/(double)totpix);
  printf("%ld points out of %ld @maxiter (%g%%).\n", maxiter_count, totpix,
	 100*(double)maxiter_count/(double)totpix);
  printf("%ld - %g%% - high value points (within 5%% from maxiter).\n",
	 highval_count, 100*(double)highval_count/(double)totpix);
  printf("Highest two values: %ld and %ld.\n", nexthighest_value, highest_value);
  if (highest_value > maxiter || (highest_value < maxiter && maxiter_count == 0)) {
    printf("You know, %ld %swasn't used as max iterations for this one. I suspect %ld was...\n", maxiter, (maxiter>highest_value)?"probably ":"", highest_value);
  }

  printf("Histogram:\n");
  for (i=0; i<20; i++) {
    printf("\t%d\t%d\n",i,histbuffer[i]);
  }

  if (munmap(dumpbuffer, sizeof(header) + sizeof(int) * (header.width * header.height)) == -1) {
    perror("munmap()");
  }
  
  fclose(dumpfile);
  return 0;
}
