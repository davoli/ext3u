 /**
  * @file undel.c
  * @author Antonio Davoli, Vasile Claudiu Perta
  *
  */
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/time.h>
#include <linux/ext3_jbd.h>
#include <linux/jbd.h>
#include <linux/highuid.h>
#include <linux/pagemap.h>
#include <linux/quotaops.h>
#include <linux/string.h>
#include <linux/buffer_head.h>
#include <linux/writeback.h>
#include <linux/mpage.h>
#include <linux/uio.h>
#include <linux/bio.h>
#include <linux/fiemap.h>
#include <linux/dcache.h>
#include <linux/namei.h>

#include "undel.h"
#include "namei.h"
#include "xattr.h"
#include "acl.h"

struct ext3u_del_entry  ext3u_de_save;
struct ext3u_del_entry  ext3u_de_remove;
struct ext3u_del_entry  ext3u_de_ioctl;


static char * ext3u_get_file_name(struct ext3u_del_entry * de);

static int ext3u_get_full_path(struct dentry * dentry, char * buf, int * name_length);

static int ext3u_skip_file(struct inode * u_inode, char * path);

static int ext3u_check_skip_entry(struct ext3u_skip_entry * sk, char * path);

static char * ext3u_get_file_name(struct ext3u_del_entry * de);

static struct ext3u_del_entry * ext3u_get_first_entry(struct inode * u_inode, struct ext3u_super_block * usb);

struct ext3u_del_entry * ext3u_find_entry(handle_t * handle, struct inode * u_inode, const char * path);

static int ext3u_update_superblock(struct ext3u_super_block * usb, struct ext3u_del_entry * de, int update);

static int ext3u_delete_entry(handle_t * handle, struct inode * u_inode, struct ext3u_del_entry * de);

static int ext3u_free_old_entries(struct inode * u_inode, struct ext3u_del_entry * de);

static int ext3u_update_entry(	handle_t * h, 
								struct inode * u_inode,
								struct ext3u_record * entry,
								struct ext3u_record * update,
								int flag
							 );

static struct ext3u_del_entry * ext3u_search_entry(	handle_t * handle,
												 	struct inode * u_inode,
												 	struct ext3u_super_block * usb,
												 	struct ext3u_record * start, 
												 	struct ext3u_record * end, 
												 	const char * path,
												 	int * entries
												  );

static void ext3u_print_super_block(struct ext3u_super_block * usb)
{

#ifdef EXT3u_DEBUG
	printk("\n\n*** EXT3u_SUPERBLOCK (%d bytes):***\n\n", sizeof(struct ext3u_super_block));
	printk(	"\tf_blocks = %d\n"
			"\tfifo_free_contigous = %d\n"
			"\tf_first.r_block = %d\n"
			"\tf_first.r_offset = %d\n"
			"\tf_first.r_size = %d\n"
			"\tf_last.r_block = %d\n"
			"\tf_last.r_offset = %d\n"
			"\tf_last.r_size = %d\n"
			"\tf_last_block = %d\n"
			"\tf_last_offset = %d\n"
			"\td_max_size = %lld\n"
			"\td_current_size = %lld\n"
			"\td_file_count = %d\n",
		 	usb->s_fifo.f_blocks,
		 	usb->s_fifo.f_free,
		 	usb->s_fifo.f_first.r_block,
		 	usb->s_fifo.f_first.r_offset,
		 	usb->s_fifo.f_first.r_size,
		 	usb->s_fifo.f_last.r_block,
		 	usb->s_fifo.f_last.r_offset,
		 	usb->s_fifo.f_last.r_size,
		 	usb->s_fifo.f_last_block,
		 	usb->s_fifo.f_last_offset,
		 	usb->s_del.d_max_size,
		 	usb->s_del.d_current_size,
			usb->s_del.d_file_count
			);
#endif
}
void ext3u_print_entry(struct ext3u_del_entry * de)
{
#ifdef EXT3u_DEBUG
		printk("\n\n");
		printk(	"\tsize = %d\n"
			"\tpath length = %d\n"
			"\tpath = %s\n"
			"\thash = %u\n"
			"\tprevious->block = %d\n"
			"\tprevious>offset = %d\n"
			"\tnext->block = %d\n"
			"\tnext->offset = %d\n"
			"\tdisk size = %d\n",
		 	de->d_size,
		 	de->d_path_length,
		 	de->d_path,
		 	de->d_hash,
		 	de->d_previous.r_block,
		 	de->d_previous.r_offset,
		 	de->d_next.r_block,
		 	de->d_next.r_offset,
			de->d_inode.i_size
			);
	printk("\n\n");
#endif
}

void ext3u_print_entry_header(struct ext3u_del_entry_header * dh)
{
#ifdef EXT3u_DEBUG

		printk(	"\td_size = %d\n"
			"\td_next.r_block = %d\n"
			"\td_next.r_offset = %d\n"
			"\td_hash = %d\n"
			"\td_path_length = %d\n",
		 	dh->d_size,
		 	dh->d_next.r_block,
		 	dh->d_next.r_offset,
		 	dh->d_hash,
		 	dh->d_path_length
			);
#endif
}

/* Partial hash update function. Assume roughly 4 bits per character */
static inline unsigned long ext3u_partial_hash(unsigned long c, unsigned long prevhash)
{
	return (prevhash + (c << 4) + (c >> 4)) * 11;
}


/* Compute the hash for a name string. */
static inline unsigned int ext3u_hash(const unsigned char *name, unsigned int len)
{
	unsigned long hash = 0;
	while (len--)
		hash = ext3u_partial_hash(*name++, hash);

	return (unsigned int) hash;
}


/* Return the full path for this dentry in buf and the length of the name in namelen. */
static int ext3u_get_full_path(struct dentry * dentry, char * buf, int * name_length)
{
	struct dentry * de = dentry;
	struct qstr entry = dentry->d_name;
	int len = 0;
	char * tmp;
	
	tmp = kmalloc(PATH_MAX+1, GFP_KERNEL);
	if (!tmp){
		return -ENOMEM;
	}

	memset(tmp, 0, PATH_MAX+1);

	len = entry.len;
	*name_length = entry.len;

	while (strcmp(entry.name,"/")) {

		strncpy(buf, tmp, PATH_MAX);
		memset(tmp, 0, PATH_MAX+1);

		strcpy(tmp, "/");
		strncat(tmp, entry.name, PATH_MAX-strlen(tmp)+1);
		strncat(tmp, buf, PATH_MAX-strlen(tmp)+1);

		de = de->d_parent;
		if (!de) {
			kfree(tmp);
			return -ENOENT;
		}	
		entry = de->d_name;
		len += entry.len;
	}
	
	strncpy(buf, tmp, PATH_MAX);

	kfree(tmp);
	return 0;
}

