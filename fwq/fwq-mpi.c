#include <mpi.h>
#include "ftq.h"
#include "affinity.h"

/**
 * macros and defines
 */

/** defaults **/
#define MAX_SAMPLES    2000000000
#define MIN_SAMPLES    1
#define DEFAULT_COUNT  10000
#define DEFAULT_LENGTH (1 << 20)
#define MAX_LENGTH     (1 << 30)
#define MIN_LENGTH     (1)
#define MULTIITER
#define ITERCOUNT      32
#define VECLEN         1024

/**
 * global variables
 */

/* samples: each sample has a timestamp and a work count. */
static unsigned long long *samples = NULL;
static long long work_length = DEFAULT_LENGTH;
static int target_seconds = -1;
static unsigned long numsamples = DEFAULT_COUNT;

/**
 * usage()
 */
void usage(char *av0) {
  fprintf(stderr,"usage: %s [-t threads] [-n samples] [-w bits] [-h] [-o outname] [-s]\n",
	  av0);
  exit(EXIT_FAILURE);
}

/*************************************************************************
 * FWQ core: does the measurement                                        *
 *************************************************************************/
void *fwq_core(void *arg, unsigned long* cycles, double* seconds)
{
  /* thread number, zero based. */
  int thread_num = (int)(intptr_t)arg;
  int i=0,offset;

  ticks tick, tock;
  ticks cycles_start, cycles_end, cycles_total;
  double time_start, time_end, time_total;
  register unsigned long done;
  register long long count;
  register long long wl = -work_length;

#ifdef DAXPY
  double da, dx[VECLEN], dy[VECLEN];
  void daxpy();

  /* Intialize FP work */
  da = 1.0e-6;
  for( i=0; i<VECLEN; i++ ) {
    dx[i] = 0.3141592654;
    dy[i] = 0.271828182845904523536;
  }
#endif

  /* affinity stuff */
  unsigned long mask = 0x1;

  offset = thread_num * numsamples;

  /***************************************************/
  /* first, warm things up with no more than 1000 iterations */
  /***************************************************/
  unsigned long long* ptr = samples + offset;
  unsigned long warmup = numsamples;
  if (numsamples > 1000) {
    warmup = 1000;
  }

  time_start = MPI_Wtime();
  for(done=0; done<warmup; done++ ) {

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
      *ptr = (unsigned long long) (tock-tick);
      ptr++;
  }
  time_end = MPI_Wtime();
  time_total = time_end - time_start;

  /* A convenient interface is to ask the user for a duration to run
   * and the number of samples to take in that duration.  Based on the
   * test above, set the work loop below appropriately */
  if (target_seconds >= 0 && warmup > 0 && numsamples > 0) {
    /* compute new work loop */
    double current_time_per_sample = time_total / (double)warmup;
    double allowed_time_per_sample = (double)target_seconds / (double)numsamples;
    double wl_adjustment_factor = allowed_time_per_sample / current_time_per_sample;
    long long new_wl = (long long) ((double)wl * wl_adjustment_factor);

    /* print new value */
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank == 0) {
      printf("Work loop of %lld takes %f seconds\n", (-wl), current_time_per_sample);
      printf("Adjusting work loop to %lld for estimated %f seconds\n", (-new_wl), allowed_time_per_sample);
    }

    /* use value computed by rank 0 */
    MPI_Bcast(&new_wl, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);
    wl = new_wl;

    if (wl >= 0) {
      printf("ERROR: can't adjust work loop to account for target runtime and desired number of samples\n");
      MPI_Abort(MPI_COMM_WORLD, 1);
    }
  }

  /****************************/
  /* now do the real sampling */
  /****************************/

  ptr = samples + offset;
  time_start = MPI_Wtime();
  cycles_start = getticks();
  for(done=0; done<numsamples; done++ ) {

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
      *ptr = (unsigned long long) tock-tick;
      ptr++;
  }
  cycles_end = getticks();
  time_end = MPI_Wtime();

  cycles_total = cycles_end - cycles_start;
  time_total = time_end - time_start;

  *cycles  = (unsigned long)cycles_total;
  *seconds = time_total;

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

