#include <string>
#include <iostream>
#ifndef __serializableStat_H
#define __serializableStat_H
struct serializableStat
{
    __mode_t v_mode;			/* File mode.  */

    __nlink_t v_nlink;			/* Link count.  */

    __uid_t v_uid;		        /* User ID of the file's owner.	*/

    __gid_t v_gid;		        /* Group ID of the file's group.*/

    __off_t v_size;			    /* Size of file, in bytes.  */

    __blkcnt_t v_blocks;		/* Number 512-byte blocks allocated. */

    __time_t v_atime;			/* Time of last access.  */

    __time_t v_mtime;			/* Time of last modification.  */

    __time_t v_ctime;			/* Time of last status change.  */


    unsigned long long v_ino;

    std::string name;

    __time_t aux_last_write;

};
#endif
