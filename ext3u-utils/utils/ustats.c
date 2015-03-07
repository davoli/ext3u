/* -------------------------------------------------------------*
 * Copyright 2009												*
 * Authors: Antonio Davoli - Vasile Claudiu Perta  				*
 * 																*
 *																*
 * ustats: undelete stats										*
 * Retrieve information about one or more mount points.			*
 * -------------------------------------------------------------*/

/*------------------------------------------------------------------------------*
 * NOTE: If there is only one partition mounted with ext3u file system, the 	*
 * command will uses automatically that. Otherwise ask to user to select one	* 
 * (or more) partition of the total availables or use -a option just for select *
 * a  specific one.																*
 *------------------------------------------------------------------------------*/

#include "ucommon.h"

int long_listing = ULS_SHORT_ENTRY;



/**
 * Print information and statistic about ext3u mount point.
 * @param mnt_point Working mount point.
 * @param ustats_info Information about status of mount point.
 */

void print_ustats_info(char *mnt_point, struct ext3u_ustats_info * ustats_info) 
{
	float a, b;
	
	printf("******* ext3u ustats information *********\n");
	printf("Mount Point: %s\n", mnt_point);
	/* superblock information */
	a = (float) ustats_info->u_current_size;
	b = (float) ustats_info->u_max_size;

	printf("Cache Size(blocks)     Max Data(bytes)    Current Data(bytes)    Use    Files available\n");
	printf("%9u%24lld%20lld %15.2f%%%11u\n", 
			ustats_info->u_fifo_blocks, 
			ustats_info->u_max_size, 
			ustats_info->u_current_size, 
			(float) ((a * 100) / b), 
			ustats_info->u_file_count);
}

/**
 * Retrieve information about status of ext3u structure.
 * @param mnt_point Mount point where is mounted an ext3u file system.
 * @return Result of operation. 
 */

int ext3u_ustats_command(char *mnt_point)
{
	int fd;									/* Mount Point file descriptor */
	int ioctl_ret;							/* Return code of ioctl */
	struct ext3u_ustats_info ustats_info;	/* Structure for ioctl communication */
	
	/* Open mount point inserted */	
	if ( ( fd = open(mnt_point, O_RDONLY) ) < 0 ) {
		fprintf(stderr, "[ustats]: Error on opening mount point\n");
		return USTATS_ERR;
	}

	/* ioctl */
	/* Kernel funcion will fill the ext3u_ustats_info structure */
	ioctl_ret = ioctl(fd, EXT3_UNDEL_IOC_USTATS, &ustats_info);
	
	/* ioctl error cases */
	if ( ioctl_ret == -1 ) {
		if (errno == EOPNOTSUPP)
			fprintf(stderr,"ustats: Undelete suport not found on '%s'!", mnt_point);
		else
			fprintf(stderr,"ioctl error: %s\n", strerror(errno));

		close(fd);
		return USTATS_ERR;
	}
	else {
		/* internal error: TO DO */
		if ( ustats_info.u_errcode != 0 ) {
			fprintf(stderr, "Error %d\n", ustats_info.u_errcode);
			close(fd);
			return USTATS_ERR;
		}
	}
	
	/* Print information gained */
	print_ustats_info(mnt_point, &ustats_info);
	
	/* Normal exit: Operation successfully executed */
	close(fd);
	
	return USTATS_OK;

}

/* ---------------------------------*
 * Print Command Usage Information	*
 * ---------------------------------*/

static void print_usage(FILE* stream, int exit_code) 
{
	fprintf(stream, "Usage: ustats [OPTIONS] MOUNT_POINT(S)\n");
	fprintf(stream, "\t Retrive information about ext3u mount points.\n");
	fprintf(stream, "\t -a Search on all availables (ext3u) mount points.\n");
	fprintf(stream, "\t -n Specific how many of oldest files view.\n");
	fprintf(stream, "\t -h Print this help.\n");
	exit(exit_code);
}

int main(int argc, char * argv[]) {
	
	char ** mnt_points;
	int mnt_number, i , all_partitions = 0, num_files = 0;
	
	int next_option;
	const char* const short_options = "han:";
	
	const struct option long_options[] = {
		{ "all",		0, NULL, 'a' },	
		{ "number",		1, NULL, 'n' },
		{ "help",		0, NULL, 'h' },
		{ NULL,			0, NULL, 0   }
	};
	
	/* Parsing Command Line Options */
	
	do {
		next_option = getopt_long (argc, argv, short_options, long_options, NULL);
		
		switch (next_option)
		{
			case 'a':
				all_partitions = 1;
				break;
			case 'h':
				print_usage (stdout, 0);
				break;
			case 'n':
				num_files = atoi(optarg);
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
	
	if ( ( argc - optind ) == 0 ) { 
		
		/*No Arguments Case: Check if there are mount points availables and/or if -a option is enabled. */
		if ( ( mnt_points = ext3u_search_mount_points(&mnt_number) ) != NULL ) {
			
			if ( mnt_number > 1 ) {
				if ( all_partitions == 0 ) { 
					
					/* Number of mount points greater than 1 and -a not inserted */
					printf("There are these mounts availables: Please select one or use -a option\n");
					
					for ( i = 0; i < mnt_number; i++ ) 
						printf("%s\n", mnt_points[i]);
					
					exit(0);
				}
				else {
					
					/* Number of mount points greater than 1 and -a inserted */
					for ( i = 0; i < mnt_number; i++ ) {
					
						/* ustats command */
						ext3u_ustats_command(mnt_points[i]);
					
						/* if num_files is greater than 0, we will show the oldest num_files deleted (first that will delete) */
						if ( num_files > 0 ) { 
							printf("Next deletables file(s):\n"); 
							ext3u_uls_command(mnt_points[i], num_files);
						}
						
						/* Newline separator */
						if ( i < (mnt_number -1) )
							printf("\n");						
					}
				}
			}
			else {
				
				/* Just one mount point exist */
				ext3u_ustats_command(mnt_points[0]);
				
				/* num_files oldest files */
				if ( num_files > 0 ) {
					printf("Next deletables file(s):\n"); 
					ext3u_uls_command(mnt_points[0], num_files);
				}
			}
			
			ext3u_free_mount_points(mnt_points, mnt_number);
		}
		else {
			/* No one mount point ext3u available */
			fprintf(stderr," No correct mount points found.\n");
			exit(-1);
		}       
	}
	else {
		
		/* One or more mount points inserted by user through command line */
		for (i = optind; i < argc; ++i) {
			
			/* Check validity of mount point inserted */
			if ( ext3u_check_mount_point(argv[i]) == 0) {
				
				/* ustats command */
				ext3u_ustats_command(argv[i]);
				
				/* num_files oldest files */
				if ( num_files > 0 ) 
					ext3u_uls_command(argv[i], num_files);
			} 
			else
				fprintf(stderr, "%s: Not correct mount point.\n", argv[i]);
		
			/* Print another newline if there are multiple mounts selected */
 			if ( i < (argc-1) )
				printf("\n");
		}
	}
	
	return 0;
}
