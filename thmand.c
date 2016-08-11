#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdarg.h>
#include <gmp.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <sys/mman.h>

FILE *logfile;

#define FLOAT long double

#define MAX_CHECK_SIZE 10000
#define MAX_RENDER_SIZE 50

#define SPEED_MAX ((long double)6.8)

#define MODE_NORMAL 0
#define MODE_SPEED 1
#define MODE_ANGLE 2
#define MODE_DE 3

#define THMAND_MODE_INITIALIZE 0
#define THMAND_MODE_CONTINUE 1
#define THMAND_MODE_UPDATE 2

unsigned long width=1024;
unsigned long height=1024;
unsigned long maxiter=1048576L;
char logprefix[128];

static int *dumpbuffer=NULL;
static long totpix_done=0;
static long lasttotpix_done=0;

static FLOAT minde=1E14;
static FLOAT maxde=-1E14;

typedef struct fileheader_s {
  int width;
  int height;
  int maxiter;
  int reserved;
} fileheader;

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

void *run(void *cfg);

#define max(x,y) (((x)>(y))?(x):(y))
#define min(x,y) (((x)<(y))?(x):(y))
#define XVAL(xpos) ((point_x0)+(((point_x1)-(point_x0))*(FLOAT)(xpos)/(FLOAT)width))
#define YVAL(ypos) ((point_y0)+(((point_y1)-(point_y0))*(FLOAT)(height - ypos)/(FLOAT)height))

typedef unsigned char byte;

FLOAT centx,centy,dx,point_x0,point_x1,point_y0,point_y1,point_size;

char *make_message(const char *fmt, va_list *ap) {
  int n;
  int size = 100;     /* Guess we need no more than 100 bytes. */
  char *p, *np;
  
  if ((p = malloc(size)) == NULL)
    return NULL;
  
  while (1) {
    
    /* Try to print in the allocated space. */
    n = vsnprintf(p, size, fmt, *ap);
    
    /* If that worked, return the string. */
    
    if (n > -1 && n < size)
      return p;
    
    /* Else try again with more space. */
    
    if (n > -1)    /* glibc 2.1 */
      size = n+1; /* precisely what is needed */
    else           /* glibc 2.0 */
      size *= 2;  /* twice the old size */
    
    if ((np = realloc (p, size)) == NULL) {
      free(p);
      return NULL;
    } else {
      p = np;
    }
  }
}


void debug(const char *fmt, ...) {
  /*
    va_list ap;
    va_start(ap, fmt);
    char *msg = make_message(fmt, &ap);
    va_end(ap);
    fprintf(stderr, "%s\n", msg);
    free(msg);
  */
}

struct taskconfig {
  int taskid;
  unsigned int x0;
  unsigned int y0;
  unsigned int x1;
  unsigned int y1;
  unsigned long maxiter;
  FLOAT de_k;
  int de_logr;
  int mode;
  int done;
  pthread_mutex_t *lock;
};

typedef struct workitem_s {
  unsigned int x0;
  unsigned int y0;
  unsigned int x1;
  unsigned int y1;
  int checked;
  struct workitem_s *next;
} *workitem;

void checkstack(workitem stack) {
  debug("Stack:");
  while (stack) {
    debug("\tEntry: (%s): (%d,%d)-(%d,%d)", stack->checked?"checked":"unchecked", stack->x0, stack->y0, stack->x1, stack->y1);
    stack = stack->next;
  }
}

void pushwork(workitem *stack, int x0, int y0, int x1, int y1, int checked) {
  workitem tmp = *stack;
  checkstack(*stack);
  *stack = (workitem)malloc(sizeof(struct workitem_s));
  (*stack)->x0 = x0;
  (*stack)->y0 = y0;
  (*stack)->x1 = x1;
  (*stack)->y1 = y1;
  (*stack)->checked = checked;
  (*stack)->next = tmp;
  debug("Pushing work (%s): (%d,%d)-(%d,%d)", checked?"checked":"unchecked", x0,y0, x1, y1);
  checkstack(*stack);
}

