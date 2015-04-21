#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <libgen.h>
#include <gmp.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <netinet/in.h>
#include <sys/mman.h>

#include "targa.h"

#define BAILOUT 4.0


/*
  .*.  .*.  **.  **.  *.*  ***  ..*  ***  .*.  .*.
  *.*  **.  ..*  ..*  *.*  *..  .*.  ..*  *.*  *.*
  *.*  .*.  .*.  **.  ***  **.  **.  .*.  .*.  .**
  *.*  .*.  *..  ..*  ..*  ..*  *.*  .*.  *.*  ..*
  .*.  ***  ***  **.  ..*  **.  .*.  .*.  .*.  .*.
*/

#define FONTWIDTH 3
#define FONTHEIGHT 5
#define MAXDIGITS 2

#define BOXWIDTH ((MAXDIGITS) * (FONTWIDTH + 1) + 2 + 1)
#define BOXHEIGHT ((FONTHEIGHT) + 2 + 1)

static unsigned char font[10*FONTHEIGHT]={0x02,0x05,0x05,0x05,0x02,0x02,0x06,0x02,0x02,0x07,0x06,0x01,0x02,0x04,0x07,0x06,0x01,0x06,0x01,0x06,0x05,0x05,0x07,0x01,0x01,0x07,0x04,0x06,0x01,0x06,0x01,0x02,0x06,0x05,0x02,0x07,0x01,0x02,0x02,0x02,0x02,0x05,0x02,0x05,0x02,0x02,0x05,0x03,0x01,0x02};


void writedigit(int digit, unsigned char r, unsigned char g, unsigned char b, unsigned char rb, unsigned char gb, unsigned char bb, int xpos, int ypos, TargaHandle file) {
  int x, y;
  for (x=0; x<=FONTWIDTH; x++) {
    for (y=0; y<=FONTHEIGHT; y++) {
      int offset = x + xpos + (y + ypos) * file->width;
      TargaSeek(file, offset);
      if (x < FONTWIDTH && y < FONTHEIGHT && font[digit * FONTHEIGHT + y]&(1<<(FONTWIDTH-x-1))) {
	TargaWrite(file, r, g, b);
      } else {
	TargaWrite(file, rb, gb, bb);
      }
    }
  }
}

void writenumber(int number, unsigned char r, unsigned char g, unsigned char b, unsigned char rb, unsigned char gb, unsigned char bb, int x, int y, TargaHandle file) {
  int xpos = x;
  xpos += (MAXDIGITS - 1) * (FONTWIDTH + 1);
  if (number == 0) {
    writedigit(0, r, g, b, rb, gb, bb, xpos, y, file);
  } else {
    int tmp = number;
    int digits = 0;
    while (tmp > 0) {
      tmp /= 10;
      digits ++;
    }
    while (number > 0) {
      int digit = number % 10;
      writedigit(digit, r, g, b, rb, gb, bb, xpos, y, file);
      xpos -= (FONTWIDTH +1);
      number /= 10;
    }
  }
}

void hsv2rgb(float H,float S,float V,unsigned char *R,unsigned char *G,unsigned char *B)
{
  int i;
  float f,p,q,t;
  H = H*6;
  if (H>=6)
    H-=6;
  i=H;
  f=H-(float)i;
  p=V*(1.0-S);
  q=V*(1.0-(S*f));
  t=V*(1.0-(S*(1.0-f)));

  switch(i) {
  case 0: (*R)=V*255; (*G)=t*255; (*B)=p*255; break;
  case 1: (*R)=q*255; (*G)=V*255; (*B)=p*255; break;
  case 2: (*R)=p*255; (*G)=V*255; (*B)=t*255; break;
  case 3: (*R)=p*255; (*G)=q*255; (*B)=V*255; break;
  case 4: (*R)=t*255; (*G)=p*255; (*B)=V*255; break;
  case 5: (*R)=V*255; (*G)=p*255; (*B)=q*255; break;
  }
}

