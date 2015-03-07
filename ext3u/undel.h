/**
 * @file undel.h
 * @autor Antonio Davoli, Vasile Claudiu Perta
 *
 */

#ifndef __UNDEL_H
#define __UNDEL_H

#include <linux/inotify.h>
#include <linux/dnotify.h>
#include <linux/audit.h>

#define EXT3u_FEATURE_COMPAT_UNDELETE	0x4000

#define EXT3u_UNDEL_DIR_INO  		EXT3_UNDEL_DIR_INO

#define EXT3u_INODE_MODE			S_IFLINK		

#define EXT3u_SUCCESS				1

#define EXT3u_SKIP_FILE				2

#define EXT3u_FEATURE_INDEX			1

#define EXT3u_BLOCK_HEADER_SIZE		4

#define EXT3u_DISK_CACHE_SIZE		8

#define EXT3u_UPDATE_PREVIOUS		1

#define EXT3u_UPDATE_NEXT			2

#define EXT3u_UPDATE_ADD			3

#define EXT3u_UPDATE_DELETE			4

#define EXT3u_ENTRY_FILE			1

#define EXT3u_ENTRY_DIR				2

#define EXT3u_ENTRY_SYMLINK			3

#define EXT3u_FIFO_END				0


#ifndef MIN
#define MIN(a,b) ( (a) <= (b) ? (a) : (b) )
#endif

#define ext3u_debug(f, a...)				\
	do {									\
	} while (0)


#define EXT3u_FIFO_NULL(r) (((r)->r_offset == 0) ? 1 : 0)

#define EXT3u_FIFO_EMPTY(usb) (EXT3u_FIFO_NULL((&((usb)->s_fifo.f_first))))

#define ext3u_hash_to_entry(h, s) 	( (h)%(s) ) 

//#define ext3u_lock(u_inode) 		spin_lock( &(u_inode->i_lock) )
#define ext3u_lock(u_inode) 		mutex_lock( &(u_inode->i_mutex) )

//#define ext3u_unlock(u_inode) 		spin_unlock( &(u_inode->i_lock) )
#define ext3u_unlock(u_inode) 		mutex_unlock( &(u_inode->i_mutex) )

#define EXT3u_HAS_FEATURE_INDEX(flag) ( (flag) & EXT3u_FEATURE_INDEX )



/* Macro needed for ioctl() - 'f' is for file system (ext2,ext3) */

#define EXT3_UNDEL_IOC_URM _IOW('f', 11, struct ext3u_urm_info)
#define EXT3_UNDEL_IOC_ULS _IOR('f', 12, struct ext3u_uls_info)
#define EXT3_UNDEL_IOC_USTATS _IOR('f', 13, struct ext3u_ustats_info)
#define EXT3_UNDEL_IOC_CONFIG _IOW('f', 14, struct ext3u_uconfig_info)


/* We redefine the ext3u_valid_inum() and add the EXT3_UNDEL_DIR_INO to the valid reserved inodes. */
static inline int ext3u_valid_inum(struct super_block *sb, unsigned long ino)
{
	return ino == EXT3_ROOT_INO ||
		ino == EXT3_JOURNAL_INO ||
		ino == EXT3_RESIZE_INO ||
		ino == EXT3_UNDEL_DIR_INO ||
		(ino >= EXT3_FIRST_INO(sb) &&
		 ino <= le32_to_cpu(EXT3_SB(sb)->s_es->s_inodes_count));
}


/* Pointer to an entry in the FIFO list. */
struct ext3u_record {
	__u32 r_block; 		/* logical block number */
	__u64 r_real_block; /* fisical block number */
	__u16 r_offset; 	/* offset in the block */
	__u16 r_size;		/* size in bytes of the entry */
};

#define EXT3u_RECORD_SIZE (sizeof(struct ext3u_record))


/* FIFO list  information */
struct ext3u_fifo_info {
	__u32 				f_blocks;				/* blocks reserved for the fifo queue */
	__u32				f_start_block; 			/* first logical block of the fifo queue */
	__u32				f_last_block; 			/* last logical block used  */
	__u32				f_last_offset;			/* offset in the last writeable block */
	__u32				f_last_block_remaining;	/* space left on the last used block */
	__u32				f_free;					/* free space on the FIFO (in bytes) */
	struct ext3u_record f_first;
	struct ext3u_record f_last;
};


/* Information about the deleted files */
struct ext3u_del_info {
	__u64 d_max_size;		/* max allowed size for ext3u filesystem */
	__u64 d_max_filesize;	/* max allowed size for a file to be saved */
	__u64 d_current_size;	/* current size */
	__u32 d_file_count; 	/* current number of all saved directories */
	__u32 d_dir_count; 		/* current number of saved files */
};


/* Static entry used to insert or read an entry from the fifo queue */
struct ext3u_del_entry {
	__u16					d_size;				/* size in bytes of this entry */
	struct ext3u_record 	d_next;				/* pointer to next entry in the fifo list*/	
	struct ext3u_record		d_previous;			/* pointer to the previous entry in the fifo list */
	__u16					d_type;				/* flag specifying the type of this entry: file, directory, link*/
	__u32					d_hash;				/* hash of the file */
	__u16					d_path_length;		/* path length */
	__u16					d_mode;				/* */
	__u16		 			d_uid;				/* */
	struct ext3_inode 		d_inode;			/* inode of the deleted file */
	char 					d_path[PATH_MAX+1];	/* buffer for the path */
};


