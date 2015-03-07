/*
 * linux/fs/ext3/ioctl.c
 *
 * Copyright (C) 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 * Copyright (C) 2009  
 * undel extensions 
 *
 */

#include <linux/fs.h>
#include <linux/jbd.h>
#include <linux/capability.h>
#include <linux/ext3_fs.h>
#include <linux/ext3_jbd.h>
#include <linux/mount.h>
#include <linux/time.h>
#include <linux/compat.h>
#include <linux/smp_lock.h>
#include <asm/uaccess.h>
#include "acl.h"
#include "undel.h"

/**
 * @brief Implements 'urm' command in kernel space.
 *
 * @param i_sb Pointer to super block of partition.
 * @param urm_info Pointer to ext3u_uls_info structure.
 *
 * @return On success it returns zero, otherwise a value different from zero indicating the error.
 */

static int ext3u_do_urm(struct super_block * i_sb, struct ext3u_urm_info * urm_info) 
{
	/* If 'urm_info->u_dpath' is non-null, we first check if */
	/* the user has the WRITE priviledges on that directory. */

	urm_info->u_errcode = ext3u_urm(i_sb, urm_info->u_path, urm_info->u_dpath);
	return urm_info->u_errcode;
}

/**
 * @brief Implements the 'ustats' command in kernel space.
 *
 * @param i_sb Pointer to super block of partition.
 * @param ustats_info Pointer to ext3u_uls_info structure.
 *
 * @return On success it returns zero, otherwise a value different from zero indicating the error.
 */

static int ext3u_do_ustats(struct super_block * i_sb, struct ext3u_ustats_info * ustats_info) 
{
	struct ext3u_super_block * usb;
	struct buffer_head * bh;
	struct inode * u_inode;
	
	/* Get the undelete_inode */
	u_inode = ext3_iget(i_sb, EXT3u_UNDEL_DIR_INO);
	if (!u_inode) {
		ustats_info->u_errcode = -EIO;
		goto exit;
	}
	
	ext3u_lock(u_inode);
	
	/* Read the undelete superblock. */
	bh = ext3_bread(NULL, u_inode, 0, 0, &(ustats_info->u_errcode));
	if (!bh) {
		ustats_info->u_errcode = -EIO;
	 	ext3u_unlock(u_inode);
		goto exit;
	}
	
	usb = (struct ext3u_super_block *) bh->b_data;	

	/* Fill ext3u_ustats_info structure */
	
	ustats_info->u_inode_size = usb->s_inode_size;
	ustats_info->u_block_size = usb->s_block_size;
	ustats_info->u_fifo_free = usb->s_fifo_free;
	ustats_info->u_fifo_blocks = usb->s_fifo.f_blocks;
	ustats_info->u_max_size = usb->s_del.d_max_size;
	ustats_info->u_current_size = usb->s_del.d_current_size;
	ustats_info->u_file_count = usb->s_del.d_file_count;
	ustats_info->u_dir_count = usb->s_del.d_dir_count;	
	
	/* Errcode */
	ustats_info->u_errcode = 0;
	
exit:
	
	ext3u_unlock(u_inode);
	
	iput(u_inode);
	return ustats_info->u_errcode;
}


/**
 * @brief Implements the 'uls' command in kernel space.
 *
 * @param i_sb Pointer to super block of partition.
 * @param uls_info Pointer to ext3u_uls_info structure.
 *
 * @return On success it returns zero, otherwise a value different from zero indicating the error.
 */

