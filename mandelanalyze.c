#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <netinet/in.h>

int main(int argc, char *argv[])
{
  FILE *infile;
  unsigned int width, height, *buffer, i, j, x, y, *hist, vmax, n;
  unsigned long wtmp, htmp;
  unsigned int maxiter;

  if (argc!=2) { fprintf(stderr, "missing filename\n"); return -1; }
  if (!(infile=fopen(argv[1], "rb"))) { perror(argv[1]); return -2; }
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

  for (i=0; i<n; i++) buffer[i]=ntohl(buffer[i]);

  vmax=0;
  maxiter=0;
  for (y=0; y<height; y++)
    for (x=0; x<width; x++){
      /*      printf("0x%04X\n", buffer[x + y*height]);*/
      if (buffer[x + y*height] != 1048576L && buffer[x + y*height] > vmax) vmax=buffer[x + y*height];
    }
  if (!(hist=(unsigned int *)malloc((vmax+1) * sizeof(unsigned int)))) {
    perror("malloc()");
    return -3;
  }
  memset(hist, 0, (vmax+1) * sizeof(unsigned int));
  for (y=0; y<height; y++)
    for (x=0; x<width; x++) {
      if (buffer[x + y*height] != 1048576L)
	hist[buffer[x + y*height]]++;
      else
	maxiter++;
    }
  for (j=vmax; j>0 && hist[j]<2; j--);
  for (i=0; i<=j; i++)
    if (hist[i] > 0) break;

  printf("maxiter\t%u\n", maxiter);
  for (; i<=j; i++)
    printf("0x%04X\t%u\n", i, hist[i]);

  free(hist);
  free(buffer);
  fclose(infile);
  return 0;
}
