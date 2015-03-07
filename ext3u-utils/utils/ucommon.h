/* -------------------------------------------------*
 * Copyright 2009									*
 * Authors: Antonio Davoli - Vasile Claudiu Perta 	*
 *													*
 * Common header file for undelete suite tool		*
 * -------------------------------------------------*/

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<sys/ioctl.h>
#include<fcntl.h>
#include<errno.h>
#include<time.h>
#include<getopt.h>
#include<pwd.h>
#include<grp.h>

#include "undel.h"

#define EXT3u_NAME "ext3u"					/* Name of EXT3u module		*/
#define MOUNTS_PROC_ENTRY "/proc/mounts"	/* Path of mounts on /proc	*/
#define FS_ROOT_LEN 2

#define BITMASK_PERMISSIONS_SIZE 12			/* Permission Bitmask Size	*/
#define IOCTL_BUFFER_SIZE 1024				/* Buffer size for ioctl	*/

#define LIST_FIFO_ORDER 0 
#define LIST_BUCKET_ORDER 1

#define ULS_NORMAL_ENTRY 1					/* Enable uls long entry	*/
#define ULS_SHORT_ENTRY 0					/* Enable uls short entry	*/

#define ULS_ERROR -1
#define ULS_OK 0

#define URM_ERR -1 
#define URM_OK 0

#define USTATS_ERR -1
#define USTATS_OK 0

/* To Do: Future work
#define UCONFIG_ERROR -1
#define UCONFIG_OK 0
*/

#define BUF_SIZE 1024
#define MAX_PATH 4096

/* ucommon.h prototypes */

void ext3u_free_mount_points(char **mnt_points, int mnt_count);
int ext3u_check_mount_point(char * mnt_point);
char ** ext3u_search_mount_points(int * mnt_count);

/* uls.c prototypes */

void ext3u_uls_command(char *mnt_point, int num_files);


