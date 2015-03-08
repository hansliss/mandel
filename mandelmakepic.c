#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <netinet/in.h>

#include "targa.h"

typedef unsigned char byte;

void hsv2rgb(float H,float S,float V,byte *R,byte *G,byte *B)
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

#ifdef DEBUG2
 fprintf(logfile,"H=%3.4f, S=%3.4f, V=%3.4f, i=%d, f=%3.4f, p=%3.4f, q=%3.4f, t=%3.4f\n",H,S,V,i,f,p,q,t);
#endif
 switch(i)
 {
	case 0: (*R)=V*255; (*G)=t*255; (*B)=p*255; break;
	case 1: (*R)=q*255; (*G)=V*255; (*B)=p*255; break;
	case 2: (*R)=p*255; (*G)=V*255; (*B)=t*255; break;
	case 3: (*R)=p*255; (*G)=q*255; (*B)=V*255; break;
	case 4: (*R)=t*255; (*G)=p*255; (*B)=V*255; break;
	case 5: (*R)=V*255; (*G)=p*255; (*B)=q*255; break;
 }
}

#define max(x,y) (((x)>(y))?(x):(y))
#define min(x,y) (((x)<(y))?(x):(y))

void rgb2hsv(byte r,byte g,byte b,float *h,float *s,float *v)
{
  float rv,gv,bv,rc,gc,bc,max,min;
  rv=(float)r/255;
  gv=(float)g/255;
  bv=(float)b/255;
  max=max(max(rv,gv),bv);
  min=min(min(rv,gv),bv);
  *v=max;
  if (max!=0)
    *s=(max-min)/max;
  else
    *s=0;
  if ((*s)==0)
    *h=0;
  else
  {
    rc=(max-rv)/(max-min);
    gc=(max-gv)/(max-min);
    bc=(max-bv)/(max-min);
    if (rv==max)
      *h=bc-gc;
    else if (gv==max)
      *h=2+rc-bc;
    else if (bv==max)
      *h=4+gc-rc;
    (*h)/=6;
    if ((*h)<0)
      (*h)+=1;
  }
}

void cnum2rgb(unsigned long colornum,byte *r,byte *g,byte *b)
{
 float h,s,v;
 if (colornum==0) {
   *r = 0;
   *g = 0;
   *b = 0;
   return;
 }
 s=1-(float)((float)((colornum&0x3FC0)>>6)/(float)0x00FF);
 // h=(float)((float)((colornum&0x7FF)/(float)0x7FF));
 h=1-(float)((float)(((colornum-0x10B+0x1000)&0x1FFF)/(float)0x1FFF));
 v=1-(float)((float)((colornum&0xF800)>>11)/(float)0x001F);
 hsv2rgb(h,s,v,r,g,b);
#ifdef DEBUG
 fprintf(logfile,"Color %u (v=%3.4f, h=%3.4f)=> (%d %d %d)\n",colornum,v,h,*r,*g,*b);
#endif
}

long rgb2cnum(byte r,byte g,byte b)
{
  float h,s,v;
  rgb2hsv(r,g,b,&h,&s,&v);
  v=v*0x03FF*8;
  h=h*255;
  return ((long)v)|((long)h);
}

void usage(char *progname) {
  fprintf(stderr, "Usage: %s -i <infile.mdump> -o <outfile.tga> [-d <deffile.def>] [-m <bailout>] [-t <norm. threshold>]\n", progname);
  fprintf(stderr, "\t[-H <h offset>] [-h <h factor>] [-S <s offset>] [-s <s factor>] [-V <v offset>] [-v <v factor>]\n");
  fprintf(stderr, "\t[-I <inversion spec for h, s and v value: 3 characters, where \"1\" means corr. value is inverted>]\n");
  fprintf(stderr, "\t[-L <log spec for h, s and v value: 3 characters, where \"1\" means we use log() of the corr. value>]\n");
  fprintf(stderr, "\t[-P <prop spec for h, s and v value: 3 characters, where \"1\" means factors are proportional>]\n");
}

void handler(int s)
{
  int status;
  while (waitpid(-1, &status, WNOHANG)>0);
  signal(SIGCHLD, handler);
}

