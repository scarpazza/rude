#include <gtest/gtest.h>
#include <iostream>
#include <fstream>

extern "C" {
#include "rudefs.h"
}

void test_hash( const char * algo,
		const unsigned hash_len_bits,
		const std::string known_hash )
{
  using namespace std;
  const char fname[] = "gtests-example.txt";
  ofstream myfile;
  myfile.open ( fname, ofstream::out | ofstream::binary);
  ASSERT_TRUE(myfile);
  myfile << "Sample file contents for hashing...";
  ASSERT_TRUE(myfile);
  myfile.close();

  unsigned char digest     [EVP_MAX_MD_SIZE+1];
  const int mdlen = hash_file(fname, algo, digest);
  ASSERT_EQ( mdlen, hash_len_bits/8 );
  char hex_digest [EVP_MAX_MD_SIZE*2+1];

  ASSERT_EQ( sprint_hash(hex_digest, digest, mdlen),
	     known_hash);

  ASSERT_EQ( unlink(fname), 0);
}

TEST(hash, sha512sum ) {
  test_hash("SHA512", 512, "d8a736d7ccd638177dee95b00414b88aceea624ba6812e02fbb2f3986d5d2ba9acfa1d1c5ef209d2f7e8495a3b82d54cbe654a5b5a199e63187530db269df898");
}

TEST(hash, sha256sum ) {
  test_hash("SHA256", 256, "c729765ce1da244524882705b865ae7059f6da0401b608c2464aa9ffa419c865");
}

TEST(hash, md5sum ) {
  test_hash("MD5", 128, "f45f6f3168087f329f6fdbb61ef3654d");
}