workitem popwork(workitem *stack) {
  workitem tmp = *stack;
  checkstack(*stack);
  if (tmp) {
    (*stack) = (*stack)->next;
    debug("Popping work (%s): (%d,%d)-(%d,%d)", tmp->checked?"checked":"unchecked", tmp->x0, tmp->y0, tmp->x1, tmp->y1);
    checkstack(*stack);
  } else debug("Stack is empty");
  return tmp;
}

unsigned long mandel(FLOAT cx, FLOAT cy, unsigned long maxiter) {
  unsigned long i=maxiter;
  FLOAT zx=cx,zy=cy,zx2=cx*cx,zy2=cy*cy;
  while((i>0)&&(zx2+zy2 < 4.0)) {
    zy=2*zx*zy + cy;
    zx=zx2-zy2 + cx;
    zx2=zx*zx;
    zy2=zy*zy;
    i--;
  }
#ifdef DEBUG
  fprintf(logfile,"mandel=%u, maxiter=%u\n",i,maxiter);
#endif
  return maxiter-i;
}
 
unsigned long mandelspeed(FLOAT cx, FLOAT cy, unsigned long maxiter) {
  unsigned long i=maxiter;
  FLOAT zx=cx,zy=cy,zx2=cx*cx,zy2=cy*cy;
  long double ox=cx,oy=cy;
  while((i>0)&&(zx2+zy2 < 4.0)) {
    ox=zx;oy=zy;
    zy=2*zx*zy + cy;
    zx=zx2-zy2 + cx;
    zx2=zx*zx;
    zy2=zy*zy;
    i--;
  }
#ifdef DEBUG
  fprintf(logfile,"mandel=%u, maxiter=%u\n",i,maxiter);
#endif
  return (unsigned long)((long double)maxiter * sqrt((zx-ox)*(zx-ox)+(zy-oy)*(zy-oy)) / SPEED_MAX);
}
 
unsigned long mandelangle(FLOAT cx, FLOAT cy, unsigned long maxiter) {
  unsigned long i=maxiter;
  FLOAT zx=cx,zy=cy,zx2=cx*cx,zy2=cy*cy;
  long double ox=cx,oy=cy;
  while((i>0)&&(zx2+zy2 < 4.0)) {
    ox=zx;oy=zy;
    zy=2*zx*zy + cy;
    zx=zx2-zy2 + cx;
    zx2=zx*zx;
    zy2=zy*zy;
    i--;
  }
#ifdef DEBUG
  fprintf(logfile,"mandel=%u, maxiter=%u\n",i,maxiter);
#endif
  return (unsigned long)(maxiter*(3.1415926535+atan2((zx-ox),(zy-oy)))/6.28320);
}

/*
    dz = 2 z dz + 1
    2 * (zx + zy * i)(dzx + dzy * i) + 1
    2 * (zx * (dzx + dzy * i) + zy * i * (dzx + dzy * i)) + 1
    2 * (zx * dzx + zx * dzy * i + zy * dzx * i - zy * dzy) + 1
    2 * zx * dzx - 2 * zy * dzy + (2 * zx * dzy + 2* zy * dzx) * i + 1
*/ 
unsigned long mandelde(FLOAT cx, FLOAT cy, unsigned long maxiter, FLOAT k, int logr) {
  unsigned long i=maxiter;
  FLOAT zx=cx,zy=cy,zx2=cx*cx,zy2=cy*cy;
  FLOAT dzx = 0, dzy = 0;
  while((i>0)&&(zx2+zy2 <= 1E6)) {
    FLOAT tmp = 2.0 * (zx * dzx - zy * dzy) + 1.0;
    dzy = 2.0 * (zx * dzy + zy * dzx);
    dzx = tmp;

    zy=2*zx*zy + cy;
    zx=zx2-zy2 + cx;
    zx2=zx*zx;
    zy2=zy*zy;
    i--;
  }
#ifdef DEBUG
  fprintf(logfile,"mandel=%u, maxiter=%u\n",i,maxiter);
#endif
  if (i == 0) return maxiter;
  else {
    FLOAT r;
    FLOAT de=log10l(zx2 + zy2) * sqrtl((zx2 + zy2) / (dzx * dzx + dzy * dzy));
    if (de > maxde) maxde=de;
    if (de < minde) minde=de;
    if (logr) {
      r = ((FLOAT)maxiter) * log10l(1/de) / k;
    } else {
      r = ((FLOAT)maxiter) * de / k;
    }
    if (r < 0) r=0;
    return ((long)r) % maxiter;
  }
}
 
