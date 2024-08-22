#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "bench_harness.h"

int bmlog(BM_Harness *bmdata)
{
  FILE *fp ;
  int err = 0;
  pid_t _pid, _ppid;

  fp = fopen(stringify_macro(BM_LOGFILE), "w");
  if (!fp) {
    error_log("Could not open <%s>. errno = %d", stringify_macro(BM_LOGFILE) , -errno);
    err = -1;
    goto err_bmlog;
  }

  fprintf(fp, "{\n");
  fprintf(fp, "  \"%s\" : \"%s\" ,\n", "bm", bmdata->bm );
  fprintf(fp, "  \"%s\" : %u ,\n", "gc_cycles", bmdata->gc_cycles );
  fprintf(fp, "  \"%s\" : %u\n", "gc_marktime_ms", bmdata->gc_marktime_ms );
  fprintf(fp, "  \"%s\" : %u\n", "gc_time_ms", bmdata->gc_time_ms );
  fprintf(fp, "  \"%s\" : %f\n", "retention-ratio", bmdata->retention_ratio );
  fprintf(fp, "}\n");

  fclose(fp);

  /* print pids for pmcstat debugging */
  error_log("exec-pid = %u, parent-pid = %u", getpid(), getppid());
  
err_bmlog: 
  return err;
}



