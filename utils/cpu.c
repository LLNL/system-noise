/***********************************************************
 * Edgar A. Leon
 * Lawrence Livermore National Laboratory
 ***********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

/* __USE_GNU is needed for CPU_ISSET definition */
#ifndef __USE_GNU
#define __USE_GNU 1
#endif
#include <sched.h>            // sched_getaffinity

/*
 * The most direct equivalent to MPI_Wtime()
 * using POSIX clock_gettime()
 */
double get_time()
{
  struct timespec ts;
  /* CLOCK_MONOTONIC: Best for measuring intervals
     (unaffected by system clock changes) */
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

/*
 * Convert a non-negative array of ints to a range
 */
int int2range(int *intarr, int size, char *range)
{
  int i, curr;
  int nc = 0;
  int start = -1;
  int prev = -2;

  for (i=0; i<size; i++) {
    curr = intarr[i];
    if (curr != prev+1) {
      /* Record end of range */
      if (start != prev && prev >= 0)
	nc += sprintf(range+nc, "-%d", prev);

      /* Record start of range */
      if (prev >= 0)
	nc += sprintf(range+nc, ",");
      nc += sprintf(range+nc, "%d", curr);
      start = curr;
    } else
      /* The last int is end of range */
      if (i == size-1)
	nc += sprintf(range+nc, "-%d", curr);

    prev = curr;
  }

  return nc;
}

/*
 * Take a string containing comma-separated integers and ranges
 * (like "2,4-8") and return an array containing all the integers
 * in that range.
 *
 * Resulting array needs to be freed by the caller.
 */
int* range2int(const char *range, int *arr_size)
{
  if (!range)
    return NULL;

  // Make a copy of the input string since strtok modifies it
  char* str_copy = malloc(strlen(range) + 1);
  strcpy(str_copy, range);

  // First pass: count total numbers to allocate array
  int count = 0;
  char* temp_copy = malloc(strlen(range) + 1);
  strcpy(temp_copy, range);

  char* token = strtok(temp_copy, ",");
  int start, end;
  while (token) {
    if (strchr(token, '-')) {
      // Range format like "4-8"
      sscanf(token, "%d-%d", &start, &end);
      count += (end - start + 1);
    } else {
      // Single number
      count++;
    }
    token = strtok(NULL, ",");
  }
  free(temp_copy);

  // Allocate array
  int *res = malloc(count * sizeof(int));
  *arr_size = count;

  // Second pass: fill the array
  int i, index=0;
  token = strtok(str_copy, ",");
  while (token) {
    if (strchr(token, '-')) {
      // Range format like "4-8"
      sscanf(token, "%d-%d", &start, &end);
      for (i = start; i <= end; i++) {
	res[index++] = i;
      }
    } else {
      // Single number
      res[index++] = atoi(token);
    }
    token = strtok(NULL, ",");
  }

  free(str_copy);

  return res;
}

/*
 * Get number of processing units (cores or hwthreads)
 */
static
int get_total_num_pus()
{
  int pus = sysconf(_SC_NPROCESSORS_ONLN);

  if ( pus < 0 )
    perror("sysconf");

  return pus;
}

/*
 * Get the affinity.
 */
static
int get_affinity(int *cpus, int *count)
{
  int i;
  cpu_set_t resmask;

  CPU_ZERO(&resmask);

  int rc = sched_getaffinity(0, sizeof(resmask), &resmask);
  if ( rc < 0 ) {
    perror("sched_getaffinity");
    return rc;
  }

  *count = 0;
  int pus = get_total_num_pus();
  for (i=0; i<pus; i++)
    if ( CPU_ISSET(i, &resmask) ) {
      cpus[*count] = i;
      (*count)++;
    }

  return 0;
}

/*
 * Get the number of CPUs where this worker can run.
 */
int get_num_cpus()
{
  cpu_set_t mask;

  CPU_ZERO(&mask);

  int rc = sched_getaffinity(0, sizeof(mask), &mask);
  if ( rc < 0 ) {
    perror("sched_getaffinity");
    return rc;
  }

  return CPU_COUNT(&mask);
}

/*
 * Print my affinity into a buffer.
 */
int get_cpu_affinity(char *outbuf)
{
  int count;
  int nc = 0;

  int *cpus = malloc(sizeof(int) * get_total_num_pus());
  get_affinity(cpus, &count);

#if 1
  nc += int2range(cpus, count, outbuf+nc);
  //printf("nc=%d count=%d\n", nc, count);
#else
  int i;
  for (i=0; i<count; i++) {
    nc += sprintf(outbuf+nc, "%d ", cpus[i]);
  }
#endif
  //nc += sprintf(outbuf+nc, "\n");

  free(cpus);

  return nc;
}
