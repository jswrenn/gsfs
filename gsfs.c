/*
  Big Brother File System
  Copyright (C) 2012 Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  This code is derived from function prototypes found /usr/include/fuse/fuse.h
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  His code is licensed under the LGPLv2.
  A copy of that code is included in the file fuse.h
  
  The point of this FUSE filesystem is to provide an introduction to
  FUSE.  It was my first FUSE filesystem as I got to know the
  software; hopefully, the comments in this code will help people who
  follow later to get a gentler introduction.

  This might be called a no-op filesystem:  it doesn't impose
  filesystem semantics on top of any other existing structure.  It
  simply reports the requests that come in, and passes them to an
  underlying filesystem.  The information is saved in a logfile named
  bbfs.log, in the directory from which you run bbfs.

  gcc -Wall `pkg-config fuse --cflags --libs` -o bbfs bbfs.c
*/

#include "params.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#include "log.h"

// Report errors to logfile and give -errno to caller
static int gsfs_error(char *str)
{
    int ret = -errno;
    
    log_msg("    ERROR %s: %s\n", str, strerror(errno));
    
    return ret;
}

// Check whether the given user is permitted to perform the given operation on the given 

//  All the paths I see are relative to the root of the mounted
//  filesystem.  In order to get to the underlying filesystem, I need to
//  have the mountpoint.  I'll save it away early on in main(), and then
//  whenever I need a path for something I'll call this to construct
//  it.
static void gsfs_fullpath(char fpath[PATH_MAX], const char *path)
{
    strcpy(fpath, gsfs_DATA->rootdir);
    strncat(fpath, path, PATH_MAX); // ridiculously long paths will
				    // break here

    log_msg("    gsfs_fullpath:  rootdir = \"%s\", path = \"%s\", fpath = \"%s\"\n",
	    gsfs_DATA->rootdir, path, fpath);
}

///////////////////////////////////////////////////////////
//
// Prototypes for all these functions, and the C-style comments,
// come indirectly from /usr/include/fuse.h
//
/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int gsfs_getattr(const char *path, struct stat *statbuf)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\ngsfs_getattr(path=\"%s\", statbuf=0x%08x)\n",
	  path, statbuf);
    gsfs_fullpath(fpath, path);
    
    retstat = lstat(fpath, statbuf);
    if (retstat != 0)
	retstat = gsfs_error("gsfs_getattr lstat");
    
    log_stat(statbuf);
    
    return retstat;
}

/** Read the target of a symbolic link
 *
 * The buffer should be filled with a null terminated string.  The
 * buffer size argument includes the space for the terminating
 * null character.  If the linkname is too long to fit in the
 * buffer, it should be truncated.  The return value should be 0
 * for success.
 */
// Note the system readlink() will truncate and lose the terminating
// null.  So, the size passed to to the system readlink() must be one
// less than the size passed to gsfs_readlink()
// gsfs_readlink() code by Bernardo F Costa (thanks!)
int gsfs_readlink(const char *path, char *link, size_t size)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("gsfs_readlink(path=\"%s\", link=\"%s\", size=%d)\n",
	  path, link, size);
    gsfs_fullpath(fpath, path);
    
    retstat = readlink(fpath, link, size - 1);
    if (retstat < 0)
	retstat = gsfs_error("gsfs_readlink readlink");
    else  {
	link[retstat] = '\0';
	retstat = 0;
    }
    
    return retstat;
}

/** Create a file node
 *
 * There is no create() operation, mknod() will be called for
 * creation of all non-directory, non-symlink nodes.
 */
// shouldn't that comment be "if" there is no.... ?
int gsfs_mknod(const char *path, mode_t mode, dev_t dev)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\ngsfs_mknod(path=\"%s\", mode=0%3o, dev=%lld)\n",
	  path, mode, dev);
    gsfs_fullpath(fpath, path);
    
    // On Linux this could just be 'mknod(path, mode, rdev)' but this
    //  is more portable
    if (S_ISREG(mode)) {
        retstat = open(fpath, O_CREAT | O_EXCL | O_WRONLY, mode);
	if (retstat < 0)
	    retstat = gsfs_error("gsfs_mknod open");
        else {
            retstat = close(retstat);
	    if (retstat < 0)
		retstat = gsfs_error("gsfs_mknod close");
	}
    } else
	if (S_ISFIFO(mode)) {
	    retstat = mkfifo(fpath, mode);
	    if (retstat < 0)
		retstat = gsfs_error("gsfs_mknod mkfifo");
	} else {
	    retstat = mknod(fpath, mode, dev);
	    if (retstat < 0)
		retstat = gsfs_error("gsfs_mknod mknod");
	}
    
    return retstat;
}

