#define _GNU_SOURCE
#include "ftq.h"
#include <pthread.h>

/* Affinity */
#include <sys/syscall.h>
#include <sys/types.h>
#include <sched.h>
#include "affinity.h"

/*
 * Macros and defines
 */

/*
 * Defaults
 */
#define MAX_SAMPLES    30000000
//#define MIN_SAMPLES    1000
#define MIN_SAMPLES    100
#define DEFAULT_COUNT  10000
#define DEFAULT_BITS   20
#define MAX_BITS       30
#define MIN_BITS       10
#define MULTIITER
#define ITERCOUNT      32
#define VECLEN         1024

/*
 * Global variables
 */

/* Samples: each sample has a timestamp and a work count. */
static unsigned long long *samples;
static long long work_length;
static int work_bits = DEFAULT_BITS;
static unsigned long nsamples = DEFAULT_COUNT;

/* Per-thread timing information */
ticks *cycles_total;
double *secs_total;

/* Threads are bound to CPUs */
int *cpus = NULL;
int cpus_size = 0;

/*
 * Usage
 */
void usage(char *argv0)
{
  printf("Usage: %s [OPTIONS]\n", argv0);
  printf("Options:\n"
	 "  -t, --threads NUM  Number of threads\n"
	 "  -c, --cpus RANGE   Bind threads to these CPUs\n"
	 "  -n, --samples NUM  Number of samples per thread\n"
	 "  -w, --work NUM     Number of work bits\n"
	 "  -o, --output FILE  Output file\n"
	 "  -s, --stdout       Output results to STDOUT\n"
	 "  -h, --help         Show this help message\n");

  exit(0);
}

/*************************************************************************
 * FWQ core measurement                                                  *
 *************************************************************************/
