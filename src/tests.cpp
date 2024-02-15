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

  ASSERT_EQ( sprint_hash(hex_digest, digest, mdlen), known_hash);

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

TEST(hash, ship_plane_collision ) {
  unsigned char digest1     [EVP_MAX_MD_SIZE+1];
  unsigned char digest2     [EVP_MAX_MD_SIZE+1];

  const int mdlen1 = hash_file("tests/ship.jpg",  "md5", digest1);
  const int mdlen2 = hash_file("tests/plane.jpg", "md5", digest2);

  ASSERT_EQ( mdlen1, 16 );
  ASSERT_EQ( mdlen2, 16 );

  ASSERT_EQ( memcmp(digest1, digest2, 16), 0);

  char hex_digest [EVP_MAX_MD_SIZE*2+1];
  const std::string known_hash("253dd04e87492e4fc3471de5e776bc3d");
  ASSERT_EQ( sprint_hash(hex_digest, digest1, mdlen1), known_hash);
}


TEST(identical, file_not_found ) {
  ASSERT_EQ( identical("file that doesnt exist","other file"), -ENOENT);
}

TEST(identical, same_file ) {
  ASSERT_TRUE( identical("README.md","README.md"));
}

TEST(identical, different_files ) {
  ASSERT_FALSE( identical("src/rudefs.c","src/rudefs.h"));
}

/*
TEST(identical, ok ) {

  ASSERT_EQ( identical(fname), 0);
}
}*/