/** Create a directory 
 If the directory is created in the root folder:
	- check if the artist name exists in the grooveshark library
	- if so, register the artist
 All other branches result in error.
*/
// http://linux.die.net/man/2/mkdir
int gsfs_mkdir(const char *path, mode_t mode)
{
	log_msg("\ngsfs_mkdir(path=\"%s\", mode=0%3o)\n",
	    path, mode);
		
	GSFS_Path_Components 
		path_components = gsfs_parse_path(path);
	
	if(path_components.level == ARTIST)
	{
		GSFS_Query_FS_Result 
			result = gsfs_query_fs(path_components.artist_name);
			
		if(result.error == SUCCESS)
			return EEXIST;
			
		switch(gsfs_register_artist(path_components))
		{
		case SUCCESS:
			return SUCCESS;
		case ERROR_ARTIST_NOT_FOUND:
			return EOPNOTSUPP;
		case ERROR_CONNECTION_LOST:
			return EOPNOTSUPP;
		}
	}
	else
	{
		// artist and album folders are read-only
		return EROFS;
	}
}

/** Remove a file 
	Removing a file from gsfs is not a supported operation.
*/
// http://linux.die.net/man/2/unlink
int gsfs_unlink(const char *path)
{
    log_msg("gsfs_unlink(path=\"%s\")\n",
	    path);	
    return EROFS;
}

/** Remove a directory 
	If directory is in filesystem root:
		allow its removal
		de-register the artist
	All other branches produce failure
*/
int gsfs_rmdir(const char *path)
{
	log_msg("gsfs_rmdir(path=\"%s\")\n",
	    path);
		
	GSFS_Path_Components 
		path_components = gsfs_parse_path(path);
		
	switch(path_components.level){
	// The folder root may not be deleted
	case ROOT:
		return EOPNOTSUPP;
	// Artists may be deleted conditionally
	case ARTIST:
		switch(gsfs_deregister_artist(path_components.artist_name)){
		case SUCCESS:
			return 0;
		case ERROR_ARTIST_NOT_FOUND:
			return EOPNOTSUPP;
		}
	case ALBUM:
		// Albums may not be deleted
		return EROFS;
	case SONG:
		// Songs may not be deleted
		return EROFS;
	}
}

/** Create a symbolic link */
// The parameters here are a little bit confusing, but do correspond
// to the symlink() system call.  The 'path' is where the link points,
// while the 'link' is the link itself.  So we need to leave the path
// unaltered, but insert the link into the mounted directory.
int gsfs_symlink(const char *path, const char *link)
{
    log_msg("\ngsfs_symlink(path=\"%s\", link=\"%s\")\n",
	    path, link); 
    return EOPNOTSUPP;
}

/** Rename a file 
	NOT SUPPORTED
*/
// both path and newpath are fs-relative
int gsfs_rename(const char *path, const char *newpath)
{
	log_msg("\ngsfs_rename(fpath=\"%s\", newpath=\"%s\")\n",
	    path, newpath);
	return EOPNOTSUPP;
}

/** Create a hard link to a file 
	NOT SUPPORTED
*/
int gsfs_link(const char *path, const char *newpath)
{
    log_msg("\ngsfs_link(path=\"%s\", newpath=\"%s\")\n",
	    path, newpath);
    return EOPNOTSUPP;
}

/** Change the permission bits of a file 
	NOT SUPPORTED
*/
int gsfs_chmod(const char *path, mode_t mode)
{
    log_msg("\ngsfs_chmod(fpath=\"%s\", mode=0%03o)\n",
	    path, mode);
    return EOPNOTSUPP;
}

/** Change the owner and group of a file 
	NOT SUPPORTED
*/
int gsfs_chown(const char *path, uid_t uid, gid_t gid)
{
    log_msg("\ngsfs_chown(path=\"%s\", uid=%d, gid=%d)\n",
	    path, uid, gid);
    return EOPNOTSUPP;
}

