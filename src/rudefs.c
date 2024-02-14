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
	(void) fi;
	int res = 0;

	printf("getattr requested path %s\n", path);
	memset(stbuf, 0, sizeof(struct stat));

	chdir(options.backing); // to do check

	if (strcmp(path, "/") == 0)
	  return stat("root", stbuf);

	chdir("root");
	return stat(path+1, stbuf);

	/*else
	  res = -ENOENT;*/

	return res;
}

static int rude_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi,
			 enum fuse_readdir_flags flags)
{
	(void) offset;
	(void) fi;
	(void) flags;

	struct dirent *entry;

	chdir(options.backing); // to do check
	chdir("root"); // to do check

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
	  //printf("  %s\n", entry->d_name);

	  //filler(buf, ".", NULL, 0, 0);
	  //filler(buf, "..", NULL, 0, 0);
	closedir(dir);
	return 0;
}

static int rude_open(const char *path, struct fuse_file_info *fi)
{
  fprintf(stderr, "rude_open: %s\n", path);
  return 0;
  // TO DO CHECK

  //if (strcmp(path+1, options.filename) != 0)
  // return -ENOENT;

	if ((fi->flags & O_ACCMODE) != O_RDONLY)
		return -EACCES;

	return 0;
}

static int rude_read(const char *path, char *buf,
		     size_t size, off_t offset,
		     struct fuse_file_info *fi)
{
  size_t len;
  (void) fi;

  // TO DO handle /

  if ( chdir(options.backing)!=0 ) {
    perror("rude_read: chdir into backing dir failed");
    return -errno;
  }
  if ( chdir("root") != 0) {
    perror("rude_read: chdir into failed");
    return -errno;
  }

  FILE * f = fopen(path+1, "rb");
  if (!f)
    return -ENOENT;
  fseek(f, 0, SEEK_END);
  len = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (offset < len) {
    if (offset + size > len)
      size = len - offset;
    fread( buf, size, 1, f);
    //memcpy(buf, contents + offset, size);
  } else
    size = 0;
  fclose(f);

  return size;
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
