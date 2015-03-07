/* -------------------------------------------------*
 * Ext3u - ext3 undelete							*
 *													*
 * 													*
 * Authors: Antonio Davoli - Vasile Claudiu Perta	*
 *													*
 * ------------------------------------------------ *
 * ucommon.c:										* 
 * Commons functions for undelete suite tool.		*
 * -------------------------------------------------*/

#include "ucommon.h"

/**
 * @param Check validity of mount point inserted.
 * @param mnt_point Mount point to check.
 * @return Return 0 if mnt_point is a valid ext3u mount point, a negative value otherwise.
 */

int ext3u_check_mount_point(char * mnt_point) 
{
	FILE * f;									/* File Descriptor */
	char buf[BUF_SIZE];							/* Buffer for read operation */
	char * ext3u_location;						/* Location where is file with mount point mounted */
	char * running, * token, delim[]=" ";		/* Parsing variables */
	int i;
	
	/* Open file with list of active mount point */
	if ( ( f = fopen(MOUNTS_PROC_ENTRY, "r") ) == NULL )
		return -1;
	
	/* Parsing file searching inside mount point passed as argument */
	while ( fgets(buf, sizeof(buf), f) != NULL ) {
		if ( ( ext3u_location = strstr(buf, EXT3u_NAME) ) != NULL ) {
			
			i = 0;
			running = buf;
			
			do {
				token = strsep(&running, delim);
			} while (++i < 2 && token != NULL);
			
			//printf("token::: %s (%s) \n", token, mnt_point);
			
			if ( !strncmp( mnt_point, token, strlen(mnt_point)) ) {
				
				token = strsep(&running, delim);
				
				/* Mount point found! Return */
				if ( !strncmp(token, EXT3u_NAME, strlen(token)) ) {
					fclose(f);
					return 0;
				}
				
			}
		}
	}
	
	fclose(f);  
	
	/* Mount point not found */
	return -1;
}


/**
 * @brief Search all mount point mounted with ext3u.
 * @param Will contain number of mount point found.
 * @return Return an array of names of all mount point found, NULL otherwise.
 */

char ** ext3u_search_mount_points(int * mnt_count) 
{
	
	char ** mnt_points = NULL;					/* Mount Points Array */ 
	FILE * f;									/* File Descriptor */
	char buf[BUF_SIZE];							/* Buffer for read operation */
	char * ext3u_location;						/* Location where is file with mount point mounted */
	char * running, * token, delim[]=" ";		/* Parsing variables */
	int i, cnt = 0;
	
	/* Open file with list of active mount point */
	if ( ( f = fopen(MOUNTS_PROC_ENTRY, "r") ) == NULL )
		return NULL;
	
	/* Parsing file searching inside all valid mount point */
	while ( fgets(buf, sizeof(buf), f) != NULL ) {
		if ( ( ext3u_location = strstr(buf, EXT3u_NAME) ) != NULL ) {
			
			/* Increase the mount_points array */
			if ( ( mnt_points = realloc(mnt_points, (cnt+1)*sizeof(char *)) ) == NULL )
				goto clean_up;
			
			i = 0;
			running = buf;
			
			do {  
				token = strsep(&running, delim);
			} while (++i < 2 && token != NULL);
			
			//printf("token::: %s (%d) \n", token, strlen(token));
			
			if ( ( mnt_points[cnt] = malloc( strlen(token) + 1 ) ) == NULL )
				goto clean_up;
			
			strncpy(mnt_points[cnt], token, strlen(token));
			
			cnt++;
		}
	}
	
	fclose(f);
	
	/* Return */
	*mnt_count = cnt;
	return mnt_points;
	
/* Error event: Clean all previous mount point found */
clean_up:
	fclose(f);
	ext3u_free_mount_points(mnt_points, cnt); 
	return NULL;
}

/**
 * @brief Free a mount points array.
 * @param Pointer to mount points array.
 * @param Number of element of array.
 */

void ext3u_free_mount_points(char **mnt_points, int mnt_count ) 
{
	int i = 0;
	
	/* Free all elements (char * allocated through malloc) */
	for ( i = 0; i < mnt_count; i++)
		free(mnt_points[i]);
	
	/* Free array */
	free(mnt_points);
}