void *fwq_core(void *arg)
{
  /* thread number, zero based. */
  int thread_num = (int)(intptr_t)arg;
  int offset;

  ticks tick, tock;
  register unsigned long done;
  register long long count;
  register long long wl = -work_length;
#ifdef DAXPY
  double da, dx[VECLEN], dy[VECLEN];
  void daxpy();
#endif

#ifdef DAXPY
  /* Intialize FP work */
  da = 1.0e-6;
  for( i=0; i<VECLEN; i++ ) {
    dx[i] = 0.3141592654;
    dy[i] = 0.271828182845904523536;
  }
#endif

  /* Bind the threads */
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET((cpus_size > 0) ? cpus[thread_num] : thread_num, &cpuset);

  int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
  if (rc != 0)
    perror("pthread_setaffinity_np");

  char str[SHORT_STR_SIZE];
  get_cpu_affinity(str);
  printf("Thread %d running on CPUs %s\n", thread_num, str);

  offset = thread_num * nsamples;

  /***************************************************/
  /* First, warm things up with 1000 test iterations */
  /***************************************************/
  for (done=0; done<MIN_SAMPLES; done++) {

#ifdef ASMx8664
    /* Core work construct written as loop in gas (GNU Assembler) for
       x86-64 in 64b mode with 16 NOPs in the loop. If your running in
       on x86 compatible hardware in 32b mode change "incq" to "incl"
       and "cmpq" to "cmpl".  You can also add/remove "nop"
       instructions to minimize instruction cache turbulence and/or
       increase/decrease the work for each pass of the loop. Verify by
       inspecting the compiler generated assembly code listing.
     */
    count = wl;
    tick = getticks();
      __asm__ __volatile__(
			   "myL1:\tincq %0\n\t"
			   "nop\n\t"
			   "nop\n\t"
			   "nop\n\t"
			   "nop\n\t"
			   "nop\n\t"
			   "nop\n\t"
			   "nop\n\t"
			   "nop\n\t"
			   "nop\n\t"
			   "nop\n\t"
			   "nop\n\t"
			   "nop\n\t"
			   "nop\n\t"
			   "nop\n\t"
			   "nop\n\t"
			   "nop\n\t"
/* Group of 16 NOPs */
			   "cmpq $0, %1\n\t"
			   "js myL1"
			   : "=r"(count)          /* output */
			   : "0"(count),"r"(count) /* input */
			   );
#elif DAXPY
      /* Work construct based on a function call and a vector update
	 operation. VECLEN should be chosen so that this work
	 construct fits into L1 cache (for all hardware threads
	 sharing a core) and have minimal hardware induced runtime
	 variation.
      */
      count = wl;
      tick = getticks();
      for(count = wl; count<0; count++) {
	daxpy( VECLEN, da, dx, 1, dy, 1 );
      }
#else
    /* This is the default work construct. Be very careful with this
      as it is most important that "count" variable be in a register
      and the loop not get optimized away by over zealous compiler
      optimizers.  If "count" not in a register you will get alot of
      variations in runtime due to memory latency.  If the loop is
      optimized away, then the sample runtime will be very short and
      not change even if the work length is increased.  For example,
      with gcc v4.3 -g optimization puts "count" in a memory
      location. -O1 and above optimization levels removes the "count"
      loop entirely.  This is why we wrote the assembly code
      above. The only way to verify what is actually happening is to
      carefully review the compiler generated assembly language.
      */
      count = wl;
      tick = getticks();
      for( count=wl; count<0; ) {
#ifdef MULTIITER
	register int k;
	for (k=0;k<ITERCOUNT;k++)
	  count++;
	for (k=0;k<(ITERCOUNT-1);k++)
	  count--;
#else
	count++;
#endif /* MULTIPLIER */
      }
#endif /* ASMx8664 or DAXPY or default */
      tock = getticks();
      samples[offset+done] = tock-tick;
  }

  /****************************/
  /* now do the real sampling */
  /****************************/

  double time_start = get_time();
  ticks cycles_start = getticks();

  for(done=0; done<nsamples; done++ ) {

#ifdef ASMx8664
    /* Core work loop in gas */
    count = wl;
    tick = getticks();
      __asm__ __volatile__("myL2:\tincq %0\n\t"
			   "nop\n\t"
			   "nop\n\t"
			   "nop\n\t"
			   "nop\n\t"
			   "nop\n\t"
			   "nop\n\t"
			   "nop\n\t"
			   "nop\n\t"
			   "nop\n\t"
			   "nop\n\t"
			   "nop\n\t"
			   "nop\n\t"
			   "nop\n\t"
			   "nop\n\t"
			   "nop\n\t"
			   "nop\n\t"
/* Group of 16 NOPs */
			   "cmpq $0, %1\n\t"
			   "js myL2"
			   : "=r"(count)          /* output */
			   : "0"(count),"r"(count) /* input */
			   );
#elif DAXPY
      /* Core work as function all and 64b FP vector update loop */
      count = wl;
      tick = getticks();
      for(count = wl; count<0; count++) {
	daxpy( VECLEN, da, dx, 1, dy, 1 );
      }
#else
      /* Default core work loop */
      count = wl;
      tick = getticks();
      for( count=wl; count<0; ) {
#ifdef MULTIITER
	register int k;
	for (k=0;k<ITERCOUNT;k++)
	  count++;
	for (k=0;k<(ITERCOUNT-1);k++)
	  count--;
#else
	count++;
#endif /* MULTIITER */
      }
#endif /* ASMx86 or DAXPY or default */
      tock = getticks();
      samples[offset+done] = tock-tick;
  }

  ticks cycles_end = getticks();
  double time_end = get_time();

  cycles_total[thread_num] = cycles_end - cycles_start;
  secs_total[thread_num] = time_end - time_start;

  return NULL;
}

void daxpy( int n, double da, double *dx, int incx, double *dy, int incy )
{
  register int k;
  for( k=0; k<n; k++ ) {
    dx[k] += da*dy[k];
  }
  return;
}

