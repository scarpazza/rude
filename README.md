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
Before you try, ensure you have the fuse3 and openssl libraries
development packages installed, e.g.:

        sudo apt install fuse3 libfuse3-dev libssl-dev

or the equivalent packages for your distribution.

Openssl is needed for the hash functions. I'm picking up cryptographically
strong functions out of laziness.

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