static int ext3u_do_uls(struct super_block * i_sb, struct ext3u_uls_info * uls_info)
{
	struct inode * u_inode;
	struct buffer_head * bh;	
	struct ext3u_super_block * usb = NULL;
	struct ext3u_uls_entry uls_entry;	
	struct ext3u_del_entry * de;
	unsigned int long block_size = i_sb->s_blocksize; 
	
	__u32 block;
	__u16 offset;	

	int err = 0, remaining, uls_buffer_remaining, uls_buffer_fill, to_copy, needed;
	char *src, *dest;
	
	if ( ( u_inode = ext3_iget(i_sb, EXT3u_UNDEL_DIR_INO) ) == NULL ) {
		uls_info->u_errcode = -EIO;
		return -EIO;
	}
	
	ext3u_lock(u_inode);

	de =  &ext3u_de_ioctl;

	block = uls_info->u_next_record.r_block;
	offset = uls_info->u_next_record.r_offset;
	
	/* First time, read super block for location of FIFO first entry */
	
	bh = ext3u_read_super(u_inode);
	if (IS_ERR(bh )) {
		err = -EIO;
		goto out;
	}
	
	usb = (struct ext3u_super_block * )bh->b_data;
	
	if ( (block == 0) && (offset == 0)) {
	
		if (usb->s_del.d_file_count == 0) {
			uls_info->u_files = 0;
			brelse(bh);
			goto out;
		}

		block = usb->s_fifo.f_first.r_block;
		offset = usb->s_fifo.f_first.r_offset;
	}
	
	brelse(bh);
	
	uls_info->u_files = 0;
	uls_buffer_fill = 0; 
	uls_buffer_remaining = uls_info->u_buffer_length;

	bh = ext3_bread(NULL, u_inode, block, 0, &err);
	if (!bh) {
		err = -EIO;
		goto out;
	}

	do {
		memset(de, 0 , sizeof(struct ext3u_del_entry));
		dest = (char*)de;

		/* The header cannot be split across blocks. */
		memcpy(dest, (char *) (bh->b_data + offset), EXT3u_DEL_HEADER_SIZE);
		dest+=EXT3u_DEL_HEADER_SIZE;	

		/* Space in the buffer needed to fill this entry */
		/* path length (int) + full path + eventualy long listing attributes*/
		needed = EXT3u_ULS_ENTRY_SIZE + (de->d_path_length + 1) + (uls_info->u_ll * EXT3u_ULL_ENTRY_SIZE);
			
		/* buffer has not a sufficient length, return an error and the new length needed */
		if ( needed > uls_info->u_buffer_length) {	
			err = -ENOMEM;
			uls_info->u_buffer_length = needed;
			uls_info->u_files = 0;
			uls_info->u_next_record.r_block = block; 
			/* Real offset is withous offset */
			uls_info->u_next_record.r_offset = offset;
			goto out;
		}
				
		/* Not enough space, return to the user. */	
		if (needed > uls_buffer_remaining) {	
			uls_info->u_errcode = 0;
			uls_info->u_next_record.r_block = block; 
			/* Real offset is withous offset */
			uls_info->u_next_record.r_offset = offset;
			goto out;
		}
		
		remaining =	de->d_size - EXT3u_DEL_HEADER_SIZE; 
		offset += EXT3u_DEL_HEADER_SIZE;
		
		/* Check the permissions first. */
		if ( (ext3u_permission(de->d_uid, de->d_mode,  MAY_WRITE) == 0 )) {	
			/* bytes, eventualy reading more consecutive blocks */	

			while (remaining > 0) {	
		
				to_copy = MIN(block_size - offset, remaining);
				src = (char *) (bh->b_data + offset);
				memcpy(dest, src, to_copy);
				
				dest += to_copy;
				offset = (offset + to_copy) % block_size;
				remaining -= to_copy;
				
				/* We reached the end of the block so we */
				/* must start reading from the next block. */
				if ((offset == 0) && (remaining != 0)) {

					offset = EXT3u_BLOCK_HEADER_SIZE;
					block = (block % usb->s_fifo.f_blocks) + 1;
					
					brelse(bh);

					bh = ext3_bread(NULL, u_inode, block, 0, &err);
					if (!bh) {
						err =  -EIO;
						goto out;
					}
				}
			}

			uls_buffer_remaining -= needed;
			uls_info->u_files++;

			/* Fill the buffer with the entry's information. */	
			if ( uls_info->u_ll == 1 ) {
				uls_entry.u_path_length = de->d_path_length;
				uls_entry.u_mtime.tv_sec = de->d_inode.i_mtime;
				uls_entry.u_size = de->d_inode.i_size;
				uls_entry.u_mode = de->d_inode.i_mode;
				uls_entry.u_uid = de->d_inode.i_uid;
				uls_entry.u_gid = de->d_inode.i_gid;
				uls_entry.u_nlink = de->d_inode.i_links_count;
				memcpy((char*)(uls_info->u_buffer+uls_buffer_fill), &uls_entry, sizeof(struct ext3u_uls_entry));
				uls_buffer_fill += sizeof(struct ext3u_uls_entry);
			}
			else {
				memcpy((char*)(uls_info->u_buffer+uls_buffer_fill), (char*)(&de->d_path_length), 2);
				uls_buffer_fill += 2;
			}
	
			memcpy((char*)(uls_info->u_buffer+uls_buffer_fill), de->d_path, de->d_path_length + 1);

			uls_buffer_fill += (de->d_path_length + 1);

			/* -n option is enabled. */
			if ( uls_info->u_max_files > 0 ) {
				uls_info->u_read_files++;
				if ( uls_info->u_read_files >= uls_info->u_max_files ) {
					uls_info->u_next_record.r_block = EXT3u_FIFO_END; 
					uls_info->u_next_record.r_offset = EXT3u_FIFO_END;
					brelse(bh);
					goto out;
				}
			}
			
			/* User buffer completly full. */
			if (uls_buffer_remaining == 0) {
				uls_info->u_next_record.r_block = de->d_next.r_block; 
				uls_info->u_next_record.r_offset = de->d_next.r_offset;
				brelse(bh);
				goto out;
			}
		}
	
		/* End of FIFO list is reached */
		if ((de->d_next.r_block == EXT3u_FIFO_END) && (de->d_next.r_offset == EXT3u_FIFO_END)) {	
			uls_info->u_next_record.r_block = EXT3u_FIFO_END; 
			uls_info->u_next_record.r_offset = EXT3u_FIFO_END;
			brelse(bh);
			goto out;
		}
		
		offset = de->d_next.r_offset;

		if ( de->d_next.r_block != block ) {
			block = de->d_next.r_block;
			brelse(bh);

			bh = ext3_bread(NULL, u_inode, block, 0, &err);
			if (!bh) {
				err = -EIO;
				goto out;
			}
		}
	} while(1);
							
out:

	ext3u_unlock(u_inode);
	iput(u_inode);
	uls_info->u_errcode = err;
	return err;
}


