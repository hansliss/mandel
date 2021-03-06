#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <libgen.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <netinet/in.h>
#include <sys/mman.h>

#include "targa.h"

typedef unsigned char byte;

typedef struct fileheader_s {
  int width;
  int height;
  int maxiter;
  int reserved;
} fileheader;

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

void kelvinToColor(double t, double *R, double *G, double *B) {
  if (t <= 6500) {
    if (t < 1000) {
      *R=0;
      *G=0;
      *B=0;
    } else {
      *R=1;
      *G=-8.09645459737e-13 * t * t * t - 4.47106840522e-9 * t * t + 2.37777009068e-4 * t - 0.194709442111;
      if (t < 2000) {
	*B=0;
      } else {
	*B=-5.87630502281e-12 * t * t * t + 8.76431011057e-8 * t * t -1.792244492e-4 * t + 5.75444669402e-2;
      }
    }
  } else {
    *R=(float)4383/t + 0.19;
    *G=(float)3078.6/t + 0.4113;
    *B=1;
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
      else /* if (bv==max) */
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

void mkdirp(char *path) {
  char *p=strdup(path);
  struct stat statbuf;
  if (strlen(p) > 0) {
    if (stat(p, &statbuf) != 0) {
      char *ptmp=strdup(p);
      mkdirp(dirname(ptmp));
      free(ptmp);
      fprintf(stderr, "Creating directory %s\n", p);
      mkdir(p, 07777);
    }
  }
  free(p);
}

void usage(char *progname) {
  fprintf(stderr, "Usage: %s -i <infile.mdump> -o <outfile.tga> [-d <deffile.def>] [-m <bailout>] [-t <norm. threshold>]\n", progname);
  fprintf(stderr, "\t[-H <h offset>] [-h <h factor>] [-S <s offset>] [-s <s factor>] [-V <v offset>] [-v <v factor>]\n");
  fprintf(stderr, "\t[-I <inversion spec for h, s and v value: 3 characters, where \"1\" means corr. value is inverted>]\n");
  fprintf(stderr, "\t[-L <log spec for h, s and v value: 3 characters, where \"1\" means we use log() of the corr. value>]\n");
  fprintf(stderr, "\t[-P <prop spec for h, s and v value: 3 characters, where \"1\" means factors are proportional>]\n");
  fprintf(stderr, "\t[-R (remove more or less all-black or all-white images)] [-D (build directory structure)]\n");
  fprintf(stderr, "\t[-z <loval>] [-Z <hival>] [-O (highlight not just maxiter but all maxiter/(2^x))\n");
  fprintf(stderr, "\t[-K <color temp offset>] [-k <color temp factor>]\n");
}

int checkForMaxiter(unsigned long val, unsigned long maxiter, int checkfractions) {
  if (val == maxiter) {
    return 1;
  }
  else if (!checkfractions) {
    return 0;
  } else {
    while (maxiter > 1) {
      if (val == maxiter) {
	return 1;
      }
      maxiter /= 2;
    }
  }
  return 0;
}


void handler(int s)
{
  int status;
  while (waitpid(-1, &status, WNOHANG)>0);
  signal(SIGCHLD, handler);
}

int main(int argc, char *argv[])
{
  FILE *dumpfile, *deffile;
  fileheader header;
  int skip;
  int build_subdirs=0;
  unsigned int width, height, i, j;
  static int *dumpbuffer=NULL;
  unsigned int *hist, vmax, looping=0;
  unsigned long loval, hival, maxiter, mival=1048576L, hival_threshold=2;
  unsigned long pixels=0, blackpixels=0, whitepixels=0;
  int x, y;
  int remove_blacks=0;

  long double hc=0.66, hk, sc=0, sk, vc=0, vk, hcs=0.66, scs=0, vcs=0, hcdiff=0, scdiff=0, vcdiff=0, rhk, rsk, rvk;
  long double hks=3.8, sks=5, vks=0, hdiff=0, sdiff=0, vdiff=0;
  long double kc, kk;
  int hind, sind, vind, hcind, scind, vcind;
  int hsteps=1, ssteps=1, vsteps=1, hcsteps=1, scsteps=1, vcsteps=1;
  int hi=0, hl=0, si=1, sl=0, vi=1, vl=0, hp=0, sp=0, vp=0;
  int hi_s=0, hl_s=0, si_s=1, sl_s=0, vi_s=1, vl_s=0, hp_s=0, sp_s=0, vp_s=0;
  int mih=0, mis=0, miv=0, mlh=0, mls=0, mlv=0, mph=0, mps=0, mpv=0;

  int use_loval=-1;
  int use_hival=-1;

  int checkfractions=0;

  struct stat statbuf;

  char *dumpfilename=NULL;
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
  int hasColorTemp=0;

  signal(SIGCHLD, handler);

  while ((o=getopt(argc, argv, "i:o:d:m:t:H:h:K:k:V:v:S:s:I:L:P:C:RDz:Z:O")) != -1) {
    switch (o) {
    case 'i': dumpfilename=optarg; break;
    case 'o': outfilename=optarg; break;
    case 'd': deffilename=optarg; break;
    case 'm': mival=atoi(optarg); break;
    case 't': hival_threshold=atoi(optarg); break;
    case 'C': scriptfilename=optarg; break;
    case 'R': remove_blacks=1; break;
    case 'D': build_subdirs=1; break;
    case 'z': use_loval=atoi(optarg); break;
    case 'Z': use_hival=atoi(optarg); break;
    case 'O': checkfractions = 1; break;
    case 'K':
      sscanf(optarg, "%Lf", &kc);
      hasColorTemp++;
      break;
    case 'k':
      sscanf(optarg, "%Lf", &kk);
      hasColorTemp++;
      break;
    case 'H':
      if (sscanf(optarg, "%Lf,%i,%Lf", &hcs, &hcsteps, &hcdiff) != 3) {
	hcsteps=1; hcdiff=0;
	sscanf(optarg, "%Lf", &hcs);
      } else looping=1;
      break;
    case 'h':
      if (sscanf(optarg, "%Lf,%i,%Lf", &hks, &hsteps, &hdiff) != 3) {
	hsteps=1; hdiff=0;
	sscanf(optarg, "%Lf", &hks);
      } else looping=1;
      break;
    case 'V':
      if (sscanf(optarg, "%Lf,%i,%Lf", &vcs, &vcsteps, &vcdiff) != 3) {
	vcsteps=1; vcdiff=0;
	sscanf(optarg, "%Lf", &vcs);
      } else looping=1;
      break;
    case 'v': 
      if (sscanf(optarg, "%Lf,%i,%Lf", &vks, &vsteps, &vdiff) != 3) {
	vsteps=1; vdiff=0;
	sscanf(optarg, "%Lf", &vks);
      } else looping=1;
      break;
    case 'S':
      if (sscanf(optarg, "%Lf,%i,%Lf", &scs, &scsteps, &scdiff) != 3) {
	scsteps=1; scdiff=0;
	sscanf(optarg, "%Lf", &scs);
      } else looping=1;
      break;
    case 's':
      if (sscanf(optarg, "%Lf,%i,%Lf", &sks, &ssteps, &sdiff) != 3) {
	ssteps=1; sdiff=0;
	sscanf(optarg, "%Lf", &sks);
      } else looping=1;
      break;
    case 'I': if (strlen(optarg)==3) {
	hi=((optarg[0]=='1')?1:0);
	si=((optarg[1]=='1')?1:0);
	vi=((optarg[2]=='1')?1:0);
	if (optarg[0] == '*') mih=1;
	if (optarg[1] == '*') mis=1;
	if (optarg[2] == '*') miv=1;
      } else {
	usage(argv[0]); return -1;
      }
      break;
    case 'L': if (strlen(optarg)==3) {
	hl=((optarg[0]=='1')?1:0);
	sl=((optarg[1]=='1')?1:0);
	vl=((optarg[2]=='1')?1:0);
	if (optarg[0] == '*') mlh=1;
	if (optarg[1] == '*') mls=1;
	if (optarg[2] == '*') mlv=1;
      } else {
	usage(argv[0]); return -1;
      }
      break;
    case 'P': if (strlen(optarg)==3) {
	hp=((optarg[0]=='1')?1:0);
	sp=((optarg[1]=='1')?1:0);
	vp=((optarg[2]=='1')?1:0);
	if (optarg[0] == '*') mph=1;
	if (optarg[1] == '*') mps=1;
	if (optarg[2] == '*') mpv=1;
      } else {
	usage(argv[0]); return -1;
      }
      break;
    default: usage(argv[0]); return -1; break;
    }
  }

  if (hasColorTemp == 2) {
    hasColorTemp=1;
  } else {
    hasColorTemp=0;
  }
  
  if (!dumpfilename || !outfilename) {
    usage(argv[0]);
    return -1;
  }

  if (checkfractions) {
    fprintf(stderr, "Will accentuate both values at maxiter and at maxiter divided by powers of two.\n");
  }
    
  if (!(dumpfile=fopen(dumpfilename, "rb"))) { perror(dumpfilename); return -2; }
  if (fread(&header, sizeof(header), 1, dumpfile) != 1) {
    perror("fread()");
    return -2;
  }
  width=ntohl(header.width);
  height=ntohl(header.height);
  if (ntohl(header.maxiter) != mival) {
    fprintf(stderr, "Warning: Changing value of maxiter to %d as read from file!\n", ntohl(header.maxiter));
  }
  mival=ntohl(header.maxiter);
  printf("Mapping file, [%d x %d] pixels\n", width, height);
  if ((dumpbuffer=(int *)mmap(NULL,
			      sizeof(header) + 
			      sizeof(int) * (width * height),
			       PROT_READ,
			       MAP_SHARED,
			       fileno(dumpfile),
			       0)) == MAP_FAILED) {
    perror("mmap()");
    return 4;
  }
  printf("Done.\n"); fflush(stdout);
  vmax=0;
  maxiter=0;
  for (y=0; y<height; y++) {
    for (x=0; x<width; x++){
      int val = ntohl(dumpbuffer[4 + x + y * width]);
      if (val != -1 && !checkForMaxiter(val, mival, checkfractions) && val > vmax) vmax=val;
    }
  }
  printf("Allocating histogram buffer.\n"); fflush(stdout);
  if (!(hist=(unsigned int *)malloc((vmax+1) * sizeof(unsigned int)))) {
    perror("malloc()");
    return -3;
  }
  printf("max count value is %u\n", vmax); fflush(stdout);
  memset(hist, 0, (vmax+1) * sizeof(unsigned int));
  for (y=0; y<height; y++)
    for (x=0; x<width; x++) {
      int val=ntohl(dumpbuffer[4 + x + y * width]);
      if (val != -1) {
	if (!checkForMaxiter(val, mival, checkfractions))
	  hist[val]++;
	else
	  maxiter++;
      }
    }
  for (j=vmax; j>0 && hist[j]<hival_threshold; j--);
  for (i=0; i<=j; i++)
    if (hist[i] > 0) break;

  free(hist);

  loval=i;
  if (loval>0) loval--;
  hival=j;
  if (hival<vmax) hival++;

  if (use_loval != -1) {
    printf("Changing loval from %lu to %d.\n", loval, use_loval);
    loval=use_loval;
  }

  if (use_hival != -1) {
    printf("Changing hival from %lu to %d.\n", hival, use_hival);
    hival=use_hival;
  }

  printf("maxiter\t%lu\nlow val\t%lu\nhigh val\t%lu\n", maxiter, loval, hival); fflush(stdout);

  if (mih || mis || miv || mlh || mls || mlv || mph || mps || mpv) {
    looping=1;
  }

  hi_s=hi;
  si_s=si;
  vi_s=vi;
  hl_s=hl;
  sl_s=sl;
  vl_s=vl;
  hp_s=hp;
  sp_s=sp;
  vp_s=vp;
  if (hi==1) mih=1;
  if (si==1) mis=1;
  if (vi==1) miv=1;
  if (hl==1) mlh=1;
  if (sl==1) mls=1;
  if (vl==1) mlv=1;
  if (hp==1) mph=1;
  if (sp==1) mps=1;
  if (vp==1) mpv=1;

  printf("H%Lg,%d,%Lg S%Lg,%d,%Lg V%Lg,%d,%Lg I%d%d%d i%d%d%d L%d%d%d l%d%d%d P%d%d%d p%d%d%d\n",
	 hks,hsteps,hdiff,sks,ssteps,sdiff,vks,vsteps,vdiff,hi,si,vi,mih,mis,miv,
	 hl,sl,vl,mlh,mls,mlv,hp,sp,vp,mph,mps,mpv);

  for (hi=hi_s; hi <= mih; hi++) {
    for (si=si_s; si <= mis; si++) {
      for (vi=vi_s; vi <= miv; vi++) {
	for (hl=hl_s; hl <= mlh; hl++) {
	  for (sl=sl_s; sl <= mls; sl++) {
	    for (vl=vl_s; vl <= mlv; vl++) {
	      for (hp=hp_s; hp <= mph; hp++) {
		for (sp=sp_s; sp <= mps; sp++) {
		  for (vp=vp_s; vp <= mpv; vp++) {
		    for (hind=0; hind < hsteps; hind++) {
		      hk = hks + (long double)hind * hdiff;
		      rhk = hk;
		      if (hp) hk /= ((double)hival-loval);
		      for (sind=0; sind < ssteps; sind++) {
			sk = sks + (long double)sind * sdiff;
			rsk = sk;
			if (sp) sk /= ((double)hival-loval);
			for (vind=0; vind < vsteps; vind++) {
			  vk = vks + (long double)vind * vdiff;
			  rvk = vk;
			  if (vp) vk /= ((double)hival-loval);
			  for (hcind=0; hcind < hcsteps; hcind++) {
			    hc = hcs + (long double)hcind * hcdiff;
			    for (scind=0; scind < scsteps; scind++) {
			      sc = scs + (long double)scind * scdiff;
			      for (vcind=0; vcind < vcsteps; vcind++) {
				vc = vcs + (long double)vcind * vcdiff;


				skip=0;

				if (looping) {
				  if (build_subdirs) {
				    sprintf(currentoutfilename, "%Lg_%Lg/%Lg_%Lg/%Lg_%Lg/I%d%d%d/L%d%d%d/P%d%d%d/%s.tga", hc, rhk, sc, rsk, vc, rvk, hi, si, vi, hl, sl, vl, hp, sp, vp, outfilename);
				    char *tmpfilename = strdup(currentoutfilename);
				    char *dirn=dirname(tmpfilename);
				    if (stat(dirn, &statbuf) != 0) {
				      mkdirp(dirn);
				    }
				    free(tmpfilename);
				  } else {
				    sprintf(currentoutfilename, "%s_%Lg_%Lg_%Lg_%Lg_%Lg_%Lg_I%d%d%d_L%d%d%d_P%d%d%d.tga", outfilename, hc, rhk, sc, rsk, vc, rvk, hi, si, vi, hl, sl, vl, hp, sp, vp);
				  }
				  printf("%s\n", currentoutfilename); fflush(stdout);
				  if (stat(currentoutfilename, &statbuf) == 0) {
				    skip=1;
				  }
				} else {
				  strncpy(currentoutfilename, outfilename, sizeof(currentoutfilename)-1);
				  currentoutfilename[sizeof(currentoutfilename)-1]='\0';
				}

				printf("H:[%Lg, %Lg%s%s%s], S:[%Lg, %Lg%s%s%s], V:[%Lg, %Lg%s%s%s]\n",
				       hc, rhk, hi?", inv":"", hl?", log":"", hp?", prop":"",
				       sc, rsk, si?", inv":"", sl?", log":"", sp?", prop":"",
				       vc, rvk, vi?", inv":"", vl?", log":"", vp?", prop":"");

				if (!skip) {
				  sprintf(tmpbuf1, "%s %s", __FILE__, __DATE__);
				  sprintf(tmpbuf2, "%s", dumpfilename);
				  if (!(th = TargaOpen(currentoutfilename, width, height, tmpbuf1, tmpbuf2, 1))) return -2;

				  tmpbuf2[0]='\0';
				  if (use_loval != -1) {
				    sprintf(tmpbuf2, "-z %d", use_loval);
				  }

				  if (use_hival != -1) {
				    sprintf(tmpbuf1, "-Z %d", use_hival);
				    if (strlen(tmpbuf2)) strcat(tmpbuf2, " ");
				    strcat(tmpbuf2, tmpbuf1);
				  }

				  if (hasColorTemp) {
				    sprintf(tmpbuf1, "-K%Lg -k%Lg", kc, kk);
				    if (strlen(tmpbuf2)) strcat(tmpbuf2, " ");
				    strcat(tmpbuf2, tmpbuf1);
				  }

				  sprintf(tmpbuf1, "-H%Lg -h%Lg -S%Lg -s%Lg -V%Lg -v%Lg -I %c%c%c -L %c%c%c -P%c%c%c%s\n",
					  hc, rhk, sc, rsk, vc, rvk,
					  hi?'1':'0', si?'1':'0', vi?'1':'0',
					  hl?'1':'0', sl?'1':'0', vl?'1':'0',
					  hp?'1':'0', sp?'1':'0', vp?'1':'0',
					  tmpbuf2);
				  TargaAddComment(th, tmpbuf1);

				  if (deffilename) {
				    if (!(deffile=fopen(deffilename, "r"))) perror(deffilename);
				    else {
				      if (fscanf(deffile, "%60Lf\n", &def_x) != 1) {
					perror("fscanf()");
					return -2;
				      }
				      if (fscanf(deffile, "%60Lf\n", &def_y) != 1) {
					perror("fscanf()");
					return -2;
				      }
				      if (fscanf(deffile, "%60Lf\n", &def_dx) != 1) {
					perror("fscanf()");
					return -2;
				      }
				      fclose(deffile);
				      sprintf(tmpbuf1, "x=%.50Lg", def_x);
				      TargaAddComment(th, tmpbuf1);
				      sprintf(tmpbuf1, "y=%.50Lg", def_y);
				      TargaAddComment(th, tmpbuf1);
				      sprintf(tmpbuf1, "dx=%.50Lg", def_dx);
				      TargaAddComment(th, tmpbuf1);
				    }
				  }


				  pixels=blackpixels=whitepixels=0;

				  for (y=0; y<height; y++) {
				    fprintf(stderr,"  Line: %d\r",y);
				    for (x=0; x<width; x++) {
				      unsigned int colornum=ntohl(dumpbuffer[4 + x + y * width]);
				      if (colornum == -1) {
					r=255;
					g=0;
					b=0;
				      } else if (checkForMaxiter(colornum, mival, checkfractions)) {
					r = 0;
					g = 0;
					b = 0;
				      } else {
#ifndef BW
					double intgr;
					double span=(hival-loval);
					h=s=v=(colornum-loval) % (int)span;
					double mr=1, mg=1, mb=1;
					if (hasColorTemp) {
					  int temp = (int)(kc + kk * h);
					  while (temp < 0) {
					    temp += 10000;
					  }
					  temp %= 10000;
					  kelvinToColor(temp, &mg, &mg, &mb);
					}
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
					r *= mr;
					g *= mg;
					b *= mb;
					if (v < 0.05) blackpixels++;
					if (v > 0.95 && s < 0.05) whitepixels++;
					pixels++;
				      }
				      TargaWrite(th, r, g, b);
				    }
				  }
				  TargaFinish(th);
				  TargaClose(th);
				  if ((double)blackpixels/(double)pixels > 0.97 ||
				      (double)whitepixels/(double)pixels > 0.97) {
				    fprintf(stderr, "Mostly black or mostly white!");
				    if (remove_blacks) {
				      if (!truncate(currentoutfilename, 0)) fprintf(stderr," Truncated!\n");
				      else fprintf(stderr, "Failed to truncate!\n");
				    } else fprintf(stderr, "Leaving.\n");
				  }
				  if (scriptfilename) {
				    if (!fork()) {
				      execl(scriptfilename, scriptfilename, currentoutfilename, NULL);
				      return 0;
				    }
				  }
				} else {
				  printf("Skipping...\n");
				}
			      }
			    }
			  }
			}
		      }
		    }
		  }
		}
	      }
	    }
	  }
	}
      }
    }
  }
  if (munmap(dumpbuffer, sizeof(header) + sizeof(int) * (header.width * header.height)) == -1) {
    perror("munmap()");
  }
  
  fclose(dumpfile);
  return 0;
}
