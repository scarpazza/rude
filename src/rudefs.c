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
    return -EBADFD; // this is the errno I use when the backing fs is not there

  if (fi)
    res = fstat(fi->fh, stbuf);
  else if (strcmp(path, "/") == 0)
    res = lstat("root", stbuf);
  else {
    if ( chdir("root") != 0) return -EBADFD;
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

  if ( chdir(options.backing) != 0 ) return -EBADFD;
  if ( chdir("root") != 0)           return -EBADFD;
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
  if ( chdir(options.backing) != 0 ) return -EBADFD;
  if ( chdir("root") != 0)           return -EBADFD;

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
{
  int res;
  (void) path;
  res = pread(fi->fh, buf, size, offset);
  if (res == -1)
    res = -errno;
  return res;
}

static const struct fuse_operations rude_oper = {
	.init           = rude_init,
	.getattr	= rude_getattr,
	.readdir	= rude_readdir,
	.open		= rude_open,
	.read		= rude_read,
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