/**
 * Forward to kernel management of uls command.
 * @param i_sb Pointer to super block of partition.
 * @param arg Pointer to buffer obtained by user space.
 *
 * @return On success it returns zero, otherwise a value different from zero indicating the error.
 */

static int ext3u_ioctl_uls(struct super_block * i_sb, unsigned long arg) 
{
	struct ext3u_uls_info uls_info;
	
	/* Copy info contained into the user space buffer and cast it on apposite structure */
	copy_from_user(&uls_info, (int __user *) arg, sizeof(struct ext3u_uls_info));
	
	/* ext3u uls command */
	ext3u_do_uls(i_sb, &uls_info);
	
	/* Return to user space buffer information filled by previous command */
	return copy_to_user((int __user *) arg, &uls_info, sizeof(struct ext3u_uls_info));
}

/**
 * Forward to kernel management of ustats command.
 * @param i_sb Pointer to super block of partition.
 * @param arg Pointer to buffer obtained by user space.
 * @return Result of operation.
 */

static int ext3u_ioctl_ustats(struct super_block * i_sb, unsigned long arg) 
{
	struct ext3u_ustats_info ustats_info;
	
	/* Copy info contained into the user space buffer and cast it on apposite structure */
	copy_from_user(&ustats_info, (int __user *) arg, sizeof(struct ext3u_ustats_info));
	
	/* ext3u ustats command */
	ext3u_do_ustats(i_sb, &ustats_info);
	
	/* Return to user space buffer information filled by previous command */
	return copy_to_user((int __user *) arg, &ustats_info, sizeof(struct ext3u_ustats_info));
}

/**
 * Forward to kernel management of urm command.
 * @param i_sb Pointer to super block of partition.
 * @param arg Pointer to buffer obtained by user space.
 * @return Result of operation.
 */

static int ext3u_ioctl_urm(struct super_block * i_sb, unsigned long arg) {
	
	struct ext3u_urm_info urm_info;
	
	/* Copy info contained into the user space buffer and cast it on apposite structure */
	copy_from_user(&urm_info, (int __user *) arg, sizeof(struct ext3u_urm_info));
	
	/* ext3u urm command */
	ext3u_do_urm(i_sb, &urm_info);

	
	/* Return to user space buffer information filled by previous command */
	return copy_to_user((int __user *) arg, &urm_info, sizeof(struct ext3u_urm_info));

}

