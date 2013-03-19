#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <gmp.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>

#define DEFAULTWIDTH 4096
#define DEFAULTHEIGHT 4096

void epsilon(mpf_t rval) {
  mpf_t f_epsilon, testval, one, two;
  mpf_init_set_d(one, 1);
  mpf_init_set_d(two, 2);
  mpf_init_set_d(f_epsilon, 1);
  mpf_init(testval);
  do {
    mpf_set(rval, f_epsilon);
    mpf_div(f_epsilon, f_epsilon, two);
    mpf_add(testval, one, f_epsilon);
  } while (mpf_cmp(testval, one) > 0);
}

void usage(char *progname) {
  fprintf(stderr, "Usage: %s -i <infile> -o <outfile> [-w <width>] [-h <height>]\n", progname);
  fprintf(stderr, "\t[-x <center xcoord>] [-y <center ycoord>] [-z <zoom>] [-m (mirror around y axis)]\n");
}

int main(int argc,char *argv[]) {
 int x=-1, y=-1, width=DEFAULTWIDTH, height=DEFAULTHEIGHT;
 int mirrorx=0;

 mpf_t z, centx, centy, dx, dy, newdx, newdy, newz, tmpf;
 mpf_t mind;

 mpf_t chx, chy, chxp, chyp;

 char *infilename=NULL, *outfilename=NULL;
 FILE *infile=NULL, *outfile=NULL;

 static char tmpbuf[PATH_MAX];

 struct stat statbuf;

 int o;

 mpf_set_default_prec(16384);
 mpf_init_set_d(z, 1);
 mpf_init(centx);
 mpf_init(centy);
 mpf_init(dx);
 mpf_init(dy);
 mpf_init(newdx);
 mpf_init(newdy);
 mpf_init(newz);
 mpf_init(tmpf);
 mpf_init(chx);
 mpf_init(chy);
 mpf_init(chxp);
 mpf_init(chyp);
 mpf_init(mind);
 epsilon(mind);

 gmp_printf("epsilon=%.4FE\n", mind);

 while ((o=getopt(argc, argv, "w:h:x:y:z:i:o:m")) != -1) {
   switch (o) {
   case 'w': width=atoi(optarg); break;
   case 'h': height=atoi(optarg); break;
   case 'x': x=atoi(optarg); break;
   case 'y': y=atoi(optarg); break;
   case 'z': gmp_sscanf(optarg, "%Fg", z); break;
   case 'i': infilename=optarg; break;
   case 'o': outfilename=optarg; break;
   case 'm': mirrorx=1; break;
   default: usage(argv[0]); return -1; break;
   }
 }

 if (!outfilename) outfilename=infilename;

 if (optind < argc || !infilename || !outfilename) {
   usage(argv[0]);
   return -1;
 }

 if (!(infile=fopen(infilename, "r"))) {
   perror(infilename);
   return -2;
 }

 if (x == -1) x = width/2;
 if (y == -1) y = width/2;

 gmp_fscanf(infile,"%Fg\n",centx);
 gmp_fscanf(infile,"%Fg\n",centy);
 gmp_fscanf(infile,"%Fg\n",dx);
 fclose(infile);

 if (!stat(outfilename, &statbuf)) {
   sprintf(tmpbuf, "%s~", outfilename);
   rename(outfilename, tmpbuf);
 }

 if (!(outfile=fopen(outfilename, "w"))) {
   perror(outfilename);
   return -2;
 }

 mpf_set(dy, dx);
 mpf_mul_ui(dy, dy, height);
 mpf_div_ui(dy, dy, width);

 if (mirrorx) {
   gmp_printf("Mirroring x around y axis\n");
   mpf_neg(centx, centx);
 }

 gmp_printf("Input:\tsize is %dx%d pixels\n\tcenter is %.Fe, %.Fe\n\tdx=%.Fe, dy=%.Fe\n", 
	width, height, centx, centy, dx, dy);

 mpf_set_si(chx, x - width/2);
 mpf_div_ui(chx, chx, width);

 mpf_set_si(chy, y - height/2);
 mpf_div_ui(chy, chy, height);

 mpf_set_ui(tmpf, 100);
 mpf_mul(chxp, chx, tmpf);
 mpf_mul(chyp, chy, tmpf);

 gmp_printf("Moving x %d pixels = %.Fg%%, y %d pixels = %.Fg%%\n", x-width/2, chxp, y-height/2, chyp);

 mpf_set(tmpf, dx);
 mpf_mul(tmpf, tmpf, chx);
 mpf_add(centx, centx, tmpf);

 mpf_set(tmpf, dy);
 mpf_mul(tmpf, tmpf, chy);
 mpf_sub(centy, centy, tmpf);

 mpf_set(newz, z);
 mpf_set(newdx, dx);
 mpf_div(newdx, newdx, z);
 mpf_set(tmpf,newdx);
 mpf_div_ui(tmpf, tmpf, width);

 if (mpf_cmp(tmpf, mind) < 0) {
   mpf_set(newdx, mind);
   mpf_mul_ui(newdx, newdx, width);
   mpf_set(newz, dx);
   mpf_div(newz, newz, newdx);
 }

 mpf_set(newdy, newdx);
 mpf_mul_ui(newdy, newdy, height);
 mpf_div_ui(newdy, newdy, width);
 mpf_set(tmpf, newdy);
 mpf_div_ui(tmpf, tmpf, height);

 if (mpf_cmp(tmpf, mind) < 0) {
   mpf_set(newdy, mind);
   mpf_mul_ui(newdy, newdy, height);
   mpf_set(newz, dy);
   mpf_div(newz, newz, newdy);
   mpf_set(newdx, dx);
   mpf_div(newdx, newdx, newz);
 }

 if (mpf_cmp(newz, z) != 0) gmp_printf("Zoom level changed from %.Fg to the maximum (at %dx%d pixels) of %.Fg\n", z, width, height, newz);
 mpf_set(dx, newdx);
 mpf_set(dy, newdy);
 mpf_set(z, newz);

 gmp_printf("Output:\tcenter is %.Fg, %.Fg\n\tdx=%.Fg, dy=%.Fg\n",
	centx, centy, dx, dy);

 gmp_fprintf(outfile, "%.Fe\n", centx);
 gmp_fprintf(outfile, "%.Fe\n", centy);
 gmp_fprintf(outfile, "%.Fe\n", dx);
 fclose(outfile);

 return 0;
}
