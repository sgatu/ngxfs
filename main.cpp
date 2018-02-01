/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/
/**
    @author Stefan Gatu <stefan.gatu@gmail.com>
*/

/** @file
 *
 * This "filesystem" provides only a single file. The mountpoint
 * needs to be a file rather than a directory. All writes to the
 * file will be discarded, and reading the file always returns
 * \0.
 *
 * Compile with:
 *
 *     gcc -Wall null.c `pkg-config fuse3 --cflags --libs` -o null
 *
 * ## Source code ##
 * \include passthrough_fh.c
 */


#define FUSE_USE_VERSION 31

#include "config.h"

#include <fuse3/fuse.h>
#include <fuse3/fuse_lowlevel.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <iostream>
#include <time.h>
#include "xxHash.h"
#include <inttypes.h>
#include <unordered_map>
#include "serializableStat.h"
#include "RequestsStats.hpp"
#include <libgen.h>
#include "cxxopts.hpp"

#define NOBODY_USER 65534
#define NOGROUP_GROUP 65534


using namespace std;


bool _p_debug = true;

string _p_mount("/mnt/nginx");

unique_ptr<RequestsStats> reqs_counter = unique_ptr<RequestsStats>(new RequestsStats());

unordered_map<unsigned long long, shared_ptr<struct serializableStat>> cache;

XXH64_hash_t root_hash = XXH64("/", 1, 0LL);
XXH64_hash_t access_hash = XXH64("/access.log", 11, 0LL);


static shared_ptr<struct serializableStat> getAttr(XXH64_hash_t h_key)
{
    unordered_map<unsigned long long, shared_ptr<struct serializableStat>>::const_iterator got = cache.find (h_key);
    shared_ptr<struct serializableStat> cache_stbuf = nullptr;
    if(got == cache.end())
    {
        //not found, create new one
        cache_stbuf = make_shared<struct serializableStat>();
        if(h_key == root_hash)
        {
            cache_stbuf->v_mode = S_IFDIR | 0777;
            cache_stbuf->v_nlink = 1;
            cache_stbuf->v_uid = NOBODY_USER;
            cache_stbuf->v_gid = NOGROUP_GROUP;
            cache_stbuf->v_size = 4096;
            cache_stbuf->v_blocks = 1;
            cache_stbuf->v_atime = cache_stbuf->v_mtime = cache_stbuf->v_ctime = time(NULL);
            cache_stbuf->v_ino = (__ino_t)h_key;
            cache_stbuf->name = string("/");
            cache_stbuf->aux_last_write = time(NULL);
        }
        else if(h_key == access_hash)
        {
            cache_stbuf->v_mode = S_IFREG | 0777;
            cache_stbuf->v_nlink = 1;
            cache_stbuf->v_uid = NOBODY_USER;
            cache_stbuf->v_gid = NOGROUP_GROUP;
            cache_stbuf->v_size = 4096;
            cache_stbuf->v_blocks = 1;
            cache_stbuf->v_atime = cache_stbuf->v_mtime = cache_stbuf->v_ctime = time(NULL);
            cache_stbuf->v_ino = (__ino_t)h_key;
            cache_stbuf->name = string("access.log");
            cache_stbuf->aux_last_write = time(NULL);

        }
        cache[h_key] = cache_stbuf;
    }
    else
    {
        cache_stbuf = got->second;
    }
    return cache_stbuf;
}
static shared_ptr<struct serializableStat> getAttr(const char *path)
{
    XXH64_hash_t h_key = XXH64(path, strlen(path), 0LL);
    shared_ptr<struct serializableStat> toR = getAttr(h_key);
    return toR;
}


static int ngxfs_getattr(const char *path, struct stat *stbuf,
                         struct fuse_file_info *fi)
{

    shared_ptr<struct serializableStat> cache_stbuf = getAttr(path);
    if(cache_stbuf == nullptr)
    {
        return -ENOENT;
    }
    stbuf->st_mode  = cache_stbuf->v_mode;
    stbuf->st_nlink = cache_stbuf->v_nlink;
    stbuf->st_uid   = cache_stbuf->v_uid;
    stbuf->st_gid   = cache_stbuf->v_gid;
    stbuf->st_size  = cache_stbuf->v_size;
    stbuf->st_blocks= cache_stbuf->v_blocks;
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = cache_stbuf->v_mtime;
    stbuf->st_ino   = cache_stbuf->v_ino;
    return 0;
}

static int ngxfs_truncate(const char *path, off_t size,
                          struct fuse_file_info *fi)
{
    XXH64_hash_t h_key = XXH64(path, strlen(path), 0LL);
    shared_ptr<struct serializableStat> stats = getAttr(h_key);

    if(stats == nullptr)
    {
        return -ENOENT;
    }
    if(h_key == root_hash)
    {
        return -EOPNOTSUPP;
    }
    reqs_counter->reset();
    return 0;
}

int
ngxfs_rename(const char *old, const char *path, unsigned int flags)
{
    return -EOPNOTSUPP;

}

static int
ngxfs_rmdir(const char *path)
{
    return -EOPNOTSUPP;
}



static int
ngxfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
              off_t offset, struct fuse_file_info *fi,
              enum fuse_readdir_flags flags)
{

    (void) offset;
    (void) fi;
    (void) flags;


