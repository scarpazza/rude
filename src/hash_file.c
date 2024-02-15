#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h> // open
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h> // mode_t, fstat, lstat

#include "rudefs.h"

char * sprint_hash(char * output, // must be at least mdlen*2+1 long
		   const  unsigned char * md,
		   const  size_t mdlen
		   )
{
  for (size_t i=0; i<mdlen; i++) {
    const unsigned char c = md[i];
    sprintf(output + 2*i, "%02x",c);
  }
  return output;
}


/* return codes:
   - negative: failure, -errno
   - positive: success, digest length
*/
int hash_file(const char * path,
	      const char * hash_function,
	      unsigned char * digest /* buffer must be EVP_MAX_MD_SIZE+1 long or longer  */
	      )
{
  struct stat filestat;
  const int fd = open (path, O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "rudefs: hash_file: open %s failed: %s",
	    path, strerror (errno));
    return -errno;
  }

  // File size must be known before mmapping
  if ( fstat (fd, & filestat) < 0) {
    fprintf(stderr, "rudefs: stat %s failed: %s", path, strerror (errno));
    return -errno;
  }
  const size_t filesize = filestat.st_size;

  // Memory-map the file.
  const char * mapped = mmap (0, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
  if (mapped == MAP_FAILED) {
    fprintf(stderr, "rudefs: mmap %s failed: %s", path, strerror (errno));
    return -errno;
  }

  size_t mdlen;
  if ( EVP_Q_digest(NULL, hash_function, NULL,
		    mapped, filesize, digest, &mdlen) == 0 ) {
    fprintf(stderr, "rudefs: EVP_Q_digest %s %s failed\n", path, hash_function );
    return -EKEYREJECTED;
  }
  digest[mdlen] = 0;
  return mdlen;
}
