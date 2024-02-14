
// TO DO:
// rename
// chmod

#define _DEFAULT_SOURCE

#define FUSE_USE_VERSION 31
#define _POSIX_SOURCE

#include <dirent.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h> //strdup
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <unistd.h> //chdir
#include <sys/stat.h> // mode_t

/*
 * Command line options
 *
 * We can't set default values for the char* fields here because
 * fuse_opt_parse would attempt to free() them when the user specifies
 * different values on the command line.
 */
static struct options {
	const char *backing;
	int show_help;
} options;

#define OPTION(t, p)                           \
    { t, offsetof(struct options, p), 1 }
static const struct fuse_opt option_spec[] = {
	OPTION("--backing=%s", backing),
	OPTION("-h", show_help),
	OPTION("--help", show_help),
	FUSE_OPT_END
};

static void *rude_init(struct fuse_conn_info *conn,
			struct fuse_config *cfg)
{
  (void) conn;
  cfg->kernel_cache = 1;
  return NULL;
}

static int rude_getattr(const char *path,
			struct stat *stbuf,
			struct fuse_file_info *fi)
{
  int res = 0;
  printf("getattr requested path %s\n", path);
  memset(stbuf, 0, sizeof(struct stat));

  if (chdir(options.backing) != 0)
    return -ENOMEDIUM; // this is the errno I use when the backing fs is not there

  if (fi)
    res = fstat(fi->fh, stbuf);
  else if (strcmp(path, "/") == 0)
    res = lstat("root", stbuf);
  else {
    if ( chdir("root") != 0) return -ENOMEDIUM;
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

  if ( chdir(options.backing) != 0 ) return -ENOMEDIUM;
  if ( chdir("root") != 0)           return -ENOMEDIUM;
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
  if ( chdir(options.backing) != 0 ) return -ENOMEDIUM;
  if ( chdir("root") != 0)           return -ENOMEDIUM;

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
  if ( chdir(options.backing) != 0 ) return -ENOMEDIUM;
  if ( chdir("root") != 0)           return -ENOMEDIUM;
  res = mkdir(path+1, mode); // TO DO SPLIT?
  if (res == -1) return -errno;
  return 0;
}

static int rude_rmdir(const char *path)
{
  int res;
  if (strcmp(path, "/") == 0) return -EPERM; // you can't delete root
  if ( chdir(options.backing) != 0 ) return -ENOMEDIUM;
  if ( chdir("root") != 0)           return -ENOMEDIUM;
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
  if (strcmp(path, "/") == 0)        return -EPERM; // nothing to do
  if ( chdir(options.backing) != 0 ) return -ENOMEDIUM;
  if ( chdir("root") != 0)           return -ENOMEDIUM;

  if (S_ISFIFO(mode))
    res = mkfifo(path+1, mode);
  else
    res = mknod(path+1, mode, rdev);
  if (res == -1)
    return -errno;
  return 0;
}

static int rude_utimens(const char *path, const struct timespec ts[2],
			struct fuse_file_info *fi)
{
  int res;

  /* don't use utime/utimes since they follow symlinks */
  if (fi)
    res = futimens(fi->fh, ts);
  else {
    if ( chdir(options.backing) != 0 ) return -ENOMEDIUM;
    if ( chdir("root") != 0)           return -ENOMEDIUM;

    if (strcmp(path, "/") == 0) {
      res = utimensat(0, ".", ts, AT_SYMLINK_NOFOLLOW);
    }
    else
      res = utimensat(0, path+1, ts, AT_SYMLINK_NOFOLLOW);
  }

  if (res == -1) return -errno;
  return 0;
}

static int rude_chmod(const char *path, mode_t new_mode,
		     struct fuse_file_info *fi)
{
  int res;

  if ( chdir(options.backing) != 0 ) return -ENOMEDIUM;
  if ( chdir("root") != 0)           return -ENOMEDIUM;
  if (strcmp(path, "/") == 0)        return -EPERM;
  /*if(fi)
    res = fchmod(fi->fh, mode); */

  struct stat old;
  lstat(path+1, &old);
  if ( ((old.st_mode & S_IWUSR) == 0) && ((new_mode & S_IWUSR) >0))
    printf("Should duplicate and pull from to hash-store\n");
  else
    if ( ((old.st_mode & S_IWUSR) > 0) && ((new_mode & S_IWUSR) ==0))
      printf("Should de-duplicate and link from hash-store\n");

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
  .mknod         = rude_mknod,
  .write         = rude_write,
  .release       = rude_release,
  .utimens       = rude_utimens,
};

static void show_help(const char *progname)
{
  printf("usage: %s [options] <mountpoint>\n\n", progname);
  printf("File-system specific options:\n"
	 "    --backing=<s>      path to the backing\n"
	 "\n");
}

int verify_backing(const char * path)
{
  printf("Verifying backing path %s\n", path);
  if ( chdir(path) !=0 )
    {
      perror("rudefs: initial backing subdir verification");
      return 0;
    }
  return 1;
}

int main(int argc, char *argv[])
{
  int ret;
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

  /* Set defaults -- we have to use strdup so that
     fuse_opt_parse can free the defaults if other
     values are specified */
  options.backing = strdup("rudefs-store");

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

  if (!verify_backing(options.backing))
    return -1;

  ret = fuse_main(args.argc, args.argv, &rude_oper, NULL);
  fuse_opt_free_args(&args);
  return ret;
}
