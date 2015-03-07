#
# Makefile for the linux ext3u filesystem's routines.
#

obj-$(CONFIG_EXT3_FS) += ext3u.o

ext3u-y	:= 	balloc.o bitmap.o dir.o file.o fsync.o ialloc.o inode.o \
	   			ioctl.o namei.o super.o symlink.o hash.o resize.o ext3_jbd.o undel.o

ext3u-$(CONFIG_EXT3_FS_XATTR)	 	+= xattr.o xattr_user.o xattr_trusted.o
ext3u-$(CONFIG_EXT3_FS_POSIX_ACL) 	+= acl.o
ext3u-$(CONFIG_EXT3_FS_SECURITY)	+= xattr_security.o

obj-m = ext3u.o
KVERSION = $(shell uname -r)
all:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean
	