int ext3_ioctl (struct inode * inode, struct file * filp, unsigned int cmd, unsigned long arg)
{
    struct ext3_inode_info *ei = EXT3_I(inode);
	unsigned int flags;
	unsigned short rsv_window_size;

	switch (cmd) {
	/* undelete update: Added three switch case for manage	*
	 * all undelete operations (uls, ustats, urm).			*
	 * Each case provide to call the correct function.		*/
    case EXT3_UNDEL_IOC_ULS: {
		if (EXT3_HAS_INCOMPAT_FEATURE(inode->i_sb, EXT3u_FEATURE_COMPAT_UNDELETE))
			return -EOPNOTSUPP;
		else
			return ext3u_ioctl_uls(inode->i_sb, arg);
	} 
	case EXT3_UNDEL_IOC_USTATS:{
		if (EXT3_HAS_INCOMPAT_FEATURE(inode->i_sb, EXT3u_FEATURE_COMPAT_UNDELETE))
			return -EOPNOTSUPP;
		else
    		return ext3u_ioctl_ustats(inode->i_sb, arg);
    }
    case EXT3_UNDEL_IOC_URM: {
		if (EXT3_HAS_INCOMPAT_FEATURE(inode->i_sb, EXT3u_FEATURE_COMPAT_UNDELETE))
			return -EOPNOTSUPP;
		else
    		return ext3u_ioctl_urm(inode->i_sb, arg);
	}
	case EXT3_IOC_GETFLAGS:
		ext3_get_inode_flags(ei);
		flags = ei->i_flags & EXT3_FL_USER_VISIBLE;
		return put_user(flags, (int __user *) arg);
	case EXT3_IOC_SETFLAGS: {
		handle_t *handle = NULL;
		int err;
		struct ext3_iloc iloc;
		unsigned int oldflags;
		unsigned int jflag;

		err = mnt_want_write(filp->f_path.mnt);
		if (err)
			return err;

		if (!is_owner_or_cap(inode)) {
			err = -EACCES;
			goto flags_out;
		}

		if (get_user(flags, (int __user *) arg)) {
			err = -EFAULT;
			goto flags_out;
		}

		if (!S_ISDIR(inode->i_mode))
			flags &= ~EXT3_DIRSYNC_FL;

		mutex_lock(&inode->i_mutex);
		/* Is it quota file? Do not allow user to mess with it */
		if (IS_NOQUOTA(inode)) {
			mutex_unlock(&inode->i_mutex);
			err = -EPERM;
			goto flags_out;
		}
		oldflags = ei->i_flags;

		/* The JOURNAL_DATA flag is modifiable only by root */
		jflag = flags & EXT3_JOURNAL_DATA_FL;

		/*
		 * The IMMUTABLE and APPEND_ONLY flags can only be changed by
		 * the relevant capability.
		 *
		 * This test looks nicer. Thanks to Pauline Middelink
		 */
		if ((flags ^ oldflags) & (EXT3_APPEND_FL | EXT3_IMMUTABLE_FL)) {
			if (!capable(CAP_LINUX_IMMUTABLE)) {
				mutex_unlock(&inode->i_mutex);
				err = -EPERM;
				goto flags_out;
			}
		}

		/*
		 * The JOURNAL_DATA flag can only be changed by
		 * the relevant capability.
		 */
		if ((jflag ^ oldflags) & (EXT3_JOURNAL_DATA_FL)) {
			if (!capable(CAP_SYS_RESOURCE)) {
				mutex_unlock(&inode->i_mutex);
				err = -EPERM;
				goto flags_out;
			}
		}


		handle = ext3_journal_start(inode, 1);
		if (IS_ERR(handle)) {
			mutex_unlock(&inode->i_mutex);
			err = PTR_ERR(handle);
			goto flags_out;
		}
		if (IS_SYNC(inode))
			handle->h_sync = 1;
		err = ext3_reserve_inode_write(handle, inode, &iloc);
		if (err)
			goto flags_err;

		flags = flags & EXT3_FL_USER_MODIFIABLE;
		flags |= oldflags & ~EXT3_FL_USER_MODIFIABLE;
		ei->i_flags = flags;

		ext3_set_inode_flags(inode);
		inode->i_ctime = CURRENT_TIME_SEC;

		err = ext3_mark_iloc_dirty(handle, inode, &iloc);
flags_err:
		ext3_journal_stop(handle);
		if (err) {
			mutex_unlock(&inode->i_mutex);
			return err;
		}

		if ((jflag ^ oldflags) & (EXT3_JOURNAL_DATA_FL))
			err = ext3_change_inode_journal_flag(inode, jflag);
		mutex_unlock(&inode->i_mutex);
flags_out:
		mnt_drop_write(filp->f_path.mnt);
		return err;
	}
	case EXT3_IOC_GETVERSION:
	case EXT3_IOC_GETVERSION_OLD:
      return put_user(inode->i_generation, (int __user *) arg);
	case EXT3_IOC_SETVERSION:
	case EXT3_IOC_SETVERSION_OLD: {
		handle_t *handle;
		struct ext3_iloc iloc;
		__u32 generation;
		int err;

		if (!is_owner_or_cap(inode))
			return -EPERM;
		err = mnt_want_write(filp->f_path.mnt);
		if (err)
			return err;
		if (get_user(generation, (int __user *) arg)) {
			err = -EFAULT;
			goto setversion_out;
		}
		handle = ext3_journal_start(inode, 1);
		if (IS_ERR(handle)) {
			err = PTR_ERR(handle);
			goto setversion_out;
		}
		err = ext3_reserve_inode_write(handle, inode, &iloc);
		if (err == 0) {
			inode->i_ctime = CURRENT_TIME_SEC;
			inode->i_generation = generation;
			err = ext3_mark_iloc_dirty(handle, inode, &iloc);
		}
		ext3_journal_stop(handle);
setversion_out:
		mnt_drop_write(filp->f_path.mnt);
		return err;
	}
#ifdef CONFIG_JBD_DEBUG
	case EXT3_IOC_WAIT_FOR_READONLY:
		/*
		 * This is racy - by the time we're woken up and running,
		 * the superblock could be released.  And the module could
		 * have been unloaded.  So sue me.
		 *
		 * Returns 1 if it slept, else zero.
		 */
		{
			struct super_block *sb = inode->i_sb;
			DECLARE_WAITQUEUE(wait, current);
			int ret = 0;

			set_current_state(TASK_INTERRUPTIBLE);
			add_wait_queue(&EXT3_SB(sb)->ro_wait_queue, &wait);
			if (timer_pending(&EXT3_SB(sb)->turn_ro_timer)) {
				schedule();
				ret = 1;
			}
			remove_wait_queue(&EXT3_SB(sb)->ro_wait_queue, &wait);
			return ret;
		}
#endif
	case EXT3_IOC_GETRSVSZ:
		if (test_opt(inode->i_sb, RESERVATION)
			&& S_ISREG(inode->i_mode)
			&& ei->i_block_alloc_info) {
			rsv_window_size = ei->i_block_alloc_info->rsv_window_node.rsv_goal_size;
			return put_user(rsv_window_size, (int __user *)arg);
		}
		return -ENOTTY;
	case EXT3_IOC_SETRSVSZ: {
		int err;

		if (!test_opt(inode->i_sb, RESERVATION) ||!S_ISREG(inode->i_mode))
			return -ENOTTY;

		err = mnt_want_write(filp->f_path.mnt);
		if (err)
			return err;

		if (!is_owner_or_cap(inode)) {
			err = -EACCES;
			goto setrsvsz_out;
		}

		if (get_user(rsv_window_size, (int __user *)arg)) {
			err = -EFAULT;
			goto setrsvsz_out;
		}

		if (rsv_window_size > EXT3_MAX_RESERVE_BLOCKS)
			rsv_window_size = EXT3_MAX_RESERVE_BLOCKS;

		/*
		 * need to allocate reservation structure for this inode
		 * before set the window size
		 */
		mutex_lock(&ei->truncate_mutex);
		if (!ei->i_block_alloc_info)
			ext3_init_block_alloc_info(inode);

		if (ei->i_block_alloc_info){
			struct ext3_reserve_window_node *rsv = &ei->i_block_alloc_info->rsv_window_node;
			rsv->rsv_goal_size = rsv_window_size;
		}
		mutex_unlock(&ei->truncate_mutex);
setrsvsz_out:
		mnt_drop_write(filp->f_path.mnt);
		return err;
	}
	case EXT3_IOC_GROUP_EXTEND: {
		ext3_fsblk_t n_blocks_count;
		struct super_block *sb = inode->i_sb;
		int err, err2;

		if (!capable(CAP_SYS_RESOURCE))
			return -EPERM;

		err = mnt_want_write(filp->f_path.mnt);
		if (err)
			return err;

		if (get_user(n_blocks_count, (__u32 __user *)arg)) {
			err = -EFAULT;
			goto group_extend_out;
		}
		err = ext3_group_extend(sb, EXT3_SB(sb)->s_es, n_blocks_count);
		journal_lock_updates(EXT3_SB(sb)->s_journal);
		err2 = journal_flush(EXT3_SB(sb)->s_journal);
		journal_unlock_updates(EXT3_SB(sb)->s_journal);
		if (err == 0)
			err = err2;
group_extend_out:
		mnt_drop_write(filp->f_path.mnt);
		return err;
	}
	case EXT3_IOC_GROUP_ADD: {
		struct ext3_new_group_data input;
		struct super_block *sb = inode->i_sb;
		int err, err2;

		if (!capable(CAP_SYS_RESOURCE))
			return -EPERM;

		err = mnt_want_write(filp->f_path.mnt);
		if (err)
			return err;

		if (copy_from_user(&input, (struct ext3_new_group_input __user *)arg,
				sizeof(input))) {
			err = -EFAULT;
			goto group_add_out;
		}

		err = ext3_group_add(sb, &input);
		journal_lock_updates(EXT3_SB(sb)->s_journal);
		err2 = journal_flush(EXT3_SB(sb)->s_journal);
		journal_unlock_updates(EXT3_SB(sb)->s_journal);
		if (err == 0)
			err = err2;
group_add_out:
		mnt_drop_write(filp->f_path.mnt);
		return err;
	}


	default:
		return -ENOTTY;
	}
}