    filler(buf, ".", NULL, 0, fuse_fill_dir_flags(0));
    filler(buf, "..", NULL, 0, fuse_fill_dir_flags(0));
    filler(buf, "access.log", NULL, 0, fuse_fill_dir_flags(0));


    return 0;
}

/**
 * Make a directory.
 */
static int
ngxfs_mkdir(const char *path, mode_t mode)
{
    return -EOPNOTSUPP;
}
static int
ngxfs_open(const char *path, struct fuse_file_info *fi)
{
    (void) fi;
    shared_ptr<struct serializableStat> info = getAttr(path);
    if(info == nullptr )
    {
        return -ENOENT;
    }
    return 0;
}
static int
ngxfs_access(const char *path, int mode)
{
    shared_ptr<struct serializableStat> info = getAttr(path);
    if(info == nullptr)
    {
        return -ENOENT;
    }
    return 0;
}
static int ngxfs_release(const char *path, struct fuse_file_info *fi)
{
    (void) fi;
    XXH64_hash_t h_key = XXH64(path, strlen(path), 0LL);
    shared_ptr<struct serializableStat> stats = getAttr(h_key);

    if(stats == nullptr)
    {
        return -ENOENT;
    }
    if(stats->aux_last_write != stats->v_mtime)
    {
        stats->v_mtime = stats->v_atime = stats->aux_last_write;
    }

    return 0;
}
/**
 * Create a new entry with the specified mode.
 */
static int
ngxfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    return -EOPNOTSUPP;
}
static int
ngxfs_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi)
{
    return -EOPNOTSUPP;
}
/**
 * Remove a directory entry.
 */
static int
ngxfs_unlink(const char *path)
{
    return -EOPNOTSUPP;
}
/**
 * Write to a file or path.
 */
static int
ngxfs_write(const char *path,
            const char *buf,
            size_t size, off_t offset, struct fuse_file_info *fi)
{


    XXH64_hash_t h_key = XXH64(path, strlen(path), 0LL);
    shared_ptr<struct serializableStat> stats = getAttr(h_key);
    if(stats == nullptr)
    {
        return -ENOENT;
    }
    reqs_counter->incr(buf[0] - '0');
    return size;
}


/**
 * Read from a file.
 */
static int
ngxfs_read(const char *path, char *buf, size_t size, off_t offset,
           struct fuse_file_info *fi)
{


    XXH64_hash_t h_key = XXH64(path, strlen(path), 0LL);
    shared_ptr<struct serializableStat> stats = getAttr(h_key);
    if(stats == nullptr || h_key != access_hash)
    {
        return -ENOENT;
    }

    return reqs_counter->write(buf);
    //return  strlen(data);
}


/**
 * Change the owner of a file/directory.
 */
static int
ngxfs_chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi)
{
    return -EOPNOTSUPP;

}

/**
 * Change the owner of a file/directory.
 */
static int
ngxfs_chmod(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    return -EOPNOTSUPP;
}


struct ngxfs_fuse_operations : fuse_operations
{
    ngxfs_fuse_operations ()
    {
        getattr    = ngxfs_getattr;
        truncate   = ngxfs_truncate;
        open       = ngxfs_open;
        read       = ngxfs_read;
        write      = ngxfs_write;
        readdir    = ngxfs_readdir;
        mkdir      = ngxfs_mkdir;
        access     = ngxfs_access;
        rmdir      = ngxfs_rmdir;
        create     = ngxfs_create;
        utimens    = ngxfs_utimens;
        unlink     = ngxfs_unlink;
        chown      = ngxfs_chown;
        chmod      = ngxfs_chmod;
        rename     = ngxfs_rename;
        release    = ngxfs_release;
    }
};

static struct ngxfs_fuse_operations ngxfs_oper;

int main(int argc, char *argv[])
{
    cxxopts::Options options("NGXFS", "NGXFS: Nginx status code stats");
    options.add_options()
    ("mount", "Mount point", cxxopts::value<std::string>(_p_mount))
    ("debug", "Debugging active", cxxopts::value<bool>(_p_debug));
    options.parse(argc, argv);

    int args_c_fuse = 6;
    const char* args_fuse[] =
    {
        "fuse-ngxfs", (char*)_p_mount.c_str(),
        "-f","-o",
        "allow_other","-o"
        "auto_unmount","-d",
        NULL
    };
    if(_p_debug)
    {
        args_c_fuse++;
    }


    struct fuse_args args = FUSE_ARGS_INIT(args_c_fuse, args_fuse);
    struct fuse_cmdline_opts opts;
    struct stat stbuf;

    if (fuse_parse_cmdline(&args, &opts) != 0)
        return 1;

    if (!opts.mountpoint)
    {
        fprintf(stderr, "missing mountpoint parameter\n");
        return 1;
    }

    if (stat(opts.mountpoint, &stbuf) == -1)
    {
        fprintf(stderr,"failed to access mountpoint %s: %s\n",
                opts.mountpoint, strerror(errno));
        return 1;
    }
    if (!S_ISDIR(stbuf.st_mode))
    {
        fprintf(stderr, "mountpoint is not a directory\n");
        return 1;
    }

    int return_main_fuse = fuse_main(args_c_fuse, args_fuse, &ngxfs_oper, NULL);

    return return_main_fuse;
}
