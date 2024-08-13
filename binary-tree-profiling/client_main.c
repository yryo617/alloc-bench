#include <stdio.h>
#include <stdbool.h>
#include <getopt.h>

#include "debug.h"
#include "cmdline.h"
#include "tree_bench.h"

extern Command_Line g_cmdline;

#ifdef BENCHLIB
int bench_main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif  
{
  int retval = 0;
  retval = parse_opts(argc, argv, default_cmdline(&g_cmdline)); 
  if (retval) { 
    uerror("Parsing options returned err: %d", retval);
    goto err_main;
  }

  /* Print out the rest of the non-option arguments */
  for (int idx=optind; idx < argc; idx++ ) { 
    uerror("Main :: non-option argument @ [%u] -> [%s]", idx, argv[idx]);
  }

  if (g_cmdline.gen_rand_sequence) { 
    unsigned int *seq_list = generate_rand_sequence(&g_cmdline);
    if (!seq_list) { 
      uerror("Error generating random sequence");
      retval = -1;
      goto err_main;
    }

    retval = save_sequence(seq_list, &g_cmdline);
    if (retval) { 
      uerror("Error saving generated random sequence to output stream [%d]", retval);
      goto err_main;
    }
  } else {
    retval = binary_tree_benchmark(&g_cmdline); 
  }

err_main: 
  free_global_config(&g_cmdline);
  return retval; 
}