void *run_checkfilled(void *cfgi) {
  unsigned int x,y;
  int filled=1;
  FLOAT xval,yval, xval1, yval1;
  int x0, x1, y0, y1;
  int changedpixels=0;
  struct taskconfig *cfg = (struct taskconfig *)cfgi;

  if (cfg->mode != MODE_NORMAL && cfg->mode != MODE_DE) return NULL;

  x0 = cfg->x0;
  x1 = cfg->x1;
  y0 = cfg->y0;
  y1 = cfg->y1;
  yval=YVAL(y0);
  yval1=YVAL(y1);
  for (x = x0; x<= x1; x++) {
    int v1 = ntohl(dumpbuffer[4 + x + y0 * width]);
    int v2 = ntohl(dumpbuffer[4 + x + y1 * width]);
    xval=XVAL(x);
    if (v1 == -1) {
      switch (cfg->mode) {
      case MODE_DE:
	v1=mandelde(xval, yval, cfg->maxiter, cfg->de_k, cfg->de_logr);
	break;
      case MODE_NORMAL:
      default:
	v1=mandel(xval, yval, cfg->maxiter);
	break;
      }
      dumpbuffer[4 + x + y0 * width] = htonl(v1);
    }
    if (v2 == -1) {
      switch (cfg->mode) {
      case MODE_DE:
	v2=mandelde(xval, yval1, cfg->maxiter, cfg->de_k, cfg->de_logr);
	break;
      case MODE_NORMAL:
      default:
	v2=mandel(xval, yval1, cfg->maxiter);
	break;
      }
      dumpbuffer[4 + x + y1 * width] = htonl(v2);
    }
    if (v1 < cfg->maxiter || v2 < cfg->maxiter) {
      filled=0;
      break;
    }
  }
  if (filled) {
    xval=XVAL(x0);
    xval1=XVAL(x1);
    for (y = y0; y<= y1; y++) {
      int v1 = ntohl(dumpbuffer[4 + x0 + y * width]);
      int v2 = ntohl(dumpbuffer[4 + x1 + y * width]);
      yval=YVAL(y);
      if (v1 == -1) {
	switch (cfg->mode) {
	case MODE_DE:
	  v1=mandelde(xval, yval, cfg->maxiter, cfg->de_k, cfg->de_logr);
	  break;
	case MODE_NORMAL:
	default:
	  v1=mandel(xval, yval, cfg->maxiter);
	  break;
	}
	dumpbuffer[4 + x0 + y * width] = htonl(v1);
      }
      if (v2 == -1) {
	switch (cfg->mode) {
	case MODE_DE:
	  v2=mandelde(xval1, yval, cfg->maxiter, cfg->de_k, cfg->de_logr);
	  break;
	case MODE_NORMAL:
	default:
	  v2=mandel(xval1, yval, cfg->maxiter);
	  break;
	}
	dumpbuffer[4 + x1 + y * width] = htonl(v2);
      }
      if (v1 < cfg->maxiter || v2 < cfg->maxiter) {
	filled=0;
	break;
      }
    }
    if (filled) {
      for (x = x0; x <= x1; x++) {
	for (y = y0; y <= y1; y++) {
	  int val=ntohl(dumpbuffer[4 + x + y * width]);
	  if (val == -1) {
	    dumpbuffer[4 + x + y * width] = htonl(cfg->maxiter);
	  }
	  changedpixels++;
	}
      }
      pthread_mutex_lock(cfg->lock);
      totpix_done += changedpixels;
      pthread_mutex_unlock(cfg->lock);
      cfg->done = 1;
      return NULL;
    }
  }
  return NULL;
}