/** Change the size of a file 
	NOT SUPPORTED
*/
int gsfs_truncate(const char *path, off_t newsize)
{
    log_msg("\ngsfs_truncate(path=\"%s\", newsize=%lld)\n",
	    path, newsize);
    return EOPNOTSUPP;
}

/** Change the access and/or modification times of a file */
/* note -- I'll want to change this as soon as 2.6 is in debian testing */
int gsfs_utime(const char *path, struct utimbuf *ubuf)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\ngsfs_utime(path=\"%s\", ubuf=0x%08x)\n",
	    path, ubuf);
    gsfs_fullpath(fpath, path);
    
    retstat = utime(fpath, ubuf);
    if (retstat < 0)
	retstat = gsfs_error("gsfs_utime utime");
    
    return retstat;
}

/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
int gsfs_open(const char *path, struct fuse_file_info *fi)
{
    log_msg("\ngsfs_open(path\"%s\", fi=0x%08x)\n",
	    path, fi);
	return 0;
}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */
// I don't fully understand the documentation above -- it doesn't
// match the documentation for the read() system call which says it
// can return with anything up to the amount of data requested. nor
// with the fusexmp code which returns the amount of data also
// returned by read.

/* Download or read binary song data from cache 
	TODO: what happens when an invalid path is given?
*/
int gsfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    log_msg("\ngsfs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi);
		
	GSFS_Path_Components 
		path_components = gsfs_parse_path(path);
		
	switch(path_components.level){
	case ROOT:
	case ARTIST:
	case ALBUM: 
		// Folders may not be read as files
		return EISDIR;
	case SONG:
		// Songs are files, may be read
		// query for a song object using path components
		GSFS_Filesystem_Song_Query_Result query_result;
		
		switch(gsfs_fs_query(&query_result,
			path_components.artist_name,
			path_components.album_name, 
			path_components.song_name))
		{
		case ERROR_ALBUM_NOT_FOUND:
		case ERROR_ARTIST_NOT_FOUND:
		case ERROR_ALBUM_NOT_FOUND:
		case ERROR_SONG_NOT_FOUND:
			// error: no such file or directory
			return ENOENT;
		case SUCCESS:
			// song struct located in memory
			GSFS_Audio_Data *audio_data;
			switch(gsfs_get_song_audio(query_result.song))
			{
			case ENOMEM:
				// out of memory
				return EOPNOTSUPP;
			case ERROR_CONNECTION_LOST:
				// connection lost
				return EOPNOTSUPP;
			case SUCCESS:
				// copy audio-data to buffer
				if(offset > audio->len)
				{
					if(offset+size > audio->len)
						size = audio->len - offset;			
					memcpy(buf, audio->data + offset, size);
				}
				else
				{
					size = 0;
				}
				return size;
			}
		}
	}
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 */
// As  with read(), the documentation above is inconsistent with the
// documentation for the write() system call.
int gsfs_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
    log_msg("\ngsfs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi);
	// files are read-only
    return EROFS;
}

/** Get file system statistics
 *
 * The 'f_frsize', 'f_favail', 'f_fsid' and 'f_flag' fields are ignored
 *
 * Replaced 'struct statfs' parameter with 'struct statvfs' in
 * version 2.5
 */
 /* TODO: anything? Not sure what goes on here */
int gsfs_statfs(const char *path, struct statvfs *statv)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\ngsfs_statfs(path=\"%s\", statv=0x%08x)\n",
	    path, statv);
    gsfs_fullpath(fpath, path);
    
    // get stats for underlying filesystem
    retstat = statvfs(fpath, statv);
    if (retstat < 0)
	retstat = gsfs_error("gsfs_statfs statvfs");
    
    log_statvfs(statv);
    return retstat;
}

/** Possibly flush cached data
 *
 * BIG NOTE: This is not equivalent to fsync().  It's not a
 * request to sync dirty data.
 *
 * Flush is called on each close() of a file descriptor.  So if a
 * filesystem wants to return write errors in close() and the file
 * has cached dirty data, this is a good place to write back data
 * and return any errors.  Since many applications ignore close()
 * errors this is not always useful.
 *
 * NOTE: The flush() method may be called more than once for each
 * open().  This happens if more than one file descriptor refers
 * to an opened file due to dup(), dup2() or fork() calls.  It is
 * not possible to determine if a flush is final, so each flush
 * should be treated equally.  Multiple write-flush sequences are
 * relatively rare, so this shouldn't be a problem.
 *
 * Filesystems shouldn't assume that flush will always be called
 * after some writes, or that if will be called at all.
 *
 * Changed in version 2.2
 */
