#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h> // open
#include <unistd.h> // close
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


int hash_string( const    char * data,
		 const    size_t len,
	         const    char * hash_function,
	         unsigned char * digest /* buffer must be EVP_MAX_MD_SIZE+1 long or longer  */
		 )
{
  size_t mdlen;
  return ( EVP_Q_digest(NULL, hash_function, NULL, data, len, digest, &mdlen) ?
	   mdlen : 0 );
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
    fprintf(stderr, "rudefs: hash_file: open %s failed: %s\n", path, strerror (errno));
    return -errno;
  }

  // File size must be known before mmapping
  if ( fstat (fd, & filestat) < 0) {
    fprintf(stderr, "rudefs: stat %s failed: %s\n", path, strerror (errno));
    return -errno;
  }
  const size_t filesize = filestat.st_size;

  // Memory-map the file.
  char * const mapped = mmap (0, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
  if (mapped == MAP_FAILED) {
    close(fd);
    fprintf(stderr, "rudefs: mmap %s failed: %s\n", path, strerror (errno));
    return -errno;
  }

  size_t mdlen;
  if ( EVP_Q_digest(NULL, hash_function, NULL,
		    mapped, filesize, digest, &mdlen) == 0 ) {
    close(fd);
    munmap(mapped, filesize);
    fprintf(stderr, "rudefs: EVP_Q_digest %s %s failed\n", path, hash_function );
    return -EKEYREJECTED;
  }
  digest[mdlen] = 0;
  close(fd);
  munmap(mapped, filesize);
  return mdlen;
}


/* return codes:
   - negative: failure, -errno
   - 1: test completed, files match
   - 0: test completed, files DO NOT matching
*/
int identical( const char * const path1, const char * const path2 )
{
  struct stat filestat1, filestat2;
  const int fd1 = open (path1, O_RDONLY);
  const int fd2 = open (path2, O_RDONLY);
  if (fd1 < 0 || fd2 < 0) {
    fprintf(stderr, "rudefs: identical: open failed: %s\n", strerror (errno));
    return -errno;
  }


  // File size must be known before mmapping
  if ( fstat (fd1, & filestat1) < 0  ||  fstat (fd2, & filestat2) < 0 ) {
    fprintf(stderr, "rudefs: identical: fstat failed: %s\n", strerror (errno));
    return -errno;
  }

  if ( filestat1.st_size != filestat2.st_size ) {
    fprintf(stderr, "rudefs: identical: sizes differ %lu != %lu\n", (long unsigned) filestat1.st_size, (long unsigned) filestat2.st_size );
    // no need to compare contents - size differs
    close(fd1);
    close(fd2);
    return 0;
  }

  // Memory-map the file.
  char * const mapped1 = mmap (0, filestat1.st_size, PROT_READ, MAP_PRIVATE, fd1, 0);
  char * const mapped2 = mmap (0, filestat1.st_size, PROT_READ, MAP_PRIVATE, fd2, 0);

  if ( mapped1 == MAP_FAILED || mapped2 == MAP_FAILED) {
    fprintf(stderr, "rudefs: identical: mmap failed: %s\n", strerror (errno));
    close(fd1);
    close(fd2);
    return -errno;
  }

  const int cmp = memcmp(mapped1, mapped2, filestat1.st_size);
  munmap(mapped1, filestat1.st_size);
  munmap(mapped2, filestat1.st_size);
  close(fd1);
  close(fd2);

  return (cmp==0) ? 1 : 0;
}