/**
 * @brief This function try to avoid saving temporary files in the
 * list of deleted files. It performes a check on the path of the
 * file given a list of extensions and directories. As a future 
 * extension, the user could define its own rules as paires
 * (directory [DIR], extensions [EXTS]) specifying which files 
 * in the directiry DIR must be skipped. Actualy it supports
 * only (/temp, *), (*, ~) and (*, .swap) where the '*' matches
 * everything.
 * 
 * @param u_inode The ext3u root inode.
 * @param path The name of file to be checked.
 *
 * @return It returns zero on success, -1 otherwise.
 */

static int ext3u_skip_file(struct inode * u_inode, char * path)
{
	struct ext3u_skip_entry sk1, sk2;
	char *ext1[]  = {"*", ""};
	char *ext2[] =  {"~", ".swp", ".o", ".noundel"};
	int err;

	sk1.sk_dir = "/temp/";
	sk1.sk_ext = ext1;
	sk1.sk_ext_len = 1;

	sk2.sk_dir = "";
	sk2.sk_ext =  ext2;
	sk2.sk_ext_len = 4;
	
	if ( (err = ext3u_check_skip_entry(&sk1, path)) )
		return err;

	if ( (err = ext3u_check_skip_entry(&sk2, path)) )
		return err;

	return 0;
}

int ext3u_skip(char * name)
{
	return ext3u_skip_file(NULL, name);

}

/**
 * @brief It checks whether a file should be skipped, given a specific
 * 'skip entry' (DIR, EXTENSIONS).
 *
 * @param sk The skip rule.
 * @param path The path of the file to be checked.
 *
 * @return It returns zero on success, -1 otherwise.
 */
static int ext3u_check_skip_entry(struct ext3u_skip_entry * sk, char * path)
{
	
	int i = sk->sk_ext_len, len, j;
	char * dir = sk->sk_dir;
	char **ext, *s, *ss;

	/* Check if dir is a prefix of 'path' */
	if (sk->sk_dir) {
		s = path;
		ss = dir;
		while(*ss != '\0' && *s++ == *ss++);
		
		if(*ss != '\0')
			return 0;
	}

	/* Check the extensions which apply to this directory.*/
	ext = sk->sk_ext;
	for (i = 0; i < sk->sk_ext_len; i++) {	
		s = (char*)(path + (strlen(path) - 1));
		ss = *(ext);

		/* The star matches all extensions */
		if (!strcmp(ss, "*"))
			return EXT3u_SKIP_FILE;

		len = strlen(*ext);
		ss += (len - 1);

		for(j = 0; j < len && (*s == *ss); j++, s--, ss--) {
			ext3u_debug("j = %d, *s = %c", j, *s);
		}

		if (j == len)
			return EXT3u_SKIP_FILE;

		ext++;
	}
	return 0;
}

/** 
 * @brief Parse the full path of the file and return only the name of the file.
 * Basically it finds the last '/' of the path and zeros it, than return the
 * pointer to the next character.
 *
 * @param de The entry to be restored.
 * @return The pointer to the file name. 
 *
 */
static char * ext3u_get_file_name(struct ext3u_del_entry * de)
{
	char *str = &(de->d_path[de->d_path_length-1]);

	while (*str != '/') 
		str--;

	*str = '\0';

	return (++str);	
}

/** TODO
 * @brief Return the first entry on the FIFO queue. 
 * The memory is allocated using kmalloc so
 * it must be released when done. 
 */
static struct ext3u_del_entry * ext3u_get_first_entry(struct inode * u_inode, struct ext3u_super_block * usb)
{
	struct buffer_head * bh;
	struct ext3u_del_entry * de;
	struct ext3u_del_entry_header * dh;
	int err, remaining, to_copy, copied;

	unsigned int block, block_size;
	__u16 offset;
	void *src, *dest;
	

	block = usb->s_fifo.f_first.r_block;
	offset = usb->s_fifo.f_first.r_offset;
	block_size = usb->s_block_size;

	/* The queue is empty */
	if(EXT3u_FIFO_EMPTY(usb)) {
		return ERR_PTR(-ENOENT);
	}
	
	bh = ext3_bread(NULL, u_inode, block, 0, &err);
	if (!bh) {
		return ERR_PTR(-EIO);
	}

	/* The header cannot be splitted across blocks. */
	dh = (struct ext3u_del_entry_header *) (bh->b_data + offset);

	/* Allocate the memory for the entry. */
	de = (struct ext3u_del_entry *) kmalloc(dh->d_size, GFP_KERNEL);
	if (de == NULL) {
		brelse(bh);
		return ERR_PTR(-ENOMEM);
	}	

	dest = (void *)de;
	src = (void *)(bh->b_data + offset);
	memset((void*)de, 0, dh->d_size);


	memcpy(dest, src, EXT3u_DEL_HEADER_SIZE);
	remaining = dh->d_size - EXT3u_DEL_HEADER_SIZE;
	copied = EXT3u_DEL_HEADER_SIZE;
	offset += EXT3u_DEL_HEADER_SIZE;
	dest += copied;


	/* Copy the rest of the entry. */
	while (remaining > 0) {
		to_copy = MIN((block_size - offset), remaining);

			
		src = (void *) (bh->b_data + offset);
		memcpy(dest, src, to_copy);
		dest+= to_copy;

		offset = (offset + to_copy) % block_size;
		copied += to_copy;
		remaining -= to_copy;

		/* If we reach the end of the block and */
		/* start reading from the next block.  */
		if ((offset == 0) && (remaining != 0)) {
				
			offset = EXT3u_BLOCK_HEADER_SIZE;
			block = (block) % usb->s_fifo.f_blocks + 1;

			bh = ext3_bread(NULL, u_inode, block, 0, &err);
			if (!bh) {
				return ERR_PTR(-EIO);
			}
		}
	}

	return de;
}