int gsfs_flush(const char *path, struct fuse_file_info *fi)
{
	log_msg("\ngsfs_flush(path=\"%s\", fi=0x%08x)\n", path, fi);
    // no need to get fpath on this one, since I work from fi->fh not the path
    log_fi(fi);

	// I believe we should treat this as always-successful
	// GSFS caches a lot of audio data, but we really don't want to erase
	// it upon every file close.
    return SUCCESS;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int gsfs_release(const char *path, struct fuse_file_info *fi)
{
    log_msg("\ngsfs_release(path=\"%s\", fi=0x%08x)\n",
	  path, fi);
    log_fi(fi);
	
	// if we introduce more advanced caching mechanisms, we'll want to implement
	// a garbage collector for cached audio data. Till then...
	return SUCCESS;
}

/** Synchronize file contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data.
 *
 * Changed in version 2.2
 */
int gsfs_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\ngsfs_fsync(path=\"%s\", datasync=%d, fi=0x%08x)\n",
	    path, datasync, fi);
    log_fi(fi);
	
	// not applicable? again, return to this when advanced caching happens
	return SUCCESS;
}

#ifdef HAVE_SYS_XATTR_H
/** Set extended attributes */
int gsfs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    log_msg("\ngsfs_setxattr(path=\"%s\", name=\"%s\", value=\"%s\", size=%d, flags=0x%08x)\n",
	    path, name, value, size, flags);
    return ENOTSUP;
}

/** Get extended attributes */
int gsfs_getxattr(const char *path, const char *name, char *value, size_t size)
{
    log_msg("\ngsfs_getxattr(path = \"%s\", name = \"%s\", value = 0x%08x, size = %d)\n",
	    path, name, value, size);
    return ENOTSUP;
}

/** List extended attributes */
int gsfs_listxattr(const char *path, char *list, size_t size)
{
    log_msg("gsfs_listxattr(path=\"%s\", list=0x%08x, size=%d)\n",
	    path, list, size);
	return ENOTSUP;
}

/** Remove extended attributes */
int gsfs_removexattr(const char *path, const char *name)
{
    log_msg("\ngsfs_removexattr(path=\"%s\", name=\"%s\")\n",
	    path, name);
    return ENOTSUP;
}
#endif

/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
int gsfs_opendir(const char *path, struct fuse_file_info *fi)
{
    log_msg("\ngsfs_opendir(path=\"%s\", fi=0x%08x)\n",
	  path, fi);
    
	GSFS_Path_Components 
		path_components = gsfs_parse_path(path);
		
	switch(path_components.level){
	case ROOT:
		// always allow root to be opened
		return SUCCESS;
	case ARTIST:
	case ALBUM: 
		// artist and album must exist to be open
		GSFS_Filesystem_Query_Result query_result;
		switch(gsfs_fs_query(&query_result,
			path_components.artist_name,
			path_components.album_name))
		{
		case ERROR_ALBUM_NOT_FOUND:
		case ERROR_ARTIST_NOT_FOUND:
		case ERROR_ALBUM_NOT_FOUND:
			// error: no such file or directory
			return ENOENT;
		case SUCCESS:
			return SUCCESS;
		}
	case SONG:
		// error: a song is not a directory
		return ENOTDIR;
	}
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */
 /* TODO: lots of shit */
int gsfs_readdir(
	const char *path, 
	void *buf, 
	fuse_fill_dir_t filler, 
	off_t offset,
	struct fuse_file_info *fi)
{ 
    log_msg("\ngsfs_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, fi=0x%08x)\n",
	    path, buf, filler, offset, fi);
		
	GSFS_Path_Components 
		path_components = gsfs_parse_path(path);
		
	switch(path_components.level){
	case ROOT:
		for(int i=0; i < gsfs_artists.length; i++)
			filler(buf, gsfs_artists[i]->name, NULL, 0);
		return SUCCESS;
		
	case ARTIST:
		GSFS_Query_FS_Result 
			result = gsfs_query_fs(path_components.artist_name);
		if(result.error != 0)
			return ENOENT;
		for(int i=0; i < result.artist->length; i++)
			filler(buf, result.artist->albums[i]->name, NULL, 0);
		return SUCCESS;
		
	case ALBUM:
		GSFS_Query_FS_Result
			result = gsfs_query_fs(path_components.artist_name, path_components.album_name);
		if(result.error != 0)
			return ENOENT;
		for(int i=0; i < result.album->length; i++)
			filler(buf, result.album->songs[i]->name, NULL, 0);
		return SUCCESS;
	case SONG:
		// error: a song is not a directory
		return ENOTDIR;
	}
}

/** Release directory
 *
 * Introduced in version 2.3
 */
int gsfs_releasedir(const char *path, struct fuse_file_info *fi)
{
    log_msg("\ngsfs_releasedir(path=\"%s\", fi=0x%08x)\n",
	    path, fi);
    return SUCCESS;
}

/** Synchronize directory contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data
 *
 * Introduced in version 2.3
 */
// when exactly is this called?  when a user calls fsync and it
// happens to be a directory? ???
int gsfs_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi)
{
    log_msg("\ngsfs_fsyncdir(path=\"%s\", datasync=%d, fi=0x%08x)\n",
	    path, datasync, fi);
    log_fi(fi);
    return SUCCESS;
}

