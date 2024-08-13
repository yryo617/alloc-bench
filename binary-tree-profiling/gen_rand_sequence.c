#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "debug.h"
#include "cmdline.h"

extern Command_Line g_cmdline;

unsigned int * generate_rand_sequence(Command_Line *opt)
{
  unsigned int idx; 
  unsigned int *ptr, *rand_vec = NULL; 

  if (!opt || !opt->logfile) {
    uerror("Incorrect options used to generate random-sequence. opt = %p", opt);
    goto err_gen_rand;
  }

  rand_vec = malloc(opt->num_iter * sizeof(unsigned int)); 
  if (!rand_vec) {
    uerror("Incorrect options used to generate random-sequence");
    goto err_gen_rand;
  }

  for(ptr = rand_vec + (opt->num_iter - 1); ptr >= rand_vec; ptr-- )
    *ptr = rand() % opt->max_tree_depth; 

  return rand_vec; 

err_gen_rand:
  return (rand_vec ? free(rand_vec), NULL : NULL);
}


int save_sequence(unsigned int *arr, Command_Line *opt)
{
  int idx = 0, retval = 0;
  if (!arr) {
    uerror("NULL destination array to store random sequence");
    retval = -1;
    goto err_seq_save;
  }

  if (!opt || !opt->logfile) { 
    uerror("options for saving random sequence invalid -- [%p]", opt);
    retval = -2;
    goto err_seq_save;
  }

  fprintf(g_cmdline.logfile, "\ng_treedepths[MAX_ITERATIONS] = {");
  for(idx=0; idx < opt->num_iter; idx++) {
    if (idx % 16 == 0) 
      fprintf(g_cmdline.logfile, "\n  ");
    fprintf(g_cmdline.logfile, "%2d%s", arr[idx], idx == (MAX_ITERATIONS - 1) ? "" : ",");
  }

  for(idx=opt->num_iter; idx < MAX_ITERATIONS; idx++) {
    if (idx % 16 == 0) 
      fprintf(g_cmdline.logfile, "\n  ");
    fprintf(g_cmdline.logfile, "0%s", idx == (MAX_ITERATIONS - 1) ? "" : ",");
  }
  fprintf(g_cmdline.logfile, "\n};\n");

err_seq_save:
  return retval;
}
