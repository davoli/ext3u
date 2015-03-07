/*------------------------------------------*
* undel.h									*
*											*
* Copyright (C) 2009 						*
* Antonio Davoli, Vasile Claudiu Perta		*
*											*
* Structures and macro definition			*
*-------------------------------------------*/ 


#ifndef __UNDEL_H

/* ioctl macros */
/* 'f' is for file system (ext2,ext3) */

#define EXT3_UNDEL_IOC_URM _IOW('f', 11, struct ext3u_urm_info)

#define EXT3_UNDEL_IOC_ULS _IOR('f', 12, struct ext3u_uls_info)

#define EXT3_UNDEL_IOC_USTATS _IOR('f', 13, struct ext3u_ustats_info)

#define EXT3_UNDEL_IOC_CONFIG _IOW('f', 14, struct ext3u_uconfig_info)

#define UNDEL_ERR -1
#define UNDEL_OK 0

typedef unsigned short umode_t;


/* Pointer to an entry in the FIFO list */
struct ext3u_record {
	unsigned int  r_block; 					/* block number */
	unsigned long long int  r_real_size; 	/* fisical block number */
	unsigned short  r_offset; 				/* offset in block */
	unsigned short  r_size; 				/* block number */
};

#define EXT3u_RECORD_SIZE (sizeof(struct ext3u_record))


/* ioctl information structures */ 

/* urm command structure*/
struct ext3u_urm_info {
	char * u_path;			/* path of the file */
	char * u_dpath;			/* path of the directory where the file will be restored */
	int u_flags;			/* */
	int u_path_length;		/* length of file path*/
	int u_dpath_length;		/* directory's path length */
	int u_errcode;			/* returned error code */			
};

/* ustats command structure */
struct ext3u_ustats_info {
	int 			u_errcode;				/* Operation Result Code */
	unsigned int	u_block_size;			/* block size in bytes */
	unsigned int	u_inode_size;			/* inode size */
	unsigned int 	u_fifo_blocks;			/* size allowed size data block */
	unsigned long long int u_max_size;		/* max size of total block */
	unsigned long long int u_current_size;	/* current size */
	unsigned int u_fifo_free;				/* free space in the fifo list, including holes */
	unsigned int u_file_count;				/* current number of saved files */
	unsigned int u_dir_count;				/* current number of all saved directory */
};

/* Short Entry for uls command */
struct ext3u_uls_short_entry {
	unsigned short u_path_length;		/* Path Length */
};

/* Short Entry for uls command */
struct ext3u_uls_entry {
	unsigned short u_path_length;	/* Path Length */
	struct timespec u_mtime;		/* Modified Time */
	unsigned int u_size;			/* Size of entry */
	umode_t u_mode;					/* Permission */
	short int u_uid;				/* User ID */
	short int u_gid;				/* Group ID */
	unsigned int u_nlink;			/* Link Number */
};

/* uls command */
struct ext3u_uls_info {
	char * u_buffer;					/* Communication Buffer */ 
	int u_buffer_length;				/* Buffer Length */
	int u_max_files;					/* Max number of files to view */
	int u_read_files;					/* How many files just read */
	int u_files;						/* Number of files contained in the buffer */
	int u_errcode;						/* Operation Result Code */
	int u_ll;							/* Long listing Option */
	int u_order;						/* Visualization Order */
	struct ext3u_record u_next_record;	/* First entry to search */
};

#define EXT3u_ULS_ENTRY_SIZE (sizeof(int))
#define EXT3u_ULL_ENTRY_SIZE (sizeof(struct ext3u_uls_entry) - sizeof(int))

struct ext3u_uconfig_info {
	char * u_buffer;
	int u_buffer_length;
	int u_dir_length;
	int u_ext_length;
	int u_list;
	int u_insert;
	int u_errcode;
	int u_mask;
	unsigned long long int u_size;
};

#endif
