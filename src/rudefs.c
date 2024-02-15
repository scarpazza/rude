// TO DO:
// when chmod u-w, remove w from also g and o
// + manage hash conflicts at all?
// + stingy vs prodigal mode
// + stats
// + rename
// + opendir
// + pedantic vs permissive mode: upon store, fcomp
// + kill_inode could be made asynchronous
// + test race conditions

#define _DEFAULT_SOURCE

#define FUSE_USE_VERSION 31
#define _POSIX_SOURCE

#include <stdlib.h> // malloc
#include <dirent.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h> //strdup
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <unistd.h> //chdir
#include <sys/stat.h> // mode_t, fstat, lstat
#include "rudefs.h" // hash_file



/*
 * Command line options
 *
 * Don't set default values for the char* fields here because
 * fuse_opt_parse would attempt to free() them when the user specifies
 * different values on the command line.
 */
static struct options {
  const char * backing;                  // backing store - requested
  char         real_backing[PATH_MAX+1]; // backing store - realpath; not set by fuse_opt_parse
  const char * hash_function;
  int          show_help;
  int          reclamation_stingy;
  int          collision_complacent;
} options;

#define OPTION(t, p)                           \
    { t, offsetof(struct options, p), 1 }
static const struct fuse_opt option_spec[] = {
	OPTION("--backing=%s", backing),
	OPTION("--hashfn=%s",  hash_function),
	OPTION("--stingy",     reclamation_stingy),
	OPTION("--complacent", collision_complacent),
	OPTION("-h", show_help),
	OPTION("--help", show_help),
	FUSE_OPT_END
};


int kill_inode(const ino_t victim)
// This is embarrassing; i have to scan all the hash store to find the inode I want
{
  DIR * dir;
  struct dirent *entry;

  if ( chdir(options.real_backing) != 0 ) return -ENOMEDIUM;
  if ( chdir(STORE_SUBDIR)         != 0 ) return -ENOMEDIUM;
  if (!(dir = opendir( "." )))            return -ENOMEDIUM;

  while ((entry = readdir(dir)) != NULL)
    if (entry->d_ino == victim) {
      printf("rudefs: found inode %lu; name %s - killing\n",
	     (long unsigned) victim, entry->d_name);
      closedir(dir);
      if ( 0 != unlink(entry->d_name) ) {
	fprintf(stderr, "rudefs: kill_inode %lu: unlink failed: %s\n",
		(long unsigned) victim, strerror (errno));
	return -errno;
      }
      return 0;
    }

  // got till here? didn't find the inode!
  closedir(dir);
  return -ENOENT;
}




int deduplicate(const char * orig_path,
		const char * hex_digest)
{
  struct stat st;
  if ( chdir(options.real_backing) != 0 ) return -ENOMEDIUM;

  char src_path   [PATH_MAX+1];
  char store_path [PATH_MAX+1];
  snprintf(store_path, PATH_MAX, "%s/%s", STORE_SUBDIR, hex_digest);
  snprintf(src_path,   PATH_MAX, "%s/%s",  ROOT_SUBDIR,  orig_path);
  printf("rudefs: de-duplicating %s -> %s\n", src_path, store_path);

  // TO DO lock appropriately
  if ( stat(store_path, &st) != 0 ) {
    // store file does not exist yet - move and link
    if ( rename(src_path, store_path) != 0 ) {
      fprintf(stderr, "rudefs: deduplicate: rename failed: %s\n", strerror (errno));
      goto error;
    }
  } else {

    if (!options.collision_complacent) {
      fprintf(stderr, "rudefs: thorough mode - checking for hash collisions\n");
      int ret;
      if ((ret = identical(src_path, store_path)) <=0 )
	return ret;
    } else {
      fprintf(stderr, "rudefs: complacent mode - not checking for hash collisions\n");

    }

    // store file already exists - delete src
    if ( 0 != remove(src_path) ) {
      fprintf(stderr, "rudefs: deduplicate: remove failed: %s\n", strerror (errno));
      goto error;
    }
  }

  if ( 0 != link(store_path, src_path) ) {
      fprintf(stderr, "rudefs: deduplicate: link failed: %s\n", strerror (errno));
      goto error;
  }

  // to do release lock
  printf("rudefs: SUCCESS de-duplicating %s -> %s\n", src_path, store_path);
  return 0;

 error:
  // release lock
  return -errno;
}



