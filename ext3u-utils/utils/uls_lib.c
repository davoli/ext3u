/* -----------------------------------------------------------------------------
 * Copyright 2009																*
 * Authors: Antonio Davoli - Vasile Claudiu Perta								*
 *																				*
 *																				*
 * uls: undelete ls utils functions												*
 * Functions for doing ls on deleted and retrievables files (ls command style)	*
 * -----------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------*
 * NOTE: If there is only one partition mounted with ext3u file system, the		*
 * command will uses automatically that. Otherwise ask to user to select one 	*
 * (or more) partition of the total availables or use -a option just for select *
 * a specific one.																*
 *------------------------------------------------------------------------------*/

#include "ucommon.h"

extern int long_listing;
extern int verbose;

int list_order = LIST_FIFO_ORDER;

/**
 * @brief These two functions (ftypelet, strmode) 
 * are used for analyse permissions' bitmask.
 */

static char ftypelet(umode_t bits)
{
	/* These are the most common, so test for them first.  */
	if (S_ISREG (bits))
		return '-';
	if (S_ISDIR (bits))
		return 'd';
	/* Other letters standardized by POSIX 1003.1-2004.  */
	if (S_ISBLK (bits))
		return 'b';
	if (S_ISCHR (bits))
		return 'c';
	if (S_ISLNK (bits))
		return 'l';
	if (S_ISFIFO (bits))
		return 'p';
	/* Other file types (though not letters) standardized by POSIX.  */
	if (S_ISSOCK (bits))
		return 's';
	return '?';
}

static void strmode(umode_t mode, char *str)
{
	str[0] = ftypelet(mode);
	str[1] = mode & S_IRUSR ? 'r' : '-';
	str[2] = mode & S_IWUSR ? 'w' : '-';
	str[3] = (mode & S_ISUID
			  ? (mode & S_IXUSR ? 's' : 'S')
			  : (mode & S_IXUSR ? 'x' : '-'));
	str[4] = mode & S_IRGRP ? 'r' : '-';
	str[5] = mode & S_IWGRP ? 'w' : '-';
	str[6] = (mode & S_ISGID
			  ? (mode & S_IXGRP ? 's' : 'S')
			  : (mode & S_IXGRP ? 'x' : '-'));
	str[7] = mode & S_IROTH ? 'r' : '-';
	str[8] = mode & S_IWOTH ? 'w' : '-';
	str[9] = (mode & S_ISVTX
			  ? (mode & S_IXOTH ? 't' : 'T')
			  : (mode & S_IXOTH ? 'x' : '-'));
	str[10] = ' ';
	str[11] = '\0';
}

/**
 * @brief Function ls-like for printing on stdout all 
 * uls_entry's information (Used by -l option).
 *
 * @param uls_entry Entry relative to file information.
 */

static void print_uls_entry(struct ext3u_uls_entry * uls_entry) 
{
	char permissions[BITMASK_PERMISSIONS_SIZE];
	struct passwd *pwd_entry;
	struct group *grp_entry;
 	struct tm * time;
	
	/* Permissions */
	strmode(uls_entry->u_mode, permissions); 
	
	/* Time */
	time = gmtime(&uls_entry->u_mtime.tv_sec);
	
	/* User ID */
	pwd_entry =	getpwuid(uls_entry->u_uid);
	if ( !pwd_entry ) {
		fprintf(stderr, "Error on retrieving inode user ID\n");
		return;
	}

	/* Group ID */
	grp_entry = getgrgid(uls_entry->u_gid);
	if ( !grp_entry ) {
		fprintf(stderr, "Error on retrieving inode group ID\n");
		return;
	}
	
	/* print entry information */
	printf("%s %u %s %s %u %d-", permissions, uls_entry->u_nlink, pwd_entry->pw_name, grp_entry->gr_name, uls_entry->u_size, 1900+time->tm_year);
	
	/* struct tm maintain in the range [0,X], so for printing in ls-like mode we need to a '0' in case of number minor than 10 */
	
	if ( time->tm_mon < 10 )
		printf("0");
	printf("%d-", time->tm_mon);
	
	if ( time->tm_mday < 10 )
		printf("0");
	printf("%d ", time->tm_mday);
	
	if ( time->tm_hour < 10 )
		printf("0");
	printf("%d:", time->tm_hour);
	
	if ( time->tm_sec < 10 )
		printf("0");
	printf("%d ", time->tm_sec);
	
	return;
}

/**
 * @brief Manage a buffer filled with uls_entries, these can be in short or long format. 
 *
 * @param mnt_point Working mount point.
 * @param buffer Buffer with information.
 * @param files_number Number of entries on the buffer.
 * @param mode Enable long listing mode.
 *
 * @return On success it returns zero. otherwise an error.
 */

