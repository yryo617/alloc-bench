#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include <getopt.h>


#include "debug.h"
#include "cmdline.h"

Command_Line g_cmdline; 
static char  g_logfilename[MAX_FILEPATH_LEN] = {0};


extern char *optarg; 
extern int optind, opterr, optopt; 

struct option g_long_options[] = {
  {"gen-rand-seq",    no_argument      , 0, 'g' },
  {"max-tree-depth",  required_argument, 0, 'm' },
  {"iteration",       required_argument, 0, 'i' },
  {"retention-ratio", required_argument, 0, 'r' },
  {"logfile",         required_argument, 0, 'l' },
  {"verbose",         optional_argument, 0, 'v' },
  {"help",            no_argument,       0, 'h' },
  {0,                 0,                 0,  0  }
};


/*!
 * Assumes the memory returned will be 
 * free'd() by caller
 */
char * genstring(struct option * c_opts, uint32_t len) 
{
  uint32_t idx, slen = 0; /* hold atleast the string terminator */
  char *optstr = NULL, *cursor; 
  struct option *optr;

  /*!
   * verify length != 0
   * Verify termination of long-option config structure
   */
  if (!len || c_opts == NULL || c_opts[len-1].name != NULL) 
    goto err_genstring; 

  /*!
   * Calculate additional bytes to hold the optional 
   * bytes
   */
  for (optr= ((idx=0),c_opts); optr->name ; idx++, optr++) { 
    if (optr->has_arg == required_argument) { 
      slen++; 
    } else if (optr->has_arg == optional_argument) { 
      slen += 2; 
    }
  }
  slen += len; 

  optstr = malloc(slen+1); 
  if (!optstr) { 
    uerror("cannot alloc(%u) bytes for option string", len+1); 
    optstr = NULL; 
    goto err_genstring; 
  }
  optstr[slen] = '\0';

  cursor = optstr;
  for (optr= ((idx=0),c_opts); optr->name ; idx++, optr++) { 
    cursor[idx] = optr->val;
    switch (optr->has_arg) 
    {
      case no_argument:
        break; 
      case optional_argument:
        cursor[++idx] = ':';  /* Intentional fall-through */
      case required_argument:
        cursor[++idx] = ':'; 
        break; 
      default:
        ucritical("Something went wrong... investigate");
	optstr = (free(optstr), NULL);
    }
  }

err_genstring: 
  return optstr; 
}

void usage(char *prog)
{
  fprintf(stdout, "Options for %s:\n", prog ? prog : "");
  fprintf(stdout, "====================================================================\n");
  fprintf(stdout, "-g, --gen-rand-seq.............generate random sequence of tree depths\n");
  fprintf(stdout, "-m, --max-tree-depth [n]...size(depth) of binary trees to create\n");
  fprintf(stdout, "-i, --iteration [n]........num of tree generation iterations\n");
  fprintf(stdout, "-r, --retention-ratio .....retention ratio of allocated trees\n");
  fprintf(stdout, "                           provided as bit shift. e.g. \n");
  fprintf(stdout, "                           -r 1 saves 1/2 the trees, -r 2 saves 1/4, -r 3 saves 1/8th trees\n");
  fprintf(stdout, "                           flag and level\n");
  fprintf(stdout, "-l, --logfile [filename]...log-file name or stdout/stderr\n");
  fprintf(stdout, "-v, --verbose [level]......debug, info, error, critical\n");
  fprintf(stdout, "                           default level error, no space between\n");
  fprintf(stdout, "                           flag and level\n");
  fprintf(stdout, "-h, --help.................Show this help message\n");
}

Command_Line * default_cmdline( Command_Line *cmd)
{
  if (cmd) {
    g_cmdline.gen_rand_sequence = false; 
    g_cmdline.logfile = DEFAULT_LOGFILE; 
    g_cmdline.logfilename = NULL;
    g_cmdline.max_tree_depth = DEFAULT_MAX_TREE_DEPTH; 
    g_cmdline.num_iter = DEFAULT_ITERATIONS; 
    g_cmdline.retention_ratio = DEFAULT_RETENTION; 
    g_cmdline.verbosity = DEFAULT_INITIAL_VERBOSITY;
  }

  return cmd;
}

