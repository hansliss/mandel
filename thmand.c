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

FILE *logfile;

#define FLOAT long double

#define MAX_CHECK_SIZE 1000
#define MAX_RENDER_SIZE 10

unsigned long width=1024;
unsigned long height=1024;
unsigned long maxiter=1048576L;

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

typedef struct finished_point_s {
  unsigned int x, y;
  unsigned long val;
  struct finished_point_s *next;
} *finished_point;

struct taskconfig {
  int taskid;
  unsigned int x0;
  unsigned int y0;
  unsigned int x1;
  unsigned int y1;
  unsigned long maxiter;
  finished_point *result;
  int done;
  pthread_mutex_t *lock;
};

void pushres(finished_point *list, int x, int y, unsigned int val) {
  finished_point tmp=*list;
  (*list) = (finished_point)malloc(sizeof(struct finished_point_s));
  if ((*list) == NULL) {
    fprintf(stderr, "malloc() failed\n"); exit(1);
  }
  (*list)->x = x;
  (*list)->y = y;
  (*list)->val = val;
  (*list)->next = tmp;
}

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
 
void *run_checkfilled(void *cfgi) {
  unsigned int x,y;
  int filled=1;
  FLOAT xval,yval, xval1, yval1;
  int x0, x1, y0, y1;
  struct taskconfig *cfg = (struct taskconfig *)cfgi;

  x0 = cfg->x0;
  x1 = cfg->x1;
  y0 = cfg->y0;
  y1 = cfg->y1;
  yval=YVAL(y0);
  yval1=YVAL(y1);
  for (x = x0; x<= x1; x++) {
    xval=XVAL(x);
    if (mandel(xval, yval, cfg->maxiter) < maxiter || mandel(xval, yval1, cfg->maxiter) < maxiter) {
      filled=0;
      break;
    }
  }
  if (filled) {
    xval=XVAL(x0);
    xval1=XVAL(x1);
    for (y = y0; y<= y1; y++) {
      yval=YVAL(y);
      if (mandel(xval, yval, cfg->maxiter) < maxiter || mandel(xval1, yval, cfg->maxiter) < maxiter) {
	filled=0;
	break;
      }
    }
    if (filled) {
      pthread_mutex_lock(cfg->lock);
      for (x = x0; x <= x1; x++)
	for (y = y0; y <= y1; y++)
	  pushres(cfg->result, x, y, maxiter);
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
  unsigned long val;
  struct taskconfig *cfg = (struct taskconfig *)cfgi;

  x0 = cfg->x0;
  x1 = cfg->x1;
  y0 = cfg->y0;
  y1 = cfg->y1;

  for (y = y0; y <= y1; y++) {
    yval=YVAL(y);
    for (x = x0; x <= x1; x++) {
      xval=XVAL(x);
      val=mandel(xval, yval, cfg->maxiter);
      pthread_mutex_lock(cfg->lock);
      pushres(cfg->result, x, y, val);
      pthread_mutex_unlock(cfg->lock);
    }
  }

  cfg->done=1;

  return NULL;
}

void usage(char *progname) {
  fprintf(stderr,"Usage: %s -d <def. file> -o <out file> -w <width> -h <height> -m <max iterations>[-p <cpu cores>]\n", progname);
  fprintf(stderr,"\tWhere <file> contains three values, each on one line:\n");
  fprintf(stderr,"\t  center X\n\t  center Y\n\t  width.\n");
}

void quadsplit(workitem *stack, workitem current) {
  int xm=(current->x0 + current->x1) / 2;
  int ym=(current->y0 + current->y1) / 2;
  fprintf(stderr, "Splitting (%d,%d) - (%d,%d) into four parts\n", current->x0, current->y0, current->x1, current->y1);
  pushwork(stack, current->x0, current->y0, xm, ym, 0);
  pushwork(stack, xm + 1, current->y0, current->x1, ym, 0);
  pushwork(stack, current->x0, ym + 1, xm, current->y1, 0);
  pushwork(stack, xm + 1, ym + 1, current->x1, current->y1, 0);
}

int main(int argc,char *argv[]) {
  static struct taskconfig *cfg=NULL;
  static pthread_t *threads=NULL;

  int i, r, nthreads=0, ncores=0;

  int o;

  char *deffilename=NULL, *dumpfilename=NULL;
  FILE *deffile, *dumpfile;

  unsigned long wtmp, htmp;
 
  static workitem workstack=NULL, current;

  static finished_point result=NULL;
  static pthread_mutex_t lock;

  long totpix=(long)width*height;
  long totpix_done=0;
  double totpix_done_percent;

  while ((o=getopt(argc, argv, "d:o:p:w:h:m:")) != EOF) {
    switch (o) {
    case 'd': deffilename=optarg; break;
    case 'o': dumpfilename=optarg; break;
    case 'p': ncores=atoi(optarg); break;
    case 'w': width=atoi(optarg); break;
    case 'h': height=atoi(optarg); break;
    case 'm': maxiter=atoi(optarg); break;
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
  
  if (fscanf(deffile,"%30Lf\n",&centx) < 1 ||
      fscanf(deffile,"%30Lf\n",&centy) < 1 ||
      fscanf(deffile,"%30Lf\n",&dx) < 1) {
    fprintf(stderr, "Error reading %s\n", deffilename);
    exit(-2);
  }
  fclose(deffile);

  if (!(dumpfile=fopen(dumpfilename, "w"))) {
    perror(dumpfilename);
    return -2;
  }

  wtmp=htonl(width);
  htmp=htonl(height);
  fwrite(&wtmp, sizeof(wtmp), 1, dumpfile);
  fwrite(&htmp, sizeof(htmp), 1, dumpfile);

  for (i=0; i<ncores; i++) cfg[i].taskid=-1;

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

  while (nthreads > 0 || workstack || result) {
    if (nthreads < ncores && (current = popwork(&workstack)) != NULL) {
      if (current->x1 - current->x0 > MAX_CHECK_SIZE &&
	  current->y1 - current->y0 > MAX_CHECK_SIZE) {
	quadsplit(&workstack, current);
      } else if (!current->checked) {
	for (i=0; i<ncores; i++) if (cfg[i].taskid == -1) break;
	if (i==ncores) {fprintf(stderr,"Fatal error\n"); return -100;}
	cfg[i].taskid=i;
	cfg[i].x0 = current->x0;
	cfg[i].y0 = current->y0;
	cfg[i].x1 = current->x1;
	cfg[i].y1 = current->y1;
	cfg[i].done = 0;
	cfg[i].maxiter = maxiter;
	cfg[i].result = &result;
	cfg[i].lock = &lock;
	if (!pthread_create(&(threads[i]), NULL, run_checkfilled, (void*)&(cfg[i]))) nthreads++;
	fprintf(stderr, "Started run_checkfilled (thread %d) for (%d;%d),(%d;%d)     \n", i,
		current->x0, current->y0, current->x1, current->y1);
      } else if (current->x1 - current->x0 > MAX_RENDER_SIZE &&
		 current->y1 - current->y0 > MAX_RENDER_SIZE) {
	quadsplit(&workstack, current);
      } else {
	for (i=0; i<ncores; i++) if (cfg[i].taskid == -1) break;
	if (i==ncores) {fprintf(stderr,"No free slots? There should be free slots!\n"); return -100;}
	cfg[i].taskid=i;
	cfg[i].x0 = current->x0;
	cfg[i].y0 = current->y0;
	cfg[i].x1 = current->x1;
	cfg[i].y1 = current->y1;
	cfg[i].done = 0;
	cfg[i].maxiter = maxiter;
	cfg[i].result = &result;
	cfg[i].lock = &lock;
	if (!pthread_create(&(threads[i]), NULL, run_dowork, (void*)&(cfg[i]))) nthreads++;
	fprintf(stderr, "Started run_dowork (thread %d) for (%d;%d),(%d;%d)     \n", i, 
		current->x0, current->y0, current->x1, current->y1);
      }
      free(current);
    }

    for (i = 0; i < ncores; i++) {
      if (cfg[i].taskid >= 0 && pthread_kill(threads[i], 0) != 0) {
	pthread_join(threads[i], (void **)&r);
	fprintf(stderr, "Thread %d terminated                          \n", i);
	cfg[i].taskid = -1;
	nthreads--;
	if (!(cfg[i].done)) {
	  pushwork(&workstack, cfg[i].x0, cfg[i].y0, cfg[i].x1, cfg[i].y1, 1);
	}
      }
    }

    if (result) {
      pthread_mutex_lock(&lock);
      while (result) {
	finished_point res=result;
	result=result->next;
	unsigned int colornum=htonl(res->val);
	fseek(dumpfile, 2 * sizeof(wtmp) + sizeof(colornum)*(res->y * width + res->x), SEEK_SET);
	fwrite(&colornum, sizeof(colornum), 1, dumpfile);
	fflush(dumpfile);
	totpix_done++;
	totpix_done_percent=(double)100 * (double)totpix_done / (double)totpix;
	/*
	  fprintf(stderr, "(%d, %d) : %ld  (%.2g%%, %d threads)                \r",
		res->x, res->y, res->val, totpix_done_percent, nthreads);
	*/
	free(res);
      }
      fprintf(stderr, "%.2g%% done, %d threads\n", totpix_done_percent, nthreads);
      pthread_mutex_unlock(&lock);
    }
    if (nthreads == ncores || !workstack) usleep(200);
  }

  fclose(dumpfile);
  return 0;
}