/**
 * @brief Update the ext3u superblock.
 * 
 * @param usb Pointer to the superblock.
 * @param de The entry wich caused the update.
 * @param update The type of the update; it can be either EXT3u_UPDATE_DELETE or EXT3u_UPDATE_ADD.
 * 
 * @return Returns zero on success, otherwise 0.
 */
static int ext3u_update_superblock(struct ext3u_super_block * usb, struct ext3u_del_entry * de, int update)
{
	int free, block_size, remaining;
	int start_block = 0, start_offset = 0;
	int end_block = 0, end_offset = 0;
	int block, offset, size;

	block_size = usb->s_block_size;

	switch (update) {

		/* The entry was removed. */
		case EXT3u_UPDATE_DELETE:

			usb->s_del.d_current_size -= de->d_inode.i_size;
			usb->s_del.d_file_count--;

			if ( !(EXT3u_FIFO_NULL(&(de->d_previous))) && !(EXT3u_FIFO_NULL(&(de->d_next))) ) {
				return 0;
			}
			
			/* The fifo list is empty, reinitialize the head and the tail*/
			if ( (EXT3u_FIFO_EMPTY(usb)) ) {
	
				memset(&(usb->s_fifo.f_first), 0, sizeof(struct ext3u_record));
				memset(&(usb->s_fifo.f_last), 0, sizeof(struct ext3u_record));

				usb->s_fifo.f_last_block = usb->s_fifo.f_start_block;
				usb->s_fifo.f_last_offset = EXT3u_BLOCK_HEADER_SIZE;
				usb->s_fifo.f_free = (usb->s_fifo.f_blocks *(block_size - EXT3u_BLOCK_HEADER_SIZE));

				return 0;
			}

			if (EXT3u_FIFO_NULL(&(de->d_previous))) {

				start_block = usb->s_fifo.f_first.r_block;
				start_offset = usb->s_fifo.f_first.r_offset;

				end_block = de->d_next.r_block;
				end_offset = de->d_next.r_offset;

				usb->s_fifo.f_first.r_block = de->d_next.r_block;
				usb->s_fifo.f_first.r_real_block = de->d_next.r_real_block;
				usb->s_fifo.f_first.r_offset = de->d_next.r_offset;	
				usb->s_fifo.f_first.r_size = de->d_next.r_size;

			}

			/* We removed the last entry, so the free space must be updated */
			if (EXT3u_FIFO_NULL(&(de->d_next))) {

				end_block = usb->s_fifo.f_last_block;
				end_offset = usb->s_fifo.f_last_offset;

				usb->s_fifo.f_last.r_block = de->d_previous.r_block;
				usb->s_fifo.f_last.r_real_block = de->d_previous.r_real_block;
				usb->s_fifo.f_last.r_offset = de->d_previous.r_offset;	
				usb->s_fifo.f_last.r_size = de->d_previous.r_size;
				
				block = de->d_previous.r_block;
				offset = de->d_previous.r_offset;
				size = de->d_previous.r_size;

				if ((offset + size) >= block_size) {
					start_offset = ((offset + size - EXT3u_BLOCK_HEADER_SIZE) %
									(block_size - EXT3u_BLOCK_HEADER_SIZE)) + EXT3u_BLOCK_HEADER_SIZE;
				} else {
					start_offset = offset + size;
				}

				if (size + offset > block_size) {
					remaining = offset + size - EXT3u_BLOCK_HEADER_SIZE;

					while(remaining > 0){
						remaining -= MIN(remaining,(block_size - offset));
						block = (block % usb->s_fifo.f_blocks) + 1;
						offset = EXT3u_BLOCK_HEADER_SIZE;
					}	
				}		
				start_block = block;
	
				if ((usb->s_block_size - start_offset) < EXT3u_WRITE_MIN) {

					start_block = (start_block % usb->s_fifo.f_blocks) + 1;
					start_offset = EXT3u_BLOCK_HEADER_SIZE;
				}

				usb->s_fifo.f_last_offset = start_offset;
			}	
			
			free = 0;
			while ((start_block != end_block) || (start_offset != end_offset)) {

				if (start_block != end_block) {
					free += (block_size - start_offset);
					start_offset = block_size;
				} else {
					start_offset++;
					free++;
				}

				if (start_offset == block_size) {
					start_offset = EXT3u_BLOCK_HEADER_SIZE;
					start_block = (start_block % usb->s_fifo.f_blocks) + 1;
				}
			}
	
			usb->s_fifo.f_free += free;

			break;
	}
	return 0;
}

	
/**
 * @brief Update the FIFO pointer of the 'entry' entry
 * passed as argument, according to the value
 * of the 'flag', which can refer to one of 
 * 'r_previous'or 'r_next' pointers.
 * @param h Handle of the transaction.
 * @param u_inode The ext3u root inode.
 * @param entry The record to be updated.
 * @param update The new record containig the update.
 * @param flag Type of the update; it specifies which pointers in the record have to be update.
 *
 * @return Returns zero on success, EIO otherwise.
 */

static int ext3u_update_entry(handle_t * h,struct inode * u_inode,struct ext3u_record * entry,struct ext3u_record * update,int flag)
{
	handle_t * handle; 
	struct buffer_head * bh;
	struct ext3u_del_entry_header * dh;
	int err = 0;
	
	/* Open a new transaction, we just have to write one block. */
	if (h == NULL) {
		handle = ext3_journal_start(u_inode, EXT3_SINGLEDATA_TRANS_BLOCKS);
		if (IS_ERR(handle)) {
	 		return PTR_ERR(handle);
		}
	} else {
		handle = h;
	}
	
	/* Read the block correspondig to this entry. */
	bh = ext3_bread(NULL, u_inode, entry->r_block, 0, &err);
	if (!bh) {
		return err;
	}

	/* The header of the entry cannot be split across the blocks */
	dh = (struct ext3u_del_entry_header *)(bh->b_data + entry->r_offset);

	/* Update the pointers. */
	switch (flag) {
		case EXT3u_UPDATE_NEXT:
			dh->d_next.r_block = update->r_block;
			dh->d_next.r_real_block = update->r_real_block;
			dh->d_next.r_offset = update->r_offset;
			dh->d_next.r_size = update->r_size;	
			break;

		case EXT3u_UPDATE_PREVIOUS:
			dh->d_previous.r_block = update->r_block;
			dh->d_previous.r_real_block = update->r_real_block;
			dh->d_previous.r_offset = update->r_offset;
			dh->d_previous.r_size = update->r_size;	
		
			if ((update->r_block != entry->r_block) && ((*((__u32 *)bh->b_data)) != entry->r_offset) ) {
				*((__u32 *)bh->b_data) = entry->r_offset;
			}
			break;

		default:
			err = ENODATA;

	}

	/* Write the block to disk */
	ext3_journal_get_write_access(handle, bh);
	ext3_journal_dirty_metadata(handle, bh);
	sync_dirty_buffer(bh);

	if (!h)
		ext3_journal_stop(handle);

	return err;
}