int main(int argc, char *argv[])
{
  FILE *infile;
  unsigned int width, height, *buffer, i, j, *hist, vmax, nmax, n;
  unsigned long wtmp, htmp;
  unsigned long loval, hival, maxiter, mival=1048576L, hival_threshold=2;
  int x, y;

  long double hc=0.66, hk, sc=0, sk, vc=0, vk;
  long double hks=3.8, sks=5, vks=0, hdiff=0, sdiff=0, vdiff=0;
  int hind, sind, vind;
  int hsteps=1, ssteps=1, vsteps=1;
  int hi=0, hl=0, si=1, sl=0, vi=1, vl=0, hp=0, sp=0, vp=0;
  

  char *infilename=NULL;
  char *outfilename=NULL;
  char *deffilename=NULL;
  char *scriptfilename=NULL;
  int o;

  char currentoutfilename[8192];

  byte r,g,b;
  TargaHandle th=NULL;
  static char tmpbuf1[1024], tmpbuf2[1024];
  long double def_x, def_y, def_dx;

  double h,s,v;

  signal(SIGCHLD, handler);

  while ((o=getopt(argc, argv, "i:o:d:m:t:H:h:V:v:S:s:I:L:P:C:")) != -1) {
    switch (o) {
    case 'i': infilename=optarg; break;
    case 'o': outfilename=optarg; break;
    case 'd': deffilename=optarg; break;
    case 'm': mival=atoi(optarg); break;
    case 't': hival_threshold=atoi(optarg); break;
    case 'C': scriptfilename=optarg; break;
    case 'H': sscanf(optarg, "%Lf", &hc); break;
    case 'h':
      if (sscanf(optarg, "%Lf,%i,%Lf", &hks, &hsteps, &hdiff) != 3) {
	hsteps=1; hdiff=0;
	sscanf(optarg, "%Lf", &hks);
      }
      break;
    case 'V': sscanf(optarg, "%Lf", &vc); break;
    case 'v': 
      if (sscanf(optarg, "%Lf,%i,%Lf", &vks, &vsteps, &vdiff) != 3) {
	vsteps=1; vdiff=0;
	sscanf(optarg, "%Lf", &vks);
      }
      break;
    case 'S': sscanf(optarg, "%Lf", &sc); break;
    case 's':
      if (sscanf(optarg, "%Lf,%i,%Lf", &sks, &ssteps, &sdiff) != 3) {
	ssteps=1; sdiff=0;
	sscanf(optarg, "%Lf", &sks);
      }
      break;
    case 'I': if (strlen(optarg)==3) {
      hi=((optarg[0]=='1')?1:0);
      si=((optarg[1]=='1')?1:0);
      vi=((optarg[2]=='1')?1:0);
    } else {
      usage(argv[0]); return -1;
    }
      break;
    case 'L': if (strlen(optarg)==3) {
      hl=((optarg[0]=='1')?1:0);
      sl=((optarg[1]=='1')?1:0);
      vl=((optarg[2]=='1')?1:0);
    } else {
      usage(argv[0]); return -1;
    }
      break;
    case 'P': if (strlen(optarg)==3) {
      hp=((optarg[0]=='1')?1:0);
      sp=((optarg[1]=='1')?1:0);
      vp=((optarg[2]=='1')?1:0);
    } else {
      usage(argv[0]); return -1;
    }
      break;
    default: usage(argv[0]); return -1; break;
    }
  }

  if (!infilename || !outfilename) {
    usage(argv[0]);
    return -1;
  }
    
  if (!(infile=fopen(infilename, "rb"))) { perror(infilename); return -2; }
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

  fclose(infile);

  for (i=0; i<n; i++) buffer[i]=ntohl(buffer[i]);

  vmax=0;
  maxiter=0;
  for (y=0; y<height; y++)
    for (x=0; x<width; x++){
      /*      printf("0x%04X\n", buffer[x + y*width]);*/
      if (buffer[x + y*width] != mival && buffer[x + y*width] > vmax) vmax=buffer[x + y*width];
    }
  if (!(hist=(unsigned int *)malloc((vmax+1) * sizeof(unsigned int)))) {
    perror("malloc()");
    return -3;
  }
  printf("max count value is %u\n", vmax); fflush(stdout);
  memset(hist, 0, (vmax+1) * sizeof(unsigned int));
  for (y=0; y<height; y++)
    for (x=0; x<width; x++) {
      if (buffer[x + y*width] != mival)
	hist[buffer[x + y*width]]++;
      else
	maxiter++;
    }
  for (j=vmax; j>0 && hist[j]<hival_threshold; j--);
  for (i=0; i<=j; i++)
    if (hist[i] > 0) break;

  loval=i;
  if (loval>0) loval--;
  hival=j;
  if (hival<vmax) hival++;

  nmax=0;

  for (i=0; i<vmax+1; i++)
    if (hist[i] > nmax) nmax = hist[i];

  printf("maxiter\t%lu\nlow val\t%lu\nhigh val\t%lu\n", maxiter, loval, hival); fflush(stdout);

  for (hind=0; hind < hsteps; hind++) {
    hk = hks + (long double)hind * hdiff;
    for (sind=0; sind < ssteps; sind++) {
      sk = sks + (long double)sind * sdiff;
      for (vind=0; vind < vsteps; vind++) {
	vk = vks + (long double)vind * vdiff;

	if (hsteps != 1 || ssteps != 1 || vsteps != 1)
	  sprintf(currentoutfilename, "%s_%Lg_%Lg_%Lg.tga", outfilename, hk, sk, vk);
	else
	  strncpy(currentoutfilename, outfilename, sizeof(currentoutfilename)-1);
	currentoutfilename[sizeof(currentoutfilename)-1]='\0';

	printf("H:[%Lg, %Lg%s%s%s], S:[%Lg, %Lg%s%s%s], V:[%Lg, %Lg%s%s%s]\n",
	       hc, hk, hi?", inv":"", hl?", log":"", hp?", prop":"",
	       sc, sk, si?", inv":"", sl?", log":"", sp?", prop":"",
	       vc, vk, vi?", inv":"", vl?", log":"", vp?", prop":"");

	sprintf(tmpbuf1, "%s %s", __FILE__, __DATE__);
	sprintf(tmpbuf2, "%s", infilename);

	if (!(th = TargaOpen(currentoutfilename, width, height, tmpbuf1, tmpbuf2, 1))) return -2;

	sprintf(tmpbuf1, "-H%Lg -h%Lg -S%Lg -s%Lg -V%Lg -v%Lg -I %c%c%c -L %c%c%c -P%c%c%c\n",
		hc, hk, sc, sk, vc, vk,
		hi?'1':'0', si?'1':'0', vi?'1':'0',
		hl?'1':'0', sl?'1':'0', vl?'1':'0',
		hp?'1':'0', sp?'1':'0', vp?'1':'0');
	TargaAddComment(th, tmpbuf1);

	if (deffilename) {
	  if (!(infile=fopen(deffilename, "r"))) perror(deffilename);
	  else {
	    if (fscanf(infile, "%60Lf\n", &def_x) != 1) {
	      perror("fscanf()");
	      return -2;
	    }
	    if (fscanf(infile, "%60Lf\n", &def_y) != 1) {
	      perror("fscanf()");
	      return -2;
	    }
	    if (fscanf(infile, "%60Lf\n", &def_dx) != 1) {
	      perror("fscanf()");
	      return -2;
	    }
	    fclose(infile);
	    sprintf(tmpbuf1, "x=%.50Lg", def_x);
	    TargaAddComment(th, tmpbuf1);
	    sprintf(tmpbuf1, "y=%.50Lg", def_y);
	    TargaAddComment(th, tmpbuf1);
	    sprintf(tmpbuf1, "dx=%.50Lg", def_dx);
	    TargaAddComment(th, tmpbuf1);
	  }
	}

	if (hp) hk /= ((double)hival-loval);
	if (sp) sk /= ((double)hival-loval);
	if (vp) vk /= ((double)hival-loval);

	for (y=0; y<height; y++) {
	  fprintf(stderr,"  Line: %d\r",y);
	  for (x=0; x<width; x++) {
	    unsigned int colornum=buffer[x+y*width];
	    if (colornum == mival) {
	      r = 0;
	      g = 0;
	      b = 0;
	    } else {
#ifndef BW
	      double intgr;
	      double span=(hival-loval);
	      h=s=v=(colornum-loval) % (int)span;
	      if (hl) h=logl(h)/logl(span); else h /= span;
	      if (sl) s=logl(s)/logl(span); else s /= span;
	      if (vl) v=logl(v)/logl(span); else v /= span;
	      h=modf(hc + hk * h, &intgr);
	      if (hi) h=1-h;
	      s=modf(sc + sk * s, &intgr);
	      if (si) s=1-s;
	      v=modf(vc + vk * v, &intgr);
	      if (vi) v=1-v;
#else
	      h=v=1;s=0;
#endif
	      hsv2rgb(h,s,v,&r,&g,&b);
	    }
	    TargaWrite(th, r, g, b);
	  }
	}
	TargaFinish(th);
	TargaClose(th);
	if (scriptfilename) {
	  if (!fork()) {
	    execl(scriptfilename, scriptfilename, currentoutfilename, NULL);
	    return 0;
	  }
	}
      }
    }
  }
  free(hist);
  free(buffer);
  return 0;
}