static int rude_unlink(const char *path)
{
  int res;
  struct stat st;

  fprintf(stderr, "rudefs: unlink: fstat on %s...\n", path);

  if ( strcmp(path, "/") == 0 )           return -EPERM;
  if ( chdir(options.real_backing) != 0 ) return -ENOMEDIUM;
  if ( chdir(ROOT_SUBDIR) != 0)           return -ENOMEDIUM;

  if ( lstat(path+1, & st) != 0 ) {
    fprintf(stderr, "rudefs: unlink: fstat on %s failed: %s\n", path, strerror (errno));
    return -errno;
  }

  fprintf(stdout, "rudefs: stat: %s has %lu hard links; inode %lu\n",
	  path, (long unsigned) st.st_nlink, (long unsigned) st.st_ino);
  if (st.st_nlink == 1) {
    if (unlink(path+1) != 0) {
      fprintf(stderr, "rudefs: 1-link unlink: unlink %s failed: %s\n", path, strerror (errno));
      return -errno;
    }
    return 0;
  }

  if ( !options.reclamation_stingy || (st.st_nlink > 2) ) {
    // I'm prodigal or the inode has more than 2 incoming link:
    // Either way, I can proceed and unlink
    if (unlink(path+1) != 0) {
      fprintf(stderr, "rudefs: prodigal/2+inode unlink: unlink %s failed: %s\n", path, strerror (errno));
      return -errno;
    }
    return 0;
  }

  // I'm stingy and it's the last incoming link - kill the inode
  return kill_inode(st.st_ino);
}


static void *rude_init(struct fuse_conn_info *conn,
			struct fuse_config *cfg)
{
  (void) conn;
  cfg->kernel_cache = 1;
  return NULL;
}

static int rude_getattr(const  char *path,
			struct stat *stbuf,
			struct fuse_file_info *fi)
{
  int res = 0;
  printf("getattr requested path %s\n", path);
  memset(stbuf, 0, sizeof(struct stat));

  if (chdir(options.real_backing) != 0) return -ENOMEDIUM; // this is the errno I use when the backing fs is not there

  if (fi)
    res = fstat(fi->fh, stbuf);
  else if (strcmp(path, "/") == 0)
    res = lstat(ROOT_SUBDIR, stbuf);
  else {
    if ( chdir(ROOT_SUBDIR) != 0) return -ENOMEDIUM;
    res = lstat(path+1, stbuf);
  }
  if (res == -1)
    return -errno;
  else
    return res;

  /*else res = -ENOENT;*/
}

static int rude_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi,
			 enum fuse_readdir_flags flags)
{
  (void) offset;
  (void) fi;
  (void) flags;

  struct dirent *entry;

  if ( chdir(options.real_backing) != 0 ) return -ENOMEDIUM;
  if ( chdir(ROOT_SUBDIR) != 0)           return -ENOMEDIUM;
  DIR * dir;

  printf("rude_readdir %s...\n", path);
  if (strcmp(path, "/") == 0) {
    dir = opendir(".");
  } else {
    dir = opendir(path+1);
  }
  if (!dir) return -ENOENT;

  while ((entry = readdir(dir)) != NULL) {
    printf("rude_readdir: returning %s\n", entry->d_name);
    filler(buf, entry->d_name, NULL, 0, 0);
  }
  closedir(dir);
  return 0;
}

static int rude_open(const  char *path,
		     struct fuse_file_info *fi)
{
  int fd;
  fprintf(stderr, "rude_open: %s\n", path);
  if ( chdir(options.real_backing) != 0 ) return -ENOMEDIUM;
  if ( chdir(ROOT_SUBDIR) != 0)                return -ENOMEDIUM;

  if (strcmp(path, "/") == 0)
    fd = open(".", fi->flags);
  else
    fd = open(path+1, fi->flags);

