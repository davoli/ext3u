#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<sys/ioctl.h>
#include<linux/fs.h>
#include<linux/ext3_fs.h>
#include"/home/double/workspace/sistemi3/ext3undel/undel.h"
#include<fcntl.h>
#include<errno.h>
#include<getopt.h>
#include<ctype.h>

#define UCONFIG_LIST 001

#define UCONFIG_INSERT 002
#define UCONFIG_REMOVE 004
#define UCONFIG_EXT 010
#define UCONFIG_SIZE 020

#define UCONFIG_INS_EXT ( UCONFIG_INSERT | UCONFIG_EXT ) 
#define UCONFIG_INS_SIZE ( UCONFIG_INSERT | UCONFIG_SIZE )
#define UCONFIG_INS_BOTH ( UCONFIG_INSERT | UCONFIG_SIZE | UCONFIG_EXT )

#define UCONFIG_REM_EXT ( UCONFIG_REMOVE | UCONFIG_EXT ) 
#define UCONFIG_REM_SIZE ( UCONFIG_REMOVE | UCONFIG_SIZE )
#define UCONFIG_REM_BOTH ( UCONFIG_REMOVE | UCONFIG_SIZE | UCONFIG_EXT )


#define EXT3u_NAME "ext3undel" 
#define MOUNTS_PROC_ENTRY "/proc/mounts"
#define IOCTL_BUFFER_SIZE 1024

#define MAX_ENTRY_SIZE 192

const char* program_name;
int verbose = 0;

//#define PATH_MAX 4095

//typedef unsigned long long __u64

#define ULS_ERROR -1
#define ULS_OK 0

int check_entry(const char *cp)
{

  /* Cases:
   *     - presence of ',' 
   *     - presence of *.c ?    
   * ------------------------- */

  return 0;
}

int get_size(const char *cp, __u64 * size)
{
	__u64 value = 0;

	if (!isdigit(*cp))
		return -1;

	while (isdigit(*cp)) {
      value = value * 10 + *cp++ - '0';
    }

	switch (*cp++) {
    case 'G':
	case 'g':
		value *= 1073741824;
		break;
	case 'M':
	case 'm':
		value *= 1048576;
		break;
    case 'K':
	case 'k':
		value *= 1024;
		break;
	case 'b':
		value *= 512;
		break;
	case 'w':
		value *= 2;
		break;

	case '\0':
      *size = value;
      return 0;

	default:
		return -1;
	}

	if (*cp)
      return -1;
    
    *size = value;
	return 0;
}



void  ext3u_uconfig_command(char * mnt_point, char *dir_entry, char *ext_entry, __u64 maxsize, int mask) 
{
  struct ext3u_uconfig_info uconfig_info;

  printf("mask %d\n", mask);
  switch(mask)
    {
    case UCONFIG_LIST:
      printf("List\n");
      break;
    case UCONFIG_INS_EXT:
      printf("insert ext\n");
      break;
    case UCONFIG_INS_SIZE:
      printf("insert size\n");
      break;
    case UCONFIG_INS_BOTH:
      printf("insert size++\n");
      break;
    case UCONFIG_REM_EXT:
      printf("insert ext\n");
      break;
    case UCONFIG_REM_SIZE:
      printf("insert size\n");
      break;
    case UCONFIG_REM_BOTH:
      printf("insert size++\n");
      break;

    default:
      printf("default\n");
    }
  
  return;
}

/* --------------------- 
 * Print usage
 * --------------------- */

void print_usage(FILE* stream, int exit_code) 
{
  fprintf(stream, "Usage: uconfig\n");
  fprintf(stream, "\t Add/Remove extensions behaviors...\n");
  fprintf(stream, "\t -m mount point,\n");
  fprintf(stream, "\t -d directory,\n");
  fprintf(stream, "\t -e entry ( *.avi,*ciccio, *.*), \n");
  fprintf(stream, "\t -s size,\n");
  fprintf(stream, "\t -l listing,\n");
  fprintf(stream, "\t -i insert new,\n");
  fprintf(stream, "\t -r remove,\n");
  fprintf(stream, "\t -v verbose mode,\n");
  fprintf(stream, "\t -h Print this help.\n");
  exit(exit_code);
}

