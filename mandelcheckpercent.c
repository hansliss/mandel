#define _FILE_OFFSET_BITS 64

#include "config.h"

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

#ifdef HAVE_LIBPNG
#include <png.h>
#endif

void usage(char *progname) {
  fprintf(stderr,"Usage: %s -i <dump file> [-m <max iterations>]\n", progname);
}

void abort_(const char * s, ...)
{
  va_list args;
  va_start(args, s);
  vfprintf(stderr, s, args);
  fprintf(stderr, "\n");
  va_end(args);
  abort();
}

typedef struct fileheader_s {
  int width;
  int height;
  int maxiter;
  int reserved;
} fileheader;

#ifdef HAVE_LIBPNG
void writepng(char *filename, fileheader *header, int lowest_value, int highest_value, int *dumpbuffer, int sixteenbits, int dolog) {
  int x, y;

  int bits=8;
  if (sixteenbits) bits=16;

  png_structp png_ptr;
  png_infop info_ptr;
  png_bytep *row_pointers;

  FILE *fp = fopen(filename, "wb");
  if (!fp)
    abort_("[write_png_file] File %s could not be opened for writing", filename);

  /* initialize stuff */
  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

  if (!png_ptr)
    abort_("[write_png_file] png_create_write_struct failed");

  info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr)
    abort_("[write_png_file] png_create_info_struct failed");

  if (setjmp(png_jmpbuf(png_ptr)))
    abort_("[write_png_file] Error during init_io");

  png_init_io(png_ptr, fp);

  /* write header */
  if (setjmp(png_jmpbuf(png_ptr)))
    abort_("[write_png_file] Error during writing header");

  png_set_IHDR(png_ptr, info_ptr, header->width, header->height,
	       bits, PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
	       PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

  png_write_info(png_ptr, info_ptr);

  /* write bytes */
  if (setjmp(png_jmpbuf(png_ptr)))
    abort_("[write_png_file] Error during writing bytes");

  row_pointers = png_malloc(png_ptr,sizeof(png_bytep) * header->height);
  int maxval=(1 << bits) - 1; 
  for (y=0; y < header->height; y++) {
    row_pointers[y]= png_malloc(png_ptr, (bits/8) * header->width);
    for (x=0; x < header->width; x++) {
      unsigned int val, inval=ntohl(dumpbuffer[4 + x + y * header->width]);
      if (inval == header->maxiter) val=maxval;
      else {
	if (inval > highest_value) val=highest_value;
	else val=inval;
	if (dolog) {
	  val=lowest_value + (double)(highest_value - lowest_value) * log10(val)/log10(highest_value);
	}
	val = ((long double)(val - lowest_value) * (long double)(maxval-1)) / (long double)(highest_value-lowest_value);
      }
      // Network byte order - MSB first
      if (bits==16) {
	row_pointers[y][x*2] = (val & 0xFF00) >> 8;
	row_pointers[y][x*2 + 1] = val & 0xFF;
      } else {
	row_pointers[y][x] = val & 0xFF;
      }
    }
  }
  png_write_image(png_ptr, row_pointers);

  /* end write */
  if (setjmp(png_jmpbuf(png_ptr)))
    abort_("[write_png_file] Error during end of write");

  png_write_end(png_ptr, NULL);

  /* cleanup heap allocation */
  for (y=0; y < header->height; y++)
    png_free(png_ptr, row_pointers[y]);
  png_free(png_ptr, row_pointers);

  fclose(fp);
}
#endif

int main(int argc,char *argv[]) {
  long totpix;
  long highest_value=0;
  long lowest_value=999999999999;
  long nexthighest_value=0;
  static int *dumpbuffer=NULL;
  static int histbuffer[20];
  fileheader header;
  char *pngfilename = NULL;
  int x, y, i;
  int o;
  int sixteenbits=1;
  int dolog=0;
  FILE *dumpfile;
  char *dumpfilename=NULL;
  long totpix_done=0, maxiter_count=0, highval_count=0;
  unsigned long maxiter=0;
  while ((o=getopt(argc, argv, "i:m:P:8L")) != EOF) {
    switch (o) {
    case 'i': dumpfilename=optarg; break;
    case 'm': maxiter=atoi(optarg); break;
    case 'P': pngfilename=optarg; break;
    case '8': sixteenbits=0; break;
    case 'L': dolog=1; break;
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
	if (val != -1 && val < lowest_value) lowest_value=val;
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
  if (lowest_value != 999999999999) printf("Lowest value: %ld.\n", lowest_value);
  printf("Highest two values: %ld and %ld.\n", nexthighest_value, highest_value);
  if (highest_value > maxiter || (highest_value < maxiter && maxiter_count == 0)) {
    printf("You know, %ld %swasn't used as max iterations for this one. I suspect %ld was...\n", maxiter, (maxiter>highest_value)?"probably ":"", highest_value);
  }

  printf("Histogram:\n");
  for (i=0; i<20; i++) {
    printf("\t%d\t%d\n",i,histbuffer[i]);
  }
  // Filter out the straddlers at the top end so they don't corrupt the scaling
  i=19;
  while (i > 0 && ((double)histbuffer[i]/(double)totpix) < 2.4E-4) i--;
  highest_value=((i + 1) * maxiter / 20);
  if (highest_value >= maxiter) highest_value=maxiter-1;
  printf("highest value after adjustment: %ld\n", highest_value);

  if (pngfilename != NULL) {
#ifdef HAVE_LIBPNG
    writepng(pngfilename, &header, lowest_value, highest_value, dumpbuffer, sixteenbits, dolog);
#else
    fprintf(stderr, "No png support on this system.\n");
#endif
  }

  if (munmap(dumpbuffer, sizeof(header) + sizeof(int) * (header.width * header.height)) == -1) {
    perror("munmap()");
  }
  
  fclose(dumpfile);
  return 0;
}