/**
 * @brief Deletes an entry by just updating the FIFO pointers.
 *
 * @param handle The handle of this transaction.
 * @param u_inode The ext3u root inode.
 * @param de The entry to be deleted.
 *
 * @return Returns zero on success, a positive number otherwise.
 */
static int ext3u_delete_entry(handle_t * handle, struct inode * u_inode, struct ext3u_del_entry * de)
{
	int err;
	
	/* Update the 'previous' FIFO pointer of the next entry. */
	if (!EXT3u_FIFO_NULL(&(de->d_next))) {
			if ((err = ext3u_update_entry(NULL, u_inode, &(de->d_next), &(de->d_previous), EXT3u_UPDATE_PREVIOUS))) {
				return err;
		}
	}
	
	/* Update the 'd_next' FIFO pointer of the previous entry. */
	if (!EXT3u_FIFO_NULL(&(de->d_previous))) {
		if ((err = ext3u_update_entry(NULL, u_inode, &(de->d_previous), &(de->d_next), EXT3u_UPDATE_NEXT))) {
			return err;
		}
	}
	
	return 0;
}


/**
 * @brief Read the ext3u superblock.
 *
 * @param u_inode The ext3u root inode.
 *
 * @return On success, it returns the buffer containig the superblock. 
 */
struct buffer_head * ext3u_read_super(struct inode * u_inode)
{
	struct buffer_head * bh;
	int err;

	bh = ext3_bread(NULL, u_inode, 0, 0, &err);
	if (!bh) {
		return ERR_PTR(-EIO);
	}
	
	return bh;
}

/** 
 * @brief Free one or more entries of the FIFO queue to make space for the new entry.
 * 1. We make space in the queue for this entry, if there is less then
 * new->d_size.
 * 2. The size of all data-blocks must be checked in order to ensure
 * that their sum remain below the value 'd_max_size' witch is the
 * the maximun size of all data blocks pointed by the entries in the
 * FIFO list. This size has been specified at filesystem creation.
 *
 * @param u_inode The ext3u root inode.
 * @param de The new entry to be added in the list.
 *
 * @return Returns zero on success, otherwise a positive number indicating the error.
 */
static int ext3u_free_old_entries(struct inode * u_inode, struct ext3u_del_entry * de)
{
	struct buffer_head * bh;
	struct inode * inode, *dir;
	struct ext3u_del_entry * dh;
	struct ext3u_super_block * usb;
	handle_t * handle;
	int err;
	int mode =  S_IFREG|S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;
	struct ext3u_record record = { 
		.r_block = 0, 
		.r_real_block = 0, 
		.r_offset = 0, 
		.r_size = 0
	};
	
	bh = ext3_bread(NULL, u_inode, 0, 0, &err);
	if (!bh) {
		return -EIO;
	}	
	usb = (struct ext3u_super_block *)bh->b_data;

	if (EXT3u_FIFO_EMPTY(usb))
		return 0;

	/* We need a directory to create the inode used */
	/* to remove the entries from the FIFO list. */
	dir = ext3_iget(u_inode->i_sb, EXT3_ROOT_INO);
	if (!dir) {
		return -EIO;
	}

	/* Free enough space in the queue for the new entry. */
	/* Keep the sum of data blocks below the value 'd_max_size'.*/

	while ( (usb->s_fifo.f_free < de->d_size) || ((usb->s_del.d_current_size + de->d_inode.i_size) > usb->s_del.d_max_size) ) {
		
		dh = ext3u_get_first_entry(u_inode, usb);
		if (IS_ERR(dh)) {
			ext3u_debug("Cannot get the first entry");
			iput(dir);
			return PTR_ERR(dh);
		}	
		ext3u_print_entry(dh);
		ext3u_delete_entry(handle, u_inode, dh);
		ext3u_update_superblock(usb, dh, EXT3u_UPDATE_DELETE);

		/* Update the first entry of the FIFO queue. */
		if (!EXT3u_FIFO_EMPTY(usb)) {
			ext3u_update_entry(NULL, u_inode, &(usb->s_fifo.f_first), &record, EXT3u_UPDATE_PREVIOUS);
		}	

		/* Use a new inode to restore the old one and then free the data blocks. */
		handle = ext3_journal_start(dir, EXT3_DELETE_TRANS_BLOCKS(dir->i_sb));

		if (IS_ERR(handle)) {
			iput(dir);
			kfree(dh);
			return PTR_ERR(handle);
		}
		
		inode = ext3_new_inode(handle, dir, mode);
		if (IS_ERR(inode)) {
			iput(dir);
			ext3_journal_stop(handle);
			kfree(dh);
			return PTR_ERR(inode);
		}
	
		/* Restore the previously deleted inode. */
		ext3u_restore_inode(NULL, inode, dh);
	
		/* Delete the inode and free the data blocks. */
		drop_nlink(inode);	
		iput(inode);
		ext3_journal_stop(handle);

		kfree(dh);
	}

	brelse(bh);
	iput(dir);
	return 0;	
}

/**
 * @brief Check if the user has the permission to restore
 * a file (only the superuser and the owner of the file)
 *
 * @param uid The user identifier of the inode's owner. 
 * @param mode The mode of the inode.
 * @param mask Permission to check.
 *
 * @return On success it returns zero and -EPERM  on error.
 */
int ext3u_permission(unsigned int uid, umode_t mode, int mask)
{	
	if ( (current->fsuid == 0)||(current->fsuid == uid) ) {
		return 0;
	}
	else
		return -EPERM;
}

/**
 * @brief Restore the inode at its state before the deletion of the file. 
 *
 * @param handle The handle of this transaction.
 * @param inode The inode to be restored.					
 * @param de  The entry on disk wich hold the old inode.
 * 
 * @return Returns zero on success.
 */