/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */
// Undocumented but extraordinarily useful fact:  the fuse_context is
// set up before this function is called, and
// fuse_get_context()->private_data returns the user_data passed to
// fuse_main().  Really seems like either it should be a third
// parameter coming in here, or else the fact should be documented
// (and this might as well return void, as it did in older versions of
// FUSE).
void *gsfs_init(struct fuse_conn_info *conn)
{
    log_msg("\ngsfs_init()\n");
    
    log_conn(conn);
    log_fuse_context(fuse_get_context());
    
    return gsfs_DATA;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void gsfs_destroy(void *userdata)
{
    log_msg("\ngsfs_destroy(userdata=0x%08x)\n", userdata);
}

/**
 * Check file access permissions
 *
 * This will be called for the access() system call.  If the
 * 'default_permissions' mount option is given, this method is not
 * called.
 *
 * This method is not called under Linux kernel versions 2.4.x
 *
 * Introduced in version 2.5
 */
int gsfs_access(const char *path, int mask)
{
	log_msg("\ngsfs_access(path=\"%s\", mask=0%o)\n",
	    path, mask);
		
	GSFS_Path_Components 
		path_components = gsfs_parse_path(path);
		
	GSFS_Query_FS_Result result;
		
	switch(path_components.level){
	case ROOT:
		return SUCCESS;
	case ARTIST:
		result = gsfs_query_fs(path_components.artist_name);
		if(result.error != 0)
			return ENOENT;
		else 
			return SUCCESS;
	case ALBUM:
		result = gsfs_query_fs(path_components.artist_name, path_components.album_name);
		if(result.error != 0)
			return ENOENT;
		else
			return SUCCESS;
	case SONG:
		// error: a song is not a directory
		return ENOTDIR;
	}
}

/**
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 */
int gsfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	//TODO
    int retstat = 0;
    char fpath[PATH_MAX];
    int fd;
    
    log_msg("\ngsfs_create(path=\"%s\", mode=0%03o, fi=0x%08x)\n",
	    path, mode, fi);
    gsfs_fullpath(fpath, path);
    
    fd = creat(fpath, mode);
    if (fd < 0)
	retstat = gsfs_error("gsfs_create creat");
    
    fi->fh = fd;
    
    log_fi(fi);
    
    return retstat;
}

/**
 * Change the size of an open file
 *
 * This method is called instead of the truncate() method if the
 * truncation was invoked from an ftruncate() system call.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the truncate() method will be
 * called instead.
 *
 * Introduced in version 2.5
 */
int gsfs_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi)
{
    log_msg("\ngsfs_ftruncate(path=\"%s\", offset=%lld, fi=0x%08x)\n",
	    path, offset, fi);
    log_fi(fi);
    return EOPNOTSUPP;
}

/**
 * Get attributes from an open file
 *
 * This method is called instead of the getattr() method if the
 * file information is available.
 *
 * Currently this is only called after the create() method if that
 * is implemented (see above).  Later it may be called for
 * invocations of fstat() too.
 *
 * Introduced in version 2.5
 */
