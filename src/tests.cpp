#include <gtest/gtest.h>
#include <iostream>
#include <fstream>
#include <sys/stat.h> // chmod
#include <stdexcept> // runtime_error
#include <ranges> // iota
extern "C" {
#include "rudefs.h"
}


// TO DO: add hardlink-counting tests when chmodding+w

// helper functions
namespace rude {
  namespace test {
    void expect_same_inode( const std::string & fname1,
			    const std::string & fname2 ) {
      struct stat st1, st2;
      ASSERT_EQ( lstat(fname1.c_str(), & st1), 0 );
      ASSERT_EQ( lstat(fname2.c_str(), & st2), 0 );
      ASSERT_EQ( st1.st_ino, st2.st_ino );
    }

    void expect_n_hardlinks( const std::string & fname, const nlink_t expected_n_links) {
      struct stat st;
      ASSERT_EQ( lstat(fname.c_str(), & st), 0 );
      ASSERT_EQ( st.st_nlink, expected_n_links );
    }

    void populate(const std::string & fname, const std::string & contents ) {
      std::ofstream f( fname, std::ofstream::out | std::ofstream::binary);
      ASSERT_TRUE( f.is_open() );
      ASSERT_TRUE( f );
      f << contents;
      f.close();
      ASSERT_TRUE( f );
    }

    void expect_readback( const std::string & fname, const std::string & contents )
    {
      std::ifstream fi( fname, std::ifstream::binary);     ASSERT_TRUE( fi.is_open() );
      fi.seekg(0, std::ios::end);                          ASSERT_TRUE( fi );
      const auto size = fi.tellg();                        ASSERT_GT(size, 0 );
      ASSERT_TRUE( fi );
      std::string r( size, '\0');                          ASSERT_EQ(r.size(), size);
      fi.seekg(0, std::ios::beg);                          ASSERT_TRUE( fi );
      fi.read(r.data(), size);                             ASSERT_TRUE( fi );
      ASSERT_EQ (r, contents);
    }

    std::string file_hash_hex(const std::string & fname,
			      const int           hash_len_bits,
			      const char        * algo)
    {
      EXPECT_TRUE(algo);
      EXPECT_NE(strlen(algo), 0);
      unsigned char digest [EVP_MAX_MD_SIZE+1];
      char hex_digest      [EVP_MAX_MD_SIZE*2+1];
      const int mdlen = hash_file(fname.c_str(), algo, digest);
      if (mdlen < 0) throw std::runtime_error( std::string( __FUNCTION__ ) + " invoking hash_file: "
					       + strerror(errno) );

      EXPECT_EQ( mdlen, (hash_len_bits/8) );
      if (mdlen != hash_len_bits/8) throw std::runtime_error(__PRETTY_FUNCTION__);
      sprint_hash(hex_digest, digest, mdlen);
      return std::string(hex_digest);
    }

    std::string data_hash_hex(const std::string & data,
			      const int           hash_len_bits,
			      const char        * algo)
    {
      EXPECT_TRUE(algo);
      EXPECT_NE(strlen(algo), 0);
      unsigned char digest [EVP_MAX_MD_SIZE+1];
      char hex_digest      [EVP_MAX_MD_SIZE*2+1];
      const int mdlen = hash_string(data.c_str(), data.size(), algo, digest);
      EXPECT_EQ( mdlen, (hash_len_bits/8) );
      if (mdlen != hash_len_bits/8) throw std::runtime_error(__PRETTY_FUNCTION__);
      sprint_hash(hex_digest, digest, mdlen);
      return std::string(hex_digest);
    }

    void test_hash_against_known( const char * algo,
				  const unsigned hash_len_bits,
				  const std::string known_hash )
    {
      using namespace std;
      const char fname[] = "gtests-example.txt";
      const std::string contents("Sample file contents for hashing...");
      populate(fname, contents );

      ASSERT_EQ( file_hash_hex(fname, hash_len_bits, algo), known_hash);
      ASSERT_EQ( unlink(fname), 0);
    }
  }
};

TEST(hash, basic_hash_string ) {
  const std::string contents("Sample file contents for hashing...");

  ASSERT_EQ( rude::test::data_hash_hex( contents, 512, "SHA512"), "d8a736d7ccd638177dee95b00414b88aceea624ba6812e02fbb2f3986d5d2ba9acfa1d1c5ef209d2f7e8495a3b82d54cbe654a5b5a199e63187530db269df898");
}

TEST(hash, known_hashes ) {
  using namespace rude::test;
  test_hash_against_known ("SHA512", 512, "d8a736d7ccd638177dee95b00414b88aceea624ba6812e02fbb2f3986d5d2ba9acfa1d1c5ef209d2f7e8495a3b82d54cbe654a5b5a199e63187530db269df898");
  test_hash_against_known ("SHA256", 256, "c729765ce1da244524882705b865ae7059f6da0401b608c2464aa9ffa419c865");
  test_hash_against_known ("MD5", 128, "f45f6f3168087f329f6fdbb61ef3654d");
}

