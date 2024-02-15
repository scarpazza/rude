# rude

RUDE is a RUdimentary DEduplicating user-space filesystem, mainly designed as an exercise.


## Goal

This is a FUSE-based filesystem that transparently auto-deduplicates
files as soon as they are made read-only for the owner (S_IWUSR, +200),
and re-duplicates them as soon as they are made writable again.

It is intended to save space on repeated, common installs of software
of similar versions.

It is designed to have minimum overhead on all operations, except `chmod u+w` and `u-w`.

## Guarantees
None. This was just made for fun.  If you use it for anything else,
you are rollerskating naked in a dark room full of sharp glass shards.

## Prerequisites
Before you try, ensure you have the fuse3 and openssl packages
installed, plus the Google test framework, e.g.:

        sudo apt install fuse3 libfuse3-dev libssl-dev libgtest-dev

or the equivalent packages for your distribution.

Openssl is needed for the hash functions. I'm picking up cryptographically
strong functions out of laziness.

## Usage

- run `make mount` to build the software and mount the filesystem under `./rude-mnt`
    - its corresponding backing filesystem will be located at `./rude-store`
- run `make unmount` when you are done
- run `make fg` to run the filesystem in foreground;
  this will allow you to see debugging messages.

## Hash function

You are free to choose any hash function supported by openssl.
To list what functions are available, simply type `openssl`
and look for the "Message Digest" section in its output:

        Message Digest commands (see the `dgst' command for more details)
        blake2b512        blake2s256        md4               md5
        rmd160            sha1              sha224            sha256
        sha3-224          sha3-256          sha3-384          sha3-512
        sha384            sha512            sha512-224        sha512-256
        shake128          shake256          sm3

Any of these functions can be passed to the `--hashfn=...` command line option that `rudefs` takes.

## Hash Collisions

Since `rudefs` uses a hash-indexed flat file store, one must decide
how to handle hash collisions, no matter how unlikely they are.

I let the user decide whether they want a 'complacent' or 'thorough'
collision detection policy. The default is thorough. You can switch to
'complacent' by specifying the `--complacent` command line switch to
`rudefs`.

Under the thorough policy, if a new file sent for deduplication has
its hash already in the store, we compare it bitwise with the file in
the store. If the comparison succeeds, we deduplicate the new file,
i.e., we replace it with a hard link to the one already in the store.

If the comparison fails, the new file is simply left alone, and no
deduplication is attempted.

Credits: the "ship" and "plane" JPEG files used in the test and
crafted to have an MD5 collision are courtesy of [Maarten
Bedewes](https://crypto.stackexchange.com/users/1172/maarten-bodewes)
and are presented in a [Crypto stack exchange
post](https://crypto.stackexchange.com/questions/1434/are-there-two-known-strings-which-have-the-same-md5-hash-value).

## To do
- testing harness
  - add google test
  - test multiple hash functions
- add locking during critical deduplication replacement
- large scale testing, e.g., with side-by-side pyvenv installs
- properly review similar solutions
- stats tool (dedup rates, space savings)
- recovery tools (e.g., incoming link fail)
- steal and implement whatever interesting ideas
- minimum performance benchmarking