  if (fd == -1) return -errno;

  fi->fh = fd;
  return 0;
}


static int rude_read(const char *path, char *buf,
		     size_t size, off_t offset,
		     struct fuse_file_info *fi)
// pure pass-through
{
  int res;
  (void) path;
  res = pread(fi->fh, buf, size, offset);
  if (res == -1)
    res = -errno;
  return res;
}

static off_t rude_lseek(const char *path, off_t off, int whence, struct fuse_file_info *fi)
// pure pass-through
{
  off_t res;
  (void) path;
  res = lseek(fi->fh, off, whence);
  if (res == -1)
    return -errno;
  return res;
}

static int rude_release(const char *path, struct fuse_file_info *fi)
{
  (void) path;
  close(fi->fh);
  return 0;
}


static int rude_mkdir(const char *path, mode_t mode)
{
  int res;
  fprintf(stderr, "rude_mkdir: %s\n", path);
  if (strcmp(path, "/") == 0) return 0; // nothing to do
  if ( chdir(options.real_backing) != 0 ) return -ENOMEDIUM;
  if ( chdir(ROOT_SUBDIR) != 0)                return -ENOMEDIUM;
  res = mkdir(path+1, mode); // TO DO SPLIT?
  if (res == -1) return -errno;
  return 0;
}

static int rude_rmdir(const char *path)
{
  int res;
  if (strcmp(path, "/") == 0) return -EPERM; // you can't delete root
  if ( chdir(options.real_backing) != 0 ) return -ENOMEDIUM;
  if ( chdir(ROOT_SUBDIR) != 0)                return -ENOMEDIUM;
  res = rmdir(path+1);
  if (res == -1) return -errno;
  return 0;
}

static int rude_write(const char *path, const char *buf, size_t size,
		      off_t offset, struct fuse_file_info *fi)
// Pure pass-through
{
  int res;
  (void) path;
  res = pwrite(fi->fh, buf, size, offset);
  if (res == -1)
    res = -errno;
  return res;
}

// copied from xmp_flush
static int rude_flush(const char *path, struct fuse_file_info *fi)
{
  int res;
  (void) path;
  /* This is called from every close on an open file, so call the
     close on the underlying filesystem. But since flush may be
     called multiple times for an open file, this must not really
     close the file.  This is important if used on a network
     filesystem like NFS which flush the data/metadata on close() */
  res = close(dup(fi->fh));
  if (res == -1)
    return -errno;
  return 0;
}

/*static int rude_releasedir(const char *path, struct fuse_file_info *fi)
{
  struct xmp_dirp *d = get_dirp(fi);
  (void) path;
  closedir(d->dp);
  free(d);
  return 0;
  }*/

static int rude_mknod(const char *path, mode_t mode, dev_t rdev)
{
  int res;
  if (strcmp(path, "/") == 0)             return -EPERM; // nothing to do
  if ( chdir(options.real_backing) != 0 ) return -ENOMEDIUM;
  if ( chdir(ROOT_SUBDIR) != 0)           return -ENOMEDIUM;

  if (S_ISFIFO(mode))
    res = mkfifo(path+1, mode);
  else
    res = mknod(path+1, mode, rdev);
  if (res == -1)
    return -errno;
  return 0;
}

static int rude_utimens(const char *path,
			const struct timespec ts[2],
			struct fuse_file_info *fi)
{
  int res;

  /* don't use utime/utimes since they follow symlinks */
  if (fi)
    res = futimens(fi->fh, ts);
  else {
    if ( chdir(options.real_backing) != 0 ) return -ENOMEDIUM;
    if ( chdir(ROOT_SUBDIR) != 0)                return -ENOMEDIUM;

    if (strcmp(path, "/") == 0) {
      res = utimensat(0, ".", ts, AT_SYMLINK_NOFOLLOW);
    }
    else
      res = utimensat(0, path+1, ts, AT_SYMLINK_NOFOLLOW);
  }

  if (res == -1) return -errno;
  return 0;
}



static int rude_chmod(const char *path,
		      mode_t new_mode,
		      struct fuse_file_info *fi)
{
  int res;

