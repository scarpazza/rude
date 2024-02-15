#include <stddef.h> // size_t
#include <openssl/crypto.h>
#include <openssl/evp.h>

#define STORE_SUBDIR "hashmap"
#define ROOT_SUBDIR  "root"

int hash_file(const    char * path,          // in
	      const    char * hash_function, // in
	      unsigned char * digest         // out
	      /* buffer must be EVP_MAX_MD_SIZE+1 long or longer  */
	      );

char * sprint_hash(char * output,
		   const unsigned char * md,
		   const size_t mdlen);

int identical( const char * const path1,
	       const char * const path2 );

