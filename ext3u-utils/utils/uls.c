/* --------------------------------------------------------------
 * Copyright 2009												*
 * Authors: Antonio Davoli - Vasile Claudiu Perta  				*
 * 																*
 *																*
 * uls: undelete ls 											*
 * List deleted and retrievables files as ls command. 			*
 * -------------------------------------------------------------*/

/*----------------------------------------------------------------------------------*
 * NOTE: If there is only one partition mounted with ext3u file system, the command	*
 * will uses automatically that. Otherwise ask to user to select one (or more)		* 
 * partitions or use -a option for select all those availables. 					*
 *----------------------------------------------------------------------------------*/

#include "ucommon.h"

int long_listing = ULS_SHORT_ENTRY;
int verbose = 0;

/* ---------------------------------*
 * Print Command Usage Information	*
 * ---------------------------------*/

static void print_usage(FILE* stream, int exit_code) 
{
	fprintf(stream, "Usage: uls [OPTIONS] MOUNT_POINT(S)\n");
	fprintf(stream, "\t List deleted files can be undelete.\n");
	fprintf(stream, "\t -a Search on all availables (ext3u) mount points.\n");
	fprintf(stream, "\t -l Enable Long listing option.\n");
	fprintf(stream, "\t -n Specific how many of oldest files view.\n");
	fprintf(stream, "\t -h Print this help.\n");
	exit(exit_code);
}

int main(int argc, char * argv[]) {
	
	char ** mnt_points;
	int mnt_number, i , all_partitions = 0, num_files = 0;
	
	int next_option;
	const char* const short_options = "lhan:";
	
	const struct option long_options[] = {
		{ "all",		0, NULL, 'a' },
		{ "long",		0, NULL, 'l' },
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
			case 'l':
				long_listing = ULS_NORMAL_ENTRY;
				break;
			case 'h':
				print_usage (stdout, 0);
				break;
			case 'n':
				num_files = atoi(optarg);
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
		
	if ( ( argc - optind ) == 0 ) { 
		
		/* No Arguments Case: Check if there are mount points availables and if -a option is enabled */
		if ( ( mnt_points = ext3u_search_mount_points(&mnt_number) ) != NULL ) {
			
			if ( mnt_number > 1 ) {
				if ( all_partitions == 0 ) {
					
					/* Number of mount points greater than 1 and -a not inserted */
					printf("There are these mounts availables: Please select one or more of them or use -a option\n");
					
					for ( i = 0; i < mnt_number; i++ ) 
						printf("%s\n", mnt_points[i]);
					
					exit(0);
				}
				else { 
					
					/* Number of mount points greater than 1 and -a inserted */
					for ( i = 0; i < mnt_number; i++ ) {	
						printf("%s:\n", mnt_points[i]);
						/* uls command */
						ext3u_uls_command(mnt_points[i], num_files);
					
						/* Newline separator */
						if ( i < (mnt_number -1) )
							printf("\n");
					}
				}
			}
			else {
				/* Just one mount point exist */
				/* uls command */
				printf("%s:\n", mnt_points[0]);
				ext3u_uls_command(mnt_points[0], num_files);
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
				printf("%s:\n", argv[i]);
				
				/* uls command */
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
