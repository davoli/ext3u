/* -------------------------------------------------------------*
 * Copyright 2009												*
 * Authors: Antonio Davoli - Vasile Claudiu Perta				*
 *																*
 *																*
 * urm: undelete rm												*
 * Recover a deleted file.										*
 * -------------------------------------------------------------*/

/*------------------------------------------------------------------------------*
 * NOTE: If there is only one partition mounted with ext3u file system, the 	*
 * command will use automatically that one. Otherwise will ask user to select 	*
 * one partition of the total available.										*
 *------------------------------------------------------------------------------*/

#include "ucommon.h"

int verbose = 0;
char * root = "/";

/**
 * Implementation of urm command in user space.
 * @param mnt_point Partitio Mount point (mounted with ext3u filesystem).
 * @param file_path Path of file to restore.
 * @param dir_path If not null it's path where doing restore of file.
 * @return Result of operation.
 */

int ext3u_urm_command(char *mnt_point, char* file_path, char * dir_path) 
{
	int fd, ioctl_ret;
	struct ext3u_urm_info urm_info = {0};
	urm_info.u_path = NULL;
	urm_info.u_dpath = NULL;

	/* Open mount point */	
	if ( ( fd = open(mnt_point, O_RDONLY) ) < 0 ) {
		fprintf(stderr, "urm: Error on opening mount point\n");
		return URM_ERR;
	}
	
	/* Create path string */
	if ( ( urm_info.u_path = malloc(strlen(file_path)+1) ) == NULL ) {
		fprintf(stderr, "urm: Error on malloc sys_call\n");
		return URM_ERR;
	}
		
	/* Create dpath string */
	if (dir_path != NULL) {
		
		if ( ( urm_info.u_dpath = malloc(strlen(dir_path)+1) ) == NULL ) {
			fprintf(stderr, "urm: Memory Error\n");
			return URM_ERR;
		}
	}
	
	/* Filling apposite structure for ioctl system call */
	strcpy(urm_info.u_path, file_path);	
	urm_info.u_path_length = strlen(file_path);
	
	/* dpath */	
	if (dir_path) {
		strcpy(urm_info.u_dpath, dir_path);
		urm_info.u_dpath_length = strlen(dir_path);
	}
	
	
	/* ioctl */
	ioctl_ret = ioctl(fd, EXT3_UNDEL_IOC_URM, &urm_info);
	
	/* Error cases management */
	if ( ioctl_ret == -1 ) {
		if (errno == EOPNOTSUPP)	
			fprintf(stderr,"urm: Undelete support not found on '%s'!\n", mnt_point);
		else
			fprintf(stderr,"urm: ioctl error: %s\n", strerror(errno));
		
		free(urm_info.u_path);
		if (urm_info.u_dpath) {
			free(urm_info.u_dpath);
		}
		
		close(fd);
		return URM_ERR;
	}
	else {
		/* Manage ext3u error situation */
		if ( urm_info.u_errcode != 0 ) {
			fprintf(stderr, "Error during recovery: ");

			if ( urm_info.u_errcode == -ENOENT )	
				fprintf(stderr, "Entry not found.\n");

			else if ( urm_info.u_errcode == -ENOMEM )
				fprintf(stderr, "Memory error.\n"); 
			
			else if ( urm_info.u_errcode == -EPERM )
				fprintf(stderr, "Permission denied.\n"); 

			else if ( urm_info.u_errcode == -EEXIST )
				fprintf(stderr, "File already exists! Try '-d' option.\n"); 

			else if ( urm_info.u_errcode == -ENODATA )
				fprintf(stderr, "Directory does not exist!\n"); 
			else
				fprintf(stderr,"urm: ioctl error: %s\n", strerror(errno));

			free(urm_info.u_path);
			
			if (urm_info.u_dpath) {
				free(urm_info.u_dpath);
			}
			
			close(fd);
			return URM_ERR;
		}
	}
	
	/* Normal exit: Operation successfully executed */
	free(urm_info.u_path);
	if (urm_info.u_dpath) {
		free(urm_info.u_dpath);
	}
	
	close(fd);
	
	return URM_OK;
}

/**
 * @brief Clean path from mount point.
 * @param mnt_point Mount point name.
 * @param path Path name.
 * @param dpath Will contain new clean name
 * @return A negative value if an error occurs, 0 otherwise.
 */