static int uls_info_dispatcher(char * mnt_point, char *buffer, int files_number, int mode) 
{
	int i, offset = 0; 
	unsigned int path_length;
	char * path;
	
	struct ext3u_uls_entry uls_entry;				/* Structure for long format */
	struct ext3u_uls_short_entry uls_short_entry;	/* Structure for short format */

	for (i = 0; i < files_number; i++) {
		
		memset(&uls_short_entry, 0, sizeof(uls_short_entry));
		memset(&uls_entry, 0, sizeof(uls_entry));

		/* Long Entry */
		if ( mode == ULS_NORMAL_ENTRY ) { 
			memcpy(&uls_entry, buffer+offset, sizeof(struct ext3u_uls_entry));
			offset += sizeof(struct ext3u_uls_entry);
			path_length = uls_entry.u_path_length;
		}
		else { /* Short Entry */
			memcpy(&uls_short_entry, buffer+offset, sizeof(struct ext3u_uls_short_entry));
			offset += sizeof(struct ext3u_uls_short_entry);
			path_length = uls_short_entry.u_path_length;
		}
	
		path = (buffer + offset);
		offset += (path_length+1);
		/* Create Path String */
		
		/*--------------------------------------------------
		* if ( ( path = malloc(path_length+1)) == NULL ) { / * +1 is for \0 * /
		* 	fprintf(stderr,"[ext3u_uls_info_dispacther]: Error on malloc (Path creation)\n");
		* 	return ULS_ERROR;
		* }
		* 
		*--------------------------------------------------*/

		/*--------------------------------------------------
		* strncpy(path, buffer+offset, path_length);
		*--------------------------------------------------*/
		
		/* If the entries are in short format the function will print only the path, 
			otherwise will print all information about each entry. */
		
		if ( mode == ULS_NORMAL_ENTRY ) 
			print_uls_entry(&uls_entry);
			
		printf("%s%s\n", mnt_point, path);
	
		/*--------------------------------------------------
		* / * Free path * /
		* free(path);
		*--------------------------------------------------*/
	}
	
	return ULS_OK;
}

/**
 * @brief Call 'uls' command on a specific mount point.
 * @param mnt_point Mount point name,
 * @param num_file Specific the maximum number of files (oldest) to view.
 */

void ext3u_uls_command(char *mnt_point, int num_files) 
{
	int fd;								/* File descriptor */
	int ioctl_ret;						/* ioctl return code */
	struct ext3u_uls_info uls_info;		/* Structure for kernel communication */

	/* Clean structure */
	memset(&uls_info, 0, sizeof(uls_info));

	/* Open mount point (for ioctl system call) */
	if ( ( fd = open(mnt_point, O_RDONLY) ) < 0 ) {
		fprintf(stderr, "[ext3u_uls_command]: Erron on opening mount point\n");
		return;
	}
	
	/* Create buffer for communication */
	if ( ( uls_info.u_buffer = malloc(IOCTL_BUFFER_SIZE) ) == NULL ) {
		fprintf(stderr, "[ext3u_uls_command]: Erron on malloc sys_call\n");
		close(fd);
		return;
	}
	
	/* Fill structure for exchange information with ioctl */
	uls_info.u_buffer_length = IOCTL_BUFFER_SIZE;
	uls_info.u_ll = long_listing;
	uls_info.u_order = list_order;
	uls_info.u_next_record.r_block = 0;
	uls_info.u_next_record.r_offset = 0;
	uls_info.u_max_files = num_files;
	uls_info.u_read_files = 0;
	uls_info.u_files = 0;
	
	/* ioctl() loop */
	do {
		
		ioctl_ret = ioctl(fd, EXT3_UNDEL_IOC_ULS, &uls_info);
	
		if ( ioctl_ret == 0 ) {
			
			/* Internal (ext3u) error */
			if ( uls_info.u_errcode != 0 ) {
				
				/* Not entry found */
				if ( uls_info.u_errcode == -ENOENT )	
					fprintf(stderr, "uls: Entry not found.\n");
			
				/* Memory buffer not sufficient. Need to allocate a bigger one */
				if ( uls_info.u_errcode == -ENOMEM ) {
					/* free old buffer */
					free(uls_info.u_buffer);
				
					/* New buffer allocation */
					if ( ( uls_info.u_buffer = malloc(uls_info.u_buffer_length) ) == NULL ) {
						fprintf(stderr, "uls: Error on malloc().\n");
						close(fd);
						return;
					}
				}
			}
			else 
				uls_info_dispatcher(mnt_point, uls_info.u_buffer, uls_info.u_files, uls_info.u_ll);
		}
		else {
			if (errno == EOPNOTSUPP)
				fprintf(stderr, "uls: undelete support not found on '%s'!\n", mnt_point);	
			else
				fprintf(stderr, "uls: ioctl() error: %s\n", strerror(errno));
		}
		
		/* Clean Buffer */
		memset(uls_info.u_buffer, 0, uls_info.u_buffer_length);
		
		/* if num_files is greater than 0, it's a version with  * 
		 * a limited number of files to read.					*/
		
		if ( uls_info.u_max_files > 0 ) {
			
			/* Update number of files read */
			//uls_info.u_read_files += uls_info.u_files;
			
			/* Force exit if number of files to read is reached, *
			 * otherwise increment files' counter still to read. */
			
			if ( uls_info.u_read_files >= uls_info.u_max_files ) 
				uls_info.u_next_record.r_block = 0;
		
		}
	
	} while ( uls_info.u_next_record.r_block != 0 );

	/* Free buffer */
	free(uls_info.u_buffer);
	close(fd);
}