/* ------------------- * 
 * Main
 * ------------------- */
   

int main(int argc, char * argv[]) {

  char ** mnt_points;
  char mount_point[PATH_MAX] = {0};
  char dir_entry[PATH_MAX] = {0};
  char ext_entry[MAX_ENTRY_SIZE] = {0};
  int mount_point_inserted = 0;
  int dir_inserted = 0, entry_inserted = 0, size_inserted = 0;
  int remove = 0, list = 0;
  int mask = 0;
  int next_option, mnt_number;
  __u64 max_size = 0;
  struct ext3u_uconfig_info uconfig_info;

  const char* const short_options = "hvm:d:s:e:lir";

  const struct option long_options[] = {
    { "help",     0, NULL, 'h' },
    { "verbose",  0, NULL, 'v' },
    { "mount_point",  1, NULL, 'm' },
    { "ext",  1, NULL, 'e' },
    { "dir",  1, NULL, 'd' },
    { "size",  1, NULL, 's' },
    { "list",  0, NULL, 'l' },
    { "insert",  0, NULL, 'i' },
    { "remove",  0, NULL, 'r' },
    { NULL,       0, NULL, 0   }
  };

  const char* output_filename = NULL;
  
  program_name = argv[0];

  do {
    next_option = getopt_long (argc, argv, short_options, long_options, NULL);

    switch (next_option)
      {
      case 'm':
        mount_point_inserted = 1;
        strncpy(mount_point, optarg, strlen(optarg));
        break;
      case 'd':
        dir_inserted = 1;
        strncpy(dir_entry, optarg, strlen(optarg));
        break;
      case 's':
        size_inserted = 1;
        if ( get_size(optarg, &max_size) != 0 ) {
          fprintf(stderr,"Error on size inserted\n");
          exit(EXIT_FAILURE);
        }
        mask = mask | UCONFIG_SIZE;
        break;
      case 'e':
        entry_inserted = 1;
        
        strncpy(ext_entry, optarg, strlen(optarg));
        printf("ext3->%s\n", ext_entry);
        
        if ( check_entry(ext_entry) != 0 ) {
          fprintf(stderr,"Error on size inserted\n");
          exit(EXIT_FAILURE);
        }
        mask = mask | UCONFIG_EXT;
        break;        
      case 'i':
        mask = mask | UCONFIG_INSERT;
        break;
      case 'r':
        mask = mask | UCONFIG_REMOVE;
        remove = 1;
        break;
      case 'l': /* Only list */
        mask = UCONFIG_LIST;
        list = 0;
        break;
      case 'h':
        print_usage (stdout, 0);
        break;
      case 'v':
        verbose = 1;
        break;
      case '?':
        print_usage (stderr, 1);
      case -1:
        break;
      default:
        exit(1);
      }
  }
  while (next_option != -1);

  /* Check Mount point */

  if ( mount_point_inserted ) {
    if ( ext3u_check_mount_point(mount_point) != 0 ) {
      fprintf(stderr, "Not valid mount point\n");
      exit(-1);
    }
  } 
  else {
    
    if ( ( mnt_points = ext3u_search_mount_points(&mnt_number) ) == NULL ) {
      fprintf(stderr, "Not valid mount point\n");
      exit(-1);
    }
    
    if ( mnt_number > 1 ) {
      fprintf(stderr, "There are more mounts: Please select one");
      ext3u_free_mount_points(mnt_points, mnt_number);
      exit(-1);
    }
    else     /* There is an only mount point */
      strncpy(mount_point, mnt_points[0], strlen(mnt_points[0]));
  }

  /* Other arguments */

  if ( mask != UCONFIG_LIST ) 
      if ( ! ( dir_inserted && ( entry_inserted || size_inserted ) ) ) {
        fprintf(stderr,"Error on arguments inserted. Please check with -h\n");
        exit(EXIT_FAILURE);
      }
  
  ext3u_uconfig_command(mount_point, dir_entry, ext_entry, max_size, mask); 
  
  return 0;
}
