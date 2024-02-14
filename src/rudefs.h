#include <stddef.h> // size_t
#include <openssl/crypto.h>
#include <openssl/evp.h>

int hash_file(const char * path,          // in
	      const char * hash_function, // in
	      char * digest               // out
	      );

char * sprint_hash(char * output,
		   const unsigned char * md,
		   const size_t mdlen);
