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
  fprintf(stderr,"Usage: %s -f <dump file> [-m <max iterations>] [-c (convert from long width/height format)] [-M (update maxiter)] [-C (convert from old format with only (32-bit) width/height)]\n", progname);
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
  long wtmp, htmp;
  int wold, hold, i;
  int convertformat=0;
  int setmaxiter=0;
  fileheader header;
  int hsize=0, trailersize=0;
  int x, y;
  int o;
  FILE *dumpfile;
  char *dumpfilename=NULL;
  long totpix_done=0, maxiter_count=0, highval_count=0;
  unsigned long maxiter=1048576L;
  while ((o=getopt(argc, argv, "f:m:cMC")) != EOF) {
    switch (o) {
    case 'f': dumpfilename=optarg; break;
    case 'm': maxiter=atoi(optarg); break;
    case 'c': convertformat=1; break;
    case 'C': convertformat=2; break;
    case 'M': setmaxiter=1; break;
    default: usage(argv[0]); return -1; break;
    }
  }

  if (dumpfilename==NULL) {
    usage(argv[0]);
    return -1;
  }

  if (!(dumpfile=fopen(dumpfilename, "r+"))) {
    perror(dumpfilename);
    return -2;
  }
  if (convertformat == 2) {
    if (fread(&wold, sizeof(wold), 1, dumpfile) != 1) {
      fprintf(stderr, "Error: File exists but no width/height.\n");
      return -5;
    }
    if (fread(&hold, sizeof(hold), 1, dumpfile) != 1) {
      fprintf(stderr, "Error: File exists but no width/height.\n");
      return -5;
    }
    header.width=ntohl(wold);
    header.height=ntohl(hold);
    hsize=sizeof(wold) + sizeof(hold);
    trailersize = sizeof(header) - hsize;
    lseek(fileno(dumpfile), sizeof(header) + sizeof(int)*(header.width*header.height-1), SEEK_SET);
    int tmpval=0;
    if (write(fileno(dumpfile), &tmpval, sizeof(tmpval)) == -1) {
      perror("write()");
      return -5;
    }
  } else if (convertformat == 1) {
    if (fread(&wtmp, sizeof(wtmp), 1, dumpfile) != 1) {
      fprintf(stderr, "Error: File exists but no width/height.\n");
      return -5;
    }
    if (fread(&htmp, sizeof(htmp), 1, dumpfile) != 1) {
      fprintf(stderr, "Error: File exists but no width/height.\n");
      return -5;
    }
    header.width=ntohl(wtmp);
    header.height=ntohl(htmp);
    hsize=sizeof(wtmp) + sizeof(htmp);
  } else {
    if (fread(&header, sizeof(header), 1, dumpfile) != 1) {
      fprintf(stderr, "Error: File exists but no width/height.\n");
      return -5;
    }
    header.width=ntohl(header.width);
    header.height=ntohl(header.height);
    header.maxiter=ntohl(header.maxiter);
    hsize=sizeof(header);
  }
  totpix=(long)header.width*header.height;

  if ((dumpbuffer=(int *)mmap(NULL,
			      hsize + 
			      sizeof(int) * (header.width * header.height) +
			      trailersize,
			      PROT_READ|PROT_WRITE,
			      MAP_SHARED,
			      fileno(dumpfile),
			      0)) == MAP_FAILED) {
    perror("mmap()");
    return 4;
  }

  if (convertformat == 1) {
    if (dumpbuffer[1] != 0 || dumpbuffer[3] != 0) {
      fprintf(stderr, "This dump file is already updated or something else is wrong.\n");
      if (munmap(dumpbuffer, sizeof(long) * 2 + sizeof(int) * (header.width * header.height)) == -1) {
	perror("munmap()");
      }
      
      fclose(dumpfile);
      return 0;
    } 
    printf("Changing format to 32-bit header.width, header.height and maxiter.\n");
    dumpbuffer[1]=dumpbuffer[2];
    dumpbuffer[2]=htonl(maxiter);
  } else if (convertformat == 2) {
    for (i = header.width*header.height - 1; i>=2; i-=2) {
      dumpbuffer[i] = dumpbuffer[i-2];
      dumpbuffer[i-1] = dumpbuffer[i-3];
    }
    dumpbuffer[2] = htonl(maxiter);
    dumpbuffer[3] = 0;
  } else if (setmaxiter) {
    dumpbuffer[2] = htonl(maxiter);
  }
    
  int highval_threshold = (95 * maxiter) / 100;
  for (y=0; y<header.height; y++) {
    fprintf(stderr, "%d\r", y);
    for (x=0; x<header.width; x++) {
      int val=ntohl(dumpbuffer[4 + x + y * header.width]);
      if (val != -1) totpix_done++;
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

  if (munmap(dumpbuffer, sizeof(long) * 2 + sizeof(int) * (header.width * header.height)) == -1) {
    perror("munmap()");
  }
  
  fclose(dumpfile);
  return 0;
}