/**
 * We use this structure to restore the FIFO ointers.  
 * The header entry cannot be splitted accross two blocks;
 * this way, restoring the FIFO pointers after un unde- 
 * lete is much more efficient, since reading one block
 * is enough.
 */

struct ext3u_del_entry_header {
	__u16					d_size;				/* size in bytes of this entry */
	struct ext3u_record 	d_next;				/* pointer to next entry in the fifo list*/	
	struct ext3u_record		d_previous;			/* pointer to the previous entry in the fifo list */
	__u16					d_type;				/* type of this entry */
	__u32					d_hash;				/* hash of the path */
	__u16					d_path_length;
	__u16					d_mode;
	unsigned int			d_uid;
};


#define EXT3u_DEL_HEADER_SIZE (sizeof(struct ext3u_del_entry_header))

#define EXT3u_DEL_ENTRY_SIZE (EXT3u_DEL_HEADER_SIZE + sizeof(struct ext3_inode))

/* The header of an entry(48 bytes) cannot be splitted accross two blocks */
#define EXT3u_WRITE_MIN	(EXT3u_DEL_HEADER_SIZE + sizeof(struct ext3_inode))


/* Information about files/directories to skip */
struct ext3u_skip_info {
	__u32 s_dir_count; 			/* number af the directories  */
	__u32 s_filext_size;		/* bytes reserved for the extensions */
	__u32 s_filext_count; 		/* number of the extensions to filter */
	__u32 s_current_size;		/* current size */
	__u32 s_size; 				/* number of reserved blocks */

};


/* The in-memory structure used to specify a 'skip rule' */
struct ext3u_skip_entry {
	int sk_ext_len; /* number of extensions in sk_ext */
	char * sk_dir;	/* the rule applies to all files in this directory or its subdirectories  */
	char **sk_ext; 	/* file extensions to be skipped */
};


/* Skip entry header on disk. */
struct ext3u_skip_header {
	__u32 s_dir_length;		/* directory path length */
	__u32 s_ext_length; 	/* extensions length */
	__u32 s_file_max_size;	/* skip files bigger than this */
};


/* Information about ext3u filesystem. */
struct ext3u_super_block {
	__u32	s_flags;
	__u32	s_block_size;		/* block size in bytes */
	__u32	s_inode_size;		/* inode size */
	__u32	s_fifo_free;		/* free space in the fifo list, including holes */
	__u64	s_block_count;		/* total number of used blocks */
	__u64	s_low_watermark; 	
	__u64	s_high_watermark;

	struct ext3u_del_info	s_del;
	struct ext3u_fifo_info 	s_fifo;
	struct ext3u_skip_info	s_skip;
};

/* Ioctl information structures */ 

/* urm command structure*/
struct ext3u_urm_info {
	char * u_path;			/* Path of file */
	char * u_dpath;			/* Path where do restore */
	int u_flag;				/* */
	int u_path_length;		/* Path Length */
	int u_dpath_length;		/* Operation Result Code */
	int u_errcode;			/* Error code */			
};


/* ustats command structure */
struct ext3u_ustats_info {
	int u_errcode;					/* Operation Result Code */
	__u32 u_block_size;				/* block size in bytes */
	__u32 u_inode_size;				/* inode size */
	__u32 u_fifo_blocks;			/* size of fifo list (block) */
	__u64 u_max_size;				/* max allowed size for data blocks */
	__u64 u_current_size;			/* current size */
	__u32 u_fifo_free;				/* free space in the fifo list, including holes */
	__u32 u_file_count;				/* current number of saved files */
	__u32 u_dir_count;				/* current number of all saved directory */
};


/* Short entry for 'uls' command */
struct ext3u_uls_short_entry {
	__u16 u_path_length;		/* Path Length */
};


/* Extendend entry for 'uls -l' command */
struct ext3u_uls_entry {
	__u16 u_path_length;		/* Path Length */
	struct timespec u_mtime;	/* Modified Time */
	unsigned int u_size;		/* Size of entry */
	umode_t u_mode;				/* Permission */
	short int u_uid;			/* User ID */
	short int u_gid;			/* Group ID */
	unsigned int u_nlink;		/* Link Number */
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
	 __u64 u_size;
};

/* We use a static entry when adding a newly deleted file to the FIFO list,
 * instead allocating one entry for each deletion. This should reduce the
 * memory management overhead.
 */

extern struct ext3u_del_entry ext3u_de_save;
extern struct ext3u_del_entry ext3u_de_remove;
extern struct ext3u_del_entry ext3u_de_ioctl;


int ext3u_save(handle_t * handle, struct dentry * de, int type);

int ext3u_urm(struct super_block * sb, char * path, char * dir);

int ext3u_restore_inode(handle_t * handle, struct inode * inode, struct ext3u_del_entry * de);

struct buffer_head * ext3u_read_super(struct inode * u_inode);

struct ext3u_del_entry * ext3u_get_entry(handle_t * handle, struct inode * u_inode, const char * path); 

void ext3u_print_entry(struct ext3u_del_entry * de);

int ext3u_permission(unsigned int uid,  umode_t mode, int mask);

int ext3u_skip(char * name);

#endif
