#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <netinet/in.h>

#include "targa.h"

void usage(char *progname) {
  fprintf(stderr, "Usage: %s -i <infile.tga> -o <outfile.xml>\n", progname);
}

char **tokenize(char *string, int *count) {
  int c=0;
  int l=strlen(string);
  char *p1=string, *p2;
  // First allocate an array with enough string pointers to handle the
  // worst case - every character in the string is a token. :)
  char **array = (char **)malloc(l * sizeof(char *));
  while (*p1) {
    p2=p1;
    while (*p2 && !isspace(*p2)) p2++;
    array[c] = (char *)malloc(p2 - p1 + 1);
    memcpy(array[c], p1, p2 - p1);
    array[c][p2-p1] = '\0';
    c++;
    while (*p2 && isspace(*p2)) p2++;
    p1=p2;
  }
  (*count)=c;
  return array;
}

void xmlwritecomponent(FILE *xmlfile, char *name, long double c, long double k, int i, int l, int p) {
  fprintf(xmlfile, "    <mc:%s>\n", name);
  fprintf(xmlfile, "      <mc:startval>%Lg</mc:startval>\n", c);
  fprintf(xmlfile, "      <mc:delta>%Lg</mc:delta>\n", k);
  fprintf(xmlfile, "      <mc:flag mc:name=\"inverted\">%s</mc:flag>\n", i?"true":"false");
  fprintf(xmlfile, "      <mc:flag mc:name=\"logarithmic\">%s</mc:flag>\n", l?"true":"false");
  fprintf(xmlfile, "      <mc:flag mc:name=\"proportional\">%s</mc:flag>\n", p?"true":"false");
  fprintf(xmlfile, "    </mc:%s>\n", name);
}

void xmlwrite(FILE *xmlfile, long double hc, long double sc, long double vc,
	     long double hk, long double sk, long double vk,
	     int hi, int si, int vi, int hl, int sl, int vl, int hp, int sp, int vp) {
  fprintf(xmlfile, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
  fprintf(xmlfile, "<mc:mandelcolour xmlns:mc=\"http://mandel.liss.nu/mandelcolour\">\n");
  fprintf(xmlfile, "  <mc:colouring>\n");
  xmlwritecomponent(xmlfile, "hue", hc, hk, hi, hl, hp);
  xmlwritecomponent(xmlfile, "saturation", sc, sk, si, sl, sp);
  xmlwritecomponent(xmlfile, "value", vc, vk, vi, vl, vp);
  fprintf(xmlfile, "  </mc:colouring>\n");
  fprintf(xmlfile, "</mc:mandelcolour>\n");
}

int main(int argc, char *argv[])
{
  TargaHandle th;
  int o, i;
  char **c_argv;
  int c_argc;

  long double hc=0.66, sc=0, vc=0;
  long double hk=3.8, sk=5, vk=0, hdiff=0, sdiff=0, vdiff=0;
  int hsteps=1, ssteps=1, vsteps=1;
  int hi=0, hl=0, si=1, sl=0, vi=1, vl=0, hp=0, sp=0, vp=0;
  
  char *infilename=NULL, *outfilename=NULL;
  FILE *outfile;
  while ((o=getopt(argc, argv, "i:o:")) != -1) {
    switch (o) {
    case 'i': infilename=optarg; break;
    case 'o': outfilename=optarg; break;
    default: usage(argv[0]); return -1; break;
    }
  }
  if (!infilename || !outfilename) {
    usage(argv[0]);
    return -1;
  }

  if ((th = TargaOpen(infilename, 0, 0, NULL, NULL, 0)) == NULL) {
    fprintf(stderr, "Error opening file\n");
    return -2;
  }

  printf("Size=(%d,%d), Author=\"%s\", Jobname=\"%s\".\n", th->width, th->height, th->author, th->jobname);
  if (th->comment1) printf("Comment1=\"%s\"\n", th->comment1);
  if (th->comment2) printf("Comment2=\"%s\"\n", th->comment2);
  if (th->comment3) printf("Comment3=\"%s\"\n", th->comment3);
  if (th->comment4) printf("Comment4=\"%s\"\n", th->comment4);
  c_argv = tokenize(th->comment1, &c_argc);
  printf("%d tokens\n", c_argc);
  for (i=0; i<c_argc; i++) {
    printf("Token %d: \"%s\"\n", i, c_argv[i]);
  }
  while ((o=getopt(c_argc, c_argv, "H:h:V:v:S:s:I:L:P:")) != -1) {
    switch (o) {
    case 'H': sscanf(optarg, "%Lf", &hc); break;
    case 'h':
      if (sscanf(optarg, "%Lf,%i,%Lf", &hk, &hsteps, &hdiff) != 3) {
	hsteps=1; hdiff=0;
	sscanf(optarg, "%Lf", &hk);
      }
      break;
    case 'V': sscanf(optarg, "%Lf", &vc); break;
    case 'v': 
      if (sscanf(optarg, "%Lf,%i,%Lf", &vk, &vsteps, &vdiff) != 3) {
	vsteps=1; vdiff=0;
	sscanf(optarg, "%Lf", &vk);
      }
      break;
    case 'S': sscanf(optarg, "%Lf", &sc); break;
    case 's':
      if (sscanf(optarg, "%Lf,%i,%Lf", &sk, &ssteps, &sdiff) != 3) {
	ssteps=1; sdiff=0;
	sscanf(optarg, "%Lf", &sk);
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

  printf("H:[%Lg, %Lg%s%s%s], S:[%Lg, %Lg%s%s%s], V:[%Lg, %Lg%s%s%s]\n",
	 hc, hk, hi?", inv":"", hl?", log":"", hp?", prop":"",
	 sc, sk, si?", inv":"", sl?", log":"", sp?", prop":"",
	 vc, vk, vi?", inv":"", vl?", log":"", vp?", prop":"");
  
  TargaClose(th);
  outfile=fopen(outfilename, "w");
  xmlwrite(outfile, hc, sc, vc, hk, sk, vk, hi, si, vi, hl, sl, vl, hp, sp, vp);
  fclose(outfile);
  return 0;
}