int parse_opts(int argc, char *argv[], Command_Line *cmdline) 
{
  int idx, retval = 0; 
  int short_opt; 
  char *short_options, *nptr;

  short_options = genstring(g_long_options, sizeof(g_long_options)/ sizeof(struct option)); 
  uinfo("short option strings = %s", short_options);

  while ((short_opt = getopt_long(argc, argv, 
                         short_options, g_long_options, NULL)) != -1) {
    switch (short_opt) { 
      case '?': 
        usage(argv[0]); 
        retval = 0;  /* intentional fall through */
        goto err_parse_opts; 
      case 'h': 
        usage(argv[0]); 
        retval = -1;  /* intentional fall through */
        goto err_parse_opts; 
      case 'v': 
        if (optarg) { 
          udebug("Option -%c recevied argument --> %s, optind = %u", short_opt, optarg, optind); 
          if (!strncmp(optarg, "debug", 5)) { 
            cmdline->verbosity = Verbosity_Debug;
          } else if (!strncmp(optarg, "info", 4)) { 
            cmdline->verbosity = Verbosity_Info;
          } else if (!strncmp(optarg, "error", 5)) { 
            cmdline->verbosity = Verbosity_Error;
          } else if (!strncmp(optarg, "critical", 8)) { 
            cmdline->verbosity = Verbosity_Critical;
          } else { 
            retval = -1;
            usage(argv[0]); 
            goto err_parse_opts; 
          }
        } else {
          udebug("Option -%c recevied with no argument, optind = %u", short_opt, optind);
          cmdline->verbosity = DEFAULT_VERBOSITY;
        }
        break;
      case 'm': 
	cmdline->max_tree_depth = strtoul(optarg, &nptr, 0);
	if (*nptr != '\0') { 
	  uerror("Error parsing tree depth - arg = %s", optarg);
          retval = -1;
          goto err_parse_opts; 
	}
        udebug("Option -m parsed argument --> %lu (0x%08lx)", cmdline->max_tree_depth, cmdline->max_tree_depth); 
        break;
      case 'i': 
	cmdline->num_iter = strtoul(optarg, &nptr, 0);
	if (*nptr != '\0') { 
	  uerror("Error parsing num of iterations - arg = %s", optarg);
          retval = -1;
          goto err_parse_opts; 
	}
        udebug("Option -i parsed argument --> %lu (0x%08lx)", cmdline->num_iter, cmdline->num_iter); 
	if (cmdline->num_iter > MAX_ITERATIONS) { 
	  uerror("Number of iterations should be less than %u", MAX_ITERATIONS);
          retval = -1;
          goto err_parse_opts; 
	}
        break;
      case 'l': 
        uinfo("Option -l recevied with argument -> [%s]", optarg);
	if (strlen(optarg) >= sizeof(g_logfilename)) { 
	  uerror("Max size of filepath = %lu", sizeof(g_logfilename)-1);
          retval = -1;
          goto err_parse_opts; 
	} 

	cmdline->logfilename = g_logfilename;
	cmdline->logfilename[sizeof(g_logfilename)-1] = '\0';
	strncpy(cmdline->logfilename, optarg, sizeof(g_logfilename)-1);
	if (!strncmp(optarg, "stdout", 6)) { 
	  cmdline->logfile = stdout;
	} else if (!strncmp(optarg, "stderr", 6)) { 
	  cmdline->logfile = stderr;
	} else { 
	  /* Open the file descriptor in 'w' mode */
	  cmdline->logfile = fopen( cmdline->logfilename, "w");
	  if (!cmdline->logfile) {
	    uerror("Could not open custom log-file [%s]. error = %d" 
	               , cmdline->logfilename, -errno);
            retval = -1;
            goto err_parse_opts; 
	  }
	} 
        break;
      case 'r': 
        uinfo("Option -r recevied with argument -> [%s]", optarg);
	cmdline->retention_ratio = strtoul(optarg, &nptr, 0);
	if (*nptr != '\0') { 
	  uerror("Error parsing retention ratio - arg = %s", optarg);
          retval = -1;
          goto err_parse_opts; 
	}
        break;
      case 'g': 
        uinfo("Option -g found. Setting sequence");
	cmdline->gen_rand_sequence = true;
        break;
      default: 
        uinfo("Something went wrong... investigate - received char - %c %u", short_opt, short_opt);
        retval = -1;
        goto err_parse_opts; 
    }
  }

  for (idx=optind; idx < argc; idx++ ) { 
    uinfo("non-option argument @ [%u] -> [%s]", idx, argv[idx]);
  }

  /* ensure that retention ratio is power of 2 */
  if (!(cmdline->num_iter >> cmdline->retention_ratio)) {
    uinfo("Warning: No allocated trees are explicitly being retained. -i %lu, -r %lu",
                     cmdline->num_iter, cmdline->retention_ratio);
  }
    

err_parse_opts: 
  if (short_options) 
    free(short_options);
  return retval;
}

void free_global_config(Command_Line *cmdline) 
{
  if (!cmdline) 
    return;

  if (cmdline->logfilename
      && strncmp(cmdline->logfilename, "stdout", 6) 
      && strncmp(cmdline->logfilename, "stderr", 6)) { 
    cmdline->logfile = cmdline->logfile ? fclose(cmdline->logfile), NULL : NULL;
    cmdline->logfilename[0] = '\0';
  }
}
