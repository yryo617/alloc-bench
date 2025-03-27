#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef BDWGC 
# include "gc.h"
# include "gc/gc_mark.h"
# define malloc(x) GC_MALLOC((x))
# ifdef IGNOREFREE
#  define free(x) {};
# else
#  define free(x) GC_FREE((x));
# endif
#else 
# include <stdlib.h>
#endif

#if defined(BM_LOGFILE)
# include "bench_harness.h"
#endif


#define INIT_SEED 42
#define TOTAL_ITERATIONS (32 * 1024)

#if defined(BDWGC)
unsigned int GC_count = 0;
static void signal_gc()
{
  GC_count++;
}
#endif

#if !defined(BENCHLIB)
int main(int argc , char *argv[])
#else 
int bench_main(int argc , char *argv[])
#endif 
{
  int i, j;

  srand(INIT_SEED);
  #ifdef BDWGC
  GC_INIT();
  if (!GC_get_start_callback()) {
     GC_set_start_callback(signal_gc);
     GC_start_performance_measurement();
  } else {
     printf("[%s:%u] | GC-notify callback already set\n", __FUNCTION__, __LINE__);
  }
  #endif

  for (i = 0; i < TOTAL_ITERATIONS; ++i)
  {
    unsigned int random_size = rand() % 1024;
    size_t sz = random_size * sizeof(int *);
    int **p = (int **) malloc(sz);
    assert(*p == 0);
    #ifdef BDWGC 
    if (i % 1024 == 0)
      printf("Heap size = %lu bytes\n", (unsigned long) GC_get_heap_size());
    #endif
    free(p);
  }

  #if defined(BM_LOGFILE)
  #if defined(BDWGC)
  printf("[%s:%d] | number of gc- cycles complete = %u, total-gc-time = %u\n",
                  __FUNCTION__, __LINE__, GC_count, GC_get_full_gc_total_time());
  BM_Harness data = { .bm = "random_mixed_alloc",
                      .gc_cycles = GC_count,
                      .gc_time_ms = GC_get_full_gc_total_time()};
  #else
  BM_Harness data = { .bm = "random_mixed_alloc",
                      .gc_cycles = 0,
                      .gc_time_ms = 0};
  #endif
  bmlog(&data);
  #endif

  return 0;
}