void *run_dowork(void *cfgi) {
  unsigned int x,y;
  FLOAT xval,yval;
  int x0, x1, y0, y1;
  int val;
  int changedpixels=0;
  struct taskconfig *cfg = (struct taskconfig *)cfgi;

  x0 = cfg->x0;
  x1 = cfg->x1;
  y0 = cfg->y0;
  y1 = cfg->y1;

  for (y = y0; y <= y1; y++) {
    yval=YVAL(y);
    for (x = x0; x <= x1; x++) {
      val = ntohl(dumpbuffer[4 + x + y * width]);
      if (val == -1) {
	xval=XVAL(x);
	switch (cfg->mode) {
	case MODE_SPEED:
	  val=mandelspeed(xval, yval, cfg->maxiter);
	  break;
	case MODE_ANGLE:
	  val=mandelangle(xval, yval, cfg->maxiter);
	  break;
	case MODE_DE:
	  val=mandelde(xval, yval, cfg->maxiter, cfg->de_k, cfg->de_logr);
	  break;
	case MODE_NORMAL:
	default:
	  val=mandel(xval, yval, cfg->maxiter);
	  break;
	}
	dumpbuffer[4 + x + y * width] = htonl(val);
      }
      changedpixels++;
    }
  }

  pthread_mutex_lock(cfg->lock);
  totpix_done += changedpixels;
  pthread_mutex_unlock(cfg->lock);
  cfg->done=1;

  return NULL;
}

void usage(char *progname) {
  fprintf(stderr,"Usage: %s -d <def. file> -o <out file> -w <width> -h <height> -m <max iterations> [-p <cpu cores>] [-O <old max iterations>] [-M <mode: s for speed, a for angle, d for distance estimator>] [-K <K for DE alg>] [-L (use the log() of the DE result)]\n", progname);
  fprintf(stderr,"\tWhere <file> contains three values, each on one line:\n");
  fprintf(stderr,"\t  center X\n\t  center Y\n\t  width.\n");
}

void quadsplit(workitem *stack, workitem current) {
  int xm=(current->x0 + current->x1) / 2;
  int ym=(current->y0 + current->y1) / 2;
  fprintf(stderr, "%sSplitting (%d,%d) - (%d,%d) into four parts\n", logprefix, current->x0, current->y0, current->x1, current->y1);
  pushwork(stack, xm + 1, ym + 1, current->x1, current->y1, 0);
  pushwork(stack, current->x0, ym + 1, xm, current->y1, 0);
  pushwork(stack, xm + 1, current->y0, current->x1, ym, 0);
  pushwork(stack, current->x0, current->y0, xm, ym, 0);
}