int ext3u_restore_inode(handle_t * handle, struct inode * inode, struct ext3u_del_entry * de)
{
	struct ext3_inode * raw_inode = &(de->d_inode);
	struct ext3_inode_info *ei = EXT3_I(inode);
	int block;

#ifdef CONFIG_EXT3_FS_POSIX_ACL
	ei->i_acl = EXT3_ACL_NOT_CACHED;
	ei->i_default_acl = EXT3_ACL_NOT_CACHED;
#endif
	inode->i_mode = le16_to_cpu(raw_inode->i_mode);
	inode->i_uid = (uid_t)le16_to_cpu(raw_inode->i_uid_low);
	inode->i_gid = (gid_t)le16_to_cpu(raw_inode->i_gid_low);
	if(!(test_opt (inode->i_sb, NO_UID32))) {
		inode->i_uid |= le16_to_cpu(raw_inode->i_uid_high) << 16;
		inode->i_gid |= le16_to_cpu(raw_inode->i_gid_high) << 16;
	}
	inode->i_nlink = 1;
	inode->i_size = le32_to_cpu(raw_inode->i_size);
	inode->i_atime.tv_sec = (signed)le32_to_cpu(raw_inode->i_atime);
	inode->i_ctime.tv_sec = (signed)le32_to_cpu(raw_inode->i_ctime);
	inode->i_mtime.tv_sec = (signed)le32_to_cpu(raw_inode->i_mtime);
	inode->i_atime.tv_nsec = inode->i_ctime.tv_nsec = inode->i_mtime.tv_nsec = 0;

	ei->i_state = 0;
	ei->i_dir_start_lookup = 0;
	ei->i_dtime = le32_to_cpu(raw_inode->i_dtime);

	inode->i_blocks = le32_to_cpu(raw_inode->i_blocks);
	ei->i_flags = le32_to_cpu(raw_inode->i_flags);
#ifdef EXT3_FRAGMENTS
	ei->i_faddr = le32_to_cpu(raw_inode->i_faddr);
	ei->i_frag_no = raw_inode->i_frag;
	ei->i_frag_size = raw_inode->i_fsize;
#endif
	ei->i_file_acl = le32_to_cpu(raw_inode->i_file_acl);
	inode->i_size |= ((__u64)le32_to_cpu(raw_inode->i_size_high)) << 32;
	ei->i_disksize = inode->i_size;
	inode->i_generation = le32_to_cpu(raw_inode->i_generation);
	/*
	 * NOTE! The in-memory inode i_data array is in little-endian order
	 * even on big-endian machines: we do NOT byteswap the block numbers!
	 */
	for (block = 0; block < EXT3_N_BLOCKS; block++)
		ei->i_data[block] = raw_inode->i_block[block];
	INIT_LIST_HEAD(&ei->i_orphan);

	if (inode->i_ino >= EXT3_FIRST_INO(inode->i_sb) + 1 && EXT3_INODE_SIZE(inode->i_sb) > EXT3_GOOD_OLD_INODE_SIZE) {
		/*
		 * When mke2fs creates big inodes it does not zero out
		 * the unused bytes above EXT3_GOOD_OLD_INODE_SIZE,
		 * so ignore those first few inodes.
		 */
		ei->i_extra_isize = le16_to_cpu(raw_inode->i_extra_isize);
		if (EXT3_GOOD_OLD_INODE_SIZE + ei->i_extra_isize > EXT3_INODE_SIZE(inode->i_sb)) {
			return -EIO;
		}
		if (ei->i_extra_isize == 0) {
			/* The extra space is currently unused. Use it. */
			ei->i_extra_isize = sizeof(struct ext3_inode) -
					    EXT3_GOOD_OLD_INODE_SIZE;
		} 
		else {
			__le32 *magic = (void *)raw_inode + EXT3_GOOD_OLD_INODE_SIZE + ei->i_extra_isize;
			if (*magic == cpu_to_le32(EXT3_XATTR_MAGIC))
				 ei->i_state |= EXT3_STATE_XATTR;
		}
	} else
		ei->i_extra_isize = 0;

	ext3_set_inode_flags(inode);

	return 0;
}

/**	
 * @brief Save a file in the FIFO list before beeing deleted.
 *
 * @param handle The handle of this transaction.
 * @param dentry The the dentry being deleted.
 * @param type The type of this entry on the FIFO queue (now only EXT3u_ENTRY_FILE is supported).
 * 
 * @return Returns zero on success, otherwise an integer indicating the error.
 */
