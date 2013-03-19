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

#define WIDTH 20480
#define HEIGHT 12800
#define MAXITER 2*524288L
unsigned long maxiter=MAXITER;
#define BAILOUT 4.0

typedef unsigned char byte;

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
	zy=2*zx*zy - cy;
	zx=zx2-zy2 - cx;
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
	zy=2*zx*zy - cy;
	zx=zx2-zy2 - cx;
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
	zy=2*zx*zy - cy;
	zx=zx2-zy2 - cx;
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
   mpf_sub(zy, zy, cy);

   mpf_sub(zx, zx2, zy2);
   mpf_sub(zx, zx, cx);

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
  fprintf(stderr, "Usage: %s [-m <bailout limit>] [-d] <infile>\n", progname);
}

int main(int argc,char *argv[])
{
 unsigned int x,y, width=WIDTH, height=HEIGHT;
 unsigned long wtmp, htmp;
 mpf_t x0,x1,y0,y1,centx,centy,dx,xval,yval,vx;

 int minprec=0;

 float tmpf;
 double tmpd;
 long double tmpld;

 mpf_t mind_f, mind_d, mind_ld, mind_gmp;

 FILE *deffile;
 char filename[80];
 unsigned int colornum;
 char *p;
 FILE *outfile=NULL;

 char *ofilename=NULL;
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

 while ((o=getopt(argc, argv, "d:m:FDLG")) != -1) {
   switch (o) {
   case 'd':
     ofilename=optarg;
     break;
   case 'm':
     maxiter=atol(optarg);
     break;
   case 'F': minprec=0; break;
   case 'D': minprec=1; break;
   case 'L': minprec=2; break;
   case 'G': minprec=3; break;
   default:
     usage(argv[0]);
     return -1;
   }
 }

 if (!ofilename && (optind < argc)) {
   ofilename=argv[optind++];
 }

 if (!ofilename || (optind < argc)) {
   usage(argv[0]);
   return -1;
 }

 strcpy(filename,ofilename);
 if ((p=(char *)strrchr(filename,'.'))!=NULL)
   *p='\0';
 strcat(filename,".def");
 if (!(deffile=fopen(filename,"r")))
   {
     fprintf(stderr,"Failed to open definition file %s\n",filename);
     return -1;
   }

 gmp_fscanf(deffile,"%Fg\n",centx);
 gmp_fscanf(deffile,"%Fg\n",centy);
 gmp_fscanf(deffile,"%Fg\n",dx);
 fclose(deffile);
 
 mpf_div_ui(x0, dx, 2);
 mpf_sub(x0, centx, x0);
 mpf_div_ui(x1, dx, 2);
 mpf_add(x1, centx, x1);

 mpf_mul_ui(y0, dx, height);
 mpf_div_ui(y0, y0, 2*width);
 mpf_sub(y0, centy, y0);

 mpf_mul_ui(y1, dx, height);
 mpf_div_ui(y1, y1, 2*width);
 mpf_add(y1, centy, y1);

 mpf_sub(vx, x1, x0);
 mpf_div_ui(vx, vx, WIDTH);

 gmp_printf("Current center is %.Fg, %.Fg, dx=%.Fg\n",centx,centy,dx);
 gmp_printf("Corners are %.Fg, %.Fg  and  %.Fg, %.Fg\n",x0,y0,x1,y1);
 gmp_printf("\npixel=%Fg - epsilons are: float=%Fg, double=%Fg, long double=%Fg, gmp=%Fg\n",
	    vx, mind_f, mind_d, mind_ld, mind_gmp);

 strcpy(filename,ofilename);

 if ((p=(char *)strrchr(filename,'.'))!=NULL)
   *p='\0';
 strcat(filename,".mdump");
 outfile=fopen(filename, "wb");
 if (!outfile) {
   perror(filename);
   return -1;
 }

 wtmp=htonl(width);
 htmp=htonl(height);
 fwrite(&wtmp, sizeof(wtmp), 1, outfile);
 fwrite(&htmp, sizeof(htmp), 1, outfile);

 if (minprec <= 0 && mpf_cmp(vx, mind_f) > 0) {
   float x0_f, y0_f, x1_f, y1_f, xval_f, yval_f;
   printf("(using \"float\" arithmetic)\n");
   x0_f=mpf_get_d(x0);
   y0_f=mpf_get_d(y0);
   x1_f=mpf_get_d(x1);
   y1_f=mpf_get_d(y1);
   for (y=0;y<height;y++) {   
     fprintf(stderr,"  Line: %d\r",y);
     yval_f = y0_f + (y1_f - y0_f)*(float)(height-y)/(float)height;
     for (x=0;x<width;x++) {
       xval_f = x0_f + (x1_f - x0_f)*(float)x/(float)width;
       colornum=f_mandel(xval_f,yval_f);
       colornum=htonl(colornum);
       fwrite(&colornum, sizeof(colornum), 1, outfile);
     }
   }
 } else if (minprec <= 1 && mpf_cmp(vx, mind_d) > 0) {
   double x0_d, y0_d, x1_d, y1_d, xval_d, yval_d;
   printf("(using \"double\" arithmetic)\n");
   x0_d=mpf_get_d(x0);
   y0_d=mpf_get_d(y0);
   x1_d=mpf_get_d(x1);
   y1_d=mpf_get_d(y1);
   for (y=0;y<height;y++) {   
     fprintf(stderr,"  Line: %d\r",y);
     yval_d = y0_d + (y1_d - y0_d)*(double)(height-y)/(double)height;
     for (x=0;x<width;x++) {
       xval_d = x0_d + (x1_d - x0_d)*(double)x/(double)width;
       colornum=d_mandel(xval_d,yval_d);
       colornum=htonl(colornum);
       fwrite(&colornum, sizeof(colornum), 1, outfile);
     }
   }
 } else if (minprec <= 2 && mpf_cmp(vx, mind_ld) > 0) {
   long double x0_ld, y0_ld, x1_ld, y1_ld, xval_ld, yval_ld;
   printf("(using \"long double\" arithmetic)\n");
   x0_ld=mpf_get_d(x0);
   y0_ld=mpf_get_d(y0);
   x1_ld=mpf_get_d(x1);
   y1_ld=mpf_get_d(y1);
   for (y=0;y<height;y++) {   
     fprintf(stderr,"  Line: %d\r",y);
     yval_ld = y0_ld + (y1_ld - y0_ld)*(long double)(height-y)/(long double)height;
     for (x=0;x<width;x++) {
       xval_ld = x0_ld + (x1_ld - x0_ld)*(long double)x/(long double)width;
       colornum=ld_mandel(xval_ld,yval_ld);
       colornum=htonl(colornum);
       fwrite(&colornum, sizeof(colornum), 1, outfile);
     }
   }
 } else if (minprec <= 3 && mpf_cmp(vx, mind_gmp) > 0) {
   printf("(using \"gmp\" multi-precision arithmetic)\n");
   for (y=0;y<height;y++) {   
     fprintf(stderr,"  Line: %d\r",y);
     mpf_sub(yval, y1, y0);
     mpf_mul_ui(yval, yval, height-y);
     mpf_div_ui(yval, yval, height);
     mpf_add(yval, yval, y0);
     
     for (x=0;x<width;x++) {
       mpf_sub(xval, x1, x0);
       mpf_mul_ui(xval, xval, x);
       mpf_div_ui(xval, xval, width);
       mpf_add(xval, xval, x0);
       colornum=gmp_mandel(xval,yval);
       colornum=htonl(colornum);
       fwrite(&colornum, sizeof(colornum), 1, outfile);
     }
   }
 } else fprintf(stderr, "pixel width is too small!\n");
 fclose(outfile);
 return 0;
}
