#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef BDWGC 
# include "gc.h"
# include "gc/gc_mark.h"
#endif

#if defined(BM_LOGFILE)
# include "bench_harness.h"
#endif

#if defined(BDWGC)
unsigned int GC_count = 0;
static void signal_gc()
{
  GC_count++;
}
#endif



#define TOTAL_ITERATIONS (8 * 1024 * 1024)

#if !defined(BENCHLIB)
int main(int argc , char *argv[])
#else
int bench_main(int argc , char *argv[])
#endif
{
  int i;
  int iter_total = TOTAL_ITERATIONS;

  #ifdef BDWGC
  GC_INIT();
  if (!GC_get_start_callback()) {
     GC_set_start_callback(signal_gc);
     GC_start_performance_measurement();
  } else {
     printf("[%s:%u] | GC-notify callback already set\n", __FUNCTION__, __LINE__);
  }


  for (i = 0; i < iter_total; ++i)
  {
    int **p = (int **) GC_MALLOC(sizeof(int *));
    int *q = (int *) GC_MALLOC_ATOMIC(sizeof(int));
    assert(*p == 0);
    *p = (int *) GC_REALLOC(q, 4 * sizeof(int));
    if (i % 1024 == 0)
      printf("Heap size = %lu bytes\n", (unsigned long) GC_get_heap_size());
  }
  #else 
  printf("Test skipped! Not compiled with GC\n");
  #endif

  #if defined(BM_LOGFILE)
  #if defined(BDWGC)
  printf("[%s:%d] | number of gc- cycles complete = %u, total-gc-time = %u\n",
                  __FUNCTION__, __LINE__, GC_count, GC_get_full_gc_total_time());
  BM_Harness data = { .bm = "small_fixed_alloc",
                      .gc_cycles = GC_count,
                      .gc_time_ms = GC_get_full_gc_total_time()};
  #else
  BM_Harness data = { .bm = "small_fixed_alloc",
                      .gc_cycles = 0,
                      .gc_time_ms = 0};
  #endif
  bmlog(&data);
  #endif

  return 0;
}