int main(int argc, char **argv)
{
  /* local variables */
  char fname_times[1024], buf[32], outname[255];
  int i,j;
  int numthreads = 1, use_threads = 0;
  int use_stdout = 0;

  int rc;
  pthread_t *threads;
  unsigned long mask = 1;

  int rank, ranks;
  MPI_Init(NULL, NULL);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &ranks);

  /* default output name prefix */
  sprintf(outname,"fwq");

     /*
      * getopt_long to parse command line options.
      * basically the code from the getopt man page.
      */
     while (1) {
       int c;
       int option_index = 0;
       static struct option long_options[] = {
	 {"help",0,0,'h'},
	 {"numsamples",0,0,'n'},
	 {"work",0,0,'w'},
         {"seconds",0,0,'d'},
	 {"outname",0,0,'o'},
	 {"stdout",0,0,'s'},
	 {"threads",0,0,'t'},
	 {0,0,0,0}
       };

       c = getopt_long(argc, argv, "n:hsw:d:o:t:",
		       long_options, &option_index);
       if (c == -1)
	 break;

       switch (c) {
       case 't':
         if (rank == 0) {
	   fprintf(stderr,"ERROR: ftq not compiled with pthreads support.\n");
         }
	 exit(EXIT_FAILURE);
	 numthreads = atoi(optarg);
	 use_threads = 1;
	 break;
       case 's':
	 use_stdout = 1;
	 break;
       case 'o':
         if (rank == 0) {
           sprintf(outname,"%s",optarg);
         }
	 break;
       case 'w':
	 work_length = atoi(optarg);
	 break;
       case 'd':
	 target_seconds = atoi(optarg);
	 break;
       case 'n':
	 numsamples = atoi(optarg);
	 break;
       case 'h':
       default:
	 usage(argv[0]);
	 break;
       }
     }

  /* sanity check */
  if (numsamples > MAX_SAMPLES) {
    if (rank == 0) {
      fprintf(stderr,"WARNING: sample count exceeds maximum.\n");
      fprintf(stderr,"         setting count to maximum.\n");
    }
    numsamples = MAX_SAMPLES;
  }
  if (numsamples < MIN_SAMPLES) {
    if (rank == 0) {
      fprintf(stderr,"WARNING: sample count less than minimum.\n");
      fprintf(stderr,"         setting count to minimum.\n");
    }
    numsamples = MIN_SAMPLES;
  }

  /* allocate sample storage */
  samples = malloc(sizeof(unsigned long long)*numsamples*numthreads);
  assert(samples != NULL);

  if (work_length > MAX_LENGTH || work_length < MIN_LENGTH) {
    if (rank == 0) {
      fprintf(stderr,"WARNING: work length invalid. set to %d.\n", MAX_LENGTH);
    }
    work_length = MAX_LENGTH;
  }

  /* Record where everybody is running */
  char cpus[SHORT_STR_SIZE];
  get_cpu_affinity(cpus);

  /* sync procs up as best we can before starting the measurements */
  MPI_Barrier(MPI_COMM_WORLD);

  /* start sampling */
  unsigned long cycles;
  double seconds;
  fwq_core(0, &cycles, &seconds);

  /* sync procs up before we start writing to files */
  MPI_Barrier(MPI_COMM_WORLD);

  unsigned long* cycles_all =
    (unsigned long*) malloc(ranks * sizeof(unsigned long));
  double* seconds_all =
    (double*) malloc(ranks * sizeof(double));
  char* cpus_all =
    (char *) malloc(ranks * sizeof(char) * SHORT_STR_SIZE);

  MPI_Gather(&cycles,  1, MPI_UNSIGNED_LONG,
	     cycles_all,  1, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);
  MPI_Gather(&seconds, 1, MPI_DOUBLE,
	     seconds_all, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
  MPI_Gather(cpus, SHORT_STR_SIZE, MPI_CHAR,
	     cpus_all, SHORT_STR_SIZE, MPI_CHAR, 0, MPI_COMM_WORLD);

  /* TODO: need to be careful that numsamples fits within an int,
   * or need to modify write so each process writes directly to the file */
  MPI_Status status;
  if (rank == 0) {
    FILE* fp = stdout;
    if (! use_stdout) {
      sprintf(fname_times, "%s", outname);
      FILE* tmp_fp = fopen(fname_times, "w");
      if(tmp_fp == NULL) {
        fprintf(stderr, "Cannot create file");
        exit(EXIT_FAILURE);
      }
      fp = tmp_fp;
    }

    for (j = 0; j < ranks; j++) {
#if 0
      printf("Speed Process %d cycles %lld seconds %f GHz %f\n",
	     j, (long long)cycles_all[j], seconds_all[j],
	     ((double)cycles_all[j]) / (seconds_all[j] * 1.0e9));
#endif
      fprintf(fp, "Speed: process %d, cycles %lld, seconds %f, GHz %f\n",
	      j, (long long)cycles_all[j], seconds_all[j],
	      ((double)cycles_all[j]) / (seconds_all[j] * 1.0e9));
    }

    printf("Process 0\n"); fflush(stdout);

    fprintf(fp, "Process 0 running on CPUs %s\n", cpus_all);
    for (i = 0; i < numsamples; i++) {
      fprintf(fp, "%lld\n", samples[i]);
    }

    for (j = 1; j < ranks; j++) {
      printf("Process %d\n", j);  fflush(stdout);
      MPI_Send(&j, 1, MPI_INT, j, 0, MPI_COMM_WORLD);
      MPI_Recv(samples, numsamples, MPI_LONG_LONG_INT,
	       j, 0, MPI_COMM_WORLD, &status);
      fprintf(fp, "Process %d running on CPUs %s\n",
	      j, cpus_all+j*SHORT_STR_SIZE);
      for (i = 0; i < numsamples; i++) {
        fprintf(fp, "%lld\n", samples[i]);
      }
    }

    if (! use_stdout) {
      fclose(fp);
    }
  } else {
    MPI_Recv(&j, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
    MPI_Ssend(samples, numsamples, MPI_LONG_LONG_INT, 0, 0, MPI_COMM_WORLD);
  }

  free(cpus_all);
  free(seconds_all);
  free(cycles_all);
  free(samples);

  MPI_Finalize();

  exit(EXIT_SUCCESS);
}
