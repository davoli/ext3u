# ext3u
Undelete support for the ext3 linux filesystem

=== 

Description

Support for recovery of deleted files in the ext3 ﬁlesystem. 
This functionality is based on a new kernel module, *ext3u*, 
that adds to the original ext3 ﬁle system a series of efficient 
structures that permit to manage and to recovery the deleted ﬁles.

Furthermore the implementation contains a custom version of the _e2fsprogs_ tool with _mkfs.ext3_ and _fsck_ customized for creation and for the integrity’s check of new ﬁle system. 

It’s included also several user commands like _uls_, to obtain information about deleted (and recoverable) ﬁles, _urm_, for recovery of ﬁles and _ustats_ that permits to control memory status of deleted ﬁles and other statistical information.