TEST(hash, ship_plane_MD5_collision ) {
  const auto hex_hash_ship  = rude::test::file_hash_hex("tests/ship.jpg",  128, "md5");
  const auto hex_hash_plane = rude::test::file_hash_hex("tests/plane.jpg", 128, "md5");
  const std::string known_hash("253dd04e87492e4fc3471de5e776bc3d");

  ASSERT_EQ( hex_hash_ship, known_hash);
  ASSERT_EQ( hex_hash_plane, known_hash);
}

TEST(identical, file_not_found ) {
  ASSERT_EQ( identical("file that doesnt exist","other file"), -ENOENT);
}

TEST(identical, same_file ) {
  ASSERT_TRUE( identical("README.md","README.md"));
}

TEST(identical, different_files ) {
  ASSERT_FALSE( identical("src/rudefs.cpp","src/rudefs.h"));
}

TEST(identical, matching_files ) {
  using namespace std;
  const string fname1("gtests-example1.txt"), fname2("gtests-example2.txt");

  for ( const auto p_fn : {&fname1, &fname2} )
    rude::test::populate(*p_fn, "sample contents used in identical match");

  ASSERT_EQ( identical(fname1.c_str(), fname2.c_str()), 1);

  for (const auto fnp : {&fname1, &fname2} )
    ASSERT_EQ( unlink(fnp->c_str()), 0);
}

TEST(basic, add_file_readback ) {
  using namespace std;
  system("make reset");
  system("make mount");
  const string
    fname("rude-mnt/gtests-new-file1.txt"),
    backing_fname("rude-store/root/gtests-new-file1.txt"),
    contents("add_file gtest: sample file contents for match testing...");

  using namespace rude::test;
  populate(fname, contents);
  system("make unmount");
  expect_readback(backing_fname, contents);
}

TEST(basic, add_1_file_to_store ) {
  using namespace std;
  system("make reset");
  system("make mount");

  const string
    fname("rude-mnt/gtests-new-file1.txt"),
    backing_fname("rude-store/root/gtests-new-file1.txt"),
    contents("add_file gtest: sample file contents for match testing..."),
    hashmap_dir("rude-store/hashmap/");

  using namespace rude::test;
  populate(fname, contents);
  // read back immediately
  expect_readback(fname, contents);

  // make read-only, forcing the addition to the store
  chmod( fname.c_str(), S_IRUSR);

  const string hash_fname = hashmap_dir + file_hash_hex(fname, 256, "sha256");

  system("make unmount");

  // check backing file - now a hard link
  expect_readback(backing_fname, contents );

  // check hashmap store file
  expect_readback(hash_fname, contents);

  // check target inodes correct
  expect_same_inode(backing_fname, hash_fname );

  // check hard link counts correct
  expect_n_hardlinks(backing_fname, 2 );
  expect_n_hardlinks(hash_fname,    2 );

  ASSERT_EQ( unlink(backing_fname.c_str()), 0);

}

TEST(basic, add_10_file_to_store ) {
  using namespace std;
  system("make reset");
  system("make mount");

  const string
    fname_templ("rude-mnt/gtests-new-file%02i.txt"),
    backing_fname_templ("rude-store/root/gtests-new-file1.txt"),
    contents("gtest: adding 10 files gtest: sample file contents for testing blah blah..."),
    hashmap_dir("rude-store/hashmap/");

  using namespace rude::test;
  for (int i: std::views::iota(0,10)) {
    char s[fname_templ.size()];
    sprintf(s, fname_templ.c_str(), i);
    populate( s, contents);
    expect_readback(s, contents);
  }

  const string hash_fname = hashmap_dir + data_hash_hex(contents, 256, "sha256");

  for (int i: std::views::iota(0,10)) {
    char s[fname_templ.size()];
    sprintf(s, fname_templ.c_str(), i);
    // make read-only, forcing the addition to the store
    chmod(s, S_IRUSR);
    // checks addition to the hash store
    expect_n_hardlinks(hash_fname, i+2 );
  }

  // check hashmap store file
  expect_readback(hash_fname, contents);

  // check hard link counts correct
  expect_n_hardlinks(hash_fname, 11 );

  for (int i: std::views::iota(0,10)) {
    expect_n_hardlinks(hash_fname, 11 - i );

    char s[fname_templ.size()];
    sprintf(s, fname_templ.c_str(), i);
    ASSERT_EQ( unlink(s), 0);
  }

  system("make unmount");

  ASSERT_EQ( unlink( hash_fname.c_str()), 0);
}

// TO DO test deletion
