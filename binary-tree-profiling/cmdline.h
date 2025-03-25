typedef struct Command_Line {
  char * logfilename; 
  FILE * logfile; 
  bool gen_rand_sequence; 
  unsigned long max_tree_depth;
  unsigned long num_iter; 
  unsigned long retention_ratio; 
  Verbosity verbosity; 
} Command_Line; 


#define udebug(fmt, args...) do { \
                             if ( g_cmdline.verbosity <= Verbosity_Debug ) {  \
			       fprintf( g_cmdline.logfile,  "[%s:%d]| "fmt"\n", __FUNCTION__, __LINE__, ##args); \
			     }  \
                           } while(0) 

#define uinfo(fmt, args...) do { \
                             if ( g_cmdline.verbosity <= Verbosity_Info ) {  \
			       fprintf( g_cmdline.logfile,  "[%s:%d]| "fmt"\n", __FUNCTION__, __LINE__, ##args); \
			     }  \
                           } while(0) 

#define uerror(fmt, args...) do { \
                             if ( g_cmdline.verbosity <= Verbosity_Error ) {  \
			       fprintf( g_cmdline.logfile,  "[%s:%d]| "fmt"\n", __FUNCTION__, __LINE__, ##args); \
			     }  \
                           } while(0) 

#define ucritical(fmt, args...) do { \
                             if ( g_cmdline.verbosity <= Verbosity_Critical ) {  \
			       fprintf( g_cmdline.logfile,  "[%s:%d]| "fmt"\n", __FUNCTION__, __LINE__, ##args); \
			     }  \
                           } while(0) 



#define DEFAULT_LOGFILE stdout
#define DEFAULT_MAX_TREE_DEPTH 16
#define DEFAULT_ITERATIONS 2048

#ifndef MAX_ITERATIONS
# define MAX_ITERATIONS 4096
#endif 

/* Retention factor for roots stored as a bit-shift */
#define DEFAULT_RETENTION 1
#define MAX_RETENTION_RATIO 1024

#define DEFAULT_INITIAL_VERBOSITY ((Verbosity)Verbosity_Debug)
#define DEFAULT_VERBOSITY ((Verbosity)Verbosity_Info)
#define MAX_FILEPATH_LEN 4096


int parse_opts(int , char **, Command_Line *);
Command_Line * default_cmdline( Command_Line *); 
void free_global_config(Command_Line *);  
