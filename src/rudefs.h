#include <stddef.h> // size_t
#include <openssl/crypto.h>
#include <openssl/evp.h>

int hash_file(const char * path,          // in
	      const char * hash_function, // in
	      char * digest               // out
	      );

void print_hash(const unsigned char * md,
		const size_t size);