int ext3u_save(handle_t * h, struct dentry * dentry, int type)
{
	struct inode * u_inode;
	struct ext3_iloc iloc;
	struct ext3u_super_block * usb;
	struct ext3u_del_entry * new_entry = &ext3u_de_save;
	struct ext3u_record r_target, r_update;
	struct buffer_head * bh, *blk_bh;
	struct ext3_inode * raw_inode;
	handle_t * handle;
	int err = 0, name_length, block, offset, remaining, to_copy;
	int end_offset = 0, end_block = 0;
	char * buf, *src, *dest;

	if (EXT3_HAS_INCOMPAT_FEATURE(dentry->d_sb, EXT3u_FEATURE_COMPAT_UNDELETE)){
		return 0;
	}
	
	buf = kmalloc(PATH_MAX+1, GFP_KERNEL);
	if (!buf) {
		return  -ENOMEM;
	}

	memset(buf, 0, PATH_MAX+1);

	/* Read the ext3u root inode */
	u_inode = ext3_iget(dentry->d_sb, EXT3u_UNDEL_DIR_INO);
	if (IS_ERR(u_inode)) {
		err = -ENOENT;
		goto free_and_exit;
	}
	
	ext3u_lock(u_inode);

	/* Read the ext3u superblock. */
	bh = ext3_bread(NULL, u_inode, 0, 0, &err);
	if (!bh) {
		goto err_exit;
	}
	
	usb = (struct ext3u_super_block *) bh->b_data;	

	/* Ignore this file if its size is bigger than allowed.  */
	if (new_entry->d_inode.i_size > usb->s_del.d_max_size) {
		goto err_exit;
	}

	brelse(bh);
	
	/* Get the full path of the file */
	if ((err = ext3u_get_full_path(dentry, buf, &name_length))) {
		goto err_exit;
	}
		
	/* Check if this entry should be skipped  */
	if ((err = ext3u_skip_file(u_inode, buf))) {
		goto err_exit;
	}

	/* Initialize and fill the entry */ 
	memset(new_entry, 0, sizeof(struct ext3u_del_entry));

	/* Save the inode state. */
	err = ext3_get_inode_loc(dentry->d_inode, &iloc);
	if  (err) {
		goto err_exit;
	}

	raw_inode = ext3_raw_inode(&iloc);
	memcpy(&(new_entry->d_inode), raw_inode, sizeof(struct ext3_inode));

	/* We must zero this, avoiding ext3_truncate() to be called. */
	dentry->d_inode->i_blocks = 0;
	dentry->d_inode->i_size = 0;
	EXT3_I(dentry->d_inode)->i_disksize = 0;

	/* Copy the path. */
	new_entry->d_path_length = strlen(buf);
	strncpy(new_entry->d_path, buf, PATH_MAX);

	/* Calculate the hash. */
	new_entry->d_hash = ext3u_hash(buf, new_entry->d_path_length);
	new_entry->d_uid = dentry->d_inode->i_uid;
	new_entry->d_mode = dentry->d_inode->i_mode;

	/* Set the type. */
	new_entry->d_type = type;

	/* Set the size in byte of this new entry. */
	new_entry->d_size = EXT3u_DEL_ENTRY_SIZE + strlen(buf) + 1;
	
	/* Now we have to write this entry in the FIFO queue. If	*/
	/* the queueu is full, then we have to free some entries 	*/
	/* until will have enough space for our entry. 				*/
	/* Also we have	to be sure that the sum of all data blocks	*/
	/* isn't greater than the maximum size specified by the u-	*/
	/* ser at file system creation time; in this case we also  	*/
	/* must free some entries and their data blocks to make so- */
	/* me room for this entry. */
	
	/* 1) If there is not enough space in the FIFO queue, we have to free some space. */

	/* 2) If we reached the max size of allowed data blocks, as specified by user at */
	/* file system creation time; therefore we must free some of the oldest files.*/


	if (ext3u_free_old_entries(u_inode, new_entry)) {
		goto err_exit;
	}

	/* Read the ext3u superblock. */
	bh = ext3_bread(NULL, u_inode, 0, 0, &err);
	if (!bh) {
		goto err_exit;
	}
	
	usb = (struct ext3u_super_block *) bh->b_data;	

	/* We can finally write, there is enough free space in the FIFO*/
	/* Update FIFO pointers. */
	new_entry->d_previous.r_block = usb->s_fifo.f_last.r_block;
	new_entry->d_previous.r_offset = usb->s_fifo.f_last.r_offset;
	new_entry->d_previous.r_size = usb->s_fifo.f_last.r_size;
	new_entry->d_next.r_block = 0;
	new_entry->d_next.r_offset = 0;
	new_entry->d_next.r_size = 0;
	
	block = usb->s_fifo.f_last_block;
	offset = usb->s_fifo.f_last_offset;

	blk_bh = ext3_bread(NULL, u_inode, block, 0, &err);
	if (!blk_bh) {
		goto err_exit;
	}	

	/* First entry in the FIFO ? */
	if (EXT3u_FIFO_NULL((&usb->s_fifo.f_first))) {

		usb->s_fifo.f_first.r_block = block;
		usb->s_fifo.f_first.r_real_block = blk_bh->b_blocknr;
		usb->s_fifo.f_first.r_size = new_entry->d_size; 
		usb->s_fifo.f_first.r_offset = offset;
	}

	/* This is the first entry starting in this */
	/* block so we must update the block header.*/
	if (*((int*)blk_bh->b_data) == 0) {
		*((int*)blk_bh->b_data) = offset;
	}

	r_update.r_block = block;
	r_update.r_real_block = blk_bh->b_blocknr;
	r_update.r_offset = offset;
	r_update.r_size = new_entry->d_size;

	src = (char*) new_entry;
	remaining = new_entry->d_size;

	handle = ext3_journal_start(u_inode, EXT3_SINGLEDATA_TRANS_BLOCKS);
	if (IS_ERR(handle)) {
		goto err_exit;
	}
		
	while (remaining > 0) {
	
		dest = (char*) (blk_bh->b_data + offset);
		to_copy = MIN(usb->s_block_size - offset, remaining);	

		memcpy(dest, src, to_copy);
		
		ext3_journal_get_write_access(handle, blk_bh);
		ext3_journal_dirty_metadata(handle, blk_bh);
		sync_dirty_buffer(blk_bh);
		brelse(blk_bh);

		src+=to_copy;
		remaining -= to_copy;
		end_offset = offset + to_copy;

		/* We need at least another block. */
		if (remaining) {		
			block = (block % usb->s_fifo.f_blocks) + 1;
			offset = EXT3u_BLOCK_HEADER_SIZE;
			blk_bh = ext3_bread(NULL, u_inode, block, 0, &err);
			if (!blk_bh) {
				goto err_exit;
			}
		}
	}
	end_block = block;

	if ((usb->s_block_size - end_offset) < EXT3u_WRITE_MIN) {
		usb->s_fifo.f_free -= (usb->s_block_size - end_offset);
		end_block = (end_block % usb->s_fifo.f_blocks) + 1;
		end_offset = EXT3u_BLOCK_HEADER_SIZE;
	}

	/* If this isn't the first entry, update the 'next' fifo pointer. */
	if (!EXT3u_FIFO_NULL(&(usb->s_fifo.f_last))){
		r_target.r_block = usb->s_fifo.f_last.r_block;
		r_target.r_real_block = usb->s_fifo.f_last.r_real_block;
		r_target.r_offset = usb->s_fifo.f_last.r_offset;
		r_target.r_size = usb->s_fifo.f_last.r_size;

		err = ext3u_update_entry(NULL, u_inode, &r_target, &r_update, EXT3u_UPDATE_NEXT);
		if (err) {
			goto err_exit;
		}	
	}	

	/* Update FIFO information. */
	usb->s_fifo.f_last.r_block = r_update.r_block;
	usb->s_fifo.f_last.r_real_block = r_update.r_real_block;
	usb->s_fifo.f_last.r_offset = r_update.r_offset;
	usb->s_fifo.f_last.r_size = r_update.r_size;
	usb->s_fifo.f_last_block = end_block;
	usb->s_fifo.f_last_offset = end_offset;
	usb->s_fifo.f_free -= new_entry->d_size; 
	usb->s_del.d_file_count++;
	usb->s_del.d_current_size += new_entry->d_inode.i_size;

	/* Write the ext3u superblock. */
	ext3_journal_get_write_access(handle, bh);
	ext3_journal_dirty_metadata(handle, bh);
	sync_dirty_buffer(bh);
	brelse(bh);

	ext3_journal_stop(handle);

err_exit:
	ext3u_unlock(u_inode);
	iput(u_inode);

free_and_exit:
	kfree(buf);
	return err;
}