int main(int argc, char *argv[])
{
  // Define long options
  static struct option long_options[] =
    {
     {"help",     no_argument,       0, 'h'},
     {"stdout",   no_argument,       0, 's'},
     {"samples",  required_argument, 0, 'n'},
     {"work",     required_argument, 0, 'w'},
     {"output",   required_argument, 0, 'o'},
     {"threads",  required_argument, 0, 't'},
     {"cpus",     required_argument, 0, 'c'},
     {0, 0, 0, 0}  // Terminator
    };

  const char *optstring = "hsn:w:o:t:c:";
  int option_index = 0;

  /* Default output name prefix */
  char outname[255];
  sprintf(outname, "fwq-th-times.dat");

  int nthreads=1;
  int use_stdout=0;

  int c;
  while ((c = getopt_long(argc, argv,
			  optstring, long_options,
			  &option_index)) != -1) {
    switch (c) {
    case 't':
      nthreads = atoi(optarg);
      break;
    case 'o':
      sprintf(outname, "%s", optarg);
      break;
    case 'w':
      work_bits = atoi(optarg);
      break;
    case 'n':
      nsamples = atoi(optarg);
      break;
    case 'c':
      cpus = range2int(optarg, &cpus_size);
      break;
    case 's':
      use_stdout = 1;
      break;
    default:
      usage(argv[0]);
      break;
    }
  }

  int i,j;
#if 0
  if (cpus)
    for (i=0; i<cpus_size; i++)
      printf("%d: %d\n", i, cpus[i]);
#endif

  /*
   * Input checks
   */
  if (cpus && cpus_size < nthreads) {
    fprintf(stderr, "WARN: Given %d CPUs, but %d are needed\n",
	    cpus_size, nthreads);
    /* Revert to default binding */
    cpus_size = 0;
  }

  if (nsamples < MIN_SAMPLES || nsamples > MAX_SAMPLES) {
    fprintf(stderr,"WARN: Num samples valid range is [%d,%d]. "
	    "Setting to %d\n", MIN_SAMPLES, MAX_SAMPLES, MIN_SAMPLES);
    nsamples = MIN_SAMPLES;
  }

  if (work_bits < MIN_BITS || work_bits > MAX_BITS) {
    fprintf(stderr,"WARN: Work bits valid range is [%d,%d]. "
	    "Setting to %d.\n", MIN_BITS, MAX_BITS, MIN_BITS);
    work_bits = MIN_BITS;
  }

  /*
   * Parameters used
   */
  work_length = 1 << work_bits;

  printf("Number of threads: %d\n", nthreads);
  printf("Work per thread: %lld\n", work_length);
  printf("Number of samples per thread: %ld\n", nsamples);
  if (!use_stdout)
    printf("Output file: %s\n", outname);

  /*
   * Storage
   */
  samples = malloc(sizeof(unsigned long long)* nsamples * nthreads);
  assert(samples != NULL);

  /* Per-thread timing */
  cycles_total = malloc(sizeof(ticks) * nthreads);
  secs_total = malloc(sizeof(double) * nthreads);

  pthread_t *threads = malloc(sizeof(pthread_t) * nthreads);
  assert(threads != NULL);

  /*
   * Do the work
   */
  int rc;
  for (i=1; i<nthreads; i++) {
    rc = pthread_create(&threads[i], NULL, fwq_core, (void *)(intptr_t)i);
    if (rc) {
      fprintf(stderr,"ERR: pthread_create failed\n");
      exit(EXIT_FAILURE);
    }
  }
  fwq_core(0);

  for (i=1; i<nthreads; i++) {
    rc = pthread_join(threads[i],NULL);
    if (rc) {
      fprintf(stderr,"ERR: pthread_join failed\n");
      exit(EXIT_FAILURE);
    }
  }

  /*
   * Output results
   */
  FILE *fp = (use_stdout) ? stdout : fopen(outname, "w");
  if (fp == NULL) {
    fprintf(stderr, "ERR: Cannot write to file %s", outname);
    exit(EXIT_FAILURE);
  }

  for (j=0; j<nthreads; j++)
    fprintf(fp, "Speed: thread %d, cycles %lld, seconds %f, GHz %f\n",
	    j, (long long)cycles_total[j], secs_total[j],
	    ((double)cycles_total[j]) / (secs_total[j] * 1.0e9));

  for (j=0; j<nthreads; j++) {
    fprintf(fp, "Thread %d running on CPUs %d\n", j,
	    (cpus_size > 0) ? cpus[j] : j);

    for (i=0;i<nsamples;i++)
      fprintf(fp, "%lld\n", samples[nsamples*j + i]);
  }

  /*
   * Clean up
   */
  if (!use_stdout)
    fclose(fp);
  if (cpus)
    free(cpus);
  free(threads);
  free(samples);
  free(cycles_total);
  free(secs_total);

  pthread_exit(NULL);

  return 0;
}