  if ( chdir(options.real_backing) != 0 ) return -ENOMEDIUM;
  if ( chdir(ROOT_SUBDIR) != 0)           return -ENOMEDIUM;
  if (strcmp(path, "/") == 0)             return -EPERM;
  /*if(fi)
    res = fchmod(fi->fh, mode); */

  struct stat old;
  lstat(path+1, &old);
  if ( ((old.st_mode & S_IWUSR) == 0) && ((new_mode & S_IWUSR) >0))
    printf("Should duplicate and pull from to hash-store\n");
  else
    if ( ((old.st_mode & S_IWUSR) > 0) && ((new_mode & S_IWUSR) ==0))
      {
	printf("rudefs: de-duplicating %s and link from hash-store, %s\n",
	       path, options.hash_function);
	unsigned char digest     [EVP_MAX_MD_SIZE+1];
	unsigned char hex_digest [EVP_MAX_MD_SIZE*2+1];
	const int mdlen = hash_file(path+1, options.hash_function, digest);
	if (mdlen < 0) return res;
	if (mdlen ==0) return -EINVAL;

	printf("hash %s (%u bits, %u chars, %u hex digits)",
	       sprint_hash(hex_digest, digest, mdlen),
	       mdlen*8, mdlen, mdlen*2);
	printf(";\n");
	if ( (res = deduplicate(path+1, hex_digest)) <0)
	  return res;
      }
  {
    printf("rude_chmod: %s\n", path);
    res = chmod(path+1, new_mode);
  }

  if (res == -1)
    return -errno;
  return 0;
}



static const struct fuse_operations rude_oper = {
  .init          = rude_init,
  .getattr	 = rude_getattr,
  .chmod         = rude_chmod,
  .mkdir         = rude_mkdir,
  .rmdir         = rude_rmdir,
  .readdir	 = rude_readdir,

  .flush         = rude_flush,
  .lseek         = rude_lseek,

  .open		 = rude_open,
  .read		 = rude_read,
  .write	 = rude_write,

  .mknod         = rude_mknod,
  .unlink        = rude_unlink,
  .release       = rude_release,
  .utimens       = rude_utimens,
};

static void show_help(const char *progname)
{
  printf("usage: %s [options] <mountpoint>\n\n", progname);
  printf("File-system specific options:\n"
	 "    --backing=<s>      path to the backing\n"
	 "    --hashfn=<s>       hash function to use, e.g., SHA256\n"
	 "    --stingy           use the 'stingy' reclamation policy rather than the prodigal (default)\n"
	 "    --complacent       use the 'complacent' hash conflict detection policy\n"
	 "\n");
}

int verify_backing()
{
  printf("rudefs: verifying backing path %s\n", options.backing);
  if ( realpath(options.backing,
		options.real_backing) == 0) {
    fprintf(stderr, "rudefs: realpath failed on defaults backing path %s: %s\n",
	    options.backing, strerror(errno));
    return -errno;
  }
  printf("rudefs: backing realpath: %s\n", options.real_backing);

  if ( chdir(options.real_backing) !=0 ) {
    perror("rudefs: initial backing subdir verification");
    return -errno;
  }
  return 0;
}

int main(int argc, char *argv[])
{
  int ret;
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

  /* Set defaults -- we have to use strdup so that
     fuse_opt_parse can free the defaults if other
     values are specified */
  options.backing       = strdup("rudefs-store");
  options.hash_function = strdup("SHA256");

  /* Parse options */
  if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1)
    return 1;

  /* When --help is specified, first print our own file-system
     specific help text, then signal fuse_main to show
     additional help (by adding `--help` to the options again)
     without usage: line (by setting argv[0] to the empty
     string) */
  if (options.show_help) {
    show_help(argv[0]);
    assert(fuse_opt_add_arg(&args, "--help") == 0);
    args.argv[0][0] = '\0';
  }

  if ( (ret = verify_backing(options.backing)) <0)
    return ret;

  ret = fuse_main(args.argc, args.argv, &rude_oper, NULL);
  fuse_opt_free_args(&args);
  return ret;
}