static int ext3u_clean_path(char *mnt_point, char *path, char **dpath) 
{
	char * mnt_point_start;
	char * path_start;
	char * new_path;
	
	/* First, we check if path is equal to mount point */	
	if ( strncmp(mnt_point, path, strlen(path)) == 0 ) {
		
		/* Root */
		if ( (new_path = malloc(FS_ROOT_LEN) ) == NULL )
			return URM_ERR;
		
		memset(new_path, 0, FS_ROOT_LEN);
		
		strcpy(new_path, "/");
	
	} else {
		
		/* Search mnt_point inside absolute path */
		if ( ( mnt_point_start = strstr(path, mnt_point) ) == NULL )
			return URM_ERR;
		
		path_start = path + strlen(mnt_point);
		
		if ( (new_path = malloc(strlen(path_start)+1) ) == NULL )
			return URM_ERR;
		
		memset(new_path, 0, strlen(path_start)+1);
		
		strcpy(new_path, path_start);
	}
	
	*dpath = new_path;
	
	return 0;
	
}

/**
 * @brief Print usage information.
 * @param stream File stream where write.
 * @param exit_code Exit code.
 */

static void print_usage(FILE* stream, int exit_code) 
{
	fprintf(stream, "Usage: urm [OPTIONS] File(s)\n");
	fprintf(stream, "\t Recovery of deleted files,\n");
	fprintf(stream, "\t -d Select a directory where restore selected file(s),\n");
	fprintf(stream, "\t -v Verbose Mode,\n"); 
	fprintf(stream, "\t -h Print this help.\n");
	exit(exit_code);
}

int main(int argc, char * argv[]) 
{
	
	char ** mnt_points;
	char mount_point[MAX_PATH] = {0};
	char dir_path[MAX_PATH] = {0};
	char * dpath;
	char * file_name;
	
	int mount_point_inserted = 0, dir_path_inserted = 0, mnt_number, i, urm_ret;
	
	int next_option;
	const char* const short_options = "vhm:d:";
	
	const struct option long_options[] = {
		{ "help",     0, NULL, 'h' },
		{ "mount_point", 1, NULL, 'm'},
		{ "dir",  1, NULL, 'd' },
		{ "verbose",  0, NULL, 'v' },
		{ NULL,       0, NULL, 0   }
	};
	
	do {
		next_option = getopt_long (argc, argv, short_options, long_options, NULL);
		
		switch (next_option)
		{
			case 'd':
				dir_path_inserted = 1;
				realpath(optarg, dir_path);
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

	/* Check options inserted */
	if ( ( argc - optind ) == 0 ) {
		fprintf(stderr, "No arguments (files) inserted.\n");
		print_usage(stderr, 1);
	}
	
	if ( mount_point_inserted ) {
		if ( ext3u_check_mount_point(mount_point) != 0 ) {
			fprintf(stderr, "Not valid mount point inserted.\n");
			exit(-1);
		}
	} 
	else {
		
		if ( ( mnt_points = ext3u_search_mount_points(&mnt_number) ) == NULL ) {
			fprintf(stderr, "Not valid mount point.\n");
			exit(-1);
		}
		
		/* Several mount points exist */
		if ( mnt_number > 1 ) {
			fprintf(stderr, "There are more mounts points availables, please select one:");
			
			for ( i = 0; i < mnt_number; i++ ) 
				printf("%s\n", mnt_points[i]);
			
			ext3u_free_mount_points(mnt_points, mnt_number);
			exit(0);
		}
		else     /* There is just one mount point */
			strncpy(mount_point, mnt_points[0], strlen(mnt_points[0]));
	}
		
	/* Clean dir_path from mount point */
	if ( dir_path_inserted ) 
		ext3u_clean_path(mount_point, dir_path, &dpath);
	
	/* Recovery files */
	for (i = optind; i < argc; ++i) {
				
		/* Clean mount point from file name */
		if ( ext3u_clean_path(mount_point, argv[i], &file_name) == URM_ERR ) {
			fprintf(stderr, "Wrong file name or mount point inserted (%s).\n", argv[i]);
			continue;
		}
		
		if (verbose) 
			printf("Restoring '%s'... ", argv[i]);
		
		/* if -d option is enabled */
		if ( dir_path_inserted )
			urm_ret = ext3u_urm_command(mount_point, file_name, dpath);
		else
			urm_ret = ext3u_urm_command(mount_point, file_name, NULL);
		
		
		if (verbose && (urm_ret == URM_OK ) ) 
			printf("done.\n");	
		
		/* Clean buffer */
		free(file_name);
	}
	
	/* Free and clean up */
	if ( !mount_point_inserted ) 
		ext3u_free_mount_points(mnt_points, mnt_number);

	/* Clean directory path */	
	if ( dir_path_inserted ) 
		free(dpath);
	
	return 0;
}