int main(int argc,char *argv[]) {
  static struct taskconfig *cfg=NULL;
  static pthread_t *threads=NULL;
  int mode=MODE_NORMAL;

  fileheader header;

  int tempindex, r, nthreads=0, ncores=1, x, y, tmpval;
  unsigned long oldmaxiter=0;

  int o;

  char *deffilename=NULL, *dumpfilename=NULL;
  FILE *deffile, *dumpfile;
  struct stat fstatbuf;

  long wtmp, htmp;

  mpf_t gcentx, gcenty, gdx;
 
  static workitem workstack=NULL, current;

  unsigned long filesize=0;
  int file_handling=THMAND_MODE_INITIALIZE;

  FLOAT de_k=21.5;
  int de_logr=0;

  static pthread_mutex_t lock;

  long totpix=(long)width*height;

  long double epsilon;
  ld_epsilon(&epsilon);
  fprintf(stderr, "Epsilon is %Lg\n", epsilon);

  snprintf(logprefix, sizeof(logprefix), "thmand: ");
  while ((o=getopt(argc, argv, "d:o:p:w:h:m:O:M:K:L")) != EOF) {
    switch (o) {
    case 'd': deffilename=optarg; break;
    case 'o': dumpfilename=optarg; break;
    case 'p': ncores=atoi(optarg); break;
    case 'w': width=atoi(optarg); break;
    case 'h': height=atoi(optarg); break;
    case 'm': maxiter=atoi(optarg); break;
    case 'O': oldmaxiter=atoi(optarg); break;
    case 'K': sscanf(optarg, "%Lf", &de_k); break;
    case 'L': de_logr=1; break;
    case 'M':
      if (optarg[0] == 's') {
	mode=MODE_SPEED;
      } else if (optarg[0] == 'a') {
	mode=MODE_ANGLE;
      } else if (optarg[0] == 'd') {
	mode=MODE_DE;
      }
      break;
    default: usage(argv[0]); return -1; break;
    }
  }

  if (!deffilename || !dumpfilename || ncores < 1) {
    usage(argv[0]);
    return -1;
  }

  totpix=(long)width*height;

  cfg=(struct taskconfig *)malloc(sizeof(struct taskconfig) * ncores);
  threads=(pthread_t *)malloc(sizeof(pthread_t) * ncores);

  if (!(deffile=fopen(deffilename,"r")))
    {
      perror(deffilename);
      return -1;
    }
  
  mpf_init(gcentx);
  mpf_init(gcenty);
  mpf_init(gdx);
  gmp_fscanf(deffile,"%Fg\n",gcentx);
  gmp_fscanf(deffile,"%Fg\n",gcenty);
  gmp_fscanf(deffile,"%Fg\n",gdx);

  centx=mpf_get_d(gcentx);
  centy=mpf_get_d(gcenty);
  dx=mpf_get_d(gdx);

  /*
    if (fscanf(deffile,"%30Lf\n",&centx) < 1 ||
    fscanf(deffile,"%30Lf\n",&centy) < 1 ||
    fscanf(deffile,"%30Lf\n",&dx) < 1) {
    fprintf(stderr, "Error reading %s\n", deffilename);
    exit(-2);
    }
  */
  fclose(deffile);


  if (stat(dumpfilename, &fstatbuf) == 0) {
    filesize=fstatbuf.st_size;
    if (!(dumpfile=fopen(dumpfilename, "r+"))) {
      perror(dumpfilename);
      return -2;
    }
    if (fread(&header, sizeof(header), 1, dumpfile) != 1) {
      fprintf(stderr, "Error: File exists but no width/height.\n");
      return -5;
    }
    wtmp=ntohl(header.width);
    htmp=ntohl(header.height);
    if (wtmp != width || htmp != height) {
      fprintf(stderr, "Existing file has different width/height (%ld,%ld). Aborting.\n", wtmp, htmp);
      return -3;
    }
    if (filesize < sizeof(header) + sizeof(int)*(width*height)) {
      lseek(fileno(dumpfile), sizeof(header) + sizeof(int)*(width*height-1), SEEK_SET);
      tmpval=0;
      if (write(fileno(dumpfile), &tmpval, sizeof(tmpval)) == -1) {
	perror("write()");
	return -5;
      }
      file_handling=THMAND_MODE_UPDATE;
    } else {
      file_handling=THMAND_MODE_CONTINUE;
    }
  } else {
    if (!(dumpfile=fopen(dumpfilename, "w+"))) {
      perror(dumpfilename);
      return -2;
    }
    lseek(fileno(dumpfile), sizeof(header) + sizeof(int)*(width*height-1), SEEK_SET);
    tmpval=0;
    if (write(fileno(dumpfile), &tmpval, sizeof(tmpval)) == -1) {
      perror("write()");
      return -5;
    }
    file_handling=THMAND_MODE_INITIALIZE;
  }

  if ((dumpbuffer=(int *)mmap(NULL,
			      sizeof(header) + 
			      sizeof(int) * (width * height),
			       PROT_READ|PROT_WRITE,
			       MAP_SHARED,
			       fileno(dumpfile),
			       0)) == MAP_FAILED) {
    perror("mmap()");
    return 4;
  }
  switch (file_handling) {
  case THMAND_MODE_INITIALIZE:
    fprintf(stderr, "Initializing file...\n");
    // Someone (me) made a mistake early on here, so the width and height
    // are 64-bit values but are treated as 32-bit values in the dump file.
    // This is why all address uses an offset of 4 ints (2 longs) instead of 2.
    dumpbuffer[0]=htonl(width);
    dumpbuffer[1]=htonl(height);
    dumpbuffer[2]=htonl(maxiter);
    dumpbuffer[3]=0;

    msync(&(dumpbuffer[0]), sizeof(header), MS_ASYNC);
    for (y=0; y<height; y++) {
      fprintf(stderr, "%d\r", y);
      for (x=0; x<width; x++) {
	dumpbuffer[4 + x + y * width] = htonl((long)-1);
      }
      msync(&(dumpbuffer[4 + y*width]), width * sizeof(int), MS_ASYNC);
    }
    fprintf(stderr, "Done.    \n");
    break;
  case THMAND_MODE_UPDATE:
    fprintf(stderr, "Dump file is partially completed by an older version. Setting all zero values to -1...\n");
    for (y=0; y<height; y++) {
      fprintf(stderr, "%d\r", y);
      for (x=0; x<width; x++) {
	int val=ntohl(dumpbuffer[4 + x + y * width]);
	if (val == 0)
	  dumpbuffer[4 + x + y * width] = ntohl(-1);
      }
      msync(&(dumpbuffer[4 + y * width]), width * sizeof(int), MS_ASYNC);
    }
    fprintf(stderr, "Done.    \n");
    break;
  default:
    if (oldmaxiter != 0 && oldmaxiter < maxiter) {
      fprintf(stderr, "New and higher maxiter value specified, so invalidating all old maxiter points.\n");
      for (y=0; y<height; y++) {
	fprintf(stderr, "%d\r", y);
	for (x=0; x<width; x++) {
	  int val=ntohl(dumpbuffer[4 + x + y * width]);
	  if (val == oldmaxiter)
	    dumpbuffer[4 + x + y * width] = htonl(-1);
	}
      }
      dumpbuffer[2] = htonl(maxiter);
      fprintf(stderr, "Done.    \n");
    }
    break;
  }
    
  for (tempindex=0; tempindex<ncores; tempindex++) {
    cfg[tempindex].taskid = -1;
    cfg[tempindex].mode = mode;
    cfg[tempindex].de_k = de_k;
    cfg[tempindex].de_logr = de_logr;
  }

  point_x0=centx-dx/2;
  point_x1=centx+dx/2;
  point_y0=centy-(((FLOAT)height)/((FLOAT)width))*dx/2;
  point_y1=centy+(((FLOAT)height)/((FLOAT)width))*dx/2;
  point_size=((point_x1-point_x0)/width);

  fprintf(stderr, "Current center is %20.20Lf, %20.20Lf, dx=%20.20Lf\n",centx,centy,dx);
  fprintf(stderr, "Corners are %20.20Lf, %20.20Lf  and  %20.20Lf, %20.20Lf\n",
	  point_x0,point_y0,point_x1,point_y1);
  fprintf(stderr, "Maxiter is %ld\n", maxiter);

  pushwork(&workstack, 0, 0, width-1, height-1, 0);

  pthread_mutex_init(&lock, NULL);
 
  while (nthreads > 0 || workstack) {
    if (nthreads < ncores && (current = popwork(&workstack)) != NULL) {
      if ((1 + current->x1 - current->x0) > MAX_CHECK_SIZE &&
	  (1 + current->y1 - current->y0) > MAX_CHECK_SIZE) {
	quadsplit(&workstack, current);
      } else if (!current->checked) {
	for (tempindex=0; tempindex<ncores; tempindex++) if (cfg[tempindex].taskid == -1) break;
	if (tempindex==ncores) {fprintf(stderr,"Fatal error\n"); return -100;}
	cfg[tempindex].taskid=tempindex;
	cfg[tempindex].x0 = current->x0;
	cfg[tempindex].y0 = current->y0;
	cfg[tempindex].x1 = current->x1;
	cfg[tempindex].y1 = current->y1;
	cfg[tempindex].done = 0;
	cfg[tempindex].maxiter = maxiter;
	cfg[tempindex].mode = mode;
	cfg[tempindex].de_k = de_k;
	cfg[tempindex].de_logr = de_logr;
	cfg[tempindex].lock = &lock;
	if (!pthread_create(&(threads[tempindex]), NULL, run_checkfilled, (void*)&(cfg[tempindex]))) {
	  nthreads++;
	  fprintf(stderr, "%sStarted run_checkfilled (thread %d) for (%d;%d),(%d;%d)     \n", logprefix, tempindex,
		  current->x0, current->y0, current->x1, current->y1);
	} else perror("pthread_create()");
      } else if ((1 + current->x1 - current->x0) > MAX_RENDER_SIZE &&
		 (1 + current->y1 - current->y0) > MAX_RENDER_SIZE) {
	quadsplit(&workstack, current);
      } else {
	for (tempindex=0; tempindex<ncores; tempindex++) if (cfg[tempindex].taskid == -1) break;
	if (tempindex==ncores) {fprintf(stderr,"No free slots? There should be free slots!\n"); return -100;}
	cfg[tempindex].taskid=tempindex;
	cfg[tempindex].x0 = current->x0;
	cfg[tempindex].y0 = current->y0;
	cfg[tempindex].x1 = current->x1;
	cfg[tempindex].y1 = current->y1;
	cfg[tempindex].done = 0;
	cfg[tempindex].maxiter = maxiter;
	cfg[tempindex].mode = mode;
	cfg[tempindex].de_k = de_k;
	cfg[tempindex].de_logr = de_logr;
	cfg[tempindex].lock = &lock;
	if (!pthread_create(&(threads[tempindex]), NULL, run_dowork, (void*)&(cfg[tempindex]))) {
	  nthreads++;
	  fprintf(stderr, "%sStarted run_dowork (thread %d) for (%d;%d),(%d;%d)     \n", logprefix, tempindex, 
		  current->x0, current->y0, current->x1, current->y1);
	} else perror("pthread_create()");
      }
      free(current);
    }

    for (tempindex = 0; tempindex < ncores; tempindex++) {
      if (cfg[tempindex].taskid >= 0 && pthread_kill(threads[tempindex], 0) != 0) {
	pthread_join(threads[tempindex], (void **)&r);
	fprintf(stderr, "%sThread %d terminated                          \n", logprefix, tempindex);
	cfg[tempindex].taskid = -1;
	nthreads--;
	if (!(cfg[tempindex].done)) {
	  pushwork(&workstack, cfg[tempindex].x0, cfg[tempindex].y0, cfg[tempindex].x1, cfg[tempindex].y1, 1);
	}
      }
    }

    if (totpix_done != lasttotpix_done) {
      snprintf(logprefix, sizeof(logprefix), "thmand: %g%% done: ", 100 * (double)totpix_done / (double)totpix);
      lasttotpix_done=totpix_done;
    }

    if (nthreads == ncores || !workstack) usleep(200);
  }

  if (munmap(dumpbuffer, sizeof(header) + sizeof(int) * (width * height)) == -1) {
    perror("munmap()");
  }
  if (mode==MODE_DE) printf("minde=%Lg, maxde=%Lg\n", minde, maxde);

  fclose(dumpfile);
  return 0;
}