int gsfs_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\ngsfs_fgetattr(path=\"%s\", statbuf=0x%08x, fi=0x%08x)\n",
	    path, statbuf, fi);
    log_fi(fi);

    // On FreeBSD, trying to do anything with the mountpoint ends up
    // opening it, and then using the FD for an fgetattr.  So in the
    // special case of a path of "/", I need to do a getattr on the
    // underlying root directory instead of doing the fgetattr().
    if (!strcmp(path, "/"))
	return gsfs_getattr(path, statbuf);
    
    retstat = fstat(fi->fh, statbuf);
    if (retstat < 0)
	retstat = gsfs_error("gsfs_fgetattr fstat");
    
    log_stat(statbuf);
    
    return retstat;
}

struct fuse_operations gsfs_oper = {
  .getattr = gsfs_getattr,
  .readlink = gsfs_readlink,
  // no .getdir -- that's deprecated
  .getdir = NULL,
  .mknod = gsfs_mknod,
  .mkdir = gsfs_mkdir,
  .unlink = gsfs_unlink,
  .rmdir = gsfs_rmdir,
  .symlink = gsfs_symlink,
  .rename = gsfs_rename,
  .link = gsfs_link,
  .chmod = gsfs_chmod,
  .chown = gsfs_chown,
  .truncate = gsfs_truncate,
  .utime = gsfs_utime,
  .open = gsfs_open,
  .read = gsfs_read,
  .write = gsfs_write,
  /** Just a placeholder, don't set */ // huh???
  .statfs = gsfs_statfs,
  .flush = gsfs_flush,
  .release = gsfs_release,
  .fsync = gsfs_fsync,
  
#ifdef HAVE_SYS_XATTR_H
  .setxattr = gsfs_setxattr,
  .getxattr = gsfs_getxattr,
  .listxattr = gsfs_listxattr,
  .removexattr = gsfs_removexattr,
#endif
  
  .opendir = gsfs_opendir,
  .readdir = gsfs_readdir,
  .releasedir = gsfs_releasedir,
  .fsyncdir = gsfs_fsyncdir,
  .init = gsfs_init,
  .destroy = gsfs_destroy,
  .access = gsfs_access,
  .create = gsfs_create,
  .ftruncate = gsfs_ftruncate,
  .fgetattr = gsfs_fgetattr
};

void gsfs_usage()
{
    fprintf(stderr, "usage:  bbfs [FUSE and mount options] rootDir mountPoint\n");
    abort();
}

int main(int argc, char *argv[])
{
    int fuse_stat;
    struct gsfs_state *gsfs_data;

    // bbfs doesn't do any access checking on its own (the comment
    // blocks in fuse.h mention some of the functions that need
    // accesses checked -- but note there are other functions, like
    // chown(), that also need checking!).  Since running bbfs as root
    // will therefore open Metrodome-sized holes in the system
    // security, we'll check if root is trying to mount the filesystem
    // and refuse if it is.  The somewhat smaller hole of an ordinary
    // user doing it with the allow_other flag is still there because
    // I don't want to parse the options string.
    if ((getuid() == 0) || (geteuid() == 0)) {
	fprintf(stderr, "Running BBFS as root opens unnacceptable security holes\n");
	return 1;
    }
    
    // Perform some sanity checking on the command line:  make sure
    // there are enough arguments, and that neither of the last two
    // start with a hyphen (this will break if you actually have a
    // rootpoint or mountpoint whose name starts with a hyphen, but so
    // will a zillion other programs)
    if ((argc < 3) || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-'))
	gsfs_usage();

    gsfs_data = malloc(sizeof(struct gsfs_state));
    if (gsfs_data == NULL) {
	perror("main calloc");
	abort();
    }

    // Pull the rootdir out of the argument list and save it in my
    // internal data
    gsfs_data->rootdir = realpath(argv[argc-2], NULL);
    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    argc--;
    
    gsfs_data->logfile = log_open();
    
    // turn over control to fuse
    fprintf(stderr, "about to call fuse_main\n");
    fuse_stat = fuse_main(argc, argv, &gsfs_oper, gsfs_data);
    fprintf(stderr, "fuse_main returned %d\n", fuse_stat);
    
    return fuse_stat;
}