#ifdef CONFIG_COMPAT
long ext3_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct inode *inode = file->f_path.dentry->d_inode;
	int ret;

	/* These are just misnamed, they actually get/put from/to user an int */
	switch (cmd) {
	case EXT3_IOC32_GETFLAGS:
		cmd = EXT3_IOC_GETFLAGS;
		break;
	case EXT3_IOC32_SETFLAGS:
		cmd = EXT3_IOC_SETFLAGS;
		break;
	case EXT3_IOC32_GETVERSION:
		cmd = EXT3_IOC_GETVERSION;
		break;
	case EXT3_IOC32_SETVERSION:
		cmd = EXT3_IOC_SETVERSION;
		break;
	case EXT3_IOC32_GROUP_EXTEND:
		cmd = EXT3_IOC_GROUP_EXTEND;
		break;
	case EXT3_IOC32_GETVERSION_OLD:
		cmd = EXT3_IOC_GETVERSION_OLD;
		break;
	case EXT3_IOC32_SETVERSION_OLD:
		cmd = EXT3_IOC_SETVERSION_OLD;
		break;
#ifdef CONFIG_JBD_DEBUG
	case EXT3_IOC32_WAIT_FOR_READONLY:
		cmd = EXT3_IOC_WAIT_FOR_READONLY;
		break;
#endif
	case EXT3_IOC32_GETRSVSZ:
		cmd = EXT3_IOC_GETRSVSZ;
		break;
	case EXT3_IOC32_SETRSVSZ:
		cmd = EXT3_IOC_SETRSVSZ;
		break;
	case EXT3_IOC_GROUP_ADD:
		break;
	default:
		return -ENOIOCTLCMD;
	}
	lock_kernel();
	ret = ext3_ioctl(inode, file, cmd, (unsigned long) compat_ptr(arg));
	unlock_kernel();
	return ret;
}
#endif
