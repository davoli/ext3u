/*  linux/fs/ext3/namei.h
 *
 * Copyright (C) 2005 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
*/
#include "undel.h"
extern struct dentry *ext3_get_parent(struct dentry *child);

int ext3u_lookup(char * path, struct super_block * sb, int * create);
int ext3u_create(struct dentry * parent, char * name, struct ext3u_del_entry * de);
struct dentry * ext3u_get_target_directory(struct super_block * sb, char * path);