/**
 * @brief Search all the entries between 'start' and 'end' checking 
 * for a pathname that equals 'path'. If found return the en-
 * try, after have deleted it form disk and updated the FIFO 
 * pointers and the ext3u_superblock.
 *
 * @param handle The handle of the transaction.
 * @param u_inode The ext3u root inode.
 * @param usb Pointer to the ext3u superblock.
 * @param start The entry in the FIFO from which we start searching.
 * @param end The entry in the FIFO list where we stop the search.
 * @param path The path of the file's entry we are looking for.
 * @entries It returns the number of the entries seen during the search.
 * 
 * @return On success it returns a pointer to the entry found, an error otherwise. 
 */

static struct ext3u_del_entry * ext3u_search_entry(handle_t * handle,
												 struct inode * u_inode,
												 struct ext3u_super_block * usb,
												 struct ext3u_record * start, 
												 struct ext3u_record * end, 
												 const char * path,
												 int * entries)
{	
	struct buffer_head * bh;
	struct ext3u_del_entry_header * dh;
	struct ext3u_del_entry * de = &ext3u_de_remove;

	int err, remaining, to_copy, copied;
	unsigned int block, block_size, hash;
	__u16 offset;
	void *src, *dest;

	memset(de, 0, sizeof(struct ext3u_del_entry));

	hash = ext3u_hash(path, strlen(path));

	block = start->r_block;
	offset = start->r_offset;
	block_size = usb->s_block_size;

	bh = ext3_bread(NULL, u_inode, block, 0, &err);
	if (!bh) {
		return ERR_PTR(-EIO);
	}

	do {	
		/* The header cannot be splitted across blocks. */
		dh = (struct ext3u_del_entry_header *) (bh->b_data + offset);
		(*entries)++;

		/* First check the hash. */
		if (dh->d_hash == hash) {
				
			dest = (void*) de;
			src = (void*)(bh->b_data + offset);

			memcpy(dest, src, EXT3u_DEL_HEADER_SIZE);

			remaining =	de->d_size - EXT3u_DEL_HEADER_SIZE; 
			copied = EXT3u_DEL_HEADER_SIZE;
			offset += EXT3u_DEL_HEADER_SIZE;
			dest += copied;

			/* The rest of the entry could be splited across    */
			/* two or more blocks, so loop until read all the   */
			/* bytes, eventualy reading more consecutive blocks */
				
			while (remaining > 0) {
			
				to_copy = MIN(block_size - offset, remaining);

				src = (void *) (bh->b_data + offset);
				memcpy(dest, src, to_copy);
				dest+= to_copy;
	
				offset = (offset + to_copy) % block_size;
				copied += to_copy;
				remaining -= to_copy;
			
				/* If we reach the end of the block and */
				/* start reading from the next block.  */
				if ((offset == 0) && (remaining != 0)) {	
					offset = EXT3u_BLOCK_HEADER_SIZE;
					block = (block) % usb->s_fifo.f_blocks + 1;

					bh = ext3_bread(NULL, u_inode, block, 0, &err);
					if (!bh) {
						return ERR_PTR(-EIO);
					}
				}
			}
			/* Now we can check the paths. */		
			if (!strncmp(path, de->d_path, PATH_MAX)) { 
				/* Check if user has the permission to restore this file */
				if (ext3u_permission(de->d_uid, de->d_mode, MAY_WRITE) == -EPERM)  {
					return ERR_PTR(-EPERM);
				} else
					return de;
			}
		}
		/* Read the next entry. */
		
		/* Read a new block if it's different. */
		if (dh->d_next.r_block != block) {
			brelse(bh);
			
			block = dh->d_next.r_block;

			bh = ext3_bread(NULL, u_inode, block, 0, &err);
			if (!bh) {
				return ERR_PTR(-EIO);
			}
		}
		offset = dh->d_next.r_offset;
		
	} while ( (block != end->r_block && offset != end->r_offset) );
		
	return ERR_PTR(-ENOENT);
}


/** 	
 * @brief This function is used when a file is restored. 
 * We perform the search starting from the and of the list
 * and going backward. We are assuming that the file the 
 * user wants to restore is one of the last deleted files.
 * This should be the most common case when un 'undelete' 
 * is needed.
 *
 * @param handle The handle of this transaction.
 * @param u_inode The ext3u root inode.
 * @param path The path of the file we are looking for.
 *
 * @return On success it returns the entry found, otherwise an error.
 */

struct ext3u_del_entry * ext3u_find_entry(handle_t * handle, struct inode * u_inode, const char * path)
{
	struct buffer_head * bh;
	struct ext3u_super_block * usb;
	struct ext3u_record start_record, end_record;
	struct ext3u_del_entry * de = NULL;
	
	int last_block, first_block, block, err, i = 0, entries = 0;
	__u32 offset, total_entries;

	/* Read the ext3u_superblock. */
	bh = ext3_bread(NULL, u_inode, 0, 0, &err);
	if (!bh) {
		return ERR_PTR(-EIO);
	}
	
	usb = (struct ext3u_super_block *) bh->b_data;	
	ext3u_print_super_block(usb);
	
	first_block = usb->s_fifo.f_first.r_block; 
	last_block = usb->s_fifo.f_last.r_block; 
	total_entries = usb->s_del.d_file_count;
	
	block = last_block;
	/* Circular shift on the list back of EXT3u_DISK_CACHE_SIZE positions. */
	for (i = 0; i < EXT3u_DISK_CACHE_SIZE && block != first_block; i++) {
		if ((--block) == 0)
			block = usb->s_fifo.f_blocks;
	}
	
	end_record.r_block =  0;
	end_record.r_offset = 0;	