void dobox(int number, int maxiter, int xpos, int ypos, TargaHandle file) {
  int x, y;
  unsigned char r, g, b, rf=0, gf=0, bf=0;
  if (number == maxiter) {
    r=0;
    g=0;
    b=0;
    rf=0xff;
    gf=0xff;
    bf=0xff;
  } else if (number == 0) {
    r=0xff;
    g=0xff;
    b=0xff;
    rf=0;
    gf=0;
    bf=0;
  } else {
    float h, s = 0.3, v = 1;
    h=((number==0)?0:log((float)number))/log((float)maxiter);
    hsv2rgb(h, s, v, &r, &g, &b);
    //fprintf(stderr, "(%f,%f,%f) translates to (%d,%d,%d)\n", h, s, v, r, g, b);
    rf=0;
    gf=0;
    bf=0;
  }
  int offset;
  for (x = 0; x < BOXWIDTH; x++) {
    y=0;
    offset=(x+xpos + (y+ypos)*file->width);
    TargaSeek(file, offset);
    TargaWrite(file, 0xE0, 0xA0, 0xA0);
    y=BOXHEIGHT;
    offset=(x+xpos + (y+ypos)*file->width);
    TargaSeek(file, offset);
    TargaWrite(file, 0xE0, 0xA0, 0xA0);
    for (y=1; y < BOXHEIGHT; y++) {
      offset=(x+xpos + (y+ypos)*file->width);
      TargaSeek(file, offset);
      TargaWrite(file, r, g, b);
    }
  }
  for (y = 0; y < BOXHEIGHT; y++) {
    x=0;
    offset=(x+xpos + (y+ypos)*file->width);
    TargaSeek(file, offset);
    TargaWrite(file, 0xE0, 0xA0, 0xA0);
    x=BOXWIDTH;
    offset=(x+xpos + (y+ypos)*file->width);
    TargaSeek(file, offset);
    TargaWrite(file, 0xE0, 0xA0, 0xA0);
  }
  writenumber(number, rf, gf, bf, r, g, b, xpos + 2, ypos + 2, file);
}


unsigned long ld_mandel(long double cx, long double cy, int maxiter)
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

void usage(char *progname) {
  fprintf(stderr, "Usage: %s -w <width> -h <height> -m <bailout limit -d <def file> -o <targa file>\n", progname);
}

int main(int argc, char *argv[]) {
  int o, x, y, val;
  FILE *deffile=NULL;
  char *deffilename=NULL;
  char *targafilename=NULL;

  int width = 0, height = 0, maxiter = 0;

  mpf_t x0,x1,y0,y1,centx,centy,dx,dy,xval,yval,vx;

  TargaHandle th=NULL;
  static char tmpbuf1[1024], tmpbuf2[1024];
  
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

  while ((o=getopt(argc, argv, "w:h:d:o:m:")) != -1) {
    switch (o) {
    case 'd':
      deffilename=optarg;
      break;
    case 'o':
      targafilename=optarg;
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
    default:
      usage(argv[0]);
      return -1;
    }
  }

  if (width * height * maxiter == 0 || !deffilename || !targafilename) {
    usage(argv[0]);
    return -1;
  }

  if (!(deffile=fopen(deffilename,"r"))) {
    fprintf(stderr,"Failed to open definition file %s\n",deffilename);
    return -1;
  }
  
  gmp_fscanf(deffile,"%Fg\n",centx);
  gmp_fscanf(deffile,"%Fg\n",centy);
  gmp_fscanf(deffile,"%Fg\n",dx);
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
  gmp_printf("\npixel=%Fg\n", vx);

  sprintf(tmpbuf1, "%s %s", __FILE__, __DATE__);
  sprintf(tmpbuf2, "%s", deffilename);
  if (!(th = TargaOpen(targafilename, width, height, tmpbuf1, tmpbuf2, 1))) return -2;
  TargaAddComment(th, "");
  TargaAddComment(th, "");
  TargaAddComment(th, "");
  TargaAddComment(th, "");

  long double x0_ld, y0_ld, x1_ld, y1_ld, xval_ld, yval_ld;
  printf("(using \"long double\" arithmetic)\n");
  x0_ld=mpf_get_d(x0);
  y0_ld=mpf_get_d(y0);
  x1_ld=mpf_get_d(x1);
  y1_ld=mpf_get_d(y1);
  for (y = 0; y < (height - BOXHEIGHT); y += BOXHEIGHT) {
    fprintf(stderr,"  Line: %d\r",y);
    yval_ld = y0_ld + (y1_ld - y0_ld)*(long double)(height-y)/(long double)height;
    for (x = 0; x < (width - BOXWIDTH); x += BOXWIDTH) {
      xval_ld = x0_ld + (x1_ld - x0_ld)*(long double)x/(long double)width;
      val=ld_mandel(xval_ld, yval_ld, maxiter);
      dobox(val, maxiter, x, y, th);
    }
  }
  TargaFinish(th);
  TargaClose(th);
  return 0;
}
