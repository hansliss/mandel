#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdarg.h>
#include <gmp.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

#define WIDTH 20480
#define HEIGHT 12800
#define MAXITER 2*524288L
unsigned long maxiter=MAXITER;
#define BAILOUT 4.0

typedef unsigned char byte;

typedef struct fileheader_s {
  int width;
  int height;
  int maxiter;
  int reserved;
} fileheader;

void xmlinit(FILE *xmlfile, char *name, time_t filetime, char *outfile, mpf_t centx, mpf_t centy, mpf_t dx,
	     mpf_t cx0, mpf_t cy0, mpf_t cx1, mpf_t cy1,
	     unsigned int width, unsigned int height, unsigned int maxiter) {
  time_t tt_now = time(NULL);
  struct tm *l_filetime=localtime(&filetime);
  fprintf(xmlfile, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
  fprintf(xmlfile, "<ml:mandellog xmlns:ml=\"http://mandel.liss.nu/mandellog\">\n");
  fprintf(xmlfile, "  <ml:setup name=\"%s\">\n", name);
  fprintf(xmlfile, "    <ml:definition ml:filedate=\"%04d-%02d-%02d %02d:%02d:%02d\">\n",
	l_filetime->tm_year + 1900,
	l_filetime->tm_mon + 1,
	l_filetime->tm_mday,
	l_filetime->tm_hour,
	l_filetime->tm_min,
	l_filetime->tm_sec);
  gmp_fprintf(xmlfile, "      <ml:center_x>%.Fg</ml:center_x>\n", centx);
  gmp_fprintf(xmlfile, "      <ml:center_y>%.Fg</ml:center_y>\n", centy);
  gmp_fprintf(xmlfile, "      <ml:d_x>%.Fg</ml:d_x>\n", dx);
  gmp_fprintf(xmlfile, "      <ml:corner_x0>%.Fg</ml:corner_x0>\n", cx0);
  gmp_fprintf(xmlfile, "      <ml:corner_y0>%.Fg</ml:corner_y0>\n", cy0);
  gmp_fprintf(xmlfile, "      <ml:corner_x1>%.Fg</ml:corner_x1>\n", cx1);
  gmp_fprintf(xmlfile, "      <ml:corner_y1>%.Fg</ml:corner_y1>\n", cy1);
  fprintf(xmlfile, "    </ml:definition>\n");
  struct tm *now=localtime(&tt_now);
  fprintf(xmlfile, "    <ml:render ml:filename=\"%s\" ml:rundate=\"%04d-%02d-%02d %02d:%02d:%02d\">\n",
	  outfile,
	  now->tm_year + 1900,
	  now->tm_mon + 1,
	  now->tm_mday,
	  now->tm_hour,
	  now->tm_min,
	  now->tm_sec);
  fprintf(xmlfile, "      <ml:width>%d</ml:width>\n", width);
  fprintf(xmlfile, "      <ml:height>%d</ml:height>\n", height);
  fprintf(xmlfile, "      <ml:maxiter>%d</ml:maxiter>\n", maxiter);
  fprintf(xmlfile, "    </ml:render>\n");
  fprintf(xmlfile, "  </ml:setup>\n");
  fprintf(xmlfile, "  <ml:log>\n");
}

void xmllog(FILE *xmlfile, unsigned int rownumber, unsigned int rendertime) {
  fprintf(xmlfile, "    <ml:rowtime ml:rowno=\"%d\">%d</ml:rowtime>\n", rownumber, rendertime);
  fflush(xmlfile);
}

void xmlfinish(FILE *xmlfile) {
  fprintf(xmlfile, "  </ml:log>\n");
  fprintf(xmlfile, "</ml:mandellog>\n");
}

void f_epsilon(float *rval) {
  float f_epsilon, testval, one, two;
  one=(float)1.0;
  two=(float)2.0;
  f_epsilon=(float)1.0;
  do {
    (*rval)=f_epsilon;
    f_epsilon /= two;
    testval = one + f_epsilon;
  } while (testval > one);
}

void d_epsilon(double *rval) {
  double f_epsilon, testval, one, two;
  one=(double)1.0;
  two=(double)2.0;
  f_epsilon=(double)1.0;
  do {
    (*rval)=f_epsilon;
    f_epsilon /= two;
    testval = one + f_epsilon;
  } while (testval > one);
}

void ld_epsilon(long double *rval) {
  long double f_epsilon, testval, one, two;
  one=(long double)1.0;
  two=(long double)2.0;
  f_epsilon=(long double)1.0;
  do {
    (*rval)=f_epsilon;
    f_epsilon /= two;
    testval = one + f_epsilon;
  } while (testval > one);
}

void gmp_epsilon(mpf_t rval) {
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

unsigned long f_mandel(float cx, float cy)
{
 unsigned long i=maxiter;
 float zx=cx,zy=cy,zx2=cx*cx,zy2=cy*cy;
#ifdef SPEED
 float dist,ox=cx,oy=cy;
#endif
#ifdef ANGLE
 float angle,ox=cx,oy=cy;
#endif
 while((i>0)&&(zx2+zy2<BAILOUT))
 {
#ifdef SPEED
   ox=zx;oy=zy;
#endif
#ifdef ANGLE
   ox=zx;oy=zy;
#endif
	zy=2*zx*zy + cy;
	zx=zx2-zy2 + cx;
	zx2=zx*zx;
	zy2=zy*zy;
	i--;
 }
#ifdef SPEED
 dist=sqrt((zx-ox)*(zx-ox)+(zy-oy)*(zy-oy));
 if (i>0)
/*   return (unsigned long)((maxiter-i)*dist/2)%(maxiter+1);*/
   return (unsigned long)(maxiter*dist/100)%(maxiter+1);
 else
   return (maxiter-i);
#else
#ifdef ANGLE
 angle=atan2((zx-ox),(zy-oy));
 if (i>0)
/*   return (unsigned long)((maxiter-i)*dist/2)%(maxiter+1);*/
   return (unsigned long)(maxiter*angle/6.28320);
 else
   return (maxiter-i);
#else
 return (maxiter-i);
#endif
#endif
}

unsigned long d_mandel(long double cx, long double cy)
{
 unsigned long i=maxiter;
 long double zx=cx,zy=cy,zx2=cx*cx,zy2=cy*cy;
#ifdef SPEED
 long double dist,ox=cx,oy=cy;
#endif
#ifdef ANGLE
 long double angle,ox=cx,oy=cy;
#endif
 while((i>0)&&(zx2+zy2<BAILOUT))
 {
#ifdef SPEED
   ox=zx;oy=zy;
#endif
#ifdef ANGLE
   ox=zx;oy=zy;
#endif
	zy=2*zx*zy + cy;
	zx=zx2-zy2 + cx;
	zx2=zx*zx;
	zy2=zy*zy;
	i--;
 }
#ifdef SPEED
 dist=sqrt((zx-ox)*(zx-ox)+(zy-oy)*(zy-oy));
 if (i>0)
/*   return (unsigned long)((maxiter-i)*dist/2)%(maxiter+1);*/
   return (unsigned long)(maxiter*dist/100)%(maxiter+1);
 else
   return (maxiter-i);
#else
#ifdef ANGLE
 angle=atan2((zx-ox),(zy-oy));
 if (i>0)
/*   return (unsigned long)((maxiter-i)*dist/2)%(maxiter+1);*/
   return (unsigned long)(maxiter*angle/6.28320);
 else
   return (maxiter-i);
#else
 return (maxiter-i);
#endif
#endif
}

unsigned long ld_mandel(long double cx, long double cy)
{
 unsigned long i=maxiter;
 long double zx=cx,zy=cy,zx2=cx*cx,zy2=cy*cy;
#ifdef SPEED
 long double dist,ox=cx,oy=cy;
#endif
#ifdef ANGLE
 long double angle,ox=cx,oy=cy;
#endif
 while((i>0)&&(zx2+zy2<BAILOUT))
 {
#ifdef SPEED
   ox=zx;oy=zy;
#endif
#ifdef ANGLE
   ox=zx;oy=zy;
#endif
	zy=2*zx*zy + cy;
	zx=zx2-zy2 + cx;
	zx2=zx*zx;
	zy2=zy*zy;
	i--;
 }
#ifdef SPEED
 dist=sqrt((zx-ox)*(zx-ox)+(zy-oy)*(zy-oy));
 if (i>0)
/*   return (unsigned long)((maxiter-i)*dist/2)%(maxiter+1);*/
   return (unsigned long)(maxiter*dist/100)%(maxiter+1);
 else
   return (maxiter-i);
#else
#ifdef ANGLE
 angle=atan2((zx-ox),(zy-oy));
 if (i>0)
/*   return (unsigned long)((maxiter-i)*dist/2)%(maxiter+1);*/
   return (unsigned long)(maxiter*angle/6.28320);
 else
   return (maxiter-i);
#else
 return (maxiter-i);
#endif
#endif
}

unsigned long gmp_mandel(mpf_t cx, mpf_t cy) {
 unsigned long i=maxiter;
 mpf_t zx, zy, zx2, zy2;
 mpf_t bailout, dist;

 mpf_init_set(zx, cx);
 mpf_init_set(zy, cy);
 mpf_init(zx2);
 mpf_mul(zx2, zx, zx);
 mpf_init(zy2);
 mpf_mul(zy2, zy, zy);

 mpf_init_set_d(bailout, 4.0);
 mpf_init(dist);

#ifdef SPEED
 mpf_t dist, ox, oy;
 mpf_init(dist);
 mpf_init_set(ox, zx);
 mpf_init_set(oy, zy);
#endif

#ifdef ANGLE
 mpf_t angle, ox, oy;
 mpf_init(angle);
 mpf_init_set(ox, zx);
 mpf_init_set(oy, zy);
#endif

 do {

#ifdef SPEED
   mpf_init_set(ox, zx);
   mpf_init_set(oy, zy);
#endif

#ifdef ANGLE
   mpf_init_set(ox, zx);
   mpf_init_set(oy, zy);
#endif

   mpf_mul_ui(zy, zy, 2);
   mpf_mul(zy, zy, zx);
   mpf_add(zy, zy, cy);

   mpf_sub(zx, zx2, zy2);
   mpf_add(zx, zx, cx);

   mpf_mul(zx2, zx, zx);
   mpf_mul(zy2, zy, zy);

   i--;

   mpf_add(dist, zx2, zy2);
 } while ((i>0) && mpf_cmp(dist, bailout) < 0);

#ifdef SPEED
 mpf_sub(zx, zx, ox);
 mpf_mul(zx, zx, zx);
 mpf_sub(zy, zy, oy);
 mpf_mul(zy, zy, zy);
 mpf_add(dist, zx, zy);
 mpf_mul_ui(dist, dist, maxiter);
 mpf_div_ui(dist, dist, 100);
 if (i>0)
   return (unsigned long)mpf_get_si(dist)%(maxiter+1);
 else
   return (maxiter-i);
#else
#ifdef ANGLE
 mpf_sub(zx, zx, ox);
 mpf_sub(zy, zy, oy);
 if (i>0)
   return (unsigned long)(maxiter * atan2(mpf_get_d(zx), mpf_get_d(zy)) / 6.28320);
 else
   return (maxiter-i);
#else
 return (maxiter-i);
#endif
#endif
}

void usage(char *progname) {
  fprintf(stderr, "Usage: %s [-w <width>] [-h <height>] [-m <bailout limit>] [-X <xml logfile>] [-g (use gmp arithmetic)] -d <deffile> -o <dumpfile>\n", progname);
}

int main(int argc,char *argv[])
{
  unsigned int x,y, width=WIDTH, height=HEIGHT, xstart=0, ystart=0;
 fileheader header;
 mpf_t x0,x1,y0,y1,centx,centy,dx,dy,xval,yval,vx;

 int usegmp=0;

 int minprec=0;

 float tmpf;
 double tmpd;
 long double tmpld;

 mpf_t mind_f, mind_d, mind_ld, mind_gmp;

 struct timeval starttime, endtime;

 FILE *deffile;
 unsigned int colornum;

 FILE *dumpfile=NULL;
 struct stat dumpfilestat;

 FILE *xmlfile=NULL;

 char *deffilename=NULL;
 char *dumpfilename=NULL;

 struct stat filestat;

 int o;

 mpf_set_default_prec(16384);

 mpf_init(centx);
 mpf_init(centy);
 mpf_init(dx);
 mpf_init(x0);
 mpf_init(x1);
 mpf_init(y0);
 mpf_init(y1);
 mpf_init(dx);
 mpf_init(dy);
 mpf_init(xval);
 mpf_init(yval);
 mpf_init(vx);
 mpf_init(mind_f);
 mpf_init(mind_d);
 mpf_init(mind_ld);
 mpf_init(mind_gmp);

 f_epsilon(&tmpf); mpf_set_d(mind_f, tmpf);
 d_epsilon(&tmpd); mpf_set_d(mind_d, tmpd);
 ld_epsilon(&tmpld); mpf_set_d(mind_ld, tmpld);
 gmp_epsilon(mind_gmp);

 while ((o=getopt(argc, argv, "w:h:d:o:m:FDLGX:g")) != -1) {
   switch (o) {
   case 'd':
     deffilename=optarg;
     break;
   case 'o':
     dumpfilename=optarg;
     break;
   case 'w':
     width=atoi(optarg);
     break;
   case 'h':
     height=atoi(optarg);
     break;
   case 'm':
     maxiter=atol(optarg);
     break;
   case 'F': minprec=0; break;
   case 'D': minprec=1; break;
   case 'L': minprec=2; break;
   case 'G': minprec=3; break;
   case 'g': usegmp=1; break;
   case 'X': if (!(xmlfile=fopen(optarg, "w"))) { perror(optarg); return -2; } break;
   default:
     usage(argv[0]);
     return -1;
   }
 }

 if (!deffilename || !dumpfilename) {
   usage(argv[0]);
   return -1;
 }

 if (!(deffile=fopen(deffilename,"r")))
   {
     fprintf(stderr,"Failed to open definition file %s\n",deffilename);
     return -1;
   }

 gmp_fscanf(deffile,"%Fg\n",centx);
 gmp_fscanf(deffile,"%Fg\n",centy);
 gmp_fscanf(deffile,"%Fg\n",dx);

 if (xmlfile) {
   fstat(fileno(deffile), &filestat);
 }

 fclose(deffile);

 if (width <= height) { 
   // dy = height * dx / width;
   mpf_set(dy, dx);
   mpf_mul_ui(dy, dy, height);
   mpf_div_ui(dy, dy, width);
 } else {
   // dy = dx_in
   // dx = width * dy / height
   mpf_set(dy, dx);
   mpf_mul_ui(dx, dx, width);
   mpf_div_ui(dx, dx, height);
 }
   
 // x0 = centx - dx / 2
 mpf_div_ui(x0, dx, 2);
 mpf_sub(x0, centx, x0);

 // x1 = centx + dx / 2
 mpf_div_ui(x1, dx, 2);
 mpf_add(x1, centx, x1);
   
 // y0 = centy - dy / 2
 mpf_div_ui(y0, dy, 2);
 mpf_sub(y0, centy, y0);

 // y1 = centy + dy / 2
 mpf_div_ui(y1, dy, 2);
 mpf_add(y1, centy, y1);
 
 mpf_sub(vx, x1, x0);
 mpf_div_ui(vx, dx, width);

 gmp_printf("Current center is %.Fg, %.Fg, dx=%.Fg\n",centx,centy,dx);
 gmp_printf("Corners are %.Fg, %.Fg  and  %.Fg, %.Fg\n",x0,y0,x1,y1);
 gmp_printf("\npixel=%Fg - epsilons are: float=%Fg, double=%Fg, long double=%Fg, gmp=%Fg\n",
	    vx, mind_f, mind_d, mind_ld, mind_gmp);

 if (xmlfile) {
   xmlinit(xmlfile, deffilename, filestat.st_mtime, dumpfilename, centx, centy, dx, x0, y0, x1, y1, width, height, maxiter);
 }

 // Try to determine if we were doing a dump already to this file, and if the width and height
 // for that dump were the same as this one. If so, continue where we left off.
 xstart=0;
 ystart=0;
 dumpfile=NULL;
 if (!stat(dumpfilename, &dumpfilestat)) {
   dumpfile=fopen(dumpfilename, "rb");
   if (fread(&header, sizeof(header), 1, dumpfile) != 1) {
     fprintf(stderr, "Can't read header from file.\n");
     return -2;
   }
   header.width=ntohl(header.width);
   header.height=ntohl(header.height);
   header.maxiter=ntohl(header.maxiter);
   if (header.width == width && header.height == height) {
     long alreadydone = (dumpfilestat.st_size - sizeof(header)) / sizeof(colornum);
     xstart = alreadydone % width;
     ystart = alreadydone / width;
     fprintf(stderr, "Continuing at (%d,%d)\n", xstart, ystart);
     fclose(dumpfile);
     dumpfile=fopen(dumpfilename, "ab");     
   } else {
     fclose(dumpfile);
     dumpfile=NULL;
   }
 }

 if (dumpfile == NULL) {
   dumpfile=fopen(dumpfilename, "wb");
   if (!dumpfile) {
     perror(dumpfilename);
     return -1;
   }

   header.width=htonl(width);
   header.height=htonl(height);
   header.maxiter=htonl(maxiter);
   fwrite(&header, sizeof(header), 1, dumpfile);
 }

 if (minprec <= 0 && mpf_cmp(vx, mind_f) > 0) {
   float x0_f, y0_f, x1_f, y1_f, xval_f, yval_f;
   unsigned int row=0;
   printf("(using \"float\" arithmetic)\n");
   x0_f=mpf_get_d(x0);
   y0_f=mpf_get_d(y0);
   x1_f=mpf_get_d(x1);
   y1_f=mpf_get_d(y1);
   for (y=ystart;y<height;y++) {   
     fprintf(stderr,"  Line: %d\r",y);
     if (xmlfile) {
       gettimeofday(&starttime, NULL);
     }
     yval_f = y0_f + (y1_f - y0_f)*(float)(height-y)/(float)height;
     for (x=xstart;x<width;x++) {
       xval_f = x0_f + (x1_f - x0_f)*(float)x/(float)width;
       colornum=f_mandel(xval_f,yval_f);
       colornum=htonl(colornum);
       fwrite(&colornum, sizeof(colornum), 1, dumpfile);
     }
     if (xmlfile) {
       gettimeofday(&endtime, NULL);
       xmllog(xmlfile, row++, (endtime.tv_sec - starttime.tv_sec) * 1000 + (endtime.tv_usec - starttime.tv_usec) / 1000);
     }
     fflush(dumpfile);
     xstart=0;
   }
 } else if (minprec <= 1 && mpf_cmp(vx, mind_d) > 0) {
   double x0_d, y0_d, x1_d, y1_d, xval_d, yval_d;
   unsigned int row=0;
   printf("(using \"double\" arithmetic)\n");
   x0_d=mpf_get_d(x0);
   y0_d=mpf_get_d(y0);
   x1_d=mpf_get_d(x1);
   y1_d=mpf_get_d(y1);
   for (y=ystart;y<height;y++) {   
     fprintf(stderr,"  Line: %d\r",y);
     if (xmlfile) {
       gettimeofday(&starttime, NULL);
     }
     yval_d = y0_d + (y1_d - y0_d)*(double)(height-y)/(double)height;
     for (x=xstart;x<width;x++) {
       xval_d = x0_d + (x1_d - x0_d)*(double)x/(double)width;
       colornum=d_mandel(xval_d,yval_d);
       colornum=htonl(colornum);
       fwrite(&colornum, sizeof(colornum), 1, dumpfile);
     }
     if (xmlfile) {
       gettimeofday(&endtime, NULL);
       xmllog(xmlfile, row++, (endtime.tv_sec - starttime.tv_sec) * 1000 + (endtime.tv_usec - starttime.tv_usec) / 1000);
     }
     fflush(dumpfile);
     xstart=0;
   }
 } else if (minprec <= 2 && mpf_cmp(vx, mind_ld) > 0) {
   long double x0_ld, y0_ld, x1_ld, y1_ld, xval_ld, yval_ld;
   unsigned int row=0;
   printf("(using \"long double\" arithmetic)\n");
   x0_ld=mpf_get_d(x0);
   y0_ld=mpf_get_d(y0);
   x1_ld=mpf_get_d(x1);
   y1_ld=mpf_get_d(y1);
   for (y=ystart;y<height;y++) {   
     fprintf(stderr,"  Line: %d\r",y);
     if (xmlfile) {
       gettimeofday(&starttime, NULL);
     }
     yval_ld = y0_ld + (y1_ld - y0_ld)*(long double)(height-y)/(long double)height;
     for (x=xstart;x<width;x++) {
       xval_ld = x0_ld + (x1_ld - x0_ld)*(long double)x/(long double)width;
       colornum=ld_mandel(xval_ld,yval_ld);
       colornum=htonl(colornum);
       fwrite(&colornum, sizeof(colornum), 1, dumpfile);
     }
     if (xmlfile) {
       gettimeofday(&endtime, NULL);
       xmllog(xmlfile, row++, (endtime.tv_sec - starttime.tv_sec) * 1000 + (endtime.tv_usec - starttime.tv_usec) / 1000);
     }
     fflush(dumpfile);
     xstart=0;
   }
 } else if (usegmp && minprec <= 3 && mpf_cmp(vx, mind_gmp) > 0) {
   unsigned int row=0;
   printf("(using \"gmp\" multi-precision arithmetic)\n");
   for (y=ystart;y<height;y++) {   
     fprintf(stderr,"  Line: %d\r",y);
     if (xmlfile) {
       gettimeofday(&starttime, NULL);
     }
     mpf_sub(yval, y1, y0);
     mpf_mul_ui(yval, yval, height-y);
     mpf_div_ui(yval, yval, height);
     mpf_add(yval, yval, y0);
     
     for (x=xstart;x<width;x++) {
       mpf_sub(xval, x1, x0);
       mpf_mul_ui(xval, xval, x);
       mpf_div_ui(xval, xval, width);
       mpf_add(xval, xval, x0);
       colornum=gmp_mandel(xval,yval);
       colornum=htonl(colornum);
       fwrite(&colornum, sizeof(colornum), 1, dumpfile);
     }
     if (xmlfile) {
       gettimeofday(&endtime, NULL);
       xmllog(xmlfile, row++, (endtime.tv_sec - starttime.tv_sec) * 1000 + (endtime.tv_usec - starttime.tv_usec) / 1000);
     }
     fflush(dumpfile);
     xstart=0;
   }
 } else fprintf(stderr, "pixel width is too small!\n");
 fclose(dumpfile);
 if (xmlfile) {
   xmlfinish(xmlfile);
   fclose(xmlfile);
 }
 return 0;
}