	while (entries < total_entries) {

		do {
			
			/* Read the offset of the first entry in this block. */
			bh = ext3_bread(NULL, u_inode, block, 0, &err);
			if (!bh) {
				ext3u_debug("Cannot read ext3u_superblock\n");
				return ERR_PTR(-EIO);
			}
		
			offset = *((unsigned int *)bh->b_data);
			brelse(bh);

			/* No entry starting in this block. */
			if (offset == 0) {
				if (block == last_block)
					return ERR_PTR(-ENOENT);
					
				block = (block % usb->s_fifo.f_blocks) + 1;
			} else 
				break;

		} while (block != last_block);

		start_record.r_block = block;
		start_record.r_offset = offset;
		
		de = ext3u_search_entry(handle, u_inode, usb, &start_record, &end_record, path, &entries);
			
		if (de) 
			return de;
		else {
			end_record.r_block =  start_record.r_block;
			end_record.r_offset =  start_record.r_offset;
		}

		block = last_block;

		/* Circular shift on the list back of EXT3u_DISK_CACHE_SIZE positions. */
		for (i = 0; i < EXT3u_DISK_CACHE_SIZE && block != first_block; i++) {
			if ((--block) == 0)
				block = usb->s_fifo.f_blocks;
		}
		last_block = block;

	}
	
	return ERR_PTR(-ENOENT);
}


/**
 * @brief Walk through all entries of the FIFO list, starting from the begining,
 * and search the entry corresponding to 'path'.
 *
 * @param handle The handle of this transaction.
 * @param u_inode Pointer to the ext3u root inode.
 * @param path The path of the file we are looking for.
 *
 * @return If found it returns the entry, otherwise an error.
 */

struct ext3u_del_entry * ext3u_get_entry(handle_t * handle, struct inode * u_inode, const char * path)
{
	struct buffer_head * bh;
	struct ext3u_super_block * usb;
	struct ext3u_record start_entry, end_entry;
	
	int err;
	__u32 entries = 0;
	
	/* Read the ext3u_superblock. */
	bh = ext3_bread(NULL, u_inode, 0, 0, &err);
	if (!bh) {
		return  ERR_PTR(-EIO);
	}
	
	usb = (struct ext3u_super_block *) bh->b_data;	

	/* FIFO list is empty*/
	if (!usb->s_del.d_file_count) {
		return ERR_PTR(-ENOENT);
	}

	start_entry.r_block = usb->s_fifo.f_first.r_block;
	start_entry.r_offset = usb->s_fifo.f_first.r_offset;

	end_entry.r_block = 0;
	end_entry.r_offset = 0;

	return (ext3u_search_entry(handle, u_inode, usb, &start_entry, &end_entry, path, &entries));

}

/**
 * @brief Restore a deleted file.
 * 
 * @param sb Pointer to the ext3u superblock.
 * @param path Full path of the file to be restored.
 * @param where Optional path of the directory where the file will be restored.
 *
 * @return On success returns zero, otherwise a negative integer specifying the error.
 */

int ext3u_urm(struct super_block* sb, char * path, char * where)
{
	struct inode * u_inode;
	struct buffer_head *bh;
	struct ext3u_super_block * usb;
	struct ext3u_del_entry * de;
	handle_t * handle;
	char * dir_path, * file_name;
	struct dentry * parent;
	int err, may_create = 1;

	/* Read the ext3u root inode. */
	u_inode = ext3_iget(sb, EXT3u_UNDEL_DIR_INO);
	if (!u_inode) {
		return  -EIO;
	}
	
	/* */
	ext3u_lock(u_inode);

	handle = ext3_journal_start(u_inode, 	EXT3_DATA_TRANS_BLOCKS(sb) + 
											EXT3_INDEX_EXTRA_TRANS_BLOCKS + 3 + 
											2 * EXT3_QUOTA_INIT_BLOCKS(sb));

	if (IS_ERR(handle)) {
		ext3u_unlock(u_inode);
		iput(u_inode);
		return -EIO;
	}

	/* Find the entry corresponding to 'path' */
	de = ext3u_get_entry(handle, u_inode, path);
	/*de = ext3u_find_entry(handle, u_inode, path);*/
	if (IS_ERR(de)) {
		ext3_journal_stop(handle);
		ext3u_unlock(u_inode);
		iput(u_inode);
		return PTR_ERR(de);
	}
	
	file_name = ext3u_get_file_name(de);
	
	/* If it was specified the directory where the file */
	/* must be restored we have to check first if exists */
	/* Return -ENODATA if the directory does not exist, and*/
	/* -EPERM if the user does not have the write  permission */
	if (where) {
		err = ext3u_lookup(where, sb, &may_create);
		if (err) {
			ext3_journal_stop(handle);
			ext3u_unlock(u_inode);
			iput(u_inode);
			return (err ==-EPERM ? err: -ENODATA);
		}
		dir_path = where;
	} else if (*(de->d_path) == '\0') {
			dir_path = "/";
	} else
		dir_path = de->d_path;

	/* Check if the user has permission to restore the file. */
	err = ext3u_lookup(dir_path, sb, &may_create);

	if ( (err == -EPERM) || ((err == -ENOENT)&&(!may_create)) ) {
		ext3_journal_stop(handle);
		ext3u_unlock(u_inode);
		iput(u_inode);
		return -EPERM;
	}

	/* Get the dentry of the directory where the file has to be restored.*/
	parent = ext3u_get_target_directory(sb, dir_path);

	if (IS_ERR(parent)) {
		ext3_journal_stop(handle);
		ext3u_unlock(u_inode);
		iput(u_inode);
		return -EIO;
	}
	/* Rrestore the file. */
	err = ext3u_create(parent, file_name, de);
	dput(parent);
	
	if (err) { 
		ext3_journal_stop(handle);
		ext3u_unlock(u_inode);
		iput(u_inode);
		return err;
	}
	/* Delete the entry */
	ext3u_delete_entry(handle, u_inode, de);

	/* Update the ext3u_superblock. */
	bh = ext3_bread(NULL, u_inode, 0, 0, &err);
	if (!bh) {
		ext3_journal_stop(handle);
		ext3u_unlock(u_inode);
		iput(u_inode);
		return -EIO;
	}
	
	usb = (struct ext3u_super_block *) bh->b_data;	

	ext3u_update_superblock(usb, de, EXT3u_UPDATE_DELETE);	
	ext3_journal_get_write_access(handle, bh);
	ext3_journal_dirty_metadata(handle, bh);
	ext3_journal_stop(handle);
	sync_dirty_buffer(bh);
	brelse(bh);	

	ext3u_unlock(u_inode);
	iput(u_inode);
	return err;
}